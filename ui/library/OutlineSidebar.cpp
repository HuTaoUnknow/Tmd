#include "ui/library/OutlineSidebar.h"

OutlineSidebar::OutlineSidebar(QWidget *parent) : QTreeWidget(parent) {
    setObjectName("outlineSidebar"); setHeaderLabel(QStringLiteral("目录")); setIndentation(14);
    connect(this, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) { emit headingActivated(item->data(0, Qt::UserRole).toInt()); });
}
void OutlineSidebar::setOutline(const MarkdownOutline &outline) {
    clear();
    QList<QPair<int, QTreeWidgetItem *>> ancestors;
    for (const auto &heading : outline) {
        while (!ancestors.isEmpty() && ancestors.back().first >= heading.level) ancestors.removeLast();
        auto *item = ancestors.isEmpty() ? new QTreeWidgetItem(this) : new QTreeWidgetItem(ancestors.back().second);
        item->setText(0, heading.text.isEmpty() ? QStringLiteral("（空标题）") : heading.text);
        item->setToolTip(0, QStringLiteral("第 %1 行 · H%2").arg(heading.line + 1).arg(heading.level));
        item->setData(0, Qt::UserRole, heading.position); item->setExpanded(true);
        QFont font = item->font(0); if (heading.level <= 2) font.setBold(true); item->setFont(0, font);
        ancestors.append({heading.level, item});
    }
}
