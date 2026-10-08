#include "core/images/ImageUploadJob.h"
#include "core/images/ImageLoader.h"
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QCoreApplication>
#include <QTimer>
#include <QUrlQuery>

ImageUploadJob::ImageUploadJob(QObject *parent) : QObject(parent), m_network(new QNetworkAccessManager(this)), m_timeout(new QTimer(this)) {
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] { m_timedOut = true; if (m_reply) m_reply->abort(); });
}
void ImageUploadJob::cancel() { m_cancelled = true; if (m_reply) m_reply->abort(); }
void ImageUploadJob::start(const QByteArray &bytes, const QString &fileName, const MarkdownSettings &settings) {
    if (m_reply) return;
    QString error; QImage image; QString extension;
    if (bytes.size() > ImageLoader::maximumBytes || !MarkdownSettingsStore::validate(settings, &error) || settings.uploadEndpoint.isEmpty()
        || !ImageLoader::decode(bytes, image, extension, &error)) {
        emit failed(error.isEmpty() ? QStringLiteral("请先配置图床上传接口。") : error); return;
    }
    m_cancelled = m_timedOut = m_responseTooLarge = false; m_urlPath = settings.uploadUrlPath;
    QUrl endpoint(settings.uploadEndpoint);
    if (settings.uploadAuthKind == 3 && !settings.uploadToken.isEmpty()) {
        QUrlQuery query(endpoint); query.removeAllQueryItems(settings.uploadAuthName);
        query.addQueryItem(settings.uploadAuthName, settings.uploadAuthPrefix + settings.uploadToken); endpoint.setQuery(query);
    }
    QNetworkRequest request(endpoint); request.setHeader(QNetworkRequest::UserAgentHeader, ("Tmd/" + QCoreApplication::applicationVersion()).toUtf8());
    request.setRawHeader("Accept", "application/json");
    // Uploads carry credentials and image content. Do not resend them to an
    // unconfigured redirect destination; configure the final endpoint instead.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    if (settings.uploadAuthKind == 1 && !settings.uploadToken.isEmpty())
        request.setRawHeader(settings.uploadAuthName.toUtf8(), (settings.uploadAuthPrefix + settings.uploadToken).toUtf8());
    auto *multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    auto field = [multipart](const QString &name, const QString &value) {
        QHttpPart part; part.setHeader(QNetworkRequest::ContentDispositionHeader, QString("form-data; name=\"%1\"").arg(name));
        part.setBody(value.toUtf8()); multipart->append(part);
    };
    for (auto it = settings.uploadFields.cbegin(); it != settings.uploadFields.cend(); ++it) field(it.key(), it.value());
    if (settings.uploadAuthKind == 2 && !settings.uploadToken.isEmpty()) field(settings.uploadAuthName, settings.uploadAuthPrefix + settings.uploadToken);
    QString safeName = QFileInfo(fileName).fileName(); safeName.replace('"', '_'); safeName.replace('\r', '_'); safeName.replace('\n', '_');
    if (safeName.isEmpty()) safeName = "image." + extension;
    QHttpPart file; file.setHeader(QNetworkRequest::ContentDispositionHeader, QString("form-data; name=\"%1\"; filename=\"%2\"").arg(settings.uploadFileField, safeName));
    file.setHeader(QNetworkRequest::ContentTypeHeader, QMimeDatabase().mimeTypeForData(bytes).name()); file.setBody(bytes); multipart->append(file);
    m_reply = m_network->post(request, multipart); multipart->setParent(m_reply);
    connect(m_reply, &QNetworkReply::uploadProgress, this, &ImageUploadJob::progress);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] { if (m_reply && m_reply->bytesAvailable() > 2 * 1024 * 1024) { m_responseTooLarge = true; m_reply->abort(); } });
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        m_timeout->stop(); auto *reply = m_reply; m_reply = nullptr; reply->deleteLater();
        if (m_cancelled) { emit failed(QStringLiteral("已取消图片上传。")); return; }
        if (m_timedOut) { emit failed(QStringLiteral("图片上传超时，请稍后重试。")); return; }
        if (m_responseTooLarge) { emit failed(QStringLiteral("图床接口返回内容超过 2 MB。")); return; }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
            emit failed(QStringLiteral("图床上传失败（HTTP %1），请检查接口地址和鉴权参数。").arg(status)); return;
        }
        QJsonParseError error; const auto json = QJsonDocument::fromJson(reply->readAll(), &error);
        QJsonValue value = json.isArray() ? QJsonValue(json.array()) : QJsonValue(json.object());
        for (const auto &key : m_urlPath.split('.')) {
            if (value.isObject()) value = value.toObject().value(key);
            else if (value.isArray()) { bool valid = false; const int index = key.toInt(&valid); value = valid && index >= 0 && index < value.toArray().size() ? value.toArray()[index] : QJsonValue{}; }
            else { value = {}; break; }
        }
        const QUrl url(value.toString(), QUrl::StrictMode);
        if (error.error != QJsonParseError::NoError || !url.isValid() || url.host().isEmpty() || (url.scheme() != "http" && url.scheme() != "https")) {
            emit failed(QStringLiteral("图床响应未找到有效图片链接，请检查返回链接字段：%1").arg(m_urlPath)); return;
        }
        emit uploaded(url.toString(QUrl::FullyEncoded));
    });
    m_timeout->start(30000);
}
