#pragma once
#include "KnowledgeTreeCanvas.h"

// The main panel uses the same index, card layout and connectors as the management page.
class KnowledgeGraphWidget : public KnowledgeTreeCanvas {
    Q_OBJECT
public:
    explicit KnowledgeGraphWidget(QWidget *parent = nullptr);
};
