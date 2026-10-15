#include "core/library/MarkdownFileIO.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {
QString nativePath(const QString &path) {
    QString native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
    if (!native.startsWith(QStringLiteral("\\\\?\\")))
        native = native.startsWith(QStringLiteral("\\\\")) ? QStringLiteral("\\\\?\\UNC\\") + native.mid(2) : QStringLiteral("\\\\?\\") + native;
    return native;
}
DWORD renameOnly(const QString &from, const QString &to) {
    const QString source = nativePath(from), target = nativePath(to);
    DWORD code = ERROR_SUCCESS;
    for (int attempt = 0; attempt < 4; ++attempt) {
        if (MoveFileExW(reinterpret_cast<LPCWSTR>(source.utf16()), reinterpret_cast<LPCWSTR>(target.utf16()), 0)) return ERROR_SUCCESS;
        code = GetLastError();
        if (attempt == 3 || (code != ERROR_SHARING_VIOLATION && code != ERROR_LOCK_VIOLATION && code != ERROR_ACCESS_DENIED)) break;
        QThread::msleep(30U << attempt);
    }
    return code;
}
QString lockedPath(const QString &root) {
    const auto locked = [](const QString &path) {
        const QString native = nativePath(path);
        const HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(native.utf16()), DELETE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle != INVALID_HANDLE_VALUE) { CloseHandle(handle); return false; }
        const DWORD reason = GetLastError();
        return reason == ERROR_SHARING_VIOLATION || reason == ERROR_LOCK_VIOLATION;
    };
    if (locked(root)) return root;
    QDirIterator paths(root, QDir::Files | QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (paths.hasNext()) { const QString path = paths.next(); if (locked(path)) return path; }
    return {};
}
struct RenameResult {
    DWORD code = ERROR_SUCCESS;
    bool sourceIntact = true;
    QStringList retained;
};
RenameResult renameWithStaging(const QString &from, const QString &to, int depth = 0) {
    RenameResult result; result.code = renameOnly(from, to);
    if (result.code != ERROR_ACCESS_DENIED || depth >= 64 || QFileInfo::exists(to)) return result;
    const QString native = nativePath(from);
    const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(native.utf16()));
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)
        || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) || !lockedPath(from).isEmpty()) return result;
    const auto entries = QDir(from).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
    if (entries.isEmpty()) return result;

    // Windows rejects an ancestor rename while a descendant is open, even when
    // that descendant shares DELETE and can itself be renamed. Stage entries on
    // the same volume, rename the now-empty original directory, then restore its
    // contents. No copies, replacements, or recursive deletion are involved.
    QTemporaryDir staging(QDir(QFileInfo(from).absolutePath()).filePath(".tmd-move-XXXXXX"));
    if (!staging.isValid()) return result;
    staging.setAutoRemove(false); // A failed rollback must never erase user data.
    struct Entry { QString original, staged, target; enum State { Original, Staged, Target } state = Original; };
    QList<Entry> moves;
    for (const auto &entry : entries)
        moves.append({entry.absoluteFilePath(), staging.filePath(entry.fileName()), QDir(to).filePath(entry.fileName())});
    bool rootMoved = false, failed = false, nestedIntact = true;
    const auto move = [&](const QString &source, const QString &destination) {
        const auto step = renameWithStaging(source, destination, depth + 1);
        if (step.code) {
            result.code = step.code; nestedIntact = nestedIntact && step.sourceIntact;
            result.retained.append(step.retained); return false;
        }
        return true;
    };
    for (auto &entry : moves) {
        if (!move(entry.original, entry.staged)) { failed = true; break; }
        entry.state = Entry::Staged;
    }
    if (!failed) {
        result.code = renameOnly(from, to);
        rootMoved = result.code == ERROR_SUCCESS; failed = !rootMoved;
    }
    if (!failed) {
        for (auto &entry : moves) {
            if (!move(entry.staged, entry.target)) { failed = true; break; }
            entry.state = Entry::Target;
        }
    }
    if (!failed) {
        QDir().rmdir(staging.path()); result.code = ERROR_SUCCESS; return result;
    }

    const DWORD failure = result.code;
    bool restored = nestedIntact;
    if (rootMoved) {
        bool emptied = true;
        for (auto it = moves.rbegin(); it != moves.rend(); ++it) {
            if (it->state != Entry::Target) continue;
            if (move(it->target, it->staged)) it->state = Entry::Staged;
            else { emptied = false; restored = false; }
        }
        if (emptied && renameOnly(to, from) == ERROR_SUCCESS) rootMoved = false;
        else restored = false;
    }
    for (auto &entry : moves) {
        if (entry.state != Entry::Staged) continue;
        if (move(entry.staged, rootMoved ? entry.target : entry.original))
            entry.state = rootMoved ? Entry::Target : Entry::Original;
        else restored = false;
    }
    QDir().rmdir(staging.path()); // Removes only an empty staging directory.
    result.code = failure; result.sourceIntact = restored && nestedIntact && !rootMoved;
    if (!result.sourceIntact) {
        result.retained.append(from);
        if (QFileInfo::exists(to)) result.retained.append(to);
        if (QFileInfo::exists(staging.path())) result.retained.append(staging.path());
        result.retained.removeDuplicates();
    }
    return result;
}
}
#endif

bool MarkdownFileIO::renameDirectory(const QString &from, const QString &to, QString *error, bool *sourceIntact) {
    if (sourceIntact) *sourceIntact = true;
#ifdef Q_OS_WIN
    const auto result = renameWithStaging(from, to);
    if (sourceIntact) *sourceIntact = result.sourceIntact;
    if (!result.code) { if (error) error->clear(); return true; }
    LPWSTR buffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, result.code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    const QString reason = buffer ? QString::fromWCharArray(buffer).trimmed() : QString{};
    if (buffer) LocalFree(buffer);
    QString message = QStringLiteral("无法移动文件夹：%1\n目标：%2\n系统错误（Windows %3）：%4").arg(from, to).arg(result.code).arg(reason);
    const QString locked = lockedPath(from);
    if (!locked.isEmpty()) message += QStringLiteral("\n被占用的路径：%1\n关闭占用该路径的应用后重试。").arg(locked);
    if (!result.sourceIntact)
        message += QStringLiteral("\n还原未完成，已保留全部剩余文件，请检查以下位置：\n%1").arg(result.retained.join('\n'));
    if (error) *error = message;
    return false;
#else
    if (!QDir().rename(from, to)) {
        if (error) *error = QStringLiteral("无法移动文件夹：%1\n目标：%2").arg(from, to);
        return false;
    }
    if (error) error->clear(); return true;
#endif
}
