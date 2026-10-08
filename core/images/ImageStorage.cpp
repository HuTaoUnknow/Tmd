#include "core/images/ImageStorage.h"
#include "core/images/ImageLoader.h"
#include "core/markdown/MarkdownImages.h"
#include "core/library/MarkdownFileIO.h"
#include "core/settings/MarkdownSettings.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUrl>

namespace { bool fail(QString *error, const QString &message) { if (error) *error = message; return false; } }
ImageStorage::ImageStorage(QString root, QString photoRoot) : m_dataRoot(QDir(root).absolutePath()),
    m_photoRoot(photoRoot.isEmpty() ? QDir(root).absoluteFilePath(MarkdownSettingsStore::current().relativePhotoPath) : QDir(photoRoot).absolutePath()) {
    m_photoRoot = QDir::cleanPath(m_photoRoot);
}
QString ImageStorage::absolutePhotoRoot(const QString &dataRoot) {
    const auto &settings = MarkdownSettingsStore::current();
    return settings.absolutePhotoPath.isEmpty() ? ImageStorage(dataRoot).photoRoot() : settings.absolutePhotoPath;
}
bool ImageStorage::ensureRoot(QString *error) const {
    return QDir().mkpath(m_photoRoot) || fail(error, QStringLiteral("无法创建图片目录：%1").arg(m_photoRoot));
}
bool ImageStorage::writeAsset(const QString &directory, const QByteArray &bytes, const QString &extension, QString &path, bool &created, QString *error) {
    created = false;
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    // Use the full digest so differing images can never overwrite a same-named source file.
    path = QDir(directory).filePath("image_" + hash + "." + extension);
    if (QFileInfo::exists(path)) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly) && file.readAll() == bytes) return true;
        return fail(error, QStringLiteral("目标图片已存在但内容不同：%1").arg(path));
    }
    if (!QDir().mkpath(directory)) return fail(error, QStringLiteral("无法创建图片目录：%1").arg(directory));
    QSaveFile file(path); file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) return fail(error, QStringLiteral("无法保存图片：%1").arg(file.errorString()));
    created = true; return true;
}
bool ImageStorage::store(const QString &relative, const QByteArray &bytes, QString &reference, QStringList *created, QString *error) const {
    const QString document = QDir(m_dataRoot).absoluteFilePath(relative);
    const QString normalized = QDir::cleanPath(relative);
    if (QDir::isAbsolutePath(relative) || normalized == ".." || normalized.startsWith("../") || !normalized.endsWith(".md", Qt::CaseInsensitive))
        return fail(error, QStringLiteral("图片必须归属于 md_data 内的 Markdown 文档。"));
    const QString subdirectory = QFileInfo(normalized).path();
    const QString directory = QDir(m_photoRoot).absoluteFilePath(subdirectory);
    // Reject directory links that would put managed assets outside md_photo.
    QFileInfo ancestor(directory);
    while (!ancestor.exists() && ancestor.absoluteFilePath() != ancestor.absolutePath()) ancestor = QFileInfo(ancestor.absolutePath());
    const QString root = MarkdownFileIO::normalizedPath(m_photoRoot), resolved = MarkdownFileIO::normalizedPath(ancestor.absoluteFilePath());
#ifdef Q_OS_WIN
    const auto sensitivity = Qt::CaseInsensitive;
#else
    const auto sensitivity = Qt::CaseSensitive;
#endif
    if (QFileInfo::exists(m_photoRoot) && resolved.compare(root, sensitivity) != 0 && !resolved.startsWith(root + '/', sensitivity))
        return fail(error, QStringLiteral("图片子目录指向了 md_photo 以外的位置。"));
    QImage image; QString extension;
    if (!ImageLoader::decode(bytes, image, extension, error)) return false;
    QString path; bool written;
    if (!writeAsset(directory, bytes, extension, path, written, error)) return false;
    if (written && created) created->append(path);
    reference = MarkdownImages::encodedPath(QDir(QFileInfo(document).absolutePath()).relativeFilePath(path));
    return true;
}
bool ImageStorage::storeFile(const QString &relative, const QString &source, QString &reference, QStringList *created, QString *error) const {
    QByteArray bytes;
    return ImageLoader::readLocal(source, QDir(m_dataRoot).filePath(relative), bytes, error) && store(relative, bytes, reference, created, error);
}
bool ImageStorage::storeImage(const QString &relative, const QImage &image, QString &reference, QString *error) const {
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) return fail(error, QStringLiteral("无法编码剪贴板图片。"));
    return store(relative, bytes, reference, nullptr, error);
}
bool ImageStorage::prepareImport(const QString &source, const QString &relative, const QString &content, QString &rewritten, QStringList &created, QString *error) const {
    const auto images = MarkdownImages::parse(content); QMap<QString, QString> replacements;
    for (const auto &image : images) {
        if (MarkdownImages::isRemote(image.source) || replacements.contains(image.source)) continue;
        QByteArray bytes; QString reference;
        if (!ImageLoader::readLocal(image.source, source, bytes, error) || !store(relative, bytes, reference, &created, error)) {
            rollback(created); created.clear(); return false;
        }
        replacements.insert(image.source, reference);
    }
    rewritten = MarkdownImages::replace(content, images, replacements); return true;
}
void ImageStorage::rollback(const QStringList &created) const {
    for (const auto &path : created) {
        QFile::remove(path);
        QString directory = QFileInfo(path).absolutePath();
        while (directory != m_photoRoot && directory.startsWith(m_photoRoot + '/')) {
            if (!QDir().rmdir(directory)) break;
            directory = QFileInfo(directory).absolutePath();
        }
    }
}
bool ImageStorage::prepareRelocation(const QString &source, const QString &relative, const QString &content, QString &rewritten, QStringList &created, QString *error) const {
    const auto images = MarkdownImages::parse(content); QMap<QString, QString> replacements;
    const QString target = QDir(m_dataRoot).filePath(relative);
    for (const auto &image : images) {
        if (MarkdownImages::isRemote(image.source) || MarkdownImages::isData(image.source) || replacements.contains(image.source)) continue;
        const QString path = MarkdownImages::localPath(image.source, source);
        if (path.isEmpty()) continue;
        const QString decoded = QUrl::fromPercentEncoding(image.source.toUtf8());
        if (QDir::isAbsolutePath(decoded) || image.source.startsWith("file:", Qt::CaseInsensitive)) continue;
        QString reference;
        if (MarkdownFileIO::normalizedPath(path).startsWith(MarkdownFileIO::normalizedPath(m_photoRoot) + '/', Qt::CaseInsensitive)) {
            QByteArray bytes;
            if (!ImageLoader::readLocal(image.source, source, bytes, error) || !store(relative, bytes, reference, &created, error)) {
                rollback(created); created.clear(); return false;
            }
        } else reference = MarkdownImages::encodedPath(QDir(QFileInfo(target).absolutePath()).relativeFilePath(path));
        replacements.insert(image.source, reference);
    }
    rewritten = MarkdownImages::replace(content, images, replacements); return true;
}
