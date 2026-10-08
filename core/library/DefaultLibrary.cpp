#include "core/library/DefaultLibrary.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace {
bool failure(QString *error, const QString &message) {
    if (error) *error = message;
    return false;
}
bool copyTree(const QString &source, const QString &destination, QString *error) {
    if (!QDir(source).exists()) return failure(error, QStringLiteral("找不到初始示例目录：%1").arg(source));
    if (!QDir().mkpath(destination)) return failure(error, QStringLiteral("无法创建知识库目录：%1").arg(destination));
    QDirIterator iterator(source, QDir::Files | QDir::Hidden | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString file = iterator.next();
        const QString target = QDir(destination).filePath(QDir(source).relativeFilePath(file));
        if (QFileInfo::exists(target)) continue;
        if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !QFile::copy(file, target))
            return failure(error, QStringLiteral("无法复制初始示例：%1").arg(target));
    }
    return true;
}
bool markInitialized(const QString &path, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write("Tmd library initialized\n") < 0 || !file.commit())
        return failure(error, QStringLiteral("无法保存知识库初始化状态：%1").arg(file.errorString()));
    return true;
}
}

QString DefaultLibrary::installedRoot() {
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return documents.isEmpty() ? QString() : QDir(documents).filePath(QStringLiteral("Tmd"));
}

bool DefaultLibrary::initialize(const QString &libraryRoot, const QString &examplesRoot, QString *error) {
    if (libraryRoot.isEmpty()) return failure(error, QStringLiteral("无法确定用户文档目录，请使用 --data-dir 指定知识库。"));
    const QDir root(QDir::cleanPath(QFileInfo(libraryRoot).absoluteFilePath()));
    if (!QDir().mkpath(root.path())) return failure(error, QStringLiteral("无法创建知识库：%1").arg(root.path()));
    QLockFile lock(root.filePath(QStringLiteral(".tmd-library.lock")));
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(5000)) return failure(error, QStringLiteral("知识库正在由另一窗口初始化，请稍后重试。"));
    const QString data = root.filePath(QStringLiteral("md_data"));
    const QString marker = root.filePath(QStringLiteral(".tmd-library-initialized"));
    if (QFileInfo::exists(marker)) {
        return QDir().mkpath(data) || failure(error, QStringLiteral("无法打开知识库目录：%1").arg(data));
    }
    if (QDir(data).exists() && !QDir(data).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System).isEmpty())
        return markInitialized(marker, error);

    QTemporaryDir staged(root.filePath(QStringLiteral(".tmd-seed-XXXXXX")));
    if (!staged.isValid()) return failure(error, QStringLiteral("无法准备知识库初始文件。"));
    const QDir staging(staged.path()), examples(examplesRoot);
    if (!copyTree(examples.filePath(QStringLiteral("md_data")), staging.filePath(QStringLiteral("md_data")), error)
        || !copyTree(examples.filePath(QStringLiteral("md_photo")), staging.filePath(QStringLiteral("md_photo")), error)) return false;

    const QString photo = root.filePath(QStringLiteral("md_photo"));
    if (QDir(photo).exists()) {
        if (!copyTree(staging.filePath(QStringLiteral("md_photo")), photo, error)) return false;
    } else if (!QDir().rename(staging.filePath(QStringLiteral("md_photo")), photo))
        return failure(error, QStringLiteral("无法初始化图片目录：%1").arg(photo));

    if (QDir(data).exists() && !QDir().rmdir(data))
        return failure(error, QStringLiteral("知识库在初始化期间发生变化，现有内容已保留。"));
    if (!QDir().rename(staging.filePath(QStringLiteral("md_data")), data))
        return failure(error, QStringLiteral("无法初始化文档目录：%1").arg(data));
    return markInitialized(marker, error);
}
