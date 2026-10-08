#include "core/knowledge/KnowledgeIndex.h"
#include "core/library/MarkdownManager.h"
#include <QQueue>
#include <QSet>
#include <algorithm>

NodeRelationType KnowledgeIndex::inverse(NodeRelationType type) {
    return MarkdownRelation::inverse(type);
}
KnowledgeIndexResult KnowledgeIndex::build(const MarkdownManager &manager, const QString &root, int maxDepth, bool showParent, bool showChild) {
    KnowledgeIndexResult result;
    const auto *current = manager.loadNode(root);
    if (!current) return result;
    result.root = current->relativePath();
    struct Neighbour { QString path; NodeRelationType type; };
    QMap<QString, QList<Neighbour>> adjacent;

    for (const auto *node : manager.allNodes()) {
        for (int i = 0; i < 5; ++i) {
            const auto type = static_cast<NodeRelationType>(i);
            for (const auto &path : node->relatedPaths(type)) {
                if (!manager.loadNode(path)) continue;
                if ((type != NodeRelationType::Parent || showParent) && ((type != NodeRelationType::Child && type != NodeRelationType::DetailOf) || showChild))
                    adjacent[node->relativePath()].append({path, type});
            }
        }
    }
    QQueue<QString> queue; queue.enqueue(result.root);
    result.members.insert(result.root); result.depths.insert(result.root, 0);
    while (!queue.isEmpty()) {
        const QString source = queue.dequeue(); result.paths.append(source);
        if (result.depths.value(source) >= qMax(0, maxDepth)) continue;
        for (const auto &neighbour : adjacent.value(source)) {
            if (result.members.contains(neighbour.path)) continue;
            result.members.insert(neighbour.path);
            result.depths.insert(neighbour.path, result.depths.value(source) + 1); queue.enqueue(neighbour.path);
        }
    }
    const auto stored = manager.explicitRelations();
    const auto groups = MarkdownRelation::alternativeGroups(stored);
    QSet<QString> drawnRoutes;
    for (auto source = stored.cbegin(); source != stored.cend(); ++source) {
        for (auto target = source->cbegin(); target != source->cend(); ++target) {
            QString from = source.key(), to = target.key(); auto type = target.value();
            if (type == NodeRelationType::Previous || type == NodeRelationType::DetailOf) { std::swap(from, to); type = MarkdownRelation::inverse(type); }
            if (!result.members.contains(from) || !result.members.contains(to)) continue;
            if (type == NodeRelationType::Next) {
                const QString a = groups.value(from, from), b = groups.value(to, to);
                if (a == b || drawnRoutes.contains(a + '\n' + b)) continue;
                drawnRoutes.insert(a + '\n' + b);
            }
            result.edges.append({from, to, type});
        }
    }
    return result;
}
