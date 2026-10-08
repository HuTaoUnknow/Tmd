#include "core/library/MarkdownManager.h"
#include "core/library/MarkdownFileIO.h"
#include "core/library/FileWatcher.h"

#include <QDir>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QtCore/qscopeguard.h>
#include <algorithm>
namespace {
bool fail(QString *error, const QString &message) { if (error) *error = message; return false; }
}
namespace {
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
    m_relations = next; applyRelations(); ++m_graphRevision; emit relationsChanged();
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
    m_relations = next; applyRelations(); ++m_graphRevision; emit relationsChanged();
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
    m_relations = next; applyRelations(); ++m_graphRevision; emit relationsChanged();
    return true;
}
