#include "DocumentSidebar.h"
#include "AppIcon.h"
#include "core/MarkdownNode.h"
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
#include <algorithm>

DocumentSidebar::DocumentSidebar(QWidget *parent) : QWidget(parent), m_pages(new QStackedWidget(this)),
    m_quickButton(new QToolButton(this)), m_search(new QLineEdit(this)), m_tree(new QTreeWidget(this)) {
    setObjectName("documentSidebar");
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *outer = new QVBoxLayout(this); outer->setContentsMargins(0, 0, 0, 0);
    outer->setSizeConstraint(QLayout::SetNoConstraint); outer->addWidget(m_pages);
    m_pages->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_pages->layout()->setSizeConstraint(QLayout::SetNoConstraint);
    auto *full = new QWidget(m_pages); full->setObjectName("documentSidebarFull");
    auto *layout = new QVBoxLayout(full); layout->setContentsMargins(14, 18, 14, 12);
    layout->setSizeConstraint(QLayout::SetNoConstraint);
    auto *brandRow = new QHBoxLayout;
    auto *icon = new QLabel(this); icon->setObjectName("brandIcon"); icon->setPixmap(treeMdIcon().pixmap(44, 44)); icon->setFixedSize(44, 44);
    auto *title = new QLabel(QStringLiteral("Tmd"), this); title->setObjectName("brand"); brandRow->addWidget(icon); brandRow->addWidget(title, 1); layout->addLayout(brandRow);
    layout->addWidget(new QLabel(QStringLiteral("Markdown 文档"), this));
    m_search->setPlaceholderText(QStringLiteral("搜索标题或文件路径")); layout->addWidget(m_search);
    m_tree->setObjectName("documentTree"); m_tree->setHeaderHidden(true); m_tree->setIndentation(16);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu); m_tree->viewport()->installEventFilter(this);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &DocumentSidebar::showContextMenu);
    layout->addWidget(m_tree);
    m_pages->addWidget(full);
    auto *rail = new QWidget(m_pages); rail->setObjectName("documentSidebarCompact");
    auto *railLayout = new QVBoxLayout(rail); railLayout->setContentsMargins(8, 20, 8, 12); railLayout->setSpacing(16);
    railLayout->setSizeConstraint(QLayout::SetNoConstraint);
    auto *railIcon = new QLabel(rail); railIcon->setObjectName("compactBrandIcon");
    railIcon->setPixmap(treeMdIcon().pixmap(44, 44)); railIcon->setFixedSize(44, 44); railIcon->setToolTip(QStringLiteral("Tmd"));
    railLayout->addWidget(railIcon, 0, Qt::AlignHCenter);
    m_quickButton->setObjectName("quickDocumentSearchButton"); m_quickButton->setFixedSize(40, 40);
    m_quickButton->setToolTip(QStringLiteral("快速搜索 Markdown 文档（Ctrl+P）"));
    m_quickButton->setAccessibleName(QStringLiteral("快速搜索文档"));
    QPixmap searchIcon(24, 24); searchIcon.fill(Qt::transparent);
    { QPainter painter(&searchIcon); painter.setRenderHint(QPainter::Antialiasing); painter.setPen(QPen(QColor("#e7e7e7"), 2));
      painter.drawEllipse(QRectF(4, 3, 12, 12)); painter.drawLine(QPointF(14, 13), QPointF(20, 19)); }
    m_quickButton->setIcon(QIcon(searchIcon)); m_quickButton->setIconSize({24, 24});
    railLayout->addWidget(m_quickButton, 0, Qt::AlignHCenter); railLayout->addStretch(1); m_pages->addWidget(rail);
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
    QMap<QString, QTreeWidgetItem *> directories;
    auto ensureDirectory = [&](const QString &path) {
        QString prefix; QTreeWidgetItem *parent = nullptr;
        for (const auto &part : path.split('/', Qt::SkipEmptyParts)) {
            prefix += (prefix.isEmpty() ? "" : "/") + part;
            if (!directories.contains(prefix)) {
                auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
                item->setText(0, part); item->setToolTip(0, prefix);
                item->setData(0, Qt::UserRole + 2, prefix); item->setExpanded(true); directories.insert(prefix, item);
            }
            parent = directories[prefix];
        }
        return parent;
    };
    for (const auto &path : directoryPaths) ensureDirectory(path);
    for (const auto *node : nodes) {
        m_entries.append({node->relativePath(), node->title()});
        const QStringList parts = node->relativePath().split('/');
        QTreeWidgetItem *parent = ensureDirectory(parts.mid(0, parts.size() - 1).join('/'));
        auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
        item->setText(0, parts.back()); item->setToolTip(0, node->title() + '\n' + node->relativePath());
        item->setData(0, Qt::UserRole, node->relativePath());
        item->setData(0, Qt::UserRole + 1, node->title());
    }
    filter();
    if (m_quickPopup && m_quickPopup->isVisible()) refreshQuickResults();
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
    if (!document.isEmpty()) {
        auto *rename = menu->addAction(QStringLiteral("重命名…")); rename->setObjectName("contextRenameDocument");
        auto *remove = menu->addAction(QStringLiteral("删除（移入回收站）")); remove->setObjectName("contextDeleteDocument");
        connect(rename, &QAction::triggered, this, [this, document] { emit renameDocumentRequested(document); });
        connect(remove, &QAction::triggered, this, [this, document] { emit deleteDocumentRequested(document); });
    } else {
        auto *create = menu->addAction(QStringLiteral("新建文件夹…")); create->setObjectName("contextCreateDirectory");
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
    m_pages->setCurrentIndex(m_mode == DisplayMode::Compact ? 1 : 0);
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
    const QString search = m_search->text().trimmed();
    std::function<bool(QTreeWidgetItem *)> visit = [&](QTreeWidgetItem *item) {
        bool visible = !item->data(0, Qt::UserRole + 2).toString().isEmpty()
            && item->data(0, Qt::UserRole + 2).toString().contains(search, Qt::CaseInsensitive);
        if (item->childCount()) { for (int i = 0; i < item->childCount(); ++i) visible = visit(item->child(i)) || visible; }
        else visible = visible || (item->text(0) + item->data(0, Qt::UserRole).toString() + item->data(0, Qt::UserRole + 1).toString()).contains(search, Qt::CaseInsensitive);
        item->setHidden(!visible); return visible;
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) visit(m_tree->topLevelItem(i));
}
