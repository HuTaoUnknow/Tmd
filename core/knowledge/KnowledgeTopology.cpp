#include "core/knowledge/KnowledgeTopology.h"
#include "core/knowledge/MarkdownRelation.h"
#include <QSet>
#include <algorithm>
#include <functional>
#include <limits>
KnowledgeIndexResult KnowledgeTopology::canonicalIndex(KnowledgeIndexResult index) {
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