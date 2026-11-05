#pragma once
#include <QObject>
#include <QStringList>
#include <QMap>
#include <map>
#include <memory>
class MarkdownNode;
class FileWatcher;
class QTimer;

// Explicitly opened files remain outside the managed library and knowledge graph.
class ExternalDocuments : public QObject {
    Q_OBJECT
public:
    explicit ExternalDocuments(const QString &libraryRoot, QObject *parent = nullptr);
    ~ExternalDocuments() override;
    MarkdownNode *find(const QString &path) const;
    QList<MarkdownNode *> nodes() const;
    MarkdownNode *open(const QString &path, QString *error = nullptr);
    bool forget(const QString &path, QString *error = nullptr, bool discard = false);
    void updateContent(const QString &path, const QString &content);
    bool save(const QString &path, QString *error, bool overwrite);
    bool reload(const QString &path, QString *error);
    void enableWatching();
    void refresh();
    QString loadError() const { return m_loadError; }
signals:
    void documentsChanged();
    void documentChanged(const QString &path);
    void titleChanged(const QString &path, const QString &title);
private:
    static QString key(const QString &path);
    bool persist(QString *error, const QString &added = {}, const QString &removed = {});
    void updateStatus(MarkdownNode *node);
    void rebuildWatches();
    QString m_root, m_record, m_loadError;
    std::map<QString, std::unique_ptr<MarkdownNode>> m_nodes;
    QMap<QString, FileWatcher *> m_watchers;
    QTimer *m_debounce, *m_poll;
    bool m_watching = false;
};
