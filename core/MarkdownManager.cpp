#include "MarkdownManager.h"
#include "MarkdownFileIO.h"
#include "FileWatcher.h"
#include "ImageStorage.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QtCore/qscopeguard.h>
#include <algorithm>

namespace {
bool fail(QString *error, const QString &message) { if (error) *error = message; return false; }
bool routeRole(NodeRelationType type) { return type == NodeRelationType::Previous || type == NodeRelationType::Next; }
bool clearSharedRoute(RelationStore &store, const QString &source, const QString &target) {
    const auto groups = MarkdownRelation::alternativeGroups(store);
    const QString a = groups.value(source, source), b = groups.value(target, target); bool removed = false;
    for (auto from = store.begin(); from != store.end(); ++from) {
        const QString group = groups.value(from.key(), from.key());
        for (auto to = from->begin(); to != from->end();) {
            const QString other = groups.value(to.key(), to.key());
            if (routeRole(to.value()) && ((group == a && other == b) || (group == b && other == a))) { to = from->erase(to); removed = true; }
            else ++to;
        }
    }
    return removed;
}
}
MarkdownManager::MarkdownManager(QString rootPath, QObject *parent) : QObject(parent) {
    QDir().mkpath(rootPath);
    m_root = MarkdownFileIO::normalizedPath(rootPath);
}
QString MarkdownManager::safePath(const QString &path, QString *error) const {
    const QString candidate = QDir::isAbsolutePath(path) ? path : QDir(m_root).absoluteFilePath(path);
    const QString resolved = MarkdownFileIO::normalizedPath(candidate);
#ifdef Q_OS_WIN
    const auto sensitivity = Qt::CaseInsensitive;
#else
    const auto sensitivity = Qt::CaseSensitive;
#endif
    // Resolve the closest existing ancestor too, preventing escape through directory symlinks.
    QFileInfo ancestor(candidate);
    while (!ancestor.exists() && ancestor.absoluteFilePath() != ancestor.absolutePath())
        ancestor = QFileInfo(ancestor.absolutePath());
    const QString ancestorPath = MarkdownFileIO::normalizedPath(ancestor.absoluteFilePath());
    if (!resolved.startsWith(m_root + '/', sensitivity)
        || (ancestorPath.compare(m_root, sensitivity) != 0 && !ancestorPath.startsWith(m_root + '/', sensitivity))
        || QFileInfo(resolved).suffix().compare("md", Qt::CaseInsensitive) != 0) {
        fail(error, QStringLiteral("请选择 md_data 内的 .md 文件，路径不能越过知识库目录。"));
        return {};
    }
    return resolved;
}
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
QString MarkdownManager::safeDirectoryPath(const QString &path, QString *error) const {
    const QString candidate = QDir::isAbsolutePath(path) ? path : QDir(m_root).absoluteFilePath(path);
    const QString relative = QDir(m_root).relativeFilePath(QDir::cleanPath(candidate));
    const auto parts = QDir::fromNativeSeparators(relative).split('/');
    if (path.isEmpty() || relative == "." || QDir::isAbsolutePath(relative)) {
        fail(error, QStringLiteral("请选择 md_data 内的文件夹，不能操作知识库根目录。")); return {};
    }
    for (const auto &part : parts) {
        if (part.isEmpty() || part == "." || part == "..") {
            fail(error, QStringLiteral("文件夹路径不能越过知识库目录。")); return {};
        }
#ifdef Q_OS_WIN
        static const QRegularExpression reserved("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\.|$)", QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression invalid("[<>:\"\\\\|?*\\x00-\\x1f]");
        if (part.endsWith('.') || part.endsWith(' ') || invalid.match(part).hasMatch() || reserved.match(part).hasMatch()) {
            fail(error, QStringLiteral("文件夹名称包含 Windows 不允许的字符或保留名称。")); return {};
        }
#endif
    }
    QFileInfo ancestor(candidate);
    while (!ancestor.exists() && ancestor.absoluteFilePath() != ancestor.absolutePath()) ancestor = QFileInfo(ancestor.absolutePath());
    const QString resolved = MarkdownFileIO::normalizedPath(candidate);
    const QString ancestorPath = MarkdownFileIO::normalizedPath(ancestor.absoluteFilePath());
#ifdef Q_OS_WIN
    const auto sensitivity = Qt::CaseInsensitive;
#else
    const auto sensitivity = Qt::CaseSensitive;
#endif
    if (!resolved.startsWith(m_root + '/', sensitivity)
        || (ancestorPath.compare(m_root, sensitivity) != 0 && !ancestorPath.startsWith(m_root + '/', sensitivity))
        || QFileInfo(candidate).isSymLink()) {
        fail(error, QStringLiteral("请选择知识库内的真实文件夹，不能越界或操作目录链接。")); return {};
    }
    return resolved;
}
QStringList MarkdownManager::directories() const {
    QStringList result;
    QDirIterator iterator(m_root, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        if (!safeDirectoryPath(path).isEmpty()) result.append(QDir(m_root).relativeFilePath(path));
    }
    result.sort(); return result;
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
bool MarkdownManager::scan(QString *error) {
    if (!QFileInfo(m_root).isDir()) return fail(error, QStringLiteral("知识库目录不存在。"));
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
    if (m_watcher) m_watcher->rebuild();
    emit documentsChanged();
    return true;
}
void MarkdownManager::updateContent(const QString &path, const QString &content) {
    if (auto *node = loadNode(path)) { node->m_content = content; emit documentChanged(node->relativePath()); }
}
bool MarkdownManager::saveDocument(const QString &path, QString *error, bool overwriteExternal) {
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
    emit documentChanged(node->relativePath());
    return true;
}
bool MarkdownManager::reloadDocument(const QString &path, QString *error) {
    auto *node = loadNode(path);
    if (!node) return fail(error, QStringLiteral("文档不存在。"));
    QString content;
    if (!MarkdownFileIO::readFile(node->filePath(), content, error)) return false;
    node->m_content = node->m_savedContent = content;
    node->m_fileIdentity = MarkdownFileIO::fileIdentity(node->filePath());
    node->m_externalChange = false;
    emit documentChanged(node->relativePath());
    return true;
}
bool MarkdownManager::createDocument(const QString &path, const QString &content, QString *error) {
    const QString absolute = safePath(path, error);
    if (absolute.isEmpty()) return false;
    if (QFileInfo::exists(absolute)) return fail(error, QStringLiteral("该文件已存在。"));
    if (!QDir().mkpath(QFileInfo(absolute).absolutePath())) return fail(error, QStringLiteral("无法创建目录。"));
    if (!MarkdownFileIO::writeFile(absolute, content, error)) return false;
    return scan(error);
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
    m_relations = next;
    for (const auto &key : documentKeys) m_nodes.erase(key);
    applyRelations();
    if (m_watcher) m_watcher->rebuild();
    emit documentsChanged(); return true;
}
bool MarkdownManager::renameDocument(const QString &path, const QString &newRelativePath, QString *error) {
    const QString oldRelative = loadNode(path) ? loadNode(path)->relativePath() : path;
    const QString from = safePath(path, error), to = safePath(newRelativePath, error);
    if (from.isEmpty() || to.isEmpty()) return false;
    if (loadNode(path) && loadNode(path)->isModified()) return fail(error, QStringLiteral("请先保存文档，再重命名。"));
    if (QFileInfo::exists(to)) return fail(error, QStringLiteral("目标文件已存在。"));
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
    emit documentRenamed(oldRelative, QDir(m_root).relativeFilePath(to));
    return scan(error);
}
bool MarkdownManager::importDocument(const QString &source, const QString &relativePath, QString *error) {
    const QString target = safePath(relativePath, error);
    if (target.isEmpty()) return false;
    if (QFileInfo::exists(target)) return fail(error, QStringLiteral("该文件已存在。"));
    QString content, rewritten; QStringList created; ImageStorage storage(m_root);
    if (!MarkdownFileIO::readFile(source, content, error) || !storage.prepareImport(source, relativePath, content, rewritten, created, error)) return false;
    if (!createDocument(relativePath, rewritten, error)) {
        if (!QFileInfo::exists(target)) storage.rollback(created);
        return false;
    }
    return true;
}
QString MarkdownManager::relationPath() const { return QDir(m_root).filePath(".tree-md-relations.json"); }
bool MarkdownManager::persistRelations(const RelationStore &store, QString *error) {
    RelationStore disk;
    QString snapshot;
    if (!MarkdownRelation::read(relationPath(), disk, snapshot, error)) return false;
    if (snapshot != m_relationSnapshot) return fail(error, QStringLiteral("关系文件已被外部修改，请刷新后重试。"));
    RelationStore portable;
    for (auto source = store.cbegin(); source != store.cend(); ++source) {
        const auto sourceNode = m_nodes.find(source.key());
        if (sourceNode == m_nodes.end()) continue;
        for (auto target = source->cbegin(); target != source->cend(); ++target) {
            const auto targetNode = m_nodes.find(target.key());
            if (targetNode != m_nodes.end()) portable[sourceNode->second->relativePath()].insert(targetNode->second->relativePath(), target.value());
        }
    }
    return MarkdownRelation::write(relationPath(), portable, m_relationSnapshot, error);
}
RelationStore MarkdownManager::explicitRelations() const {
    RelationStore portable;
    for (auto source = m_relations.cbegin(); source != m_relations.cend(); ++source) {
        const auto *node = loadNode(source.key()); if (!node) continue;
        for (auto target = source->cbegin(); target != source->cend(); ++target)
            if (const auto *related = loadNode(target.key())) portable[node->relativePath()].insert(related->relativePath(), target.value());
    }
    return portable;
}
void MarkdownManager::applyRelations() {
    for (auto &entry : m_nodes) for (auto &paths : entry.second->m_relations) paths.clear();
    const auto groups = MarkdownRelation::alternativeGroups(m_relations);
    QMap<QString, QStringList> members;
    for (const auto &entry : m_nodes) members[groups.value(entry.first, entry.first)].append(entry.first);
    for (auto source = m_relations.cbegin(); source != m_relations.cend(); ++source) {
        auto *node = loadNode(source.key()); if (!node) continue;
        for (auto target = source->cbegin(); target != source->cend(); ++target) {
            auto *related = loadNode(target.key()); if (!related) continue;
            const auto type = target.value();
            if (type == NodeRelationType::Previous || type == NodeRelationType::Next) {
                const QString a = groups.value(source.key(), source.key()), b = groups.value(target.key(), target.key());
                if (a == b) continue;
                for (const auto &from : members.value(a)) for (const auto &to : members.value(b)) {
                    auto *left = loadNode(from), *right = loadNode(to);
                    left->m_relations[static_cast<int>(type)].append(right->relativePath());
                    right->m_relations[static_cast<int>(MarkdownRelation::inverse(type))].append(left->relativePath());
                }
            } else {
                node->m_relations[static_cast<int>(type)].append(related->relativePath());
                related->m_relations[static_cast<int>(MarkdownRelation::inverse(type))].append(node->relativePath());
            }
        }
    }
    for (const auto &group : members) if (group.size() > 1) {
        for (const auto &path : group) {
            auto &alternatives = loadNode(path)->m_relations[static_cast<int>(NodeRelationType::Parent)]; alternatives.clear();
            for (const auto &peer : group) if (peer != path) alternatives.append(loadNode(peer)->relativePath());
        }
    }
    for (auto &entry : m_nodes) for (auto &paths : entry.second->m_relations) { paths.removeDuplicates(); std::sort(paths.begin(), paths.end()); }
}
bool MarkdownManager::addRelation(const QString &source, const QString &target, NodeRelationType type, QString *error) {
    const QString sourceKey = keyFor(source), targetKey = keyFor(target);
    if (!loadNode(source) || !loadNode(target)) return fail(error, QStringLiteral("请选择存在的 Markdown 节点。"));
    if (sourceKey == targetKey) return fail(error, QStringLiteral("不能关联文档自身。"));
    if (MarkdownRelation::key(type).isEmpty()) return fail(error, QStringLiteral("无效关系类型。"));
    if (routeRole(type)) {
        const auto groups = MarkdownRelation::alternativeGroups(m_relations);
        if (groups.value(sourceKey, sourceKey) == groups.value(targetKey, targetKey))
            return fail(error, QStringLiteral("同级替代知识共享学习路线，不能在组内建立前后关系。"));
        if (loadNode(source)->relatedPaths(type).contains(loadNode(target)->relativePath())) return true;
    }
    if (m_relations.value(sourceKey).contains(targetKey)) {
        if (m_relations.value(sourceKey).value(targetKey) == type) return true;
        return fail(error, QStringLiteral("这两个节点已有相互对应的角色，请调整或移除原关系。"));
    }
    if (m_relations.value(targetKey).contains(sourceKey)) {
        if (m_relations.value(targetKey).value(sourceKey) == MarkdownRelation::inverse(type)) return true;
        return fail(error, QStringLiteral("这两个节点已有相互对应的角色，请调整或移除原关系。"));
    }
    RelationStore next = m_relations;
    next[sourceKey].insert(targetKey, type);
    if (!MarkdownRelation::canonicalize(next, next, error)) return false;
    if (!persistRelations(next, error)) return false;
    m_relations = next; applyRelations(); emit documentsChanged();
    return true;
}
bool MarkdownManager::removeRelation(const QString &source, const QString &target, QString *error) {
    RelationStore next = m_relations;
    const QString sourceKey = keyFor(source), targetKey = keyFor(target);
    const bool shared = (next.value(sourceKey).contains(targetKey) && routeRole(next.value(sourceKey).value(targetKey))) ||
        (next.value(targetKey).contains(sourceKey) && routeRole(next.value(targetKey).value(sourceKey))) ||
        (!next.value(sourceKey).contains(targetKey) && !next.value(targetKey).contains(sourceKey));
    const bool routes = shared && clearSharedRoute(next, sourceKey, targetKey);
    const bool forward = next[sourceKey].remove(targetKey), backward = next[targetKey].remove(sourceKey);
    if (!forward && !backward && !routes) return fail(error, QStringLiteral("关系不存在。"));
    if (!persistRelations(next, error)) return false;
    m_relations = next; applyRelations(); emit documentsChanged();
    return true;
}
bool MarkdownManager::setRelation(const QString &source, const QString &target, NodeRelationType type, QString *error) {
    const QString sourceKey = keyFor(source), targetKey = keyFor(target);
    if (!loadNode(source) || !loadNode(target)) return fail(error, QStringLiteral("请选择存在的 Markdown 节点。"));
    if (sourceKey == targetKey) return fail(error, QStringLiteral("不能关联文档自身。"));
    if (MarkdownRelation::key(type).isEmpty()) return fail(error, QStringLiteral("无效关系类型。"));
    RelationStore next = m_relations;
    if (routeRole(type)) {
        const auto groups = MarkdownRelation::alternativeGroups(next);
        if (groups.value(sourceKey, sourceKey) == groups.value(targetKey, targetKey))
            return fail(error, QStringLiteral("同级替代知识共享学习路线，不能在组内建立前后关系。"));
        clearSharedRoute(next, sourceKey, targetKey);
    }
    // Both views share one persisted edge. Replace the pair atomically in either direction.
    next[sourceKey].remove(targetKey);
    next[targetKey].remove(sourceKey);
    next[sourceKey].insert(targetKey, type);
    if (!MarkdownRelation::canonicalize(next, next, error)) return false;
    if (!persistRelations(next, error)) return false;
    m_relations = next; applyRelations(); emit documentsChanged();
    return true;
}
void MarkdownManager::enableWatching() {
    if (m_watcher) return;
    m_watcher = new FileWatcher(m_root, this);
    connect(m_watcher, &FileWatcher::changed, this, [this] {
        QString error;
        if (!scan(&error)) emit refreshFailed(error);
    });
}
