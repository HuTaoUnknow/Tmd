#include "core/library/MarkdownManager.h"
#include "core/library/ExternalDocuments.h"
#include "core/library/MarkdownFileIO.h"
#include "core/library/LibraryReferences.h"
#include "core/library/LibraryScanLock.h"
#include "core/images/ImageStorage.h"
#include <QDir>
#include <QFileInfo>

QList<MarkdownNode *> MarkdownManager::externalNodes() const { return m_external->nodes(); }
bool MarkdownManager::openExternalDocument(const QString &path, QString *error) { return m_external->open(path, error) != nullptr; }
bool MarkdownManager::forgetExternalDocument(const QString &path, QString *error, bool discard) { return m_external->forget(path, error, discard); }
QString MarkdownManager::backupExternalDocument(const QString &path, QString *error) {
    const LibraryScanLock transaction(m_root);
    if (!transaction.locked()) { if (error) *error = transaction.error(); return {}; }
    const QString source = MarkdownFileIO::normalizedPath(path);
    if (QFileInfo(source).suffix().compare("md", Qt::CaseInsensitive) != 0 || !safePath(source).isEmpty()) {
        if (error) *error = QStringLiteral("请选择知识库以外的 MD 文件。"); return {};
    }
    QString content;
    if (const auto *node = m_external->find(source)) content = node->content();
    else if (!MarkdownFileIO::readFile(source, content, error)) return {};
    const QString base = QFileInfo(source).completeBaseName();
    QString target = base + ".md";
    if (safePath(target, error).isEmpty()) return {};
    for (int suffix = 2; !validateDocumentName(target) || QFileInfo::exists(QDir(m_root).filePath(target)); ++suffix) {
        if (suffix > 10000) { if (error) *error = QStringLiteral("无法生成唯一的备份文件名。"); return {}; }
        target = base + QStringLiteral("（备份%1）.md").arg(suffix);
    }
    ImageStorage storage(m_root); QString rewritten; QStringList created;
    if (!storage.prepareRelocation(source, target, content, rewritten, created, error)) return {};
    rewritten = LibraryReferences::rewrite(rewritten, m_root, source, QDir(m_root).filePath(target), {}, {});
    if (!createDocument(target, rewritten, error)) {
        if (!QFileInfo::exists(QDir(m_root).filePath(target))) storage.rollback(created);
        return {};
    }
    return target;
}
