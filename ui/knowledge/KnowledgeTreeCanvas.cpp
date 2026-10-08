#include "ui/knowledge/KnowledgeTreeCanvas.h"
#include "ui/knowledge/KnowledgeNodeWidget.h"
#include "ui/knowledge/KnowledgeTreeLayout.h"
#include "ui/knowledge/KnowledgeTreeRouter.h"
#include "core/library/MarkdownManager.h"
#include "core/knowledge/MarkdownRelation.h"
#include <QApplication>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QShowEvent>
#include "core/settings/KnowledgeViewSettings.h"
#include <QSignalBlocker>
#include <QSpinBox>
#include <QScrollBar>
#include <QDataStream>
#include <QTimer>
#include <cmath>
#include <algorithm>

using namespace KnowledgeTreeRouter;

namespace {
void arrowHead(QGraphicsScene *scene, const QPainterPath &path, QColor color, QList<QGraphicsItem *> *items = nullptr, qreal size = 10) {
    const QPointF end = path.pointAtPercent(1), before = path.pointAtPercent(.96);
    const double angle = std::atan2(end.y() - before.y(), end.x() - before.x());
    const auto a = end - QPointF(std::cos(angle - .5) * size, std::sin(angle - .5) * size);
    const auto b = end - QPointF(std::cos(angle + .5) * size, std::sin(angle + .5) * size);
    auto *head = scene->addPolygon(QPolygonF{end, a, b}, Qt::NoPen, color); head->setZValue(-.5);
    if (items) { head->setZValue(5); items->append(head); }
}
}
KnowledgeTreeCanvas::KnowledgeTreeCanvas(QWidget *parent) : QGraphicsView(parent) {
    setObjectName("knowledgeTreeCanvas"); setScene(new QGraphicsScene(this)); setAcceptDrops(true);
    setRenderHint(QPainter::Antialiasing); setBackgroundBrush(QColor("#1b211e"));
    setFrameShape(QFrame::NoFrame); setDragMode(ScrollHandDrag);
    // Panning handles overflow. Fixed policies also prevent fit/resize feedback
    // when a tall nested tree alternately adds and removes scrollbars.
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTransformationAnchor(AnchorUnderMouse); setFocusPolicy(Qt::StrongFocus);
    setResizeAnchor(AnchorViewCenter);
    setToolTip(QStringLiteral("以当前文档为中心，前后各展开最多 5 层；滚轮缩放，拖动空白处平移"));
    connect(&KnowledgeViewSettings::shared(), &KnowledgeViewSettings::zoomChanged, this, &KnowledgeTreeCanvas::applyZoomPercent);
    applyZoomPercent(KnowledgeViewSettings::shared().zoomPercent());
}
void KnowledgeTreeCanvas::setReadOnly(bool readOnly) {
    m_readOnly = readOnly; setAcceptDrops(!readOnly); viewport()->setAcceptDrops(!readOnly);
    clearPreview(); m_dragSource.clear(); m_dragging = false;
    for (auto *card : m_cards) { card->setFlag(QGraphicsItem::ItemIsSelectable, !readOnly); card->setConnectionPorts(false); }
}
QRectF KnowledgeTreeCanvas::nodeRect(const QString &path) const {
    const auto *card = m_cards.value(path); return card ? card->sceneBoundingRect() : QRectF{};
}
void KnowledgeTreeCanvas::updateTitle(const QString &path, const QString &title) {
    if (auto *card = m_cards.value(path)) card->setTitle(title);
}
void KnowledgeTreeCanvas::setGraph(MarkdownManager *manager, const QString &root) {
    if (m_manager == manager && m_index.root == root && m_graphRevision == manager->graphRevision()) {
        for (auto it = m_cards.cbegin(); it != m_cards.cend(); ++it)
            if (const auto *node = manager->loadNode(it.key())) it.value()->setTitle(node->title());
        return;
    }
    m_graphRevision = manager->graphRevision();
    const QPointF previousCenter = mapToScene(viewport()->rect().center());
    const int previousHorizontal = horizontalScrollBar()->value(), previousVertical = verticalScrollBar()->value();
    const bool changedRoot = m_index.root != root;
    const bool keepView = m_manager == manager && m_cards.contains(root)
        && viewport()->rect().adjusted(20, 20, -20, -20).contains(mapFromScene(nodeRect(root).center()));
    clearPreview(); m_dragSource.clear(); m_dragging = false; m_cards.clear(); m_edges.clear(); m_selectedEdge = -1;
    scene()->clear(); m_index = KnowledgeIndex::build(*manager, root);
    // Compute coordinates for the complete connected tree, then display only
    // the five-step index. Boundary cards keep their coordinates on recenter.
    auto complete = KnowledgeIndex::build(*manager, root, manager->allNodes().size());
    auto paths = complete.paths; paths.sort();
    QByteArray signature; QDataStream snapshot(&signature, QIODevice::WriteOnly);
    snapshot << paths;
    for (const auto &edge : complete.edges) snapshot << edge.source << edge.target << qint32(edge.type);
    if (m_manager != manager || signature != m_layoutSignature) {
        m_layout = KnowledgeTreeLayout::build(complete); m_layoutSignature = signature;
    }
    m_manager = manager; const auto &layout = m_layout;
    for (const auto &path : m_index.paths) {
        const auto *node = manager->loadNode(path); if (!node) continue;
        const bool current = path == m_index.root;
        const int level = layout.levels.value(path);
        const QString role = level < 0 ? QStringLiteral("同级替代") : level > 0 ? QStringLiteral("详细说明 / 实践") : current ? QStringLiteral("当前知识节点") : QStringLiteral("学习节点");
        auto *card = new KnowledgeNodeWidget(path, node->title(), role, current, level);
        card->setCardMetrics(layout.rects.value(path).size(), layout.titleSizes.value(path));
        scene()->addItem(card); card->setPos(layout.rects.value(path).topLeft());
        card->setAcceptedMouseButtons(Qt::NoButton); card->setFlag(QGraphicsItem::ItemIsSelectable, !m_readOnly);
        m_cards.insert(path, card);
    }
    QList<QRectF> obstacles; for (const auto &rect : layout.rects) obstacles.append(rect);
    for (const auto &edge : m_index.edges) {
        const QString source = edge.type == NodeRelationType::Parent ? edge.source : layout.routeAnchors.value(edge.source);
        const QString target = edge.type == NodeRelationType::Parent ? edge.target : layout.routeAnchors.value(edge.target);
        QRectF targetRect = nodeRect(target);
        if (edge.type == NodeRelationType::Child) {
            // An alternative stack shares its incoming detail connection.
            for (const auto &peer : m_index.paths) if (layout.routeAnchors.value(peer) == layout.routeAnchors.value(target))
                targetRect.setTop(qMin(targetRect.top(), nodeRect(peer).top()));
        }
        const auto path = avoidCards(connector(nodeRect(source), targetRect, edge.type), edge.type, obstacles);
        const bool route = edge.type == NodeRelationType::Next, detail = edge.type == NodeRelationType::Child;
        QPen pen(QColor(route ? "#9bc8af" : "#90aab7"), route ? 3.2 : 1.6, edge.type == NodeRelationType::Parent ? Qt::NoPen : Qt::SolidLine);
        pen.setJoinStyle(Qt::RoundJoin); pen.setCapStyle(Qt::RoundCap);
        auto *line = scene()->addPath(path, pen); line->setZValue(-1);
        line->setData(0, static_cast<int>(edge.type));
        line->setToolTip(edge.source + QStringLiteral(" → ") + MarkdownRelation::label(edge.type) + " · " + edge.target);
        if (route || detail) arrowHead(scene(), path, QColor(route ? "#a8d6bd" : "#90aab7"), nullptr, detail ? 8 : 10);
        m_edges.append({edge, line});
    }
    scene()->setSceneRect(scene()->itemsBoundingRect().adjusted(-420, -220, 420, 220));
    selectNode(m_readOnly ? QString{} : m_cards.contains(m_selectedNode) ? m_selectedNode : m_index.root);
    if (keepView) { horizontalScrollBar()->setValue(previousHorizontal); verticalScrollBar()->setValue(previousVertical); }
    else if (changedRoot) centerCurrent(); else centerOn(previousCenter);
}
void KnowledgeTreeCanvas::applyZoomPercent(int percent) {
    const bool changed = m_zoomPercent != percent;
    m_zoomPercent = percent;
    setTransform(QTransform::fromScale(percent / 100.0, percent / 100.0));
    if (changed) emit zoomPercentChanged(percent);
}
void KnowledgeTreeCanvas::setZoomPercent(int percent) {
    percent = qBound(20, percent, 150);
    if (percent == m_zoomPercent) return;
    KnowledgeViewSettings::shared().setZoomPercent(percent);
}
QSpinBox *KnowledgeTreeCanvas::createZoomControl(QWidget *parent) {
    auto *control = new QSpinBox(parent); control->setObjectName("knowledgeTreeZoom");
    control->setRange(20, 150); control->setSingleStep(5); control->setSuffix("%");
    control->setKeyboardTracking(false); control->setValue(m_zoomPercent);
    control->setMinimumSize(90, 32);
    control->setToolTip(QStringLiteral("两个知识树页面共用并记住缩放倍率（20%～150%）"));
    connect(control, &QSpinBox::valueChanged, this, &KnowledgeTreeCanvas::setZoomPercent);
    connect(this, &KnowledgeTreeCanvas::zoomPercentChanged, control, [control](int percent) {
        const QSignalBlocker blocker(control); control->setValue(percent);
    });
    return control;
}
void KnowledgeTreeCanvas::restoreView() {
    applyZoomPercent(KnowledgeViewSettings::shared().zoomPercent()); centerCurrent();
}
void KnowledgeTreeCanvas::centerCurrent() {
    const QRectF overview = scene()->itemsBoundingRect();
    if (overview.width() * transform().m11() + 24 <= viewport()->width()
        && overview.height() * transform().m22() + 24 <= viewport()->height()) { centerOn(overview.center()); return; }
    // Center the current card together with its attached alternatives and
    // immediate details. Their asymmetry should not clip the lower cards.
    QRectF focus = nodeRect(m_index.root);
    const QString anchor = m_layout.routeAnchors.value(m_index.root);
    QSet<QString> groups{anchor};
    for (const auto &edge : m_index.edges)
        if (edge.type == NodeRelationType::Child && m_layout.routeAnchors.value(edge.source) == anchor)
            groups.insert(m_layout.routeAnchors.value(edge.target));
    for (const auto &path : m_index.paths)
        if (groups.contains(m_layout.routeAnchors.value(path))) focus = focus.united(nodeRect(path));
    centerOn(focus.center());
}
KnowledgeNodeWidget *KnowledgeTreeCanvas::cardAt(QPointF point) const {
    for (auto *card : m_cards) if (card->sceneBoundingRect().contains(point)) return card;
    return nullptr;
}
KnowledgeNodeWidget *KnowledgeTreeCanvas::nearestCard(QPointF point, const QString &exclude) const {
    double minimum = std::numeric_limits<double>::max(); KnowledgeNodeWidget *nearest = nullptr;
    for (auto *card : m_cards) {
        if (card->path() == exclude) continue;
        const QRectF rect = card->sceneBoundingRect();
        const double dx = qMax(qMax(rect.left() - point.x(), point.x() - rect.right()), 0.0);
        const double dy = qMax(qMax(rect.top() - point.y(), point.y() - rect.bottom()), 0.0);
        const double distance = dx * dx + dy * dy;
        if (distance < minimum) { minimum = distance; nearest = card; }
    }
    return nearest;
}
void KnowledgeTreeCanvas::selectNode(const QString &path) {
    m_selectedNode = path; selectEdge(-1);
    for (auto *card : m_cards) { card->setSelected(card->path() == path); card->setConnectionPorts(card->path() == path); }
    emit selectionChanged();
}
void KnowledgeTreeCanvas::selectEdge(int index) {
    m_selectedEdge = index;
    for (int i = 0; i < m_edges.size(); ++i) {
        const bool route = m_edges[i].edge.type == NodeRelationType::Next, peer = m_edges[i].edge.type == NodeRelationType::Parent;
        QPen pen(QColor(i == index ? "#d8d596" : route ? "#9bc8af" : "#90aab7"), i == index ? 4 : route ? 3.2 : 1.6, peer && i != index ? Qt::NoPen : Qt::SolidLine);
        pen.setJoinStyle(Qt::RoundJoin); m_edges[i].line->setPen(pen);
    }
    if (index >= 0) {
        m_selectedNode.clear(); for (auto *card : m_cards) { card->setSelected(false); card->setConnectionPorts(false); }
    }
    emit selectionChanged();
}
void KnowledgeTreeCanvas::clearPreview() {
    for (auto *item : m_previewItems) delete item;
    m_previewItems.clear(); m_preview = {}; emit previewChanged({});
}
void KnowledgeTreeCanvas::showPreview(const QString &source, const QString &target, NodeRelationType type, const QRectF &sourceRect, const QRectF &targetRect, const QString &message) {
    clearPreview();
    if (source.isEmpty() || target.isEmpty() || source == target) return;
    m_preview = {true, source, target, message, type};
    const QPen pen(QColor("#b6e5ce"), 2, Qt::DashLine);
    for (const auto &rect : {sourceRect, targetRect}) {
        auto *frame = scene()->addRect(rect.adjusted(-5, -5, 5, 5), pen, Qt::NoBrush); frame->setZValue(4); m_previewItems.append(frame);
    }
    const auto path = connector(sourceRect, targetRect, type);
    auto *line = scene()->addPath(path, pen); line->setZValue(5); m_previewItems.append(line);
    if (type != NodeRelationType::Parent) arrowHead(scene(), path, QColor("#b6e5ce"), &m_previewItems, type == NodeRelationType::Child ? 8 : 10);
    auto *label = scene()->addText(message, QFont("Microsoft YaHei", 10)); label->setDefaultTextColor(QColor("#d0f4e1"));
    label->setPos(path.pointAtPercent(.5) + QPointF(0, 12)); label->setZValue(6); m_previewItems.append(label);
    emit previewChanged(message);
}
void KnowledgeTreeCanvas::previewExternal(const QString &path, QPointF point) {
    if (!m_manager || !m_manager->loadNode(path) || m_cards.contains(path)) { clearPreview(); return; }
    auto *anchor = nearestCard(point, path); if (!anchor) { clearPreview(); return; }
    const QRectF anchorRect = anchor->sceneBoundingRect(); const auto type = direction(point, anchorRect);
    const int ownerLevel = m_layout.levels.value(m_layout.routeAnchors.value(anchor->path(), anchor->path()));
    const int level = type == NodeRelationType::Child ? ownerLevel + 1 : type == NodeRelationType::Parent ? -1 : ownerLevel;
    const QSizeF size = type == NodeRelationType::Parent ? QSizeF(anchorRect.width() * .92, anchorRect.height() * .78) : KnowledgeNodeWidget::cardSize(level);
    QRectF ghost(point - QPointF(size.width() / 2, size.height() / 2), size);
    // Keep the suggested card outside the anchor so even a drop on its body has a clear preview.
    if (ghost.intersects(anchorRect.adjusted(-20, -20, 20, 20))) {
        if (type == NodeRelationType::Previous) ghost.moveRight(anchorRect.left() - 100);
        else if (type == NodeRelationType::Next) ghost.moveLeft(anchorRect.right() + 100);
        else if (type == NodeRelationType::Parent) ghost.moveBottom(anchorRect.top());
        else ghost.moveTop(anchorRect.bottom() + qMax(38.0, anchorRect.height() * .6));
    }
    const QString message = QStringLiteral("%1 的%2：%3").arg(m_manager->loadNode(anchor->path())->title(), MarkdownRelation::label(type), m_manager->loadNode(path)->title());
    showPreview(anchor->path(), path, type, anchorRect, ghost, message);
    QFont previewFont("Segoe UI"); previewFont.setPointSizeF(KnowledgeNodeWidget::titlePointSize(level)); previewFont.setWeight(QFont::DemiBold);
    auto *title = scene()->addText(m_manager->loadNode(path)->title(), previewFont); title->setTextWidth(ghost.width() - 20);
    title->setDefaultTextColor(QColor("#d0f4e1")); title->setPos(ghost.topLeft() + QPointF(8, 8)); title->setZValue(6); m_previewItems.append(title);
}
void KnowledgeTreeCanvas::previewConnection(QPointF point) {
    auto *target = cardAt(point);
    if (!target || target->path() == m_dragSource || !m_cards.contains(m_dragSource)) { clearPreview(); return; }
    const auto sourceRect = nodeRect(m_dragSource), targetRect = target->sceneBoundingRect();
    const auto type = m_port >= 0 ? static_cast<NodeRelationType>(m_port) : direction(targetRect.center(), sourceRect);
    const QString message = QStringLiteral("%1 的%2：%3").arg(m_manager->loadNode(m_dragSource)->title(), MarkdownRelation::label(type), m_manager->loadNode(target->path())->title());
    showPreview(m_dragSource, target->path(), type, sourceRect, targetRect, message);
}
void KnowledgeTreeCanvas::commitPreview() {
    const auto preview = m_preview; clearPreview();
    if (preview.valid) emit relationRequested(preview.source, preview.target, preview.type);
}
void KnowledgeTreeCanvas::dragEnterEvent(QDragEnterEvent *event) {
    const QString path = QString::fromUtf8(event->mimeData()->data(mimeType()));
    if (!m_readOnly && m_manager && m_manager->loadNode(path) && !m_cards.contains(path)) { event->setDropAction(Qt::CopyAction); event->accept(); }
    else event->ignore();
}
void KnowledgeTreeCanvas::dragMoveEvent(QDragMoveEvent *event) {
    if (m_readOnly) { event->ignore(); return; }
    previewExternal(QString::fromUtf8(event->mimeData()->data(mimeType())), mapToScene(event->position().toPoint()));
    if (m_preview.valid) { event->setDropAction(Qt::CopyAction); event->accept(); } else event->ignore();
}
void KnowledgeTreeCanvas::dragLeaveEvent(QDragLeaveEvent *event) { clearPreview(); event->accept(); }
void KnowledgeTreeCanvas::dropEvent(QDropEvent *event) {
    if (m_readOnly) { event->ignore(); return; }
    previewExternal(QString::fromUtf8(event->mimeData()->data(mimeType())), mapToScene(event->position().toPoint()));
    if (!m_preview.valid) { event->ignore(); return; }
    event->setDropAction(Qt::CopyAction); event->accept(); commitPreview();
}
void KnowledgeTreeCanvas::mousePressEvent(QMouseEvent *event) {
    if (m_readOnly) {
        m_pressedPath.clear(); m_pressPosition = event->position().toPoint();
        if (event->button() == Qt::LeftButton) if (auto *card = cardAt(mapToScene(m_pressPosition))) m_pressedPath = card->path();
        QGraphicsView::mousePressEvent(event); return;
    }
    if (event->button() != Qt::LeftButton) { QGraphicsView::mousePressEvent(event); return; }
    const QPointF point = mapToScene(event->position().toPoint());
    // The touching upper card expresses an alternative without an extra bus.
    // Its seam can still be selected on either side of the connection port.
    QPainterPathStroker seam; seam.setWidth(6 / transform().m11());
    for (int i = 0; i < m_edges.size(); ++i) if (m_edges[i].edge.type == NodeRelationType::Parent &&
        qAbs(point.x() - m_edges[i].line->path().boundingRect().center().x()) > 18 &&
        seam.createStroke(m_edges[i].line->path()).contains(point)) {
        selectEdge(i); event->accept(); return;
    }
    if (auto *card = cardAt(point)) {
        selectNode(card->path()); m_dragSource = card->path(); m_pressPosition = event->position().toPoint(); m_port = -1;
        for (int i = 0; i < 4; ++i) if (QLineF(point, port(card->sceneBoundingRect(), static_cast<NodeRelationType>(i))).length() < 18) { m_port = i; break; }
        event->accept(); return;
    }
    QPainterPathStroker stroker; stroker.setWidth(16 / transform().m11());
    for (int i = 0; i < m_edges.size(); ++i) if (stroker.createStroke(m_edges[i].line->path()).contains(point)) {
        selectEdge(i); event->accept(); return;
    }
    selectNode({}); QGraphicsView::mousePressEvent(event);
}
void KnowledgeTreeCanvas::mouseMoveEvent(QMouseEvent *event) {
    if (m_readOnly) {
        if ((event->position().toPoint() - m_pressPosition).manhattanLength() >= QApplication::startDragDistance()) m_pressedPath.clear();
        QGraphicsView::mouseMoveEvent(event); return;
    }
    if (!m_dragSource.isEmpty() && event->buttons().testFlag(Qt::LeftButton)) {
        if (!m_dragging && (event->position().toPoint() - m_pressPosition).manhattanLength() >= QApplication::startDragDistance()) m_dragging = true;
        if (m_dragging) { previewConnection(mapToScene(event->position().toPoint())); setCursor(Qt::CrossCursor); }
        event->accept(); return;
    }
    QGraphicsView::mouseMoveEvent(event);
}
void KnowledgeTreeCanvas::mouseReleaseEvent(QMouseEvent *event) {
    if (m_readOnly) {
        const QString path = m_pressedPath; m_pressedPath.clear(); QGraphicsView::mouseReleaseEvent(event);
        if (event->button() == Qt::LeftButton && !path.isEmpty() && (event->position().toPoint() - m_pressPosition).manhattanLength() < QApplication::startDragDistance()) emit nodeActivated(path);
        return;
    }
    if (!m_dragSource.isEmpty() && event->button() == Qt::LeftButton) {
        if (m_dragging) { previewConnection(mapToScene(event->position().toPoint())); commitPreview(); }
        m_dragSource.clear(); m_dragging = false; unsetCursor(); event->accept(); return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}
void KnowledgeTreeCanvas::mouseDoubleClickEvent(QMouseEvent *event) {
    if (!cardAt(mapToScene(event->position().toPoint()))) { restoreView(); event->accept(); return; }
    if (m_readOnly) { QGraphicsView::mouseDoubleClickEvent(event); return; }
    if (auto *card = cardAt(mapToScene(event->position().toPoint()))) {
        m_dragSource.clear(); m_dragging = false; clearPreview(); emit rootRequested(card->path()); event->accept(); return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}
void KnowledgeTreeCanvas::removeSelectedRelation() {
    if (m_selectedEdge < 0 || m_selectedEdge >= m_edges.size()) return;
    const auto edge = m_edges[m_selectedEdge].edge; emit relationRemoveRequested(edge.source, edge.target);
}
void KnowledgeTreeCanvas::keyPressEvent(QKeyEvent *event) {
    if (m_readOnly) { QGraphicsView::keyPressEvent(event); return; }
    if (event->key() == Qt::Key_Escape && (!m_dragSource.isEmpty() || m_preview.valid)) {
        clearPreview(); m_dragSource.clear(); m_dragging = false; unsetCursor(); event->accept(); return;
    }
    if (event->key() == Qt::Key_Delete && hasSelectedEdge()) { removeSelectedRelation(); event->accept(); return; }
    QGraphicsView::keyPressEvent(event);
}
void KnowledgeTreeCanvas::wheelEvent(QWheelEvent *event) {
    setZoomPercent(qRound(m_zoomPercent * std::pow(1.12, event->angleDelta().y() / 120.0)));
    event->accept();
}
void KnowledgeTreeCanvas::showEvent(QShowEvent *event) {
    QGraphicsView::showEvent(event); restoreView();
    // The surrounding splitter settles after showEvent. Center using its final
    // viewport size so a fitting tree opens as a complete overview.
    QTimer::singleShot(0, this, &KnowledgeTreeCanvas::centerCurrent);
}
