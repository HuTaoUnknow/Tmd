#include "ui/knowledge/KnowledgeNodeWidget.h"
#include <QGraphicsSceneMouseEvent>
#include <QCursor>
#include <QPainter>
#include <QTextLayout>
#include <QTextOption>
#include <cmath>

KnowledgeNodeWidget::KnowledgeNodeWidget(QString path, QString title, QString role, bool current, int hierarchyLevel)
    : m_path(std::move(path)), m_title(std::move(title)), m_role(std::move(role)),
      m_hierarchyLevel(hierarchyLevel), m_current(current) {
    setAcceptHoverEvents(true); setCursor(Qt::PointingHandCursor);
    m_cardSize = cardSize(hierarchyLevel, current); m_titleSize = titlePointSize(hierarchyLevel);
    setToolTip(m_title + '\n' + m_role + '\n' + m_path);
}
void KnowledgeNodeWidget::setTitle(const QString &title) {
    if (m_title == title) return;
    m_title = title; setToolTip(m_title + '\n' + m_role + '\n' + m_path); update();
}
QSizeF KnowledgeNodeWidget::cardSize(int level, bool current) {
    const qreal width = current ? 290.0 : 250.0;
    if (level < 0) return {width * std::pow(.92, -level), 78 * std::pow(.97, qMin(4, -level - 1))};
    if (level > 0) return {width * .62 * std::pow(.9, qMin(4, level - 1)), 66 * std::pow(.95, qMin(4, level - 1))};
    return {width, 100};
}
qreal KnowledgeNodeWidget::titlePointSize(int level) { return level < 0 ? 13.2 : level > 0 ? 11.5 * std::pow(.95, qMin(4, level - 1)) : 14; }
void KnowledgeNodeWidget::setCardMetrics(QSizeF size, qreal titleSize) { prepareGeometryChange(); m_cardSize = size; m_titleSize = titleSize; update(); }
QRectF KnowledgeNodeWidget::boundingRect() const { return {QPointF{}, m_cardSize}; }
void KnowledgeNodeWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(QColor(isSelected() ? "#b6edd4" : m_current ? "#8bc7b0" : m_hover ? "#7c9c8c" : "#3e4943"), m_current || isSelected() ? 2 : 1));
    painter->setBrush(QColor(m_current ? "#2b4037" : m_hover ? "#303d36" : m_hierarchyLevel > 0 ? "#202b31" : m_hierarchyLevel < 0 ? "#29332e" : "#242c28"));
    const QRectF rect = boundingRect().adjusted(1, 1, -1, -1);
    const qreal padding = qMax(8.0, 15.0 * std::pow(.84, qBound(0, m_hierarchyLevel, 5)));
    const qreal radius = m_hierarchyLevel > 0 ? 5 : rect.height() / 2;
    painter->drawRoundedRect(rect, radius, radius);
    QFont font("Segoe UI"); font.setPointSizeF(m_titleSize); font.setWeight(QFont::DemiBold); painter->setFont(font);
    painter->setPen(QColor("#eeeeee"));
    const QRectF textRect = rect.adjusted(padding, 8, -padding, -8);
    // Keep the route readable at a glance. The complete title, role and path
    // remain in the tooltip; oversized titles are limited to two lines.
    const auto metrics = painter->fontMetrics(); QStringList lines;
    const QString title = QString(m_title).replace('\n', ' ');
    QTextLayout titleLayout(title, font); QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere); titleLayout.setTextOption(option);
    titleLayout.beginLayout();
    for (int i = 0; i < 2; ++i) {
        auto line = titleLayout.createLine(); if (!line.isValid()) break;
        line.setLineWidth(textRect.width());
        lines.append(i ? metrics.elidedText(title.mid(line.textStart()).trimmed(), Qt::ElideRight, textRect.width()) : title.mid(line.textStart(), line.textLength()).trimmed());
    }
    titleLayout.endLayout();
    painter->drawText(textRect, Qt::AlignCenter, lines.join('\n'));
    if (m_ports) {
        painter->setPen(QPen(QColor("#a8dac2"), 2)); painter->setBrush(QColor("#20372b"));
        for (const auto &point : {QPointF(6, rect.center().y()), QPointF(rect.right() - 5, rect.center().y()), QPointF(rect.center().x(), 6), QPointF(rect.center().x(), rect.bottom() - 5)})
            painter->drawEllipse(point, 5, 5);
    }
}
void KnowledgeNodeWidget::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) { event->accept(); emit activated(m_path); return; }
    QGraphicsObject::mousePressEvent(event);
}
void KnowledgeNodeWidget::hoverEnterEvent(QGraphicsSceneHoverEvent *) { m_hover = true; update(); }
void KnowledgeNodeWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *) { m_hover = false; update(); }
