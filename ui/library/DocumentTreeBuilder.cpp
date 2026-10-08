#include "ui/library/DocumentTreeBuilder.h"
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <functional>
QTreeWidgetItem *DocumentTreeBuilder::directory(const QString &path) {
    QString prefix; QTreeWidgetItem *parent = nullptr;
    for (const auto &part : path.split('/', Qt::SkipEmptyParts)) {
        prefix += (prefix.isEmpty() ? "" : "/") + part;
        if (!m_directories.contains(prefix)) {
            auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
            item->setText(0, part); item->setToolTip(0, prefix);
            item->setData(0, Qt::UserRole + 2, prefix); item->setExpanded(true);
            item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled); m_directories.insert(prefix, item);
        }
        parent = m_directories[prefix];
    }
    return parent;
}
QTreeWidgetItem *DocumentTreeBuilder::document(const QString &path, const QString &title) {
    const int separator = path.lastIndexOf('/');
    auto *parent = directory(separator < 0 ? QString{} : path.left(separator));
    auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
    item->setText(0, path.mid(separator + 1)); item->setToolTip(0, title + '\n' + path);
    item->setData(0, Qt::UserRole, path); item->setData(0, Qt::UserRole + 1, title);
    return item;
}
void DocumentTreeBuilder::updateTitle(QTreeWidget *tree, const QString &path, const QString &title) {
    QTreeWidgetItemIterator iterator(tree);
    while (*iterator) {
        auto *item = *iterator;
        if (item->data(0, Qt::UserRole).toString() == path) {
            item->setData(0, Qt::UserRole + 1, title); item->setToolTip(0, title + '\n' + path); return;
        }
        ++iterator;
    }
}
void DocumentTreeBuilder::filter(QTreeWidget *tree, const QString &query, bool matchDirectories) {
    std::function<bool(QTreeWidgetItem *)> visit = [&](QTreeWidgetItem *item) {
        bool visible = matchDirectories && !item->data(0, Qt::UserRole + 2).toString().isEmpty()
            && item->data(0, Qt::UserRole + 2).toString().contains(query, Qt::CaseInsensitive);
        for (int i = 0; i < item->childCount(); ++i) visible = visit(item->child(i)) || visible;
        if (!item->data(0, Qt::UserRole).toString().isEmpty())
            visible = visible || (item->text(0) + item->data(0, Qt::UserRole).toString() + item->data(0, Qt::UserRole + 1).toString()).contains(query, Qt::CaseInsensitive);
        item->setHidden(!visible); return visible;
    };
    for (int i = 0; i < tree->topLevelItemCount(); ++i) visit(tree->topLevelItem(i));
}
