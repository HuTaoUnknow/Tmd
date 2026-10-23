#include "ui/library/DocumentFileTree.h"
#include "ui/library/DocumentTreeBuilder.h"
#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QFileInfo>
#include <QPainter>
#include <QScrollBar>
#include <QMouseEvent>
#include <QSettings>
#include <QCryptographicHash>
#include <QDir>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QRubberBand>
#include <QTimer>
#include <QtCore/qscopeguard.h>

namespace {
const QString mimeType = "application/x-tmd-library-item";
class TextItemDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem row(option); initStyleOption(&row, index);
        const bool modified = index.data(DocumentTreeBuilder::ModifiedRole).toBool();
        const bool missing = !index.data(DocumentTreeBuilder::MissingReferencesRole).toStringList().isEmpty();
        const auto *widget = row.widget; auto *style = widget ? widget->style() : QApplication::style();
        if (modified || missing) {
            const QRect text = style->subElementRect(QStyle::SE_ItemViewItemText, &row, widget);
            row.text = row.fontMetrics.elidedText(row.text, row.textElideMode, qMax(0, text.width() - 20));
        }
        style->drawControl(QStyle::CE_ItemViewItem, &row, painter, widget);
        if (modified || missing) {
            painter->save(); painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen); painter->setBrush(QColor(missing ? "#ef6464" : "#d5d5d2"));
            painter->drawEllipse(QPointF(option.rect.right() - 9, option.rect.center().y()), 3.5, 3.5); painter->restore();
        }
    }
    QRect textRect(const QTreeWidget *view, const QModelIndex &index) const {
        QStyleOptionViewItem option; option.initFrom(view); option.widget = view;
        initStyleOption(&option, index); option.rect = view->visualRect(index);
        if (option.rect.isEmpty()) return {};
        QRect rect = view->style()->subElementRect(QStyle::SE_ItemViewItemText, &option, view);
        if (index.data(DocumentTreeBuilder::ModifiedRole).toBool() || !index.data(DocumentTreeBuilder::MissingReferencesRole).toStringList().isEmpty()) rect.setRight(rect.right() - 20);
        const QString visible = option.fontMetrics.elidedText(option.text, view->textElideMode(), rect.width());
        rect.setWidth(qMin(rect.width(), option.fontMetrics.horizontalAdvance(visible)));
        return rect;
    }
};
}
DocumentFileTree::DocumentFileTree(QWidget *parent) : QTreeWidget(parent), m_origin(QUuid::createUuid().toString()) {
    setDragDropMode(QAbstractItemView::DragDrop); setDragEnabled(true); setAcceptDrops(true);
    setDefaultDropAction(Qt::MoveAction); setDropIndicatorShown(false); setSelectionMode(QAbstractItemView::ExtendedSelection);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setItemDelegate(new TextItemDelegate(this));
    m_rubberBand = new QRubberBand(QRubberBand::Rectangle, viewport());
    m_boxScrollTimer = new QTimer(this); m_boxScrollTimer->setInterval(40);
    connect(m_boxScrollTimer, &QTimer::timeout, this, [this] {
        const int y = m_boxPosition.y(), height = viewport()->height();
        const int delta = y < 24 ? -18 : y > height - 24 ? 18 : 0;
        if (delta) { verticalScrollBar()->setValue(verticalScrollBar()->value() + delta); updateBoxSelection(); }
    });
    const auto remember = [this](QTreeWidgetItem *item, bool expanded) {
        if (m_restoring) return;
        const QString path = item->data(0, Qt::UserRole + 2).toString();
        if (path.isEmpty()) return;
        if (expanded) m_collapsed.remove(path); else m_collapsed.insert(path);
        saveDirectoryStates();
    };
    connect(this, &QTreeWidget::itemExpanded, this, [remember](QTreeWidgetItem *item) { remember(item, true); });
    connect(this, &QTreeWidget::itemCollapsed, this, [remember](QTreeWidgetItem *item) { remember(item, false); });
}
void DocumentFileTree::setLibraryRoot(const QString &root) {
    QString canonical = QDir::cleanPath(QFileInfo(root).absoluteFilePath());
#ifdef Q_OS_WIN
    canonical = canonical.toCaseFolded();
#endif
    m_settingsKey = "documentTree/" + QString::fromLatin1(QCryptographicHash::hash(canonical.toUtf8(), QCryptographicHash::Sha256).toHex()) + "/collapsed";
    const auto paths = QSettings().value(m_settingsKey).toStringList();
    m_collapsed = QSet<QString>(paths.begin(), paths.end()); restoreDirectoryStates();
}
void DocumentFileTree::saveDirectoryStates() {
    if (!m_settingsKey.isEmpty()) QSettings().setValue(m_settingsKey, QStringList(m_collapsed.begin(), m_collapsed.end()));
}
void DocumentFileTree::restoreDirectoryStates() {
    m_restoring = true;
    QTreeWidgetItemIterator items(this);
    while (*items) {
        const QString path = (*items)->data(0, Qt::UserRole + 2).toString();
        if (!path.isEmpty()) (*items)->setExpanded(!m_collapsed.contains(path));
        ++items;
    }
    m_restoring = false;
}
void DocumentFileTree::remapDirectoryState(const QString &from, const QString &to) {
    QSet<QString> remapped;
    for (const QString &path : m_collapsed) remapped.insert(path == from || path.startsWith(from + '/') ? to + path.mid(from.size()) : path);
    m_collapsed = remapped; saveDirectoryStates();
}
void DocumentFileTree::finishTransientExpansion() {
    const QSignalBlocker blocker(this);
    QTreeWidgetItemIterator items(this);
    while (*items) { if (m_autoExpanded.contains((*items)->data(0, Qt::UserRole + 2).toString())) (*items)->setExpanded(false); ++items; }
    m_autoExpanded.clear(); m_preview = {}; viewport()->update();
}
QRect DocumentFileTree::textRect(QTreeWidgetItem *item) const {
    if (!item) return {};
    return static_cast<TextItemDelegate *>(itemDelegate())->textRect(this, indexFromItem(item));
}
void DocumentFileTree::mousePressEvent(QMouseEvent *event) {
    m_dragEligible = false;
    if (event->button() == Qt::LeftButton) {
        auto *item = itemAt(event->position().toPoint());
        const bool branch = item && item->childCount() && event->position().x() < visualItemRect(item).left();
        m_dragEligible = textRect(item).contains(event->position().toPoint());
        if (!m_dragEligible && !branch) {
            setFocus(); m_boxSelecting = true; m_boxPosition = event->position().toPoint();
            m_boxOrigin = m_boxPosition + QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value());
            m_boxInitial.clear();
            if (event->modifiers().testFlag(Qt::ControlModifier))
                for (auto *selected : selectedItems()) m_boxInitial.insert(selected->data(0, Qt::UserRole).toString());
            else clearSelection();
            m_rubberBand->setGeometry(QRect(m_boxPosition, QSize())); m_rubberBand->show(); m_boxScrollTimer->start();
            event->accept(); return;
        }
    }
    QTreeWidget::mousePressEvent(event);
}
void DocumentFileTree::updateBoxSelection() {
    const QPoint offset(horizontalScrollBar()->value(), verticalScrollBar()->value());
    const QRect rect = QRect(m_boxOrigin, m_boxPosition + offset).normalized();
    m_rubberBand->setGeometry(rect.translated(-offset));
    QTreeWidgetItemIterator items(this);
    while (*items) {
        auto *item = *items; const QString path = item->data(0, Qt::UserRole).toString();
        const QRect area = textRect(item);
        item->setSelected(!path.isEmpty() && (m_boxInitial.contains(path) || (!area.isEmpty() && rect.intersects(area.translated(offset)))));
        ++items;
    }
}
void DocumentFileTree::mouseMoveEvent(QMouseEvent *event) {
    if (m_boxSelecting) { m_boxPosition = event->position().toPoint(); updateBoxSelection(); event->accept(); return; }
    QTreeWidget::mouseMoveEvent(event);
}
void DocumentFileTree::mouseReleaseEvent(QMouseEvent *event) {
    if (m_boxSelecting && event->button() == Qt::LeftButton) {
        m_boxPosition = event->position().toPoint(); updateBoxSelection(); m_boxSelecting = false;
        m_boxScrollTimer->stop(); m_rubberBand->hide(); event->accept(); return;
    }
    QTreeWidget::mouseReleaseEvent(event); m_dragEligible = false;
}
void DocumentFileTree::updateGeometries() {
    if (m_updatingGeometries) return;
    m_updatingGeometries = true;
    const auto reset = qScopeGuard([this] { m_updatingGeometries = false; });
    auto *scroll = verticalScrollBar();
    const int previousValue = scroll->value();
    QTreeWidget::updateGeometries();
    // Extend the real scroll range without adding selectable dummy items.
    QTreeWidgetItem *last = nullptr;
    for (int row = topLevelItemCount() - 1; row >= 0; --row)
        if (!topLevelItem(row)->isHidden()) { last = topLevelItem(row); break; }
    while (last && last->isExpanded()) {
        QTreeWidgetItem *child = nullptr;
        for (int row = last->childCount() - 1; row >= 0; --row)
            if (!last->child(row)->isHidden()) { child = last->child(row); break; }
        if (!child) break;
        last = child;
    }
    const int contentHeight = last ? visualItemRect(last).bottom() + 1 + scroll->value() : 0;
    const int padding = 120;
    scroll->setMaximum(qMax(0, contentHeight + padding - viewport()->height()));
    scroll->setValue(qMin(previousValue, scroll->maximum()));
    viewport()->update();
}
QMimeData *DocumentFileTree::mimeData(const QList<QTreeWidgetItem *> &items) const {
    auto *mime = new QMimeData;
    if (items.isEmpty()) return mime;
    QJsonArray entries;
    for (auto *item : items) {
        const QString document = item->data(0, Qt::UserRole).toString(), directory = item->data(0, Qt::UserRole + 2).toString();
        if (document.isEmpty() && (directory.isEmpty() || items.size() != 1)) return mime;
        entries.append(QJsonObject{{"path", document.isEmpty() ? directory : document}, {"directory", document.isEmpty()}});
    }
    mime->setData(mimeType, QJsonDocument(QJsonObject{{"origin", m_origin}, {"items", entries}}).toJson(QJsonDocument::Compact));
    return mime;
}
void DocumentFileTree::startDrag(Qt::DropActions) {
    if (!m_dragEligible || selectedItems().isEmpty()) return;
    auto *mime = mimeData(selectedItems());
    if (!mime->hasFormat(mimeType)) { delete mime; return; }
    QDrag drag(this); drag.setMimeData(mime); drag.exec(Qt::MoveAction);
    finishTransientExpansion();
    // The manager owns the actual move. Never let the item model delete source rows.
}
DocumentFileTree::Target DocumentFileTree::target(const QMimeData *mime, const QPoint &position) const {
    Target result;
    if (!mime || !mime->hasFormat(mimeType)) return result;
    const auto data = QJsonDocument::fromJson(mime->data(mimeType)).object();
    if (data.value("origin").toString() != m_origin) return result;
    const auto entries = data.value("items").toArray();
    if (entries.isEmpty()) return result;
    for (const auto &entry : entries) {
        const auto object = entry.toObject(); const QString path = object.value("path").toString();
        if (path.isEmpty() || (entries.size() != 1 && object.value("directory").toBool())) return {};
        result.sources.append(path);
    }
    result.directory = entries.size() == 1 && entries.at(0).toObject().value("directory").toBool();
    auto *item = itemAt(position);
    if (item && !item->data(0, Qt::UserRole).toString().isEmpty()) item = item->parent();
    result.directoryPath = item ? item->data(0, Qt::UserRole + 2).toString() : QString();
    if (item) result.index = indexFromItem(item);
#ifdef Q_OS_WIN
    const auto sensitivity = Qt::CaseInsensitive;
#else
    const auto sensitivity = Qt::CaseSensitive;
#endif
    result.valid = true; bool moving = false;
    for (const QString &source : result.sources) {
        const QString destination = (result.directoryPath.isEmpty() ? QString() : result.directoryPath + '/') + QFileInfo(source).fileName();
        moving = moving || destination.compare(source, sensitivity) != 0;
        if (result.directory && (result.directoryPath.compare(source, sensitivity) == 0 || result.directoryPath.startsWith(source + '/', sensitivity))) result.valid = false;
    }
    result.valid = result.valid && moving;
    return result;
}
void DocumentFileTree::dragEnterEvent(QDragEnterEvent *event) {
    if (target(event->mimeData(), event->position().toPoint()).sources.isEmpty()) { event->ignore(); return; }
    event->setDropAction(Qt::MoveAction); event->accept();
}
void DocumentFileTree::dragMoveEvent(QDragMoveEvent *event) {
    m_preview = target(event->mimeData(), event->position().toPoint()); viewport()->update();
    if (!m_preview.valid) { event->ignore(); return; }
    if (auto *folder = itemAt(event->position().toPoint()); folder && !folder->data(0, Qt::UserRole + 2).toString().isEmpty() && !folder->isExpanded()) {
        m_autoExpanded.insert(folder->data(0, Qt::UserRole + 2).toString());
        const QSignalBlocker blocker(this); folder->setExpanded(true);
    }
    event->setDropAction(Qt::MoveAction); event->accept();
}
void DocumentFileTree::dragLeaveEvent(QDragLeaveEvent *event) { finishTransientExpansion(); event->accept(); }
void DocumentFileTree::dropEvent(QDropEvent *event) {
    const auto destination = target(event->mimeData(), event->position().toPoint()); finishTransientExpansion();
    if (!destination.valid) { event->ignore(); return; }
    event->setDropAction(Qt::MoveAction); event->accept();
    if (destination.sources.size() == 1) emit moveRequested(destination.sources.front(), destination.directory, destination.directoryPath);
    else emit moveDocumentsRequested(destination.sources, destination.directoryPath);
}
void DocumentFileTree::paintEvent(QPaintEvent *event) {
    QTreeWidget::paintEvent(event);
    if (!m_preview.valid) return;
    QPainter painter(viewport()); painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#d9b77e"), 2)); painter.setBrush(QColor(217, 183, 126, 22));
    const QRect area = m_preview.index.isValid() ? visualRect(m_preview.index) : viewport()->rect();
    painter.drawRoundedRect(QRectF(area).adjusted(2, 2, -2, -2), 5, 5);
}
