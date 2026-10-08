#include "ui/library/DocumentSidebar.h"
#include "ui/library/DocumentTreeBuilder.h"
#include "ui/common/AppIcon.h"
#include "core/library/MarkdownNode.h"
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QToolButton>
#include <QDialog>
#include <QListWidget>
#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QMenu>
#include <QMouseEvent>
#include <QGraphicsOpacityEffect>
#include <algorithm>

namespace {
qreal shrinkProgress(int width, int wide, int narrow) {
    const qreal t = qBound(0.0, qreal(wide - width) / (wide - narrow), 1.0);
    return t * t * (3.0 - 2.0 * t);
}
QColor blend(const QColor &from, const QColor &to, qreal progress) {
    return QColor(qRound(from.red() + (to.red() - from.red()) * progress),
        qRound(from.green() + (to.green() - from.green()) * progress),
        qRound(from.blue() + (to.blue() - from.blue()) * progress));
}
}

DocumentSidebar::DocumentSidebar(QWidget *parent) : QWidget(parent), m_pages(new QStackedWidget(this)),
    m_quickButton(new QToolButton(this)), m_search(new QLineEdit(this)), m_tree(new QTreeWidget(this)) {
    setObjectName("documentSidebar");
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *outer = new QVBoxLayout(this); outer->setContentsMargins(0, 0, 0, 0);
    outer->setSizeConstraint(QLayout::SetNoConstraint); outer->addWidget(m_pages);
    m_pages->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_pages->layout()->setSizeConstraint(QLayout::SetNoConstraint);
    auto *full = new QWidget(m_pages); full->setObjectName("documentSidebarFull");
    // Width drives the header geometry directly, so dragging never waits for
    // an animation or snaps to a fixed rail width.
    m_brandIcon = new QLabel(full); m_brandIcon->setObjectName("brandIcon"); m_brandIcon->setPixmap(treeMdIcon().pixmap(44, 44));
    m_brandTitle = new QLabel(QStringLiteral("Tmd"), full); m_brandTitle->setObjectName("brand");
    m_brandOpacity = new QGraphicsOpacityEffect(m_brandTitle); m_brandTitle->setGraphicsEffect(m_brandOpacity);
    m_search->setParent(full); m_search->setObjectName("documentSearch");
    m_search->setPlaceholderText(QStringLiteral("搜索标题或文件路径"));
    m_tree->setParent(full);
    m_tree->setObjectName("documentTree"); m_tree->setHeaderHidden(true); m_tree->setIndentation(16);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu); m_tree->viewport()->installEventFilter(this);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &DocumentSidebar::showContextMenu);
    m_pages->addWidget(full);
    auto *rail = new QWidget(m_pages); rail->setObjectName("documentSidebarCompact");
    m_compactIcon = new QLabel(rail); m_compactIcon->setObjectName("compactBrandIcon"); m_compactIcon->setToolTip(QStringLiteral("Tmd"));
    m_quickButton->setParent(rail); m_quickButton->setObjectName("quickDocumentSearchButton");
    m_quickButton->setToolTip(QStringLiteral("快速搜索 Markdown 文档（Ctrl+P）"));
    m_quickButton->setAccessibleName(QStringLiteral("快速搜索文档"));
    QPixmap searchIcon(24, 24); searchIcon.fill(Qt::transparent);
    { QPainter painter(&searchIcon); painter.setRenderHint(QPainter::Antialiasing); painter.setPen(QPen(QColor("#e7e7e7"), 2));
      painter.drawEllipse(QRectF(4, 3, 12, 12)); painter.drawLine(QPointF(14, 13), QPointF(20, 19)); }
    m_quickButton->setIcon(QIcon(searchIcon)); m_quickButton->setIconSize({24, 24});
    m_pages->addWidget(rail);
    connect(m_quickButton, &QToolButton::clicked, this, &DocumentSidebar::openQuickSearch);
    connect(m_search, &QLineEdit::textChanged, this, &DocumentSidebar::filter);
    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        if (m_rightClick) return;
        const QString path = item->data(0, Qt::UserRole).toString();
        if (!path.isEmpty()) emit documentActivated(path);
    });
    connect(m_tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        const QString path = item->data(0, Qt::UserRole).toString();
        if (!path.isEmpty()) emit documentActivated(path);
    });
}
void DocumentSidebar::setDocuments(const QList<MarkdownNode *> &nodes, const QStringList &directoryPaths) {
    const QSignalBlocker blocker(m_tree);
    m_tree->clear();
    m_entries.clear();
    DocumentTreeBuilder builder(m_tree);
    for (const auto &path : directoryPaths) builder.directory(path);
    for (const auto *node : nodes) {
        const QString title = node->title(); m_entries.append({node->relativePath(), title});
        builder.document(node->relativePath(), title);
    }
    filter();
    if (m_quickPopup && m_quickPopup->isVisible()) refreshQuickResults();
}
void DocumentSidebar::updateTitle(const QString &path, const QString &title) {
    for (auto &entry : m_entries) if (entry.path == path) entry.title = title;
    DocumentTreeBuilder::updateTitle(m_tree, path, title);
    filter(); if (m_quickPopup && m_quickPopup->isVisible()) refreshQuickResults();
}
void DocumentSidebar::selectPath(const QString &path) {
    m_current = path;
    const QSignalBlocker blocker(m_tree);
    if (path.isEmpty()) { m_tree->setCurrentItem(nullptr); m_tree->clearSelection(); return; }
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        if ((*it)->data(0, Qt::UserRole).toString() == path) { m_tree->setCurrentItem(*it); return; }
        ++it;
    }
    m_tree->clearSelection();
}
void DocumentSidebar::selectDirectory(const QString &path) {
    m_search->clear();
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        if ((*it)->data(0, Qt::UserRole + 2).toString() == path) {
            m_tree->setCurrentItem(*it); m_tree->scrollToItem(*it); return;
        }
        ++it;
    }
}
void DocumentSidebar::showContextMenu(const QPoint &position) {
    auto *item = m_tree->itemAt(position);
    const QString document = item ? item->data(0, Qt::UserRole).toString() : QString();
    const QString directory = item ? item->data(0, Qt::UserRole + 2).toString() : QString();
    if (item) m_tree->setCurrentItem(item);
    else { m_tree->setCurrentItem(nullptr); m_tree->clearSelection(); }
    auto *menu = new QMenu(this); menu->setObjectName("documentContextMenu");
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    QString parentPath = directory;
    if (!document.isEmpty()) {
        const int separator = document.lastIndexOf('/');
        parentPath = separator < 0 ? QString() : document.left(separator);
    }
    auto *newDocument = menu->addAction(QStringLiteral("新建 MD 文档")); newDocument->setObjectName("contextCreateDocument");
    connect(newDocument, &QAction::triggered, this, [this, parentPath] { emit createDocumentRequested(parentPath); });
    if (!document.isEmpty()) {
        menu->addSeparator();
        auto *rename = menu->addAction(QStringLiteral("重命名")); rename->setObjectName("contextRenameDocument");
        auto *remove = menu->addAction(QStringLiteral("删除（移入回收站）")); remove->setObjectName("contextDeleteDocument");
        connect(rename, &QAction::triggered, this, [this, document] { emit renameDocumentRequested(document); });
        connect(remove, &QAction::triggered, this, [this, document] { emit deleteDocumentRequested(document); });
    } else {
        auto *create = menu->addAction(QStringLiteral("新建文件夹")); create->setObjectName("contextCreateDirectory");
        connect(create, &QAction::triggered, this, [this, directory] { emit createDirectoryRequested(directory); });
        if (!directory.isEmpty()) {
            menu->addSeparator();
            auto *remove = menu->addAction(QStringLiteral("删除文件夹（移入回收站）")); remove->setObjectName("contextDeleteDirectory");
            connect(remove, &QAction::triggered, this, [this, directory] { emit deleteDirectoryRequested(directory); });
        }
    }
    menu->popup(m_tree->viewport()->mapToGlobal(position));
}
void DocumentSidebar::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event); updateDisplayMode();
}
void DocumentSidebar::updateDisplayMode() {
    m_mode = width() < hiddenThreshold ? DisplayMode::Hidden : width() < compactThreshold ? DisplayMode::Compact : DisplayMode::Expanded;
    m_pages->setVisible(m_mode != DisplayMode::Hidden);
    const int w = width(); const bool iconsOnly = w <= compactWidth;
    m_pages->setCurrentIndex(iconsOnly ? 1 : 0);
    const qreal header = shrinkProgress(w, 220, compactThreshold);
    const qreal circle = shrinkProgress(w, 220, circleThreshold);
    const int margin = qRound(14 - 4 * shrinkProgress(w, compactThreshold, circleThreshold));
    const int iconX = qRound(margin + ((w - 44) / 2.0 - margin) * header);
    m_brandIcon->setGeometry(iconX, 18, 44, 44);
    m_brandTitle->setGeometry(iconX + 54, 18, qMax(0, w - margin - iconX - 54), 44);
    m_brandTitle->setVisible(w >= compactThreshold); m_brandOpacity->setOpacity(1 - header);
    const int normalHeight = qMin(44, m_search->fontMetrics().height() + 18);
    const int searchHeight = qRound(normalHeight + (44 - normalHeight) * circle);
    const int searchWidth = qRound((w - 2 * margin) * (1 - circle) + 44 * circle);
    const int searchY = 78;
    m_search->setGeometry((w - searchWidth) / 2, searchY, searchWidth, searchHeight);
    m_search->setPlaceholderText(w >= compactThreshold ? QStringLiteral("搜索标题或文件路径") : QStringLiteral("搜索"));
    const int radius = qRound(6 + 16 * circle), padding = qRound(8 - 4 * circle);
    const QString style = QStringLiteral(
        "QLineEdit#documentSearch { background: %1; color: #e7e7e7; border: 1px solid %2; border-radius: %3px; padding: %4px; }"
        "QLineEdit#documentSearch:focus { border-color: #7a9e8d; }")
        .arg(blend(QColor("#252525"), QColor("#28332c"), circle).name(), blend(QColor("#3a3a3a"), QColor("#526b5d"), circle).name())
        .arg(radius).arg(padding);
    if (style != m_searchStyle) { m_searchStyle = style; m_search->setStyleSheet(style); }
    const int treeY = searchY + searchHeight + 14;
    m_tree->setGeometry(14, treeY, qMax(0, w - 28), qMax(0, height() - treeY - 12));
    m_tree->setVisible(w >= compactThreshold);

    const int diameter = qRound(44 - 20 * shrinkProgress(w, compactWidth, hiddenThreshold));
    if (diameter != m_iconDiameter) {
        m_iconDiameter = diameter; m_compactIcon->setPixmap(treeMdIcon().pixmap(diameter, diameter));
        m_quickButton->setIconSize({qRound(diameter * 0.55), qRound(diameter * 0.55)});
        m_quickButton->setStyleSheet(QStringLiteral(
            "QToolButton#quickDocumentSearchButton { border-radius: %1px; padding: 0; }").arg(diameter / 2));
    }
    const int railX = (w - diameter) / 2;
    m_compactIcon->setGeometry(railX, 18, diameter, diameter);
    m_quickButton->setGeometry(railX, 18 + diameter + qRound(16 * diameter / 44.0), diameter, diameter);
}
void DocumentSidebar::openQuickSearch() {
    if (!m_quickPopup) {
        m_quickPopup = new QDialog(window(), Qt::Popup); m_quickPopup->setObjectName("quickDocumentPopup");
        auto *layout = new QVBoxLayout(m_quickPopup); layout->setContentsMargins(14, 14, 14, 12); layout->setSpacing(10);
        auto *title = new QLabel(QStringLiteral("快速搜索"), m_quickPopup); title->setObjectName("quickSearchTitle"); layout->addWidget(title);
        m_quickSearch = new QLineEdit(m_quickPopup); m_quickSearch->setObjectName("quickDocumentSearch");
        m_quickSearch->setPlaceholderText(QStringLiteral("搜索 Markdown 标题或路径")); m_quickSearch->setClearButtonEnabled(true); layout->addWidget(m_quickSearch);
        m_quickResults = new QListWidget(m_quickPopup); m_quickResults->setObjectName("quickDocumentResults"); layout->addWidget(m_quickResults, 1);
        m_quickCount = new QLabel(m_quickPopup); m_quickCount->setObjectName("quickDocumentCount"); m_quickCount->setWordWrap(true); layout->addWidget(m_quickCount);
        connect(m_quickSearch, &QLineEdit::textChanged, this, &DocumentSidebar::refreshQuickResults);
        connect(m_quickSearch, &QLineEdit::returnPressed, this, &DocumentSidebar::activateQuickResult);
        connect(m_quickResults, &QListWidget::itemClicked, this, [this] { activateQuickResult(); });
        connect(m_quickResults, &QListWidget::itemActivated, this, [this] { activateQuickResult(); });
        m_quickSearch->installEventFilter(this);
    }
    m_quickSearch->clear(); refreshQuickResults();
    QPoint position = m_quickButton->isVisible() ? m_quickButton->mapToGlobal(QPoint(m_quickButton->width() + 8, 0)) : window()->mapToGlobal(QPoint(16, 72));
    auto *screen = QGuiApplication::screenAt(position); if (!screen) screen = window()->screen();
    const QRect available = screen->availableGeometry();
    m_quickPopup->resize(qMin(420, available.width() - 16), qMin(380, available.height() - 16));
    position.setX(qBound(available.left() + 8, position.x(), available.right() - m_quickPopup->width() - 7));
    position.setY(qBound(available.top() + 8, position.y(), available.bottom() - m_quickPopup->height() - 7));
    m_quickPopup->move(position); m_quickPopup->show(); m_quickSearch->setFocus();
}
void DocumentSidebar::refreshQuickResults() {
    const QString selected = m_quickResults->currentItem() ? m_quickResults->currentItem()->data(Qt::UserRole).toString() : m_current;
    const QString query = m_quickSearch->text().trimmed();
    const auto terms = query.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    QList<SearchEntry> matches;
    for (const auto &entry : m_entries) {
        const QString searchable = entry.title + ' ' + entry.path;
        bool match = true; for (const auto &term : terms) if (!searchable.contains(term, Qt::CaseInsensitive)) { match = false; break; }
        if (match) matches.append(entry);
    }
    auto rank = [&](const SearchEntry &entry) {
        const QString base = QFileInfo(entry.path).completeBaseName();
        if (!query.isEmpty() && (base.compare(query, Qt::CaseInsensitive) == 0 || entry.title.compare(query, Qt::CaseInsensitive) == 0)) return 0;
        return !query.isEmpty() && (base.startsWith(query, Qt::CaseInsensitive) || entry.title.startsWith(query, Qt::CaseInsensitive)) ? 1 : 2;
    };
    std::stable_sort(matches.begin(), matches.end(), [&](const SearchEntry &a, const SearchEntry &b) {
        const int ar = rank(a), br = rank(b); return ar != br ? ar < br : QString::localeAwareCompare(a.path, b.path) < 0;
    });
    m_quickResults->clear(); int current = 0;
    for (const auto &entry : matches) {
        auto *item = new QListWidgetItem(entry.title + '\n' + entry.path, m_quickResults); item->setData(Qt::UserRole, entry.path); item->setToolTip(entry.path);
        if (entry.path == selected) current = m_quickResults->count() - 1;
    }
    if (m_quickResults->count()) m_quickResults->setCurrentRow(current);
    m_quickCount->setText(matches.isEmpty() ? QStringLiteral("没有匹配的 Markdown 文档") : QStringLiteral("%1 篇文档 · ↑↓ 选择 · 回车打开 · Esc 关闭").arg(matches.size()));
}
void DocumentSidebar::activateQuickResult() {
    if (!m_quickResults->currentItem()) return;
    const QString path = m_quickResults->currentItem()->data(Qt::UserRole).toString();
    m_quickPopup->hide(); emit documentActivated(path);
}
bool DocumentSidebar::eventFilter(QObject *object, QEvent *event) {
    if (object == m_tree->viewport() && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick))
        m_rightClick = static_cast<QMouseEvent *>(event)->button() == Qt::RightButton;
    if (object == m_quickSearch && event->type() == QEvent::KeyPress) {
        const auto key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Down || key == Qt::Key_Up) {
            if (m_quickResults->count()) m_quickResults->setCurrentRow(qBound(0, m_quickResults->currentRow() + (key == Qt::Key_Down ? 1 : -1), m_quickResults->count() - 1));
            return true;
        }
        if (key == Qt::Key_Escape) { m_quickPopup->hide(); return true; }
    }
    return QWidget::eventFilter(object, event);
}
void DocumentSidebar::filter() {
    DocumentTreeBuilder::filter(m_tree, m_search->text().trimmed(), true);
}
