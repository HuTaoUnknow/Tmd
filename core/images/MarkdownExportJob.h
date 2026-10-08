#pragma once
#include "core/markdown/MarkdownImages.h"
#include <QObject>
#include <QMap>
class ImageLoader;
class MarkdownExportJob : public QObject {
    Q_OBJECT
public:
    explicit MarkdownExportJob(QObject *parent = nullptr);
    void start(const QString &content, const QString &sourceDocument, const QString &destination, bool includeImages);
    void cancel();
signals:
    void progress(int done, int total);
    void finished(bool success, const QString &message);
private:
    void next();
    void commit();
    void complete(bool success, const QString &message);
    ImageLoader *m_loader;
    QString m_content, m_document, m_destination;
    QVector<MarkdownImage> m_images;
    QStringList m_sources, m_created;
    QMap<QString, QPair<QByteArray, QString>> m_assets;
    int m_index = 0;
    qint64 m_totalBytes = 0;
    bool m_active = false;
};
