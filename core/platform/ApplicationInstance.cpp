#include "core/platform/ApplicationInstance.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QStandardPaths>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
constexpr int maximumRequest = 1024 * 1024;
QString normalized(const QString &path) {
    const QFileInfo info(path);
    QString result = info.canonicalFilePath();
    if (result.isEmpty()) result = info.absoluteFilePath();
    result = QDir::cleanPath(result);
#ifdef Q_OS_WIN
    result = result.toCaseFolded();
#endif
    return result;
}
}
QString ApplicationInstance::identity(const QString &executable, const QString &library) {
    return normalized(executable) + '\n' + normalized(library);
}
ApplicationInstance::ApplicationInstance(const QString &identity, const QString &runtimeDirectory, QObject *parent)
    : QObject(parent), m_server(new QLocalServer(this)) {
    const auto userScope = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    m_name = "tmd-" + QString::fromLatin1(QCryptographicHash::hash((userScope + '\n' + identity).toUtf8(), QCryptographicHash::Sha256).toHex());
    m_directory = runtimeDirectory.isEmpty() ? QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("tmd-instances") : runtimeDirectory;
    m_lock = std::make_unique<QLockFile>(QDir(m_directory).filePath(m_name + ".lock"));
    m_lock->setStaleLockTime(0);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (auto *socket = m_server->nextPendingConnection()) {
            auto buffer = std::make_shared<QByteArray>();
            auto handled = std::make_shared<bool>(false);
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            auto receive = [this, socket, buffer, handled] {
                if (*handled) return;
                *buffer += socket->readAll();
                if (buffer->size() > maximumRequest) { *handled = true; socket->abort(); return; }
                const int end = buffer->indexOf('\n');
                if (end < 0) return;
                *handled = true;
                QJsonParseError error;
                const auto document = QJsonDocument::fromJson(buffer->left(end), &error);
                const auto envelope = document.object();
                if (error.error != QJsonParseError::NoError || !document.isObject()
                    || envelope.value("version").toInt() != 1 || !envelope.value("request").isObject()) {
                    socket->abort(); return;
                }
                m_requests.append(envelope.value("request").toObject());
                socket->write("OK\n"); socket->flush(); socket->disconnectFromServer();
                emit requestsAvailable();
            };
            connect(socket, &QLocalSocket::readyRead, this, receive);
            if (socket->bytesAvailable()) receive();
            QTimer::singleShot(10000, socket, [socket] { socket->abort(); socket->deleteLater(); });
        }
    });
}
ApplicationInstance::~ApplicationInstance() {
    if (m_primary) { m_server->close(); QLocalServer::removeServer(m_name); }
}
bool ApplicationInstance::listen(QString *error) {
    // Only the lock owner may remove an endpoint left behind by a crash.
    QLocalServer::removeServer(m_name);
    if (!m_server->listen(m_name)) {
        if (error) *error = m_server->errorString();
        m_lock->unlock(); return false;
    }
    m_primary = true; return true;
}
ApplicationInstance::Result ApplicationInstance::start(const QJsonObject &request, QString *error, int timeoutMs) {
    if (m_primary) return Result::Primary;
    if (!QDir().mkpath(m_directory)) { if (error) *error = QStringLiteral("无法创建启动协调目录：%1").arg(m_directory); return Result::Failed; }
    const auto payload = QJsonDocument(QJsonObject{{"version", 1}, {"request", request}}).toJson(QJsonDocument::Compact) + '\n';
    if (payload.size() > maximumRequest) { if (error) *error = QStringLiteral("打开文件的启动参数过长。"); return Result::Failed; }
    QElapsedTimer elapsed; elapsed.start();
    while (elapsed.elapsed() < timeoutMs) {
        if (m_lock->tryLock(0)) return listen(error) ? Result::Primary : Result::Failed;
        if (m_lock->error() == QLockFile::PermissionError) {
            if (error) *error = QStringLiteral("无法访问启动协调锁。"); return Result::Failed;
        }
        QLocalSocket socket; socket.connectToServer(m_name);
        if (socket.waitForConnected(qMax(1, qMin(100, timeoutMs - int(elapsed.elapsed()))))) {
#ifdef Q_OS_WIN
            qint64 pid = 0; QString host, app;
            if (m_lock->getLockInfo(&pid, &host, &app) && pid > 0) AllowSetForegroundWindow(DWORD(pid));
#endif
            socket.write(payload);
            if (socket.bytesToWrite() && !socket.waitForBytesWritten(qMax(1, timeoutMs - int(elapsed.elapsed())))) break;
            QByteArray reply;
            while (elapsed.elapsed() < timeoutMs) {
                reply += socket.readAll();
                if (reply.startsWith("OK\n")) return Result::Forwarded;
                if (!socket.waitForReadyRead(qMax(1, timeoutMs - int(elapsed.elapsed())))) {
                    reply += socket.readAll();
                    if (reply.startsWith("OK\n")) return Result::Forwarded;
                    break;
                }
            }
            // A connected owner may already have accepted the request. Never
            // retry delivery or start a second window after an uncertain ACK.
            break;
        }
        QThread::msleep(25);
    }
    if (error) *error = QStringLiteral("已有 Tmd 正在启动或暂时无法响应，请稍后再次打开。");
    return Result::Failed;
}
QVector<QJsonObject> ApplicationInstance::takeRequests() {
    QVector<QJsonObject> result; result.swap(m_requests); return result;
}
