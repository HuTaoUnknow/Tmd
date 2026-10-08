#pragma once
#include <QMap>
#include <QString>
class QTreeWidget;
class QTreeWidgetItem;
class DocumentTreeBuilder {
public:
    explicit DocumentTreeBuilder(QTreeWidget *tree) : m_tree(tree) {}
    QTreeWidgetItem *directory(const QString &path);
    QTreeWidgetItem *document(const QString &path, const QString &title);
    static void updateTitle(QTreeWidget *tree, const QString &path, const QString &title);
    static void filter(QTreeWidget *tree, const QString &query, bool matchDirectories);
private:
    QTreeWidget *m_tree;
    QMap<QString, QTreeWidgetItem *> m_directories;
};
