#include "core/library/MarkdownManager.h"
#include "core/library/MarkdownFileIO.h"
#include "core/library/LibraryPathPolicy.h"
#include "core/library/FileWatcher.h"
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
    QDirIterator iterator(m_root, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        if (!safeDirectoryPath(path).isEmpty()) result.append(QDir(m_root).relativeFilePath(path));
    }
    result.sort(); return result;
}
bool MarkdownManager::scan(QString *error) {
    const auto restoreWatches = qScopeGuard([this] { if (m_watcher) m_watcher->rebuild(); });
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
    QMap<QString, QStringList> identities;
    for (const auto &entry : m_nodes) if (!entry.second->m_fileIdentity.isEmpty()) identities[entry.second->m_fileIdentity].append(entry.first);
    QMap<QString, QString> renamedKeys, renamedPaths;
    std::map<QString, std::unique_ptr<MarkdownNode>> discovered;
    QDirIterator files(m_root, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString filePath = files.next();
        if (QFileInfo(filePath).suffix().compare("md", Qt::CaseInsensitive) != 0) continue;
        const QString absolute = safePath(filePath);
        if (absolute.isEmpty()) continue;
        QString content, readError;
        const QString key = keyFor(absolute);
        auto previous = m_nodes.find(key);
        const QString identity = MarkdownFileIO::fileIdentity(absolute);
        if (previous == m_nodes.end() && !identity.isEmpty() && identities.value(identity).size() == 1) {
            const QString oldKey = identities.value(identity).front();
            auto old = m_nodes.find(oldKey);
            if (old != m_nodes.end() && !MarkdownFileIO::exists(old->second->filePath())) {
                previous = old; renamedKeys.insert(oldKey, key);
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
        const QString title = node->title();
        emit documentChanged(node->relativePath());
        if (title != previousTitle) emit documentTitleChanged(node->relativePath(), title);
    }
}
void MarkdownManager::enableWatching() {
    if (m_watcher) return;
    m_watcher = new FileWatcher(m_root, this);
    connect(m_watcher, &FileWatcher::changed, this, [this] {
        QString error;
        if (!scan(&error)) emit refreshFailed(error);
    });
}
