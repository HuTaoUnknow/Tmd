#pragma once
#include "core/KnowledgeIndex.h"
#include <QRectF>

struct KnowledgeTreeLayoutResult {
    QMap<QString, QRectF> rects;
    QMap<QString, int> levels, nestedDepths;
    QMap<QString, QString> routeAnchors;
    QMap<QString, qreal> titleSizes;
};
class KnowledgeTreeLayout {
public:
    static KnowledgeTreeLayoutResult build(const KnowledgeIndexResult &index);
};
