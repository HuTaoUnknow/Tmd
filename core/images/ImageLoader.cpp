#include "core/images/ImageLoader.h"
#include "core/markdown/MarkdownImages.h"
#include <QBuffer>
#include <QFile>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QCoreApplication>
#include <QTimer>

namespace { bool fail(QString *error, const QString &message) { if (error) *error = message; return false; } }
ImageLoader::ImageLoader(QObject *parent) : QObject(parent), m_network(new QNetworkAccessManager(this)) {}
bool ImageLoader::readLocal(const QString &source, const QString &document, QByteArray &bytes, QString *error) {
    if (MarkdownImages::isData(source)) {
        const int comma = source.indexOf(',');
        if (comma < 0 || source.size() > maximumBytes * 4 / 3 + 4096) return fail(error, QStringLiteral("内嵌图片格式错误或超过 32 MB。"));
        const QByteArray payload = QByteArray::fromPercentEncoding(source.mid(comma + 1).toLatin1());
        if (source.left(comma).endsWith(";base64", Qt::CaseInsensitive)) {
            const auto decoded = QByteArray::fromBase64Encoding(payload, QByteArray::AbortOnBase64DecodingErrors);
            if (!decoded) return fail(error, QStringLiteral("内嵌图片的 Base64 数据无效。"));
            bytes = decoded.decoded;
        } else bytes = payload;
        if (bytes.size() > maximumBytes) return fail(error, QStringLiteral("图片超过 32 MB。"));
        return true;
    }
    const QString path = MarkdownImages::localPath(source, document);
    if (path.isEmpty()) return fail(error, QStringLiteral("不支持的图片地址。请选择本地文件、HTTP(S) 图床或内嵌图片。"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, QStringLiteral("无法读取图片 %1：%2").arg(path, file.errorString()));
    if (file.size() > maximumBytes) return fail(error, QStringLiteral("图片超过 32 MB：%1").arg(path));
    bytes = file.read(maximumBytes + 1);
    if (file.error() != QFile::NoError) return fail(error, file.errorString());
    return bytes.size() <= maximumBytes || fail(error, QStringLiteral("图片超过 32 MB。"));
}
bool ImageLoader::decode(const QByteArray &bytes, QImage &image, QString &extension, QString *error) {
    QBuffer buffer; buffer.setData(bytes); buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer); reader.setDecideFormatFromContent(true);
    const QSize size = reader.size();
    if (size.isValid() && qint64(size.width()) * size.height() > 64000000) return fail(error, QStringLiteral("图片尺寸超过 6400 万像素。"));
    const QByteArray format = reader.format(); image = reader.read();
    if (image.isNull()) return fail(error, QStringLiteral("无法解析图片：%1").arg(reader.errorString()));
    extension = QString::fromLatin1(format).toLower();
    if (extension == "jpeg") extension = "jpg";
    if (extension.isEmpty()) extension = "png";
    return true;
}
void ImageLoader::cancel() {
    if (m_reply) { auto *reply = m_reply; m_reply = nullptr; reply->disconnect(this); reply->abort(); reply->deleteLater(); }
    m_bytes.clear();
}
void ImageLoader::deliver(const QByteArray &bytes) {
    QImage image; QString extension, error;
    if (!decode(bytes, image, extension, &error)) emit failed(error);
    else emit loaded(bytes, image, extension);
}
void ImageLoader::load(const QString &source, const QString &document) {
    cancel();
    if (!MarkdownImages::isRemote(source)) {
        QByteArray bytes; QString error;
        if (!readLocal(source, document, bytes, &error)) emit failed(error); else deliver(bytes);
        return;
    }
    QNetworkRequest request{QUrl(source)};
    request.setTransferTimeout(15000);
    request.setMaximumRedirectsAllowed(5);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent", ("Tmd/" + QCoreApplication::applicationVersion()).toUtf8());
    m_reply = m_network->get(request);
    auto read = [this] {
        if (!m_reply) return;
        const qint64 available = m_reply->bytesAvailable();
        if (m_bytes.size() + available > maximumBytes) {
            cancel(); emit failed(QStringLiteral("图床图片超过 32 MB。")); return;
        }
        m_bytes += m_reply->readAll();
    };
    connect(m_reply, &QNetworkReply::readyRead, this, read);
    connect(m_reply, &QNetworkReply::finished, this, [this, read] {
        if (!m_reply) return;
        read(); if (!m_reply) return;
        auto *reply = m_reply; m_reply = nullptr; reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) emit failed(QStringLiteral("读取图床图片失败：%1").arg(reply->errorString()));
        else deliver(m_bytes);
        m_bytes.clear();
    });
    auto *deadline = new QTimer(m_reply); deadline->setSingleShot(true); deadline->start(30000);
    connect(deadline, &QTimer::timeout, this, [this] { cancel(); emit failed(QStringLiteral("读取图床图片超时。")); });
}
