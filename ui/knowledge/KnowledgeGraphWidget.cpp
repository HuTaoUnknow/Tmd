#include "ui/knowledge/KnowledgeGraphWidget.h"

KnowledgeGraphWidget::KnowledgeGraphWidget(QWidget *parent) : KnowledgeTreeCanvas(parent) {
    setObjectName("knowledgeGraph"); setReadOnly(true); setMinimumHeight(80);
}
