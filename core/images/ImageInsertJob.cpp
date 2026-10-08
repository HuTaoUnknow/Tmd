#include "core/images/ImageInsertJob.h"
#include "core/images/ImageUploadJob.h"
#include "core/images/ImageLoader.h"
#include "core/images/ImageStorage.h"
#include "core/markdown/MarkdownImages.h"
#include <QBuffer>
#include <QDir>
#include <QFileInfo>

ImageInsertJob::ImageInsertJob(QString root, QString document, MarkdownSettings settings, QObject *parent)
    : QObject(parent), m_root(std::move(root)), m_document(std::move(document)), m_settings(std::move(settings)) {
    m_photoRoot = m_settings.imageMode == 1 && !m_settings.absolutePhotoPath.isEmpty()
        ? m_settings.absolutePhotoPath : QDir(m_root).absoluteFilePath(m_settings.relativePhotoPath);
}
void ImageInsertJob::start(const QImage &image) {
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) { emit failed(QStringLiteral("无法编码剪贴板图片。")); return; }
    if (m_settings.imageMode == 2 && !m_settings.uploadEndpoint.isEmpty()) upload(bytes, QStringLiteral("粘贴图片.png"));
    else store(bytes, QStringLiteral("粘贴的图片"));
}
void ImageInsertJob::start(const QUrl &source) {
    const QString name = QFileInfo(source.isLocalFile() ? source.toLocalFile() : source.path()).fileName();
    if (source.isLocalFile()) {
        QByteArray bytes; QString error;
        if (!ImageLoader::readLocal(source.toLocalFile(), {}, bytes, &error)) { emit failed(error); return; }
        if (m_settings.imageMode == 2 && !m_settings.uploadEndpoint.isEmpty()) { upload(bytes, name); return; }
        if (m_settings.imageMode == 1 && m_settings.absolutePhotoPath.isEmpty()) {
            QImage image; QString extension;
            if (!ImageLoader::decode(bytes, image, extension, &error)) { emit failed(error); return; }
            emit ready(MarkdownImages::encodedPath(source.toLocalFile()), name, {}); return;
        }
        store(bytes, name);
    } else if (MarkdownImages::isRemote(source.toString())) {
        if (m_settings.imageMode == 2) { emit ready(source.toString(QUrl::FullyEncoded), name, {}); return; }
        auto *loader = new ImageLoader(this);
        connect(loader, &ImageLoader::loaded, this, [this, name](const QByteArray &bytes, const QImage &, const QString &) { store(bytes, name); });
        connect(loader, &ImageLoader::failed, this, &ImageInsertJob::failed);
        loader->load(source.toString(), QDir(m_root).filePath(m_document));
    } else emit failed(QStringLiteral("不支持的图片地址。"));
}
void ImageInsertJob::store(const QByteArray &bytes, const QString &name) {
    // The job captures its destination and mode before asynchronous work starts.
    QString reference, error;
    if (!ImageStorage(m_root, m_photoRoot).store(m_document, bytes, reference, nullptr, &error)) { emit failed(error); return; }
    if (m_settings.imageMode == 1) reference = MarkdownImages::encodedPath(MarkdownImages::localPath(reference, QDir(m_root).filePath(m_document)));
    emit ready(reference, name, m_settings.imageMode == 2 ? QStringLiteral("图片已按相对路径保存；可在图片保存方式中配置图床上传接口。") : QString{});
}
void ImageInsertJob::upload(const QByteArray &bytes, const QString &name) {
    m_upload = new ImageUploadJob(this);
    connect(m_upload, &ImageUploadJob::progress, this, &ImageInsertJob::progress);
    connect(m_upload, &ImageUploadJob::failed, this, &ImageInsertJob::failed);
    connect(m_upload, &ImageUploadJob::uploaded, this, [this, name](const QString &url) {
        QString error;
        MarkdownSettingsStore::rememberHostedLink(name, url, &error);
        emit ready(url, name, error);
    });
    emit uploadStarted(name);
    m_upload->start(bytes, name, m_settings);
}
void ImageInsertJob::cancel() { if (m_upload) m_upload->cancel(); }
