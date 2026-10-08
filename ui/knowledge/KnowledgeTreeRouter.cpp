#include "ui/knowledge/KnowledgeTreeRouter.h"
#include "core/knowledge/KnowledgeIndex.h"
#include <QPainterPathStroker>
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>
QPointF KnowledgeTreeRouter::port(const QRectF &rect, NodeRelationType type) {
    switch (type) {
    case NodeRelationType::Previous: return {rect.left(), rect.center().y()};
    case NodeRelationType::Next: return {rect.right(), rect.center().y()};
    case NodeRelationType::Parent: return {rect.center().x(), rect.top()};
    case NodeRelationType::Child: return {rect.center().x(), rect.bottom()};
    case NodeRelationType::DetailOf: return {rect.center().x(), rect.top()};
    }
    return rect.center();
}
NodeRelationType KnowledgeTreeRouter::direction(QPointF point, const QRectF &rect) {
    const QPointF delta = point - rect.center();
    if (std::abs(delta.x()) / (rect.width() / 2) >= std::abs(delta.y()) / (rect.height() / 2))
        return delta.x() < 0 ? NodeRelationType::Previous : NodeRelationType::Next;
    return delta.y() < 0 ? NodeRelationType::Parent : NodeRelationType::Child;
}
QPainterPath KnowledgeTreeRouter::connector(const QRectF &from, const QRectF &to, NodeRelationType type) {
    if (type == NodeRelationType::Child) {
        const QPointF start(from.center().x(), from.bottom()), end(to.center().x(), to.top());
        const qreal middle = (start.y() + end.y()) / 2;
        QPainterPath path(start);
        path.lineTo(start.x(), middle); path.lineTo(end.x(), middle); path.lineTo(end);
        return path;
    }
    if (type == NodeRelationType::Parent) {
        const QRectF upper = from.center().y() < to.center().y() ? from : to;
        QPainterPath path(QPointF(upper.center().x() - upper.width() * .32, upper.bottom()));
        path.lineTo(upper.center().x() + upper.width() * .32, upper.bottom()); return path;
    }
    const QPointF start = port(from, type), end = port(to, KnowledgeIndex::inverse(type));
    const QPointF step[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {0, -1}};
    const QPointF offset = step[static_cast<int>(type)] * qMax(65.0, QLineF(start, end).length() * .4);
    QPainterPath path(start);
    if (qAbs(start.y() - end.y()) < .01 && end.x() > start.x()) path.lineTo(end);
    else path.cubicTo(start + offset, end - offset, end);
    return path;
}
QPainterPath KnowledgeTreeRouter::avoidCards(const QPainterPath &direct, NodeRelationType type, const QList<QRectF> &cards) {
    if (type == NodeRelationType::Parent) return direct;
    QPainterPathStroker stroke; stroke.setWidth(1);
    const auto ink = stroke.createStroke(direct);
    bool obstructed = false;
    for (const auto &card : cards) if (ink.intersects(card.adjusted(1, 1, -1, -1))) { obstructed = true; break; }
    if (!obstructed) return direct;
    // Shared references, skipped steps and cycles sometimes cross another
    // card. Route only those exceptional links through the free corridors.
    const QPointF start = direct.pointAtPercent(0), end = direct.pointAtPercent(1);
    const QPointF outward = type == NodeRelationType::Child ? QPointF(0, 18) : type == NodeRelationType::Previous ? QPointF(-18, 0) : QPointF(18, 0);
    const QPointF a = start + outward, b = end - outward;
    QList<qreal> xs{a.x(), b.x()}, ys{a.y(), b.y()};
    for (const auto &card : cards) { xs << card.left() - 18 << card.right() + 18; ys << card.top() - 18 << card.bottom() + 18; }
    auto unique = [](QList<qreal> &values) { std::sort(values.begin(), values.end()); values.erase(std::unique(values.begin(), values.end()), values.end()); };
    unique(xs); unique(ys);
    const int width = xs.size(), height = ys.size();
    auto point = [&](int cell) { return QPointF(xs[cell % width], ys[cell / width]); };
    QList<QRectF> interiors; for (const auto &card : cards) interiors.append(card.adjusted(1, 1, -1, -1));
    std::vector<signed char> passages(width * height * 2, -1);
    auto clear = [&](int from, int to) {
        const int key = qMin(from, to) * 2 + (qAbs(from - to) == width ? 1 : 0);
        if (passages[key] >= 0) return passages[key] == 1;
        const auto p = point(from), q = point(to);
        for (const auto &r : interiors) {
            if (p.y() == q.y()) {
                if (p.y() > r.top() && p.y() < r.bottom() && qMax(p.x(), q.x()) > r.left() && qMin(p.x(), q.x()) < r.right()) { passages[key] = 0; return false; }
            } else if (p.x() > r.left() && p.x() < r.right() && qMax(p.y(), q.y()) > r.top() && qMin(p.y(), q.y()) < r.bottom()) { passages[key] = 0; return false; }
        }
        passages[key] = 1;
        return true;
    };
    const int source = (ys.indexOf(a.y()) * width + xs.indexOf(a.x())) * 3;
    const int targetCell = ys.indexOf(b.y()) * width + xs.indexOf(b.x());
    auto heuristic = [&](int state) { const auto p = point(state / 3); return qAbs(p.x() - b.x()) + qAbs(p.y() - b.y()); };
    std::vector<qreal> costs(width * height * 3, std::numeric_limits<qreal>::max());
    std::vector<int> parents(costs.size(), -1);
    using Entry = std::pair<qreal, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
    costs[source] = 0; open.push({heuristic(source), source}); int finish = -1;
    while (!open.empty()) {
        const auto [estimate, state] = open.top(); open.pop();
        if (estimate > costs[state] + heuristic(state) + .01) continue;
        const int cell = state / 3, column = cell % width, row = cell / width;
        if (cell == targetCell) { finish = state; break; }
        const int adjacent[] = {column ? cell - 1 : -1, column + 1 < width ? cell + 1 : -1, row ? cell - width : -1, row + 1 < height ? cell + width : -1};
        for (int i = 0; i < 4; ++i) {
            const int neighbour = adjacent[i], orientation = i < 2 ? 1 : 2;
            if (neighbour < 0 || !clear(cell, neighbour)) continue;
            const int nextState = neighbour * 3 + orientation;
            const qreal cost = costs[state] + QLineF(point(cell), point(neighbour)).length() + (state % 3 && state % 3 != orientation ? 28 : 0);
            if (cost >= costs[nextState]) continue;
            costs[nextState] = cost; parents[nextState] = state; open.push({cost + heuristic(nextState), nextState});
        }
    }
    if (finish < 0) return direct;
    QList<QPointF> points;
    for (int state = finish; state >= 0; state = parents[state]) points.prepend(point(state / 3));
    points.prepend(start); points.append(end);
    QList<QPointF> compact;
    for (const auto &p : points) {
        if (!compact.isEmpty() && compact.last() == p) continue;
        while (compact.size() > 1 && ((compact[compact.size() - 2].x() == compact.last().x() && compact.last().x() == p.x()) ||
            (compact[compact.size() - 2].y() == compact.last().y() && compact.last().y() == p.y()))) compact.removeLast();
        compact.append(p);
    }
    QPainterPath path(compact.front()); for (int i = 1; i < compact.size(); ++i) path.lineTo(compact[i]); return path;
}
