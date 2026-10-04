#pragma once
#include "MarkdownNode.h"
#include <QList>
#include <QMap>
#include <QPoint>

class MarkdownManager;
struct KnowledgeEdge {
    QString source, target;
    NodeRelationType type = NodeRelationType::Next;
};
struct KnowledgeIndexResult {
    QString root;
    QStringList paths;
    QList<KnowledgeEdge> edges;
    QMap<QString, QPoint> positions;
    QMap<QString, int> depths;
};
class KnowledgeIndex {
public:
    // Expand every branch up to five relationship steps from the root in both directions.
    // Reciprocal roles expose incoming references; shared nodes and cycles are visited once.
    static constexpr int LayerLimit = 5;
    static KnowledgeIndexResult build(const MarkdownManager &manager, const QString &root,
        int maxDepth = LayerLimit, bool showParent = true, bool showChild = true);
    static NodeRelationType inverse(NodeRelationType type);
};
