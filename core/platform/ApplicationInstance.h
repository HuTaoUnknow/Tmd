#pragma once
#include <QObject>
#include <QJsonObject>
#include <QVector>
#include <memory>
class QLocalServer;
class QLockFile;

// One GUI owner per executable/user/library. Later launches deliver requests
// over a local socket; the lock also covers the owner's startup interval.
class ApplicationInstance : public QObject {
    Q_OBJECT
public:
    enum class Result { Primary, Forwarded, Failed };
    explicit ApplicationInstance(const QString &identity, const QString &runtimeDirectory = {}, QObject *parent = nullptr);
    ~ApplicationInstance() override;
    static QString identity(const QString &executable, const QString &library);
    Result start(const QJsonObject &request, QString *error = nullptr, int timeoutMs = 5000);
    QVector<QJsonObject> takeRequests();
signals:
    void requestsAvailable();
private:
    bool listen(QString *error);
    QString m_directory, m_name;
    std::unique_ptr<QLockFile> m_lock;
    QLocalServer *m_server;
    QVector<QJsonObject> m_requests;
    bool m_primary = false;
};
