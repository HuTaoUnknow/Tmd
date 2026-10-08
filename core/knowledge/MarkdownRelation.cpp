#include "core/knowledge/MarkdownRelation.h"
#include "core/library/MarkdownFileIO.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <functional>
#include <algorithm>

namespace {
bool fail(QString *error, const QString &message) { if (error) *error = message; return false; }
bool relative(const QString &path) {
    return !path.isEmpty() && !QDir::isAbsolutePath(path) && !path.contains(':') && !path.contains('\\')
        && !path.split('/').contains("..") && QFileInfo(path).suffix().compare("md", Qt::CaseInsensitive) == 0;
}
}
QString MarkdownRelation::key(NodeRelationType type) {
    const QStringList keys = {"previous", "next", "parent", "child", "detail-of"};
    const int index = static_cast<int>(type);
    return index >= 0 && index < keys.size() ? keys[index] : QString{};
}
QString MarkdownRelation::label(NodeRelationType type) {
    const QStringList labels = {QStringLiteral("前节点"), QStringLiteral("后节点"), QStringLiteral("同级替代"), QStringLiteral("详细说明 / 实践"), QStringLiteral("所属知识")};
    const int index = static_cast<int>(type);
    return index >= 0 && index < labels.size() ? labels[index] : QString{};
}
NodeRelationType MarkdownRelation::inverse(NodeRelationType type) {
    switch (type) {
    case NodeRelationType::Previous: return NodeRelationType::Next;
    case NodeRelationType::Next: return NodeRelationType::Previous;
    case NodeRelationType::Parent: return NodeRelationType::Parent;
    case NodeRelationType::Child: return NodeRelationType::DetailOf;
    case NodeRelationType::DetailOf: return NodeRelationType::Child;
    }
    return type;
}
QMap<QString, QString> MarkdownRelation::alternativeGroups(const RelationStore &store) {
    QMap<QString, QString> parents;
    std::function<QString(const QString &)> root = [&](const QString &path) {
        if (!parents.contains(path)) parents[path] = path;
        if (parents.value(path) != path) parents[path] = root(parents.value(path));
        return parents.value(path);
    };
    for (auto source = store.cbegin(); source != store.cend(); ++source)
        for (auto target = source->cbegin(); target != source->cend(); ++target) {
            const QString a = root(source.key()), b = root(target.key());
            if (target.value() == NodeRelationType::Parent && a != b) parents[qMax(a, b)] = qMin(a, b);
        }
    const auto paths = parents.keys(); for (const auto &path : paths) parents[path] = root(path);
    return parents;
}
bool MarkdownRelation::validateAlternativeRoutes(const RelationStore &store, QString *error) {
    const auto groups = alternativeGroups(store); RelationStore routes;
    for (auto source = store.cbegin(); source != store.cend(); ++source)
        for (auto target = source->cbegin(); target != source->cend(); ++target) {
            if ((target.value() == NodeRelationType::Child || target.value() == NodeRelationType::DetailOf) && groups.value(source.key(), source.key()) == groups.value(target.key(), target.key()))
                return fail(error, QStringLiteral("同级替代知识不能同时作为组内知识的详细说明。"));
            if (target.value() != NodeRelationType::Previous && target.value() != NodeRelationType::Next) continue;
            QString a = groups.value(source.key(), source.key()), b = groups.value(target.key(), target.key());
            if (a == b) continue;
            auto type = target.value(); if (a > b) { std::swap(a, b); type = inverse(type); }
            if (routes.value(a).contains(b) && routes.value(a).value(b) != type)
                return fail(error, QStringLiteral("同级替代知识的前后路线冲突，请先统一它们与 %1 的前后关系。").arg(b));
            routes[a].insert(b, type);
        }
    return true;
}
bool MarkdownRelation::canonicalize(const RelationStore &store, RelationStore &result, QString *error) {
    RelationStore canonical;
    for (auto source = store.cbegin(); source != store.cend(); ++source) {
        for (auto target = source->cbegin(); target != source->cend(); ++target) {
            if (source.key() == target.key() || key(target.value()).isEmpty())
                return fail(error, QStringLiteral("关系文件含自引用或未知关系类型。"));
            const bool forward = source.key() < target.key();
            const QString first = forward ? source.key() : target.key(), second = forward ? target.key() : source.key();
            const auto type = forward ? target.value() : inverse(target.value());
            if (canonical.value(first).contains(second) && canonical.value(first).value(second) != type)
                return fail(error, QStringLiteral("%1 与 %2 的双向关系不一致，已保留原文件。请统一为前后或上下互相对应的角色。").arg(first, second));
            canonical[first].insert(second, type);
        }
    }
    result = canonical; return true;
}
bool MarkdownRelation::read(const QString &path, RelationStore &store, QString &snapshot, QString *error) {
    RelationStore parsed;
    QString text;
    if (!QFileInfo::exists(path)) { store.clear(); snapshot.clear(); return true; }
    if (!MarkdownFileIO::readFile(path, text, error)) return false;
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(error, QStringLiteral("关系文件损坏，已保留原文件：%1").arg(parseError.errorString()));
    const auto object = document.object();
    const int version = object.value("version").toInt();
    if ((version != 1 && version != 2) || !object.value("relations").isArray())
        return fail(error, QStringLiteral("关系文件版本或格式不受支持。"));
    for (const auto value : object.value("relations").toArray()) {
        const auto relation = value.toObject();
        const QString source = relation.value("source").toString(), target = relation.value("target").toString();
        const QString type = relation.value("type").toString();
        const int index = QStringList{"previous", "next", "parent", "child", "detail-of"}.indexOf(type);
        if (!relative(source) || !relative(target) || source == target || index < 0 || (version == 1 && index > 3))
            return fail(error, QStringLiteral("关系文件含无效路径、自引用或未知关系类型。"));
        if (parsed[source].contains(target)) return fail(error, QStringLiteral("同一对节点存在重复关系。"));
        parsed[source].insert(target, static_cast<NodeRelationType>(index));
    }
    // Old reciprocal parent/child records described one upper connection. Prefer
    // its explicit parent role when both records are present; leave the file intact.
    if (version == 1) {
        const auto sources = parsed.keys();
        for (const auto &source : sources) for (const auto &target : parsed.value(source).keys()) {
            const auto type = parsed.value(source).value(target);
            if (type == NodeRelationType::Parent && parsed.value(target).contains(source) && parsed.value(target).value(source) == NodeRelationType::Child)
                parsed[target][source] = NodeRelationType::Parent;
        }
    }
    RelationStore canonical;
    if (!canonicalize(parsed, canonical, error)) return false;
    if (!validateAlternativeRoutes(canonical, error)) return false;
    store = canonical; snapshot = text;
    return true;
}
bool MarkdownRelation::write(const QString &path, const RelationStore &store, QString &snapshot, QString *error) {
    RelationStore canonical;
    if (!canonicalize(store, canonical, error)) return false;
    if (!validateAlternativeRoutes(canonical, error)) return false;
    QJsonArray entries;
    for (auto source = canonical.cbegin(); source != canonical.cend(); ++source)
        for (auto target = source->cbegin(); target != source->cend(); ++target)
            entries.append(QJsonObject{{"source", source.key()}, {"target", target.key()}, {"type", key(target.value())}});
    const QString text = QString::fromUtf8(QJsonDocument(QJsonObject{{"version", 2}, {"relations", entries}}).toJson(QJsonDocument::Indented));
    if (!MarkdownFileIO::writeFile(path, text, error)) return false;
    snapshot = text;
    return true;
}
