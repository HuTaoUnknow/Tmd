#pragma once
#include <QObject>
#include <QImage>
#include <QByteArray>
class QNetworkAccessManager;
class QNetworkReply;

// Local files, embedded data, and HTTP(S) share the same validation and size limits.
class ImageLoader : public QObject {
    Q_OBJECT
public:
    explicit ImageLoader(QObject *parent = nullptr);
    void load(const QString &source, const QString &documentPath);
    void cancel();
    static constexpr qint64 maximumBytes = 32 * 1024 * 1024;
    static bool readLocal(const QString &source, const QString &documentPath, QByteArray &bytes, QString *error);
    static bool decode(const QByteArray &bytes, QImage &image, QString &extension, QString *error);
signals:
    void loaded(const QByteArray &bytes, const QImage &image, const QString &extension);
    void failed(const QString &error);
private:
    void deliver(const QByteArray &bytes);
    QNetworkAccessManager *m_network;
    QNetworkReply *m_reply = nullptr;
    QByteArray m_bytes;
};
