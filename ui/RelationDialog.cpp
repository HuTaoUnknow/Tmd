#include "RelationDialog.h"
#include "KnowledgeTreeCanvas.h"
#include "core/MarkdownManager.h"
#include <QDialogButtonBox>
#include <QDrag>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSplitter>
#include <QSpinBox>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
const QString instructions = QStringLiteral("拖入文档或拖动连接点建立关系。点击连线或上节点接缝两侧可选中关系，Delete 移除。双击卡片重新索引；滚轮缩放，空白处拖动平移。前后最多各 5 层。");
class AvailableDocuments : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;
protected:
    void startDrag(Qt::DropActions) override {
        if (!currentItem()) return;
        const QString path = currentItem()->data(0, Qt::UserRole).toString();
        if (path.isEmpty()) return;
        auto *mime = new QMimeData; mime->setData(KnowledgeTreeCanvas::mimeType(), path.toUtf8());
        QDrag drag(this); drag.setMimeData(mime); drag.exec(Qt::CopyAction, Qt::CopyAction);
    }
};
}
RelationDialog::RelationDialog(MarkdownManager *manager, QString source, QWidget *parent)
    : QDialog(parent), m_manager(manager), m_source(std::move(source)), m_available(new AvailableDocuments(this)),
      m_search(new QLineEdit(this)), m_canvas(new KnowledgeTreeCanvas(this)), m_current(new QLabel(this)),
      m_count(new QLabel(this)), m_status(new QLabel(this)) {
    setObjectName("knowledgeTreeDialog"); setWindowTitle(QStringLiteral("知识树")); resize(1240, 780); setMinimumSize(880, 580);
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(16, 16, 16, 12); layout->setSpacing(10);
    auto *toolbar = new QHBoxLayout; m_current->setObjectName("knowledgeTreeCurrent"); toolbar->addWidget(m_current, 1);
    toolbar->addWidget(new QLabel(QStringLiteral("缩放"), this)); toolbar->addWidget(m_canvas->createZoomControl(this));
    layout->addLayout(toolbar);
    auto *splitter = new QSplitter(Qt::Horizontal, this); splitter->setObjectName("knowledgeTreeSplitter");
    auto *left = new QWidget(splitter); auto *leftLayout = new QVBoxLayout(left); leftLayout->setContentsMargins(0, 0, 10, 0);
    auto *heading = new QLabel(QStringLiteral("未显示的文档"), left); heading->setObjectName("availableDocumentsHeading");
    heading->setToolTip(QStringLiteral("超过 5 层范围，或尚未连接到当前知识树的文档")); leftLayout->addWidget(heading);
    m_search->setObjectName("knowledgeTreeSearch"); m_search->setPlaceholderText(QStringLiteral("搜索标题或路径")); leftLayout->addWidget(m_search);
    m_available->setObjectName("availableDocumentTree"); m_available->setHeaderHidden(true); m_available->setIndentation(18);
    m_available->setDragEnabled(true); m_available->setDragDropMode(QAbstractItemView::DragOnly); m_available->setSelectionMode(QAbstractItemView::SingleSelection);
    leftLayout->addWidget(m_available, 1); leftLayout->addWidget(m_count); left->setMinimumWidth(240);
    splitter->addWidget(left); splitter->addWidget(m_canvas); splitter->setSizes({285, 900}); splitter->setStretchFactor(1, 1); layout->addWidget(splitter, 1);
    m_status->setObjectName("knowledgeTreeHint"); m_status->setWordWrap(true); m_status->setText(instructions); m_status->setMinimumHeight(44); layout->addWidget(m_status);
    auto *bottom = new QHBoxLayout;
    auto *remove = new QPushButton(QStringLiteral("移除选中连线"), this); remove->setObjectName("removeTreeRelationButton"); remove->setEnabled(false); bottom->addWidget(remove);
    auto *reindex = new QPushButton(QStringLiteral("以选中节点为中心"), this); reindex->setObjectName("reindexTreeButton"); bottom->addWidget(reindex);
    bottom->addStretch(1); auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this); buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭")); bottom->addWidget(buttons); layout->addLayout(bottom);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(remove, &QPushButton::clicked, m_canvas, &KnowledgeTreeCanvas::removeSelectedRelation);
    connect(reindex, &QPushButton::clicked, this, [this] { setCurrentNode(m_canvas->selectedNode()); });
    connect(m_canvas, &KnowledgeTreeCanvas::selectionChanged, this, [this, remove, reindex] {
        remove->setEnabled(m_canvas->hasSelectedEdge()); reindex->setEnabled(!m_canvas->selectedNode().isEmpty());
    });
    connect(m_canvas, &KnowledgeTreeCanvas::previewChanged, this, [this](const QString &message) {
        m_status->setText(message.isEmpty() ? instructions : QStringLiteral("松开鼠标将保存：") + message);
    });
    // Defer persistence until the originating mouse/drop event finishes using graphics items.
    connect(m_canvas, &KnowledgeTreeCanvas::relationRequested, this, [this](const QString &sourcePath, const QString &target, NodeRelationType type) {
        QString error;
        if (!m_manager->setRelation(sourcePath, target, type, &error)) {
            m_status->setText(QStringLiteral("关系未保存：") + error);
            QMessageBox::warning(this, QStringLiteral("无法保存知识树"), error);
        }
    }, Qt::QueuedConnection);
    connect(m_canvas, &KnowledgeTreeCanvas::relationRemoveRequested, this, [this](const QString &sourcePath, const QString &target) {
        QString error;
        if (!m_manager->removeRelation(sourcePath, target, &error)) QMessageBox::warning(this, QStringLiteral("无法移除连线"), error);
    }, Qt::QueuedConnection);
    connect(m_canvas, &KnowledgeTreeCanvas::rootRequested, this, &RelationDialog::setCurrentNode, Qt::QueuedConnection);
    connect(m_available, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) { setCurrentNode(item->data(0, Qt::UserRole).toString()); });
    connect(m_search, &QLineEdit::textChanged, this, &RelationDialog::filter);
    connect(manager, &MarkdownManager::documentRenamed, this, [this](const QString &oldPath, const QString &newPath) { if (m_source == oldPath) m_source = newPath; });
    connect(manager, &MarkdownManager::documentsChanged, this, &RelationDialog::refresh);
    connect(manager, &MarkdownManager::documentChanged, this, [this] { refresh(); });
    refresh(); QTimer::singleShot(0, m_canvas, &KnowledgeTreeCanvas::restoreView);
}
void RelationDialog::setCurrentNode(const QString &path) {
    const auto *node = m_manager->loadNode(path); if (!node) return;
    m_source = node->relativePath(); refresh(); m_canvas->restoreView();
}
void RelationDialog::refresh() {
    const auto nodes = m_manager->allNodes();
    if (!m_manager->loadNode(m_source)) m_source = nodes.isEmpty() ? QString{} : nodes.front()->relativePath();
    m_canvas->setGraph(m_manager, m_source); m_available->clear();
    const auto *current = m_manager->loadNode(m_source);
    m_current->setText(current ? QStringLiteral("当前节点 · %1").arg(current->title()) : QStringLiteral("知识库中暂无文档"));
    m_current->setToolTip(m_source);
    QMap<QString, QTreeWidgetItem *> directories; int available = 0;
    for (const auto *node : nodes) {
        if (m_canvas->index().positions.contains(node->relativePath())) continue;
        ++available; const auto parts = node->relativePath().split('/'); QString prefix; QTreeWidgetItem *parent = nullptr;
        for (int i = 0; i + 1 < parts.size(); ++i) {
            prefix += (prefix.isEmpty() ? "" : "/") + parts[i];
            if (!directories.contains(prefix)) {
                auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_available);
                item->setText(0, parts[i]); item->setExpanded(true); item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled); directories.insert(prefix, item);
            }
            parent = directories[prefix];
        }
        auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_available);
        item->setText(0, parts.back()); item->setToolTip(0, node->title() + '\n' + node->relativePath());
        item->setData(0, Qt::UserRole, node->relativePath()); item->setData(0, Qt::UserRole + 1, node->title());
        item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
    }
    m_count->setText(QStringLiteral("5 层范围内 %1 篇 · 未显示 %2 篇").arg(m_canvas->index().paths.size()).arg(available)); filter();
}
void RelationDialog::filter() {
    const QString search = m_search->text().trimmed();
    std::function<bool(QTreeWidgetItem *)> visit = [&](QTreeWidgetItem *item) {
        bool visible = false;
        for (int i = 0; i < item->childCount(); ++i) visible = visit(item->child(i)) || visible;
        if (!item->data(0, Qt::UserRole).toString().isEmpty())
            visible = (item->text(0) + item->data(0, Qt::UserRole).toString() + item->data(0, Qt::UserRole + 1).toString()).contains(search, Qt::CaseInsensitive);
        item->setHidden(!visible); return visible;
    };
    for (int i = 0; i < m_available->topLevelItemCount(); ++i) visit(m_available->topLevelItem(i));
}
