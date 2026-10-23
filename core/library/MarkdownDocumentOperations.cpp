#include "core/library/MarkdownManager.h"
#include "core/markdown/MarkdownImport.h"
#include "core/library/MarkdownFileIO.h"
#include "core/library/FileWatcher.h"
#include "core/library/LibraryScanLock.h"
#include "core/images/ImageStorage.h"
#include <QDir>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QtCore/qscopeguard.h>
#include <algorithm>
namespace {
bool fail(QString *error, const QString &message) { if (error) *error = message; return false; }
}
bool MarkdownManager::createDirectory(const QString &path, QString *error) {
    const QString absolute = safeDirectoryPath(path, error);
    if (absolute.isEmpty()) return false;
    if (QFileInfo::exists(absolute)) return fail(error, QStringLiteral("同名文件或文件夹已存在。"));
    const QFileInfo info(absolute);
    if (!QDir(info.absolutePath()).mkdir(info.fileName())) return fail(error, QStringLiteral("无法创建文件夹，请检查上级目录和写入权限。"));
    if (m_watcher) m_watcher->rebuild();
    emit documentsChanged(); return true;
}
QStringList MarkdownManager::modifiedDocuments() const {
    QStringList paths;
    for (const auto *node : allNodes()) if (node->isModified()) paths.append(node->relativePath());
    std::sort(paths.begin(), paths.end()); return paths;
}
DocumentSaveResult MarkdownManager::saveAllModified() {
    DocumentSaveResult result;
    for (const auto &path : modifiedDocuments()) {
        QString error;
        if (saveDocument(path, &error)) result.saved.append(path);
        else result.failures.insert(path, error);
    }
    return result;
}
bool MarkdownManager::saveDocument(const QString &path, QString *error, bool overwriteExternal) {
    const LibraryScanLock transaction(m_root); if (!transaction.locked()) return fail(error,transaction.error());
    auto *node = loadNode(path);
    if (!node) return fail(error, QStringLiteral("文档已被移除，请另存为新文档。"));
    QString disk;
    if (!MarkdownFileIO::readFile(node->filePath(), disk, error)) return false;
    if (disk != node->m_savedContent && !overwriteExternal) {
        node->m_externalChange = true;
        return fail(error, QStringLiteral("文档已被外部修改。请先重新加载，或明确选择覆盖。"));
    }
    if (!MarkdownFileIO::writeFile(node->filePath(), node->content(), error)) return false;
    node->m_savedContent = node->m_content;
    node->m_fileIdentity = MarkdownFileIO::fileIdentity(node->filePath());
    node->m_externalChange = false;
    updateReferenceStatus(node);
    rememberSavedFile(node);
    emit documentChanged(node->relativePath());
    return true;
}
bool MarkdownManager::reloadDocument(const QString &path, QString *error) {
    auto *node = loadNode(path);
    if (!node) return fail(error, QStringLiteral("文档不存在。"));
    QString content;
    if (!MarkdownFileIO::readFile(node->filePath(), content, error)) return false;
    const QString previousTitle = node->title();
    node->m_content = node->m_savedContent = content;
    node->m_fileIdentity = MarkdownFileIO::fileIdentity(node->filePath());
    node->m_externalChange = false;
    updateReferenceStatus(node);
    emit documentChanged(node->relativePath());
    if (node->title() != previousTitle) emit documentTitleChanged(node->relativePath(), node->title());
    return true;
}
bool MarkdownManager::createDocument(const QString &path, const QString &content, QString *error) {
    const LibraryScanLock transaction(m_root); if (!transaction.locked()) return fail(error,transaction.error());
    const QString absolute = safePath(path, error);
    if (absolute.isEmpty()) return false;
    if (QFileInfo::exists(absolute)) return fail(error, QStringLiteral("该文件已存在。"));
    if (!validateDocumentName(path,error)) return false;
    if (!QDir().mkpath(QFileInfo(absolute).absolutePath())) return fail(error, QStringLiteral("无法创建目录。"));
    if (!MarkdownFileIO::writeFile(absolute, content, error)) return false;
    return m_batchImport || scan(error);
}
bool MarkdownManager::deleteDocument(const QString &path, QString *error) {
    const QString absolute = safePath(path, error);
    if (absolute.isEmpty()) return false;
    if (!QFileInfo(absolute).isFile()) return fail(error, QStringLiteral("文档不存在，请刷新列表。"));
    return recyclePath(absolute, {keyFor(path)}, error);
}
bool MarkdownManager::deleteDirectory(const QString &path, QString *error) {
    const QString absolute = safeDirectoryPath(path, error);
    if (absolute.isEmpty()) return false;
    if (!QFileInfo(absolute).isDir()) return fail(error, QStringLiteral("文件夹不存在，请刷新列表。"));
    QSet<QString> keys;
#ifdef Q_OS_WIN
    const auto sensitivity = Qt::CaseInsensitive;
#else
    const auto sensitivity = Qt::CaseSensitive;
#endif
    for (const auto &entry : m_nodes)
        if (entry.second->filePath().startsWith(absolute + '/', sensitivity)) keys.insert(entry.first);
    return recyclePath(absolute, keys, error);
}
bool MarkdownManager::recyclePath(const QString &absolute, const QSet<QString> &documentKeys, QString *error) {
    const LibraryScanLock transaction(m_root); if (!transaction.locked()) return fail(error,transaction.error());
    const QSignalBlocker watcherBlocker(m_watcher);
    // Release native directory watch handles while the Windows shell recycles a subtree.
    if (m_watcher) m_watcher->suspend();
    const auto resumeWatching = qScopeGuard([this] { if (m_watcher) m_watcher->rebuild(); });
    RelationStore next = m_relations;
    for (const auto &key : documentKeys) {
        next.remove(key);
        for (auto &targets : next) targets.remove(key);
    }
    // Resolve metadata conflicts before touching the file. Trash the original name for Windows Restore.
    if (!persistRelations(next, error)) return false;
    if (!MarkdownFileIO::moveToTrash(absolute, error)) {
        QString rollback;
        if (!persistRelations(m_relations, &rollback))
            fail(error, (error ? *error : QString{}) + QStringLiteral("；知识关系还原失败：%1，请刷新后重试。").arg(rollback));
        return false;
    }
    const auto deleted=LibraryFileIndex::key(absolute);
    m_pendingDeletedPaths.insert(deleted);
    for (auto file=m_fileIndex.entries.begin();file!=m_fileIndex.entries.end();++file)
        if (file.key()==deleted || file.key().startsWith(deleted+'/')) {
            // Deliberate deletion is not a copy-and-delete move. Only restoring
            // the original file identity may repair this target automatically.
            file->digest.clear();
        }
    m_relations = next;
    for (const auto &key : documentKeys) m_nodes.erase(key);
    applyRelations();
    ++m_graphRevision; return scan(error);
}
bool MarkdownManager::renameDocument(const QString &path, const QString &newRelativePath, QString *error) {
    const LibraryScanLock transaction(m_root); if (!transaction.locked()) return fail(error,transaction.error());
    const QString oldRelative = loadNode(path) ? loadNode(path)->relativePath() : path;
    const QString from = safePath(path, error), to = safePath(newRelativePath, error);
    if (from.isEmpty() || to.isEmpty()) return false;
    if (loadNode(path) && loadNode(path)->isModified()) return fail(error, QStringLiteral("请先保存文档，再重命名。"));
    if (QFileInfo::exists(to)) return fail(error, QStringLiteral("目标文件已存在。"));
    if (!validateDocumentName(newRelativePath,error,path)) return false;
    if (!QDir().mkpath(QFileInfo(to).absolutePath())) return fail(error, QStringLiteral("无法创建目标目录。"));
    QString original, relocated; QStringList created; ImageStorage storage(m_root);
    if (!MarkdownFileIO::readFile(from, original, error) || !storage.prepareRelocation(from, newRelativePath, original, relocated, created, error)) return false;
    const QString oldKey = keyFor(path), newKey = keyFor(newRelativePath);
    RelationStore next;
    for (auto source = m_relations.cbegin(); source != m_relations.cend(); ++source)
        for (auto target = source->cbegin(); target != source->cend(); ++target)
            next[source.key() == oldKey ? newKey : source.key()].insert(target.key() == oldKey ? newKey : target.key(), target.value());
    if (!MarkdownFileIO::renameFile(from, to, error)) { storage.rollback(created); return false; }
    if (relocated != original && !MarkdownFileIO::writeFile(to, relocated, error)) {
        QString rollback;
        if (MarkdownFileIO::renameFile(to, from, &rollback)) storage.rollback(created);
        else fail(error, (error ? *error : QString{}) + QStringLiteral("；还原失败，文档保留在 %1：%2").arg(to, rollback));
        return false;
    }
    // Node paths must be updated before serializing portable display paths.
    auto extracted = m_nodes.extract(oldKey);
    if (!extracted.empty()) {
        extracted.key() = newKey;
        extracted.mapped()->m_filePath = to;
        extracted.mapped()->m_relativePath = QDir(m_root).relativeFilePath(to);
        m_nodes.insert(std::move(extracted));
    }
    if (!persistRelations(next, error)) {
        QString rollback;
        const bool restoredContent = relocated == original || MarkdownFileIO::writeFile(to, original, &rollback);
        if (!MarkdownFileIO::renameFile(to, from, &rollback) || !restoredContent)
            fail(error, (error ? *error : QString{}) + QStringLiteral("；重命名还原失败：%1").arg(rollback));
        else storage.rollback(created);
        auto restored = m_nodes.extract(newKey);
        if (!restored.empty()) {
            restored.key() = oldKey;
            restored.mapped()->m_filePath = from;
            restored.mapped()->m_relativePath = QDir(m_root).relativeFilePath(from);
            m_nodes.insert(std::move(restored));
        }
        return false;
    }
    if (auto *node = loadNode(newRelativePath)) {
        node->m_content = node->m_savedContent = relocated;
        node->m_fileIdentity = MarkdownFileIO::fileIdentity(to);
    }
    m_relations = next;
    m_pendingReferenceMoves.insert(LibraryFileIndex::key(from),to);
    emit documentRenamed(oldRelative, QDir(m_root).relativeFilePath(to));
    return scan(error);
}
bool MarkdownManager::importDocument(const QString &source, const QString &relativePath, QString *error) {
    const LibraryScanLock transaction(m_root); if (!transaction.locked()) return fail(error,transaction.error());
    const QString target = safePath(relativePath, error);
    if (target.isEmpty()) return false;
    if (QFileInfo::exists(target)) return fail(error, QStringLiteral("该文件已存在。"));
    if (!validateDocumentName(relativePath,error)) return false;
    QString content, rewritten; QStringList created; ImageStorage storage(m_root);
    if (!MarkdownFileIO::readFile(source, content, error)) return false;
    content = MarkdownImport::convertIndentedCode(content);
    if (!storage.prepareImport(source, relativePath, content, rewritten, created, error)) return false;
    if (!createDocument(relativePath, rewritten, error)) {
        if (!QFileInfo::exists(target)) storage.rollback(created);
        return false;
    }
    return true;
}
bool MarkdownManager::copyDocument(const QString &source, const QString &targetPath, const QString &content, QString *error) {
    const LibraryScanLock transaction(m_root); if (!transaction.locked()) return fail(error,transaction.error());
    const QString target = safePath(targetPath, error);
    if (target.isEmpty()) return false;
    if (QFileInfo::exists(target)) return fail(error, QStringLiteral("该文件已存在。"));
    if (!validateDocumentName(targetPath,error)) return false;
    QString rewritten = content; QStringList created; ImageStorage storage(m_root);
    if (!source.isEmpty()) {
        const QString original = safePath(source, error);
        if (original.isEmpty() || !storage.prepareRelocation(original, targetPath, content, rewritten, created, error)) return false;
    }
    if (!createDocument(targetPath, rewritten, error)) {
        if (!QFileInfo::exists(target)) storage.rollback(created);
        return false;
    }
    return true;
}
