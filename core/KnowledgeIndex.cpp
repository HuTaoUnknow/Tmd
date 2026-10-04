#include "KnowledgeIndex.h"
#include "MarkdownManager.h"
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
    result.positions.insert(result.root, {0, 0}); result.depths.insert(result.root, 0);
    QSet<QString> occupied{"0,0"};
    const QPoint steps[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {0, -1}};
    auto key = [](QPoint point) { return QString::number(point.x()) + ',' + QString::number(point.y()); };
    while (!queue.isEmpty()) {
        const QString source = queue.dequeue(); result.paths.append(source);
        if (result.depths.value(source) >= qMax(0, maxDepth)) continue;
        for (const auto &neighbour : adjacent.value(source)) {
            if (result.positions.contains(neighbour.path)) continue;
            const QPoint desired = result.positions.value(source) + steps[static_cast<int>(neighbour.type)];
            QPoint position = desired;
            for (int offset = 1; occupied.contains(key(position)); ++offset) {
                const int distance = (offset + 1) / 2 * (offset % 2 ? 1 : -1);
                position = desired + (static_cast<int>(neighbour.type) < 2 ? QPoint(0, distance) : QPoint(distance, 0));
            }
            occupied.insert(key(position)); result.positions.insert(neighbour.path, position);
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
            if (!result.positions.contains(from) || !result.positions.contains(to)) continue;
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
