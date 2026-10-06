#pragma once
#include "MarkdownSettings.h"
#include <QObject>
#include <QByteArray>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class ImageUploadJob : public QObject {
    Q_OBJECT
public:
    explicit ImageUploadJob(QObject *parent = nullptr);
    void start(const QByteArray &image, const QString &fileName, const MarkdownSettings &settings);
    void cancel();
signals:
    void uploaded(const QString &url);
    void failed(const QString &message);
    void progress(qint64 sent, qint64 total);
private:
    QNetworkAccessManager *m_network;
    QNetworkReply *m_reply = nullptr;
    QTimer *m_timeout;
    QString m_urlPath;
    bool m_cancelled = false, m_timedOut = false, m_responseTooLarge = false;
};
