#pragma once
#include "MarkdownNode.h"
#include "MarkdownRelation.h"
#include <QObject>
#include <memory>
#include <map>
class FileWatcher;

class MarkdownManager : public QObject {
    Q_OBJECT
public:
    explicit MarkdownManager(QString rootPath, QObject *parent = nullptr);
    QString rootPath() const { return m_root; }
    bool scan(QString *error = nullptr);
    void enableWatching();
    MarkdownNode *loadNode(const QString &path) const;
    QList<MarkdownNode *> allNodes() const;
    RelationStore explicitRelations() const;
    void updateContent(const QString &path, const QString &content);
    bool saveDocument(const QString &path, QString *error = nullptr, bool overwriteExternal = false);
    bool reloadDocument(const QString &path, QString *error = nullptr);
    bool createDocument(const QString &relativePath, const QString &content, QString *error = nullptr);
    bool deleteDocument(const QString &path, QString *error = nullptr);
    bool renameDocument(const QString &path, const QString &newRelativePath, QString *error = nullptr);
    bool importDocument(const QString &source, const QString &relativePath, QString *error = nullptr);
    bool addRelation(const QString &source, const QString &target, NodeRelationType type, QString *error = nullptr);
    bool setRelation(const QString &source, const QString &target, NodeRelationType type, QString *error = nullptr);
    bool removeRelation(const QString &source, const QString &target, QString *error = nullptr);
    QStringList warnings() const { return m_warnings; }
signals:
    void documentsChanged();
    void documentChanged(const QString &path);
    void documentRenamed(const QString &oldPath, const QString &newPath);
    void refreshFailed(const QString &message);
private:
    QString safePath(const QString &path, QString *error = nullptr) const;
    QString keyFor(const QString &path) const;
    QString relationPath() const;
    bool persistRelations(const RelationStore &store, QString *error);
    void applyRelations();
    RelationStore m_relations;
    QString m_relationSnapshot;
    QString m_root;
    std::map<QString, std::unique_ptr<MarkdownNode>> m_nodes;
    QStringList m_warnings;
    FileWatcher *m_watcher = nullptr;
};
