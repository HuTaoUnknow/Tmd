#pragma once
#include "core/knowledge/NodeRelationType.h"
#include <QString>
#include <QMap>

using RelationStore = QMap<QString, QMap<QString, NodeRelationType>>;
class MarkdownRelation {
public:
    static QString label(NodeRelationType type);
    static QString key(NodeRelationType type);
    static NodeRelationType inverse(NodeRelationType type);
    static QMap<QString, QString> alternativeGroups(const RelationStore &store);
    static bool validateAlternativeRoutes(const RelationStore &store, QString *error = nullptr);
    static bool canonicalize(const RelationStore &store, RelationStore &result, QString *error = nullptr);
    static bool read(const QString &path, RelationStore &store, QString &snapshot, QString *error = nullptr);
    static bool write(const QString &path, const RelationStore &store, QString &snapshot, QString *error = nullptr);
};
