#pragma once
#include "core/settings/MarkdownSettings.h"
#include <QObject>
#include <QImage>
#include <QUrl>
class ImageUploadJob;
class ImageInsertJob : public QObject {
    Q_OBJECT
public:
    ImageInsertJob(QString dataRoot, QString document, MarkdownSettings settings, QObject *parent = nullptr);
    void start(const QUrl &source);
    void start(const QImage &image);
    void cancel();
signals:
    void ready(const QString &reference, const QString &name, const QString &notice);
    void failed(const QString &error);
    void uploadStarted(const QString &name);
    void progress(qint64 sent, qint64 total);
private:
    void store(const QByteArray &bytes, const QString &name);
    void upload(const QByteArray &bytes, const QString &name);
    QString m_root, m_document, m_photoRoot;
    MarkdownSettings m_settings;
    ImageUploadJob *m_upload = nullptr;
};
