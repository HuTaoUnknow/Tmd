#include "KnowledgeTreeLayout.h"
#include "KnowledgeNodeWidget.h"
#include "core/MarkdownRelation.h"
#include <QSet>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace {
struct Block {
    QMap<QString, QRectF> cards;
    QRectF bounds;
    void append(const Block &other, QPointF offset) {
        const bool empty = cards.isEmpty();
        for (auto it = other.cards.cbegin(); it != other.cards.cend(); ++it) cards[it.key()] = it.value().translated(offset);
        bounds = empty ? other.bounds.translated(offset) : bounds.united(other.bounds.translated(offset));
    }
};
struct Attachment { QString source, target, component; };
KnowledgeIndexResult canonicalIndex(KnowledgeIndexResult index) {
    // Selection is a viewport concern. Choose a foundation from the stored
    // learning routes and hierarchy, never from the currently clicked card.
    index.paths.sort();
    RelationStore alternatives;
    QMap<QString, QString> routeComponent;
    for (const auto &path : index.paths) routeComponent[path] = path;
    std::function<QString(const QString &)> component = [&](const QString &path) {
        if (routeComponent[path] != path) routeComponent[path] = component(routeComponent[path]);
        return routeComponent[path];
    };
    QMap<QString, QStringList> adjacent;
    QMap<QString, int> cardScore;
    for (const auto &edge : index.edges) {
        adjacent[edge.source].append(edge.target); adjacent[edge.target].append(edge.source);
        if (edge.type == NodeRelationType::Parent) alternatives[edge.source][edge.target] = edge.type;
        if (edge.type == NodeRelationType::Next || edge.type == NodeRelationType::Parent) {
            const QString a = component(edge.source), b = component(edge.target);
            routeComponent[qMax(a, b)] = qMin(a, b);
        }
        if (edge.type == NodeRelationType::Next) { ++cardScore[edge.source]; ++cardScore[edge.target]; }
        else if (edge.type == NodeRelationType::Child) cardScore[edge.source] += 2;
    }
    const auto groups = MarkdownRelation::alternativeGroups(alternatives);
    QMap<QString, QStringList> members;
    QMap<QString, QSet<QString>> children, parents, next, previous;
    for (const auto &path : index.paths) members[component(path)].append(path);
    for (const auto &edge : index.edges) {
        if (edge.type == NodeRelationType::Child && component(edge.source) != component(edge.target)) {
            children[component(edge.source)].insert(component(edge.target)); parents[component(edge.target)].insert(component(edge.source));
        } else if (edge.type == NodeRelationType::Next) {
            const QString a = groups.value(edge.source, edge.source), b = groups.value(edge.target, edge.target);
            if (a != b) { next[a].insert(b); previous[b].insert(a); }
        }
    }
    auto reachable = [](const QString &start, const QMap<QString, QSet<QString>> &links) {
        QStringList queue{start}; QSet<QString> seen{start};
        for (int i = 0; i < queue.size(); ++i) for (const auto &target : links.value(queue[i]))
            if (!seen.contains(target)) { seen.insert(target); queue.append(target); }
        return seen;
    };
    QString foundation; int best = std::numeric_limits<int>::min();
    const bool hasRoot = std::any_of(members.keyBegin(), members.keyEnd(), [&](const auto &key) { return parents.value(key).isEmpty(); });
    for (auto it = members.cbegin(); it != members.cend(); ++it) {
        if (hasRoot && !parents.value(it.key()).isEmpty()) continue;
        int score = 0;
        for (const auto &key : reachable(it.key(), children)) score += members.value(key).size();
        // A detail cycle has no top-level root. Prefer the node supplying more
        // explanations over its backlink, then use a deterministic path tie.
        if (!hasRoot) score = (children.value(it.key()).size() - parents.value(it.key()).size()) * (index.paths.size() + 1) + score;
        if (score > best) { best = score; foundation = it.key(); }
    }
    QString anchor; best = -1;
    QSet<QString> candidates;
    for (const auto &path : members.value(foundation)) if (previous.value(groups.value(path, path)).isEmpty()) candidates.insert(groups.value(path, path));
    if (candidates.isEmpty()) for (const auto &path : members.value(foundation)) candidates.insert(groups.value(path, path));
    QStringList sorted = candidates.values(); sorted.sort();
    for (const auto &group : sorted) {
        const int score = reachable(group, next).size();
        if (score > best) { best = score; anchor = group; }
    }
    QString root; best = -1;
    for (const auto &path : members.value(foundation)) if (groups.value(path, path) == anchor && cardScore.value(path) > best) {
        best = cardScore.value(path); root = path;
    }
    index.root = root; index.depths.clear(); index.depths[root] = 0;
    QStringList queue{root};
    for (int i = 0; i < queue.size(); ++i) for (const auto &target : adjacent.value(queue[i])) if (!index.depths.contains(target)) {
        index.depths[target] = index.depths.value(queue[i]) + 1; queue.append(target);
    }
    return index;
}
}
KnowledgeTreeLayoutResult KnowledgeTreeLayout::build(const KnowledgeIndexResult &requested) {
    KnowledgeTreeLayoutResult result;
    if (requested.paths.isEmpty()) return result;
    const auto index = canonicalIndex(requested);
    RelationStore alternatives;
    for (const auto &edge : index.edges) if (edge.type == NodeRelationType::Parent) alternatives[edge.source][edge.target] = edge.type;
    const auto sets = MarkdownRelation::alternativeGroups(alternatives);
    QMap<QString, QStringList> groups;
    QMap<QString, int> routeScore, attachmentDepth;
    for (const auto &path : index.paths) groups[sets.value(path, path)].append(path);
    for (const auto &edge : index.edges) if (edge.type == NodeRelationType::Next) { ++routeScore[edge.source]; ++routeScore[edge.target]; }
    for (const auto &edge : index.edges) if (edge.type == NodeRelationType::Child && sets.value(edge.source, edge.source) != sets.value(edge.target, edge.target)) {
        attachmentDepth[edge.target] = qMin(attachmentDepth.value(edge.target, std::numeric_limits<int>::max()), index.depths.value(edge.source));
    }
    QStringList leaders;
    for (auto group = groups.begin(); group != groups.end(); ++group) {
        std::sort(group->begin(), group->end()); QString leader = group->front();
        // Keep the explicitly attached detail at the base of its alternative
        // stack. A backlink or a route on its peer must not swap the cards.
        for (const auto &path : *group) {
            const int candidateDepth = attachmentDepth.value(path, std::numeric_limits<int>::max());
            const int leaderDepth = attachmentDepth.value(leader, std::numeric_limits<int>::max());
            if (candidateDepth < leaderDepth || (candidateDepth == leaderDepth &&
                (routeScore.value(path) > routeScore.value(leader) ||
                (routeScore.value(path) == routeScore.value(leader) && index.depths.value(path) < index.depths.value(leader))))) leader = path;
        }
        if (group->contains(index.root)) leader = index.root;
        leaders.append(leader);
        for (const auto &path : *group) result.routeAnchors[path] = leader;
    }
    std::sort(leaders.begin(), leaders.end());
    QMap<QString, QStringList> next, previous, neighbours;
    for (const auto &edge : index.edges) if (edge.type == NodeRelationType::Next) {
        const QString a = result.routeAnchors.value(edge.source), b = result.routeAnchors.value(edge.target);
        if (a == b || next[a].contains(b)) continue;
        next[a].append(b); previous[b].append(a); neighbours[a].append(b); neighbours[b].append(a);
    }
    // A learning route is laid out as one unit. A detail backlink must never
    // turn one of its predecessor/successor cards into a separate vertical list.
    QMap<QString, QString> componentOf;
    QMap<QString, QStringList> components;
    for (const auto &leader : leaders) if (!componentOf.contains(leader)) {
        QStringList queue{leader}; componentOf[leader] = leader;
        for (int i = 0; i < queue.size(); ++i) for (const auto &peer : neighbours.value(queue[i])) if (!componentOf.contains(peer)) {
            componentOf[peer] = leader; queue.append(peer);
        }
        std::sort(queue.begin(), queue.end()); components[leader] = queue;
    }
    QList<Attachment> candidates;
    for (const auto &edge : index.edges) if (edge.type == NodeRelationType::Child) {
        const QString a = result.routeAnchors.value(edge.source), b = result.routeAnchors.value(edge.target);
        if (componentOf.value(a) != componentOf.value(b)) candidates.append({a, b, componentOf.value(b)});
    }
    std::sort(candidates.begin(), candidates.end(), [&](const auto &a, const auto &b) {
        if (index.depths.value(a.source) != index.depths.value(b.source)) return index.depths.value(a.source) < index.depths.value(b.source);
        return a.source == b.source ? a.target < b.target : a.source < b.source;
    });
    QMap<QString, QString> owners;
    QMap<QString, QList<Attachment>> children;
    for (const auto &candidate : candidates) {
        if (owners.contains(candidate.component)) continue;
        QString ancestor = componentOf.value(candidate.source); QSet<QString> seen;
        while (owners.contains(ancestor) && !seen.contains(ancestor)) { seen.insert(ancestor); ancestor = owners.value(ancestor); }
        if (ancestor == candidate.component) continue;
        owners[candidate.component] = componentOf.value(candidate.source); children[candidate.source].append(candidate);
    }
    const QString rootComponent = componentOf.value(result.routeAnchors.value(index.root));
    QString forestRoot = rootComponent;
    while (owners.contains(forestRoot)) forestRoot = owners.value(forestRoot);

    std::function<Block(const QString &, const QString &, int)> buildComponent;
    buildComponent = [&](const QString &component, const QString &anchor, int depth) {
        QMap<QString, Block> blocks;
        const int detailDepth = depth;
        for (const auto &leader : components.value(component)) {
            Block block;
            const QSizeF size = KnowledgeNodeWidget::cardSize(detailDepth, 0, false);
            const QRectF main(-size.width() / 2, -size.height() / 2, size.width(), size.height());
            block.cards[leader] = main; block.bounds = main;
            result.levels[leader] = detailDepth; result.titleSizes[leader] = KnowledgeNodeWidget::titlePointSize(detailDepth);
            qreal upper = main.top(); int ordinal = 0;
            for (const auto &path : groups.value(sets.value(leader, leader))) if (path != leader) {
                ++ordinal;
                const QSizeF peerSize(size.width() * std::pow(.92, ordinal), size.height() * .78 * std::pow(.97, ordinal - 1));
                upper -= peerSize.height();
                const QRectF peer(-peerSize.width() / 2, upper, peerSize.width(), peerSize.height());
                block.cards[path] = peer; block.bounds = block.bounds.united(peer);
                result.levels[path] = -ordinal; result.titleSizes[path] = result.titleSizes.value(leader) * .94;
            }
            // Reserve the complete width of every nested branch, then put all
            // siblings in a common row. Bounds include aliases and sub-routes.
            QList<Block> branches; qreal width = 0, above = 0;
            const qreal siblingGap = detailDepth ? 42 : 56;
            for (const auto &child : children.value(leader)) {
                auto branch = buildComponent(child.component, child.target, depth + 1);
                width += branch.bounds.width(); above = qMax(above, -branch.bounds.top()); branches.append(branch);
            }
            if (!branches.isEmpty()) {
                width += siblingGap * (branches.size() - 1);
                qreal left = -width / 2;
                const qreal y = main.bottom() + qMax(38.0, main.height() * .6) + above;
                for (const auto &branch : branches) {
                    const qreal x = branches.size() == 1 ? 0 : left - branch.bounds.left();
                    block.append(branch, {x, y}); left += branch.bounds.width() + siblingGap;
                }
            }
            blocks[leader] = block;
        }
        // Collapse route cycles before assigning columns, so cyclic graphs
        // remain finite and all acyclic predecessor links point to the right.
        QMap<QString, int> discovery, low, scc;
        QStringList stack; QSet<QString> active; int sequence = 0, count = 0;
        std::function<void(const QString &)> visit = [&](const QString &node) {
            discovery[node] = low[node] = ++sequence; stack.append(node); active.insert(node);
            for (const auto &target : next.value(node)) {
                if (!discovery.contains(target)) { visit(target); low[node] = qMin(low[node], low[target]); }
                else if (active.contains(target)) low[node] = qMin(low[node], discovery[target]);
            }
            if (low[node] == discovery[node]) {
                QString member;
                do { member = stack.takeLast(); active.remove(member); scc[member] = count; } while (member != node);
                ++count;
            }
        };
        for (const auto &node : components.value(component)) if (!discovery.contains(node)) visit(node);
        QMap<int, QSet<int>> outgoing, incoming; QMap<int, int> indegree, ranks, suffix;
        for (const auto &node : components.value(component)) for (const auto &target : next.value(node)) if (scc[node] != scc[target]) {
            outgoing[scc[node]].insert(scc[target]); incoming[scc[target]].insert(scc[node]);
        }
        QList<int> order;
        for (int i = 0; i < count; ++i) { indegree[i] = incoming[i].size(); if (!indegree[i]) order.append(i); }
        for (int i = 0; i < order.size(); ++i) for (int target : outgoing.value(order[i])) {
            ranks[target] = qMax(ranks.value(target), ranks.value(order[i]) + 1);
            if (!--indegree[target]) order.append(target);
        }
        const auto prefix = ranks;
        QSet<int> descendants{scc.value(anchor)}; QList<int> queue{scc.value(anchor)};
        for (int i = 0; i < queue.size(); ++i) for (int target : outgoing.value(queue[i])) if (!descendants.contains(target)) { descendants.insert(target); queue.append(target); }
        for (auto it = order.crbegin(); it != order.crend(); ++it) {
            int latest = std::numeric_limits<int>::max();
            for (int target : outgoing.value(*it)) { latest = qMin(latest, ranks.value(target) - 1); suffix[*it] = qMax(suffix.value(*it), suffix.value(target) + 1); }
            if (!descendants.contains(*it) && latest != std::numeric_limits<int>::max()) ranks[*it] = latest;
        }
        // Choose a continuous route through the current card as the horizontal
        // baseline; parallel branches use separate lanes in the same columns.
        QSet<QString> primary{anchor};
        auto trace = [&](QString cursor, const QMap<QString, QStringList> &links, bool forwards) {
            QSet<QString> visited{cursor};
            while (true) {
                QString choice; int best = -1;
                for (const auto &target : links.value(cursor)) if (!visited.contains(target) && scc[target] != scc[cursor]) {
                    const int score = forwards ? suffix.value(scc[target]) : prefix.value(scc[target]);
                    if (score > best || (score == best && target < choice)) { best = score; choice = target; }
                }
                if (choice.isEmpty()) break;
                primary.insert(choice); visited.insert(choice); cursor = choice;
            }
        };
        trace(anchor, previous, false); trace(anchor, next, true);
        QMap<int, QStringList> columns;
        for (const auto &node : components.value(component)) columns[ranks.value(scc[node])].append(node);
        QMap<QString, qreal> ys; QMap<int, qreal> leftExtent, rightExtent, xs;
        for (auto column = columns.begin(); column != columns.end(); ++column) {
            std::sort(column->begin(), column->end(), [&](const QString &a, const QString &b) {
                if (primary.contains(a) != primary.contains(b)) return primary.contains(a);
                if (index.depths.value(a) != index.depths.value(b)) return index.depths.value(a) < index.depths.value(b);
                return a < b;
            });
            qreal upper = blocks.value(column->front()).bounds.top(), lower = blocks.value(column->front()).bounds.bottom();
            for (int i = 0; i < column->size(); ++i) {
                const QString node = column->at(i); const QRectF bounds = blocks.value(node).bounds;
                qreal y = 0;
                if (i % 2) { y = upper - 48 - bounds.bottom(); upper = bounds.top() + y; }
                else if (i) { y = lower + 48 - bounds.top(); lower = bounds.bottom() + y; }
                ys[node] = y; leftExtent[column.key()] = qMax(leftExtent.value(column.key()), -bounds.left());
                rightExtent[column.key()] = qMax(rightExtent.value(column.key()), bounds.right());
            }
        }
        qreal x = 0; int last = 0; bool first = true;
        for (auto column = columns.cbegin(); column != columns.cend(); ++column) {
            if (!first) x += rightExtent.value(last) + leftExtent.value(column.key()) + (detailDepth ? 76 : 120);
            xs[column.key()] = x; last = column.key(); first = false;
        }
        const qreal anchorX = xs.value(ranks.value(scc.value(anchor))), anchorY = ys.value(anchor);
        Block combined;
        for (const auto &node : components.value(component)) combined.append(blocks.value(node), {xs.value(ranks.value(scc.value(node))) - anchorX, ys.value(node) - anchorY});
        return combined;
    };
    QString rootAnchor = index.root;
    if (forestRoot != rootComponent) {
        QString cursor = rootComponent;
        while (owners.contains(cursor)) {
            const QString owner = owners.value(cursor);
            for (const auto &candidate : candidates) if (candidate.component == cursor && componentOf.value(candidate.source) == owner) { rootAnchor = candidate.source; break; }
            cursor = owner;
        }
    }
    Block all = buildComponent(forestRoot, result.routeAnchors.value(rootAnchor), 0);
    // Shared details can have several owners. Place the additional roots above
    // the existing tree rather than reusing an occupied grid position.
    for (auto component = components.cbegin(); component != components.cend(); ++component) if (component.key() != forestRoot && !owners.contains(component.key())) {
        QString anchor = component->front(), target;
        for (const auto &candidate : candidates) if (componentOf.value(candidate.source) == component.key() && all.cards.contains(candidate.target)) { anchor = candidate.source; target = candidate.target; break; }
        const Block branch = buildComponent(component.key(), anchor, 0);
        const qreal x = target.isEmpty() ? 0 : all.cards.value(target).center().x();
        all.append(branch, {x, all.bounds.top() - 70 - branch.bounds.bottom()});
    }
    result.rects = all.cards;
    const QPointF origin = result.rects.value(index.root).topLeft();
    for (auto &rect : result.rects) rect.translate(-origin);
    return result;
}
