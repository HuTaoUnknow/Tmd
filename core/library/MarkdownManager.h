#pragma once
#include "core/library/MarkdownNode.h"
#include "core/knowledge/MarkdownRelation.h"
#include <QObject>
#include <QSet>
#include <memory>
#include <map>
class FileWatcher;
struct LibraryImportResult {
    QStringList imported;
    QMap<QString, QString> failures;
    QString error;
    int imageReferences = 0;
    bool success() const { return error.isEmpty() && failures.isEmpty() && !imported.isEmpty(); }
};

class MarkdownManager : public QObject {
    Q_OBJECT
public:
    explicit MarkdownManager(QString rootPath, QObject *parent = nullptr);
    QString rootPath() const { return m_root; }
    bool scan(QString *error = nullptr);
    void enableWatching();
    MarkdownNode *loadNode(const QString &path) const;
    QList<MarkdownNode *> allNodes() const;
    QStringList directories() const;
    RelationStore explicitRelations() const;
    quint64 graphRevision() const { return m_graphRevision; }
    void updateContent(const QString &path, const QString &content);
    bool saveDocument(const QString &path, QString *error = nullptr, bool overwriteExternal = false);
    bool reloadDocument(const QString &path, QString *error = nullptr);
    bool createDocument(const QString &relativePath, const QString &content, QString *error = nullptr);
    bool deleteDocument(const QString &path, QString *error = nullptr);
    bool createDirectory(const QString &relativePath, QString *error = nullptr);
    bool deleteDirectory(const QString &relativePath, QString *error = nullptr);
    bool renameDocument(const QString &path, const QString &newRelativePath, QString *error = nullptr);
    bool copyDocument(const QString &source, const QString &target, const QString &content, QString *error = nullptr);
    LibraryImportResult importDirectory(const QString &sourceDirectory);
    bool importDocument(const QString &source, const QString &relativePath, QString *error = nullptr);
    bool addRelation(const QString &source, const QString &target, NodeRelationType type, QString *error = nullptr);
    bool setRelation(const QString &source, const QString &target, NodeRelationType type, QString *error = nullptr);
    bool removeRelation(const QString &source, const QString &target, QString *error = nullptr);
    QStringList warnings() const { return m_warnings; }
signals:
    void documentsChanged();
    void relationsChanged();
    void documentChanged(const QString &path);
    void documentTitleChanged(const QString &path, const QString &title);
    void documentRenamed(const QString &oldPath, const QString &newPath);
    void refreshFailed(const QString &message);
private:
    QString safePath(const QString &path, QString *error = nullptr) const;
    QString safeDirectoryPath(const QString &path, QString *error = nullptr) const;
    bool recyclePath(const QString &absolute, const QSet<QString> &documentKeys, QString *error);
    QString keyFor(const QString &path) const;
    QString relationPath() const;
    bool persistRelations(const RelationStore &store, QString *error);
    void applyRelations();
    bool m_batchImport = false;
    quint64 m_graphRevision = 0;
    RelationStore m_relations;
    QString m_relationSnapshot;
    QString m_root;
    std::map<QString, std::unique_ptr<MarkdownNode>> m_nodes;
    QStringList m_warnings;
    FileWatcher *m_watcher = nullptr;
};
