#include "MarkdownExportJob.h"
#include "ImageLoader.h"
#include "ImageStorage.h"
#include "MarkdownFileIO.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
MarkdownExportJob::MarkdownExportJob(QObject *parent) : QObject(parent), m_loader(new ImageLoader(this)) {
    connect(m_loader, &ImageLoader::loaded, this, [this](const QByteArray &bytes, const QImage &, const QString &extension) {
        if (!m_active) return;
        m_totalBytes += bytes.size();
        if (m_totalBytes > 256 * 1024 * 1024) { complete(false, QStringLiteral("本次导出图片合计超过 256 MB。")); return; }
        m_assets.insert(m_sources[m_index], {bytes, extension}); ++m_index;
        emit progress(m_index, m_sources.size()); QTimer::singleShot(0, this, &MarkdownExportJob::next);
    });
    connect(m_loader, &ImageLoader::failed, this, [this](const QString &error) { if (m_active) complete(false, error); });
}
void MarkdownExportJob::start(const QString &content, const QString &source, const QString &destination, bool includeImages) {
    if (m_active) cancel();
    m_active = true; m_content = content; m_document = source; m_destination = destination;
    m_sources.clear(); m_assets.clear(); m_created.clear(); m_index = 0; m_totalBytes = 0;
    m_images = includeImages ? MarkdownImages::parse(content) : QVector<MarkdownImage>{};
    const QString from = MarkdownFileIO::normalizedPath(source), to = MarkdownFileIO::normalizedPath(destination);
#ifdef Q_OS_WIN
    const auto sensitivity = Qt::CaseInsensitive;
#else
    const auto sensitivity = Qt::CaseSensitive;
#endif
    if (from.compare(to, sensitivity) == 0) { complete(false, QStringLiteral("导出位置不能覆盖当前知识库文档，请选择其他位置。")); return; }
    for (const auto &image : m_images) if (!m_sources.contains(image.source)) m_sources.append(image.source);
    emit progress(0, m_sources.size()); QTimer::singleShot(0, this, &MarkdownExportJob::next);
}
void MarkdownExportJob::next() {
    if (!m_active) return;
    if (m_index == m_sources.size()) { commit(); return; }
    m_loader->load(m_sources[m_index], m_document);
}
void MarkdownExportJob::commit() {
    const QString directory = QFileInfo(m_destination).absolutePath(); QMap<QString, QString> replacements;
    for (auto it = m_assets.cbegin(); it != m_assets.cend(); ++it) {
        QString path, error; bool created;
        if (!ImageStorage::writeAsset(directory, it->first, it->second, path, created, &error)) { complete(false, error); return; }
        if (created) m_created.append(path);
        replacements.insert(it.key(), MarkdownImages::encodedPath(QFileInfo(path).fileName()));
    }
    QString error;
    if (!QDir().mkpath(directory) || !MarkdownFileIO::writeFile(m_destination, MarkdownImages::replace(m_content, m_images, replacements), &error)) {
        complete(false, error.isEmpty() ? QStringLiteral("无法创建导出目录。") : error); return;
    }
    complete(true, QStringLiteral("已导出 Markdown 和 %1 张图片：%2").arg(m_assets.size()).arg(m_destination));
}
void MarkdownExportJob::cancel() { if (m_active) { m_loader->cancel(); complete(false, QStringLiteral("导出已取消。")); } }
void MarkdownExportJob::complete(bool success, const QString &message) {
    m_active = false;
    if (!success) for (const auto &path : m_created) QFile::remove(path);
    m_created.clear(); m_assets.clear(); emit finished(success, message);
}
