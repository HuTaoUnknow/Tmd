#include "core/library/MarkdownManager.h"
#include "core/library/MarkdownFileIO.h"
#include "core/library/LibraryPathPolicy.h"
#include "core/library/FileWatcher.h"
#include "core/library/LibraryScanLock.h"
#include "core/images/ImageStorage.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QtCore/qscopeguard.h>
#include <algorithm>

namespace {
bool fail(QString *error, const QString &message) { if (error) *error = message; return false; }

}
MarkdownManager::MarkdownManager(QString rootPath, QObject *parent) : QObject(parent) {
    QDir().mkpath(rootPath);
    m_root = MarkdownFileIO::normalizedPath(rootPath);
}
QString MarkdownManager::safePath(const QString &path, QString *error) const { return LibraryPathPolicy(m_root).document(path, error); }
QString MarkdownManager::keyFor(const QString &path) const {
    const QString absolute = safePath(path);
    if (absolute.isEmpty()) return {};
    QString relative = QDir(m_root).relativeFilePath(absolute);
#ifdef Q_OS_WIN
    relative = relative.toCaseFolded();
#endif
    return relative;
}
MarkdownNode *MarkdownManager::loadNode(const QString &path) const {
    const auto it = m_nodes.find(keyFor(path));
    return it == m_nodes.end() ? nullptr : it->second.get();
}
QList<MarkdownNode *> MarkdownManager::allNodes() const {
    QList<MarkdownNode *> nodes;
    for (const auto &entry : m_nodes) nodes.append(entry.second.get());
    return nodes;
}
QString MarkdownManager::safeDirectoryPath(const QString &path, QString *error) const { return LibraryPathPolicy(m_root).directory(path, error); }
QStringList MarkdownManager::directories() const {
    QStringList result;
    QDirIterator iterator(m_root, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        if (!safeDirectoryPath(path).isEmpty()) result.append(QDir(m_root).relativeFilePath(path));
    }
    result.sort(); return result;
}
bool MarkdownManager::scan(QString *error) {
    const LibraryScanLock transaction(m_root);
    if (!transaction.locked()) return fail(error,transaction.error());
    const auto restoreWatches = qScopeGuard([this] { if (m_watcher) m_watcher->rebuild(); if (m_photoWatcher) m_photoWatcher->rebuild(); });
    if (!QFileInfo(m_root).isDir()) return fail(error, QStringLiteral("知识库目录不存在。"));
    QStringList previousPaths;
    for (const auto &entry : m_nodes) previousPaths.append(entry.second->relativePath());
    const RelationStore previousRelations = m_relations;
    RelationStore diskRelations;
    QString snapshot;
    if (!MarkdownRelation::read(relationPath(), diskRelations, snapshot, error)) return false;
    // Validate path aliases before changing the live document collection.
    RelationStore normalized;
    for (auto source = diskRelations.cbegin(); source != diskRelations.cend(); ++source)
        for (auto target = source->cbegin(); target != source->cend(); ++target) {
            const QString s = keyFor(source.key()), t = keyFor(target.key());
            if (s.isEmpty() || t.isEmpty() || s == t || normalized[s].contains(t))
                return fail(error, QStringLiteral("关系文件包含越界路径、别名重复关系或自引用。"));
            normalized[s].insert(t, target.value());
        }
    m_warnings.clear();
    // Another window may already have completed this move and repaired the URLs.
    m_fileIndexError.clear();
    m_fileIndexWritable=m_fileIndex.load(QDir(m_root).filePath(".tree-md-files.json"),&m_fileIndexError);
    m_fileIndexLoaded=true;
    for (auto file=m_fileIndex.entries.begin();file!=m_fileIndex.entries.end();++file)
        for (const auto &deleted : m_pendingDeletedPaths)
            if (file.key()==deleted || file.key().startsWith(deleted+'/')) file->digest.clear();
    if (!m_fileIndexError.isEmpty()) m_warnings.append(m_fileIndexError);
    const auto inventory=LibraryFileIndex::capture({m_root,ImageStorage(m_root).photoRoot()},m_fileIndex.entries);
    auto fileMoves=LibraryFileIndex::moves(m_fileIndex.entries,inventory);
    const auto structuralMoves=fileMoves;
    for (auto move=structuralMoves.cbegin();move!=structuralMoves.cend();++move) {
        if (!m_fileIndex.entries.value(move.key()).directory) continue;
        for (auto file=m_fileIndex.entries.cbegin();file!=m_fileIndex.entries.cend();++file)
            if (file.key().startsWith(move.key()+'/')) {
                const QString target=move.value()+file->path.mid(move.key().size());
                const auto found=inventory.constFind(LibraryFileIndex::key(target));
                if (found!=inventory.cend() && found->directory==file->directory) fileMoves.insert(file.key(),found->path);
            }
    }
    for (auto move=m_pendingReferenceMoves.cbegin();move!=m_pendingReferenceMoves.cend();++move) {
        fileMoves.insert(move.key(),move.value());
        for (auto file=m_fileIndex.entries.cbegin();file!=m_fileIndex.entries.cend();++file)
            if (file.key().startsWith(move.key()+'/')) {
                const QString target=move.value()+file->path.mid(move.key().size());
                if (QFileInfo::exists(target)) fileMoves.insert(file.key(),target);
            }
    }
    QMap<QString, QStringList> identities;
    for (const auto &entry : m_nodes) if (!entry.second->m_fileIdentity.isEmpty()) identities[entry.second->m_fileIdentity].append(entry.first);
    QMap<QString, QString> renamedKeys, renamedPaths;
    for (auto move=fileMoves.cbegin();move!=fileMoves.cend();++move) {
        if (QFileInfo(move.value()).suffix().compare("md",Qt::CaseInsensitive)!=0 || !QFileInfo(move.value()).isFile()) continue;
        const auto from=keyFor(move.key()), to=keyFor(move.value());
        if (from.isEmpty() || to.isEmpty() || from==to) continue;
        renamedKeys.insert(from,to);
        const auto old=m_fileIndex.entries.value(move.key());
        renamedPaths.insert(QDir(m_root).relativeFilePath(old.path.isEmpty() ? move.key() : old.path),QDir(m_root).relativeFilePath(move.value()));
    }
    for (const auto &entry : m_nodes) {
        const auto *node=entry.second.get(); if (QFileInfo::exists(node->filePath())) continue;
        const QString target=m_fileIndex.redirects.value(LibraryFileIndex::key(node->filePath()));
        const QString next=target.isEmpty() ? QString() : keyFor(target);
        if (!next.isEmpty() && QFileInfo(target).isFile()) {
            renamedKeys.insert(entry.first,next); renamedPaths.insert(node->relativePath(),QDir(m_root).relativeFilePath(target));
        }
    }
    QMap<QString,QString> bufferOrigins;
    std::map<QString, std::unique_ptr<MarkdownNode>> discovered;
    QDirIterator files(m_root, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString filePath = files.next();
        if (QFileInfo(filePath).suffix().compare("md", Qt::CaseInsensitive) != 0) continue;
        const QString absolute = safePath(filePath);
        if (absolute.isEmpty()) continue;
        QString content, readError;
        const QString key = keyFor(absolute);
        auto previous = m_nodes.find(key);
        if (previous==m_nodes.end() && renamedKeys.values().contains(key)) previous=m_nodes.find(renamedKeys.key(key));
        const QString identity = MarkdownFileIO::fileIdentity(absolute);
        if (previous == m_nodes.end() && !identity.isEmpty() && identities.value(identity).size() == 1) {
            const QString oldKey = identities.value(identity).front();
            auto old = m_nodes.find(oldKey);
            if (old != m_nodes.end() && !MarkdownFileIO::exists(old->second->filePath())) {
                previous = old; renamedKeys.insert(oldKey, key);
                fileMoves.insert(LibraryFileIndex::key(old->second->filePath()),absolute);
                renamedPaths.insert(old->second->relativePath(), QDir(m_root).relativeFilePath(absolute));
            }
        }
        if (!MarkdownFileIO::readFile(absolute, content, &readError)) {
            m_warnings.append(QDir(m_root).relativeFilePath(absolute) + ": " + readError);
            if (previous != m_nodes.end()) discovered.emplace(key, std::make_unique<MarkdownNode>(*previous->second));
            continue;
        }
        auto node = previous != m_nodes.end() ? std::make_unique<MarkdownNode>(*previous->second) : std::make_unique<MarkdownNode>();
        if (node->isModified()) {
            bufferOrigins.insert(LibraryFileIndex::key(absolute),node->filePath());
            node->m_externalChange = content != node->m_savedContent;
        } else {
            node->m_content = content;
            node->m_savedContent = content;
            node->m_externalChange = false;
        }
        node->m_filePath = absolute;
        node->m_relativePath = QDir(m_root).relativeFilePath(absolute);
        node->m_fileIdentity = identity;
        discovered.emplace(key, std::move(node));
    }
    m_nodes = std::move(discovered);
    m_relationSnapshot = snapshot;
    RelationStore valid;
    bool pruned = false;
    for (auto source = normalized.cbegin(); source != normalized.cend(); ++source) {
        const QString sourceKey = renamedKeys.value(source.key(), source.key());
        for (auto target = source->cbegin(); target != source->cend(); ++target) {
            const QString targetKey = renamedKeys.value(target.key(), target.key());
            if (!m_nodes.count(sourceKey) || !m_nodes.count(targetKey)) { pruned = true; continue; }
            if (sourceKey == targetKey || valid[sourceKey].contains(targetKey))
                return fail(error, QStringLiteral("关系文件包含路径别名形成的重复关系或自引用。"));
            valid[sourceKey].insert(targetKey, target.value());
        }
    }
    m_relations = valid;
    applyRelations();
    if ((pruned || !renamedKeys.isEmpty()) && !persistRelations(valid, error)) {
        m_warnings.append(QStringLiteral("悬空关系已从内存移除，但关系文件写回失败：%1").arg(error ? *error : QString{}));
    }
    if (error) error->clear();
    for (auto renamed = renamedPaths.cbegin(); renamed != renamedPaths.cend(); ++renamed) emit documentRenamed(renamed.key(), renamed.value());
    maintainReferences(fileMoves,inventory,bufferOrigins);
    m_pendingReferenceMoves.clear();
    m_pendingDeletedPaths.clear();
    QStringList currentPaths;
    for (const auto &entry : m_nodes) currentPaths.append(entry.second->relativePath());
    if (previousPaths != currentPaths || previousRelations != m_relations) ++m_graphRevision;
    emit documentsChanged();
    return true;
}
void MarkdownManager::updateContent(const QString &path, const QString &content) {
    if (auto *node = loadNode(path)) {
        if (node->m_content == content) return;
        const QString previousTitle = node->title(); node->m_content = content;
        updateReferenceStatus(node);
        const QString title = node->title();
        emit documentChanged(node->relativePath());
        if (title != previousTitle) emit documentTitleChanged(node->relativePath(), title);
    }
}
void MarkdownManager::enableWatching() {
    if (!m_watcher) {
        m_watcher = new FileWatcher(m_root, this);
        connect(m_watcher, &FileWatcher::changed, this, [this] {
            QString error;
            if (!scan(&error)) emit refreshFailed(error);
        });
    }
    const QString photoRoot=ImageStorage(m_root).photoRoot();
    const auto photoKey=LibraryFileIndex::key(photoRoot), rootKey=LibraryFileIndex::key(m_root);
    if (photoKey==rootKey || photoKey.startsWith(rootKey+'/')) {
        // The recursive library watch already covers these images; a descendant
        // root handle would unnecessarily complicate moving its parent folder.
        if (m_photoWatcher) { m_photoWatcher->suspend(); m_photoWatcher->deleteLater(); m_photoWatcher=nullptr; }
        m_photoWatchRoot=photoRoot; return;
    }
    if (m_photoWatcher && m_photoWatchRoot==photoRoot) return;
    if (m_photoWatcher) { m_photoWatcher->suspend(); m_photoWatcher->deleteLater(); }
    m_photoWatchRoot=photoRoot;
    m_photoWatcher=new FileWatcher(photoRoot,this);
    connect(m_photoWatcher,&FileWatcher::changed,this,[this] {
        QString error; if (!scan(&error)) emit refreshFailed(error);
    });
}
