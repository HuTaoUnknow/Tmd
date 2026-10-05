#include "MarkdownFileIO.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringDecoder>
#include <QThread>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace {
bool fail(QString *error, const QString &message) {
    if (error) *error = message;
    return false;
}
}
bool MarkdownFileIO::readFile(const QString &path, QString &content, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, file.errorString());
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) return fail(error, file.errorString());
    QStringDecoder decoder(QStringDecoder::Utf8);
    QString decoded = decoder(bytes);
    if (decoder.hasError()) return fail(error, QStringLiteral("文件不是有效的 UTF-8：%1").arg(path));
    // QTextDocument works with LF internally; preserve the existing line ending on save.
    decoded.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    content = decoded;
    if (error) error->clear();
    return true;
}
bool MarkdownFileIO::writeFile(const QString &path, const QString &content, QString *error) {
    QString output = content;
    QFile old(path);
    if (old.open(QIODevice::ReadOnly) && old.read(65536).contains("\r\n")) {
        output.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        output.replace(QStringLiteral("\n"), QStringLiteral("\r\n"));
    }
    // Windows cannot replace a destination while this read handle remains open.
    old.close();
    const QByteArray bytes = output.toUtf8();
    QString lastError;
    for (int attempt = 0; attempt < 4; ++attempt) {
        bool retry = false;
        {
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly)) return fail(error, file.errorString());
            if (file.write(bytes) != bytes.size()) return fail(error, file.errorString());
            if (file.commit()) { if (error) error->clear(); return true; }
            lastError = file.errorString();
#ifdef Q_OS_WIN
            // A scanner or another reader may briefly deny replacement on Windows.
            // Keep atomic replacement; never fall back to truncating the original.
            retry = file.error() == QFileDevice::RenameError || file.error() == QFileDevice::PermissionsError;
#endif
        }
        if (!retry || attempt == 3) break;
        QThread::msleep(30U << attempt);
    }
    return fail(error, lastError);
}
bool MarkdownFileIO::exists(const QString &path) { return QFileInfo(path).isFile(); }
bool MarkdownFileIO::removeFile(const QString &path, QString *error) {
    QFile file(path);
    if (!file.remove()) return fail(error, file.errorString());
    return true;
}
bool MarkdownFileIO::moveToTrash(const QString &path, QString *error, QString *pathInTrash) {
    QFile file(path);
    // Qt's Windows shell progress sink rejects permanent deletion. Never fall back to remove().
    if (!file.moveToTrash())
        return fail(error, QStringLiteral("无法移入系统回收站，未执行永久删除：%1").arg(file.errorString()));
    if (pathInTrash) *pathInTrash = file.fileName();
    if (error) error->clear();
    return true;
}
bool MarkdownFileIO::renameFile(const QString &from, const QString &to, QString *error) {
    QFile file(from);
    if (!file.rename(to)) return fail(error, file.errorString());
    return true;
}
QString MarkdownFileIO::normalizedPath(const QString &path) {
    const QFileInfo info(path);
    return QDir::fromNativeSeparators(info.exists() ? info.canonicalFilePath() : info.absoluteFilePath());
}
QString MarkdownFileIO::fileIdentity(const QString &path) {
#ifdef Q_OS_WIN
    const HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return {};
    BY_HANDLE_FILE_INFORMATION info{};
    const bool ok = GetFileInformationByHandle(handle, &info);
    CloseHandle(handle);
    if (!ok) return {};
    return QStringLiteral("%1:%2:%3").arg(info.dwVolumeSerialNumber).arg(info.nFileIndexHigh).arg(info.nFileIndexLow);
#else
    struct stat info{};
    if (::stat(QFile::encodeName(path).constData(), &info) != 0) return {};
    return QStringLiteral("%1:%2").arg(static_cast<qulonglong>(info.st_dev)).arg(static_cast<qulonglong>(info.st_ino));
#endif
}
