#pragma once
#include "core/knowledge/NodeRelationType.h"
#include <QPainterPath>
#include <QRectF>
#include <QList>
namespace KnowledgeTreeRouter {
QPointF port(const QRectF &rect, NodeRelationType type);
NodeRelationType direction(QPointF point, const QRectF &rect);
QPainterPath connector(const QRectF &from, const QRectF &to, NodeRelationType type);
QPainterPath avoidCards(const QPainterPath &direct, NodeRelationType type, const QList<QRectF> &cards);
}
