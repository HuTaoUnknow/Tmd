#include "MainWindow.h"
#include "DocumentSidebar.h"
#include "OutlineSidebar.h"
#include "MarkdownEditor.h"
#include "MarkdownDocumentView.h"
#include "KnowledgeGraphWidget.h"
#include "RelationDialog.h"
#include "core/ImageStorage.h"
#include "core/ImageLoader.h"
#include "core/MarkdownImages.h"
#include "core/MarkdownExportJob.h"
#include "core/MarkdownManager.h"
#include "core/MarkdownParser.h"
#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QMenu>
#include <QToolButton>
#include <QActionGroup>
#include <QDesktopServices>
#include <QCheckBox>
#include <QGridLayout>
#include <QProgressDialog>
#include <QDir>
#include <QFrame>
#include <QResizeEvent>
#include <QSpinBox>
#include <QScopedValueRollback>

MainWindow::MainWindow(const QString &dataPath, QWidget *parent)
    : QMainWindow(parent), m_manager(new MarkdownManager(dataPath, this)), m_editor(new MarkdownEditor(this)),
      m_document(new MarkdownDocumentView(this)), m_documents(new DocumentSidebar(this)), m_outline(new OutlineSidebar(this)),
      m_graph(new KnowledgeGraphWidget(this)), m_relationPanel(new QWidget(this)), m_title(new QLabel(this)),
      m_notice(new QLabel(this)), m_reload(new QPushButton(QStringLiteral("重新加载"), this)), m_parseTimer(new QTimer(this)) {
    setObjectName("mainWindow"); setWindowTitle("Tmd"); resize(1360, 880); setMinimumSize(420, 620);
    setFont(QFont("Segoe UI", 10));
    auto *toolbar = addToolBar(QStringLiteral("文档")); toolbar->setObjectName("mainToolbar"); toolbar->setMovable(false);
    auto *fileMenu = new QMenu(QStringLiteral("文件"), toolbar); fileMenu->setObjectName("fileMenu");
    auto *fileButton = new QToolButton(toolbar); fileButton->setText(QStringLiteral("文件")); fileButton->setObjectName("fileButton");
    fileButton->setMenu(fileMenu); fileButton->setPopupMode(QToolButton::InstantPopup); toolbar->addWidget(fileButton);
    auto action = [&](QMenu *menu, const QString &label, const QKeySequence &shortcut, const std::function<void()> &callback) {
        auto *a = menu->addAction(label); if (!shortcut.isEmpty()) a->setShortcut(shortcut);
        addAction(a); connect(a, &QAction::triggered, this, callback); return a;
    };
    action(fileMenu, QStringLiteral("新建"), QKeySequence::New, [this] { createDocument(); })->setObjectName("newAction");
    action(fileMenu, QStringLiteral("导入"), {}, [this] { importDocument(); })->setObjectName("importAction");
    action(fileMenu, QStringLiteral("保存"), QKeySequence::Save, [this] { saveCurrent(); })->setObjectName("saveAction");
    auto *copyMenu = fileMenu->addMenu(QStringLiteral("另存为 / 导出")); copyMenu->setObjectName("saveCopyMenu");
    action(copyMenu, QStringLiteral("另存为新文档"), QKeySequence::SaveAs, [this] { saveAs(); })->setObjectName("saveAsAction");
    action(copyMenu, QStringLiteral("导出 Markdown…"), {}, [this] { exportDocument(); })->setObjectName("exportAction");
    auto *imageMenu = fileMenu->addMenu(QStringLiteral("图片存放方式")); imageMenu->setObjectName("imageStorageMenu");
    auto *imageGroup = new QActionGroup(this); imageGroup->setExclusive(true);
    const QStringList modes = {QStringLiteral("相对路径（默认）"), QStringLiteral("绝对路径"), QStringLiteral("图床（图片链接）")};
    for (int i = 0; i < modes.size(); ++i) {
        auto *a = imageMenu->addAction(modes[i]); a->setCheckable(true); a->setChecked(i == 0); a->setObjectName(QString("imageMode%1").arg(i)); imageGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, i] { m_imageMode = static_cast<ImageMode>(i); m_editor->setHostedLinkInput(i == 2); m_document->setHostedLinkInput(i == 2); });
    }
    fileMenu->addSeparator();
    action(fileMenu, QStringLiteral("重命名"), {}, [this] { renameDocument(); })->setObjectName("renameAction");
    action(fileMenu, QStringLiteral("删除"), {}, [this] { deleteDocument(); })->setObjectName("deleteAction");
    action(fileMenu, QStringLiteral("刷新"), QKeySequence("F5"), [this] { refreshDocuments(); })->setObjectName("refreshAction");
    fileMenu->addSeparator();
    action(fileMenu, QStringLiteral("关于 Tmd…"), {}, [this] {
        QString version = QCoreApplication::applicationVersion();
        if (version.isEmpty()) version = QStringLiteral("开发版本");
        const QString notices = QUrl::fromLocalFile(QDir(QCoreApplication::applicationDirPath()).filePath("THIRD_PARTY_NOTICES.md")).toString();
        QMessageBox::about(this, QStringLiteral("关于 Tmd"), QStringLiteral(
            "<h3>Tmd %1</h3><p>用 Markdown 写知识，用知识树连接学习过程。</p>"
            "<p>Copyright © 2026 HuTaoUnknow · MIT</p>"
            "<p>使用 Qt 共享库（LGPL）与 Open Sans / DejaVu 字体。第三方组件保留原有授权。</p>"
            "<p><a href=\"https://github.com/HuTaoUnknow/Tmd\">项目主页与使用教程</a> · "
            "<a href=\"%2\">第三方组件说明</a></p>").arg(version.toHtmlEscaped(), notices.toHtmlEscaped()));
    })->setObjectName("aboutAction");
    auto *spring = new QWidget(toolbar); spring->setObjectName("headerSpring"); spring->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred); toolbar->addWidget(spring);
    auto *viewGroup = new QActionGroup(this); viewGroup->setExclusive(true);
    m_documentAction = new QAction(QStringLiteral("文档"), this); addAction(m_documentAction); m_documentAction->setObjectName("documentModeAction"); m_documentAction->setCheckable(true); m_documentAction->setChecked(true); m_documentAction->setShortcut(QKeySequence("Ctrl+Shift+D"));
    m_documentAction->setToolTip(QStringLiteral("直接编辑排版后的文档，可粘贴或拖入图片")); viewGroup->addAction(m_documentAction);
    m_sourceAction = new QAction(QStringLiteral("原文"), this); addAction(m_sourceAction); m_sourceAction->setObjectName("sourceModeAction"); m_sourceAction->setCheckable(true); m_sourceAction->setShortcut(QKeySequence("Ctrl+Shift+M"));
    m_sourceAction->setToolTip(QStringLiteral("编辑 Markdown 原文")); viewGroup->addAction(m_sourceAction);
    auto *modeFrame = new QFrame(toolbar); modeFrame->setObjectName("documentModeFrame"); modeFrame->setFrameShape(QFrame::StyledPanel);
    auto *modeLayout = new QHBoxLayout(modeFrame); modeLayout->setContentsMargins(3, 3, 3, 3); modeLayout->setSpacing(2);
    for (auto *a : {m_documentAction, m_sourceAction}) { auto *button = new QToolButton(modeFrame); button->setDefaultAction(a); modeLayout->addWidget(button); }
    toolbar->addWidget(modeFrame);
    connect(m_documentAction, &QAction::triggered, this, [this] { setSourceMode(false); });
    connect(m_sourceAction, &QAction::triggered, this, [this] { setSourceMode(true); });
    m_outlineAction = toolbar->addAction(QStringLiteral("目录")); m_outlineAction->setObjectName("outlineAction"); m_outlineAction->setShortcut(QKeySequence("Ctrl+Shift+O"));
    m_outlineAction->setCheckable(true); m_outlineAction->setChecked(true);
    connect(m_outlineAction, &QAction::toggled, this, [this](bool visible) {
        m_outline->setVisible(visible);
        if (!m_horizontal || m_applyingLayout) return;
        if (m_layoutMode == LayoutMode::Portrait) m_portraitOutlineVisible = visible;
        setColumnWidths(m_documents->isHidden() ? 0 : m_horizontal->sizes().value(0), visible ? m_portraitOutlineWidth : 0);
    });
    auto *quickSearch = new QAction(this); quickSearch->setObjectName("quickDocumentSearchAction");
    quickSearch->setShortcut(QKeySequence("Ctrl+P")); addAction(quickSearch);
    connect(quickSearch, &QAction::triggered, m_documents, &DocumentSidebar::openQuickSearch);
    m_relationAction = toolbar->addAction(QStringLiteral("知识树")); m_relationAction->setShortcut(QKeySequence("Ctrl+Shift+K"));
    m_relationAction->setObjectName("relationsAction"); m_relationAction->setCheckable(true);
    connect(m_relationAction, &QAction::toggled, this, [this](bool visible) {
        m_relationPanel->setVisible(visible);
        if (visible) { sizeRelationPanel(); QTimer::singleShot(0, this, &MainWindow::sizeRelationPanel); }
    });
    auto *central = new QWidget(this); auto *layout = new QVBoxLayout(central); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(0);
    auto *vertical = new QSplitter(Qt::Vertical, central);
    m_vertical = vertical;
    vertical->setObjectName("documentTreeSplitter"); vertical->setChildrenCollapsible(false);
    auto *horizontal = new QSplitter(Qt::Horizontal, vertical);
    m_horizontal = horizontal; horizontal->setObjectName("documentColumns");
    horizontal->addWidget(m_documents);
    auto *editorPanel = new QWidget(horizontal); auto *editorLayout = new QVBoxLayout(editorPanel); editorLayout->setContentsMargins(12, 14, 12, 8);
    m_title->setObjectName("documentTitle"); m_title->setText(QStringLiteral("你的知识，从一个节点开始")); editorLayout->addWidget(m_title);
    auto *noticeRow = new QHBoxLayout; m_notice->setObjectName("notice"); m_notice->setWordWrap(true);
    noticeRow->addWidget(m_notice, 1); noticeRow->addWidget(m_reload); editorLayout->addLayout(noticeRow);
    m_notice->hide(); m_reload->hide(); m_views = new QStackedWidget(editorPanel); m_views->setObjectName("documentViews");
    m_views->addWidget(m_document); m_views->addWidget(m_editor); editorLayout->addWidget(m_views, 1);
    horizontal->addWidget(editorPanel); horizontal->addWidget(m_outline);
    horizontal->setCollapsible(0, true); horizontal->setCollapsible(1, false); horizontal->setHandleWidth(6);
    horizontal->setSizes({245, 900, 215}); horizontal->setStretchFactor(1, 1);
    connect(horizontal, &QSplitter::splitterMoved, this, [this] { normalizeSidebarWidth(); });
    auto *graphLayout = new QVBoxLayout(m_relationPanel); graphLayout->setContentsMargins(12, 8, 12, 6);
    auto *graphToolbar = new QToolBar(m_relationPanel); graphToolbar->setMovable(false);
    graphToolbar->setObjectName("relationToolbar");
    graphToolbar->addWidget(new QLabel(QStringLiteral("缩放"), graphToolbar)); graphToolbar->addWidget(m_graph->createZoomControl(graphToolbar));
    auto *relationSpring = new QWidget(graphToolbar); relationSpring->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred); graphToolbar->addWidget(relationSpring);
    auto *manage = graphToolbar->addAction(QStringLiteral("完善知识树")); manage->setObjectName("manageTreeAction"); manage->setToolTip(QStringLiteral("打开知识树，拖动文档与节点建立关系"));
    connect(manage, &QAction::triggered, this, &MainWindow::openKnowledgeTree);
    graphLayout->addWidget(graphToolbar); graphLayout->addWidget(m_graph, 1);
    vertical->addWidget(m_relationPanel); vertical->setSizes({600, 300}); vertical->setStretchFactor(0, 2); vertical->setStretchFactor(1, 1);
    m_relationPanel->hide(); layout->addWidget(vertical); setCentralWidget(central); m_editor->setEnabled(false); m_document->setEnabled(false);
    connect(m_documents, &DocumentSidebar::documentActivated, this, [this](const QString &path) { openDocument(path); }, Qt::QueuedConnection);
    // The clicked graphics item remains alive until the mouse event finishes.
    connect(m_graph, &KnowledgeGraphWidget::nodeActivated, this, [this](const QString &path) { openDocument(path); }, Qt::QueuedConnection);
    connect(m_outline, &OutlineSidebar::headingActivated, this, [this](int position) { if (m_sourceAction->isChecked()) m_editor->goToPosition(position); else m_document->goToSourcePosition(position); });
    connect(m_editor, &MarkdownEditor::imageFilesInserted, this, &MainWindow::insertImageFiles);
    connect(m_editor, &MarkdownEditor::clipboardImageInserted, this, &MainWindow::insertClipboardImage);
    connect(m_document, &MarkdownDocumentView::imageInputPositionRequested, this, [this](int position) { QTextCursor cursor(m_editor->document()); cursor.setPosition(qBound(0, position, m_editor->document()->characterCount() - 1)); m_editor->setTextCursor(cursor); });
    connect(m_document, &MarkdownDocumentView::imageFilesInserted, this, &MainWindow::insertImageFiles);
    connect(m_document, &MarkdownDocumentView::clipboardImageInserted, this, &MainWindow::insertClipboardImage);
    connect(m_document, &MarkdownDocumentView::sourceEdited, this, [this](const QString &source, int position) {
        if (m_loading || m_current.isEmpty()) return;
        const QString previous = m_editor->source(); int prefix = 0, oldEnd = previous.size(), newEnd = source.size();
        while (prefix < oldEnd && prefix < newEnd && previous[prefix] == source[prefix]) ++prefix;
        while (oldEnd > prefix && newEnd > prefix && previous[oldEnd - 1] == source[newEnd - 1]) { --oldEnd; --newEnd; }
        QTextCursor cursor(m_editor->document()); cursor.beginEditBlock(); cursor.setPosition(prefix); cursor.setPosition(oldEnd, QTextCursor::KeepAnchor);
        cursor.insertText(source.mid(prefix, newEnd - prefix)); cursor.endEditBlock();
        cursor.setPosition(qBound(0, position, m_editor->document()->characterCount() - 1)); m_editor->setTextCursor(cursor);
        updateEditorStatus();
    });
    connect(m_document, &MarkdownDocumentView::undoRequested, this, [this] { m_editor->undo(); renderDocument(); });
    connect(m_document, &MarkdownDocumentView::redoRequested, this, [this] { m_editor->redo(); renderDocument(); });
    auto undoAvailability = [this] { m_document->setUndoAvailability(m_editor->document()->isUndoAvailable(), m_editor->document()->isRedoAvailable()); };
    connect(m_editor->document(), &QTextDocument::undoAvailable, this, undoAvailability);
    connect(m_editor->document(), &QTextDocument::redoAvailable, this, undoAvailability);
    connect(m_document, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
        if (url.toString().startsWith('#')) { m_document->scrollToAnchor(url.fragment()); return; }
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).suffix().compare("md", Qt::CaseInsensitive) == 0) openDocument(url.toLocalFile());
        else if (MarkdownImages::isRemote(url.toString())) QDesktopServices::openUrl(url);
    });
    for (const auto &shortcut : {QKeySequence::Undo, QKeySequence::Redo}) {
        auto *a = new QAction(this); a->setShortcut(shortcut); addAction(a);
        connect(a, &QAction::triggered, this, [this, shortcut] { if (shortcut == QKeySequence::Undo) m_editor->undo(); else m_editor->redo(); if (m_documentAction->isChecked()) renderDocument(); });
    }
    connect(m_reload, &QPushButton::clicked, this, &MainWindow::reloadCurrent);
    connect(m_manager, &MarkdownManager::documentsChanged, this, [this] { if (!m_busy) refreshView(); });
    connect(m_manager, &MarkdownManager::documentRenamed, this, [this](const QString &oldPath, const QString &newPath) { if (m_current == oldPath) m_current = newPath; });
    connect(m_manager, &MarkdownManager::refreshFailed, this, [this](const QString &error) {
        m_notice->setText(QStringLiteral("自动刷新失败，已保留当前内容：") + error); m_notice->show();
    });
    m_parseTimer->setSingleShot(true); m_parseTimer->setInterval(180);
    connect(m_parseTimer, &QTimer::timeout, this, [this] { refreshOutline(); refreshGraph(); if (m_documentAction->isChecked()) renderDocument(); });
    connect(m_editor, &QTextEdit::textChanged, this, [this] {
        if (m_loading || m_current.isEmpty()) return;
        m_manager->updateContent(m_current, m_editor->source()); m_parseTimer->start();
        const auto *node = m_manager->loadNode(m_current);
        const bool modified = node ? node->isModified() : m_editor->document()->isModified();
        setWindowTitle(m_current + (modified ? " *" : "") + " — Tmd");
    });
    connect(m_editor, &QTextEdit::cursorPositionChanged, this, &MainWindow::updateEditorStatus);
    auto *zoom = new QLabel(QStringLiteral("100%"), this); zoom->setObjectName("editorZoom");
    zoom->setToolTip(QStringLiteral("Ctrl + 滚轮缩放字体：50%～200%；Ctrl+0 恢复 100%")); statusBar()->addPermanentWidget(zoom);
    auto applyZoom = [this, zoom](int percent) {
        const int value = qBound(50, percent, 200); m_document->setZoomPercent(value); m_editor->setZoomPercent(value); zoom->setText(QString::number(value) + '%');
    };
    connect(m_document, &MarkdownDocumentView::zoomRequested, this, applyZoom);
    connect(m_editor, &MarkdownEditor::zoomRequested, this, applyZoom);
    auto *resetZoom = new QAction(this); resetZoom->setObjectName("resetZoomAction"); resetZoom->setShortcut(QKeySequence("Ctrl+0")); addAction(resetZoom);
    connect(resetZoom, &QAction::triggered, this, [applyZoom] { applyZoom(100); });
    QString error;
    if (!ImageStorage(m_manager->rootPath()).ensureRoot(&error)) QTimer::singleShot(0, this, [this, error] { showError(error); });
    if (!m_manager->scan(&error)) QTimer::singleShot(0, this, [this, error] { showError(error); });
    else if (!m_manager->allNodes().isEmpty()) {
        const QString initial = m_manager->loadNode("README.md") ? "README.md" : m_manager->allNodes().front()->relativePath();
        openDocument(initial);
    }
    statusBar()->showMessage(QStringLiteral("知识库：%1").arg(m_manager->rootPath()));
    m_manager->enableWatching();
    QTimer::singleShot(0, this, &MainWindow::applyResponsiveLayout);
}
void MainWindow::sizeRelationPanel() {
    if (!m_relationAction->isChecked()) return;
    const int available = qMax(0, m_vertical->height() - m_vertical->handleWidth());
    const int treeHeight = available / 3;
    m_vertical->setSizes({available - treeHeight, treeHeight});
}
void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event); applyResponsiveLayout();
}
void MainWindow::applyResponsiveLayout() {
    if (!m_horizontal) return;
    const auto mode = height() * 2 >= width() * 3 ? LayoutMode::NarrowPortrait : height() > width() ? LayoutMode::Portrait : LayoutMode::Landscape;
    if (m_layoutInitialized && m_layoutMode == mode) return;
    const QScopedValueRollback guard(m_applyingLayout, true);
    if (m_layoutInitialized) {
        const auto sizes = m_horizontal->sizes();
        if (m_layoutMode == LayoutMode::Landscape) m_landscapeDocumentWidth = sizes.value(0, 200);
        if (m_layoutMode == LayoutMode::Portrait) {
            m_portraitOutlineVisible = m_outlineAction->isChecked();
            if (m_portraitOutlineVisible) m_portraitOutlineWidth = qMax(100, sizes.value(2, 215));
        }
    }
    m_layoutInitialized = true; m_layoutMode = mode;
    m_documents->setVisible(mode != LayoutMode::NarrowPortrait);
    m_outlineAction->setChecked(mode == LayoutMode::Portrait && m_portraitOutlineVisible);
    m_outline->setVisible(m_outlineAction->isChecked());
    const int documents = mode == LayoutMode::NarrowPortrait ? 0 : mode == LayoutMode::Portrait ? DocumentSidebar::compactWidth : m_landscapeDocumentWidth;
    const int outline = m_outlineAction->isChecked() ? m_portraitOutlineWidth : 0;
    setColumnWidths(documents, outline);
}
void MainWindow::setColumnWidths(int documents, int outline) {
    const int handles = (m_documents->isHidden() ? 0 : 1) + (outline ? 1 : 0);
    const int available = qMax(0, m_horizontal->width() - handles * m_horizontal->handleWidth());
    m_horizontal->setSizes({documents, qMax(0, available - documents - outline), outline});
}
void MainWindow::normalizeSidebarWidth() {
    if (m_applyingLayout || m_layoutMode == LayoutMode::NarrowPortrait) return;
    const auto sizes = m_horizontal->sizes(); const int width = sizes.value(0);
    const int target = width < DocumentSidebar::hiddenThreshold ? 0 : width < DocumentSidebar::compactThreshold ? DocumentSidebar::compactWidth : width;
    if (target != width) setColumnWidths(target, sizes.value(2));
}
void MainWindow::showError(const QString &error) { QMessageBox::warning(this, QStringLiteral("Tmd"), error); }
QString MainWindow::askDocumentPath(const QString &title, const QString &initial) {
    bool ok = false;
    QString path = QInputDialog::getText(this, title, QStringLiteral("相对于 md_data 的路径（例如 Cpp/智能指针.md）："), QLineEdit::Normal, initial, &ok).trimmed();
    if (!ok || path.isEmpty()) return {};
    if (!path.endsWith(".md", Qt::CaseInsensitive)) path += ".md";
    return path;
}
bool MainWindow::openDocument(const QString &path) {
    if (path == m_current && m_manager->loadNode(path)) { m_documents->selectPath(path); return true; }
    if (!mayLeaveDocument()) { m_documents->selectPath(m_current); return false; }
    auto *node = m_manager->loadNode(path);
    if (!node) { showError(QStringLiteral("文档不存在，请刷新列表。")); return false; }
    m_current = node->relativePath(); m_loading = true; m_editor->setSource(node->content()); m_loading = false;
    m_editor->setEnabled(true); m_document->setEnabled(true); m_parseTimer->stop(); m_documents->selectPath(m_current);
    refreshOutline(); refreshGraph(); refreshView(); m_views->currentWidget()->setFocus();
    return true;
}
bool MainWindow::saveCurrent() {
    if (m_current.isEmpty() || !m_manager->loadNode(m_current)) return saveAs();
    m_manager->updateContent(m_current, m_editor->source());
    QString error;
    if (!m_manager->saveDocument(m_current, &error)) {
        const auto *node = m_manager->loadNode(m_current);
        if (!node || !node->hasExternalChange()) { showError(error); return false; }
        const auto choice = QMessageBox::warning(this, QStringLiteral("外部修改冲突"), error + QStringLiteral("\n覆盖将替换外部修改。是否覆盖？"), QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Cancel);
        if (choice != QMessageBox::Save || !m_manager->saveDocument(m_current, &error, true)) { if (choice == QMessageBox::Save) showError(error); return false; }
    }
    m_editor->document()->setModified(false); refreshView(); statusBar()->showMessage(QStringLiteral("已保存：%1").arg(m_current), 4000);
    return true;
}
bool MainWindow::saveAs() {
    const QString path = askDocumentPath(QStringLiteral("另存为新文档"), m_current.isEmpty() ? QStringLiteral("新节点.md") : m_current);
    if (path.isEmpty()) return false;
    if (QFileInfo::exists(QDir(m_manager->rootPath()).filePath(path))) { showError(QStringLiteral("该文件已存在。")); return false; }
    QString error, content = m_editor->source(); QStringList created; ImageStorage storage(m_manager->rootPath());
    if (!m_current.isEmpty() && !storage.prepareRelocation(QDir(m_manager->rootPath()).filePath(m_current), path, content, content, created, &error)) { showError(error); return false; }
    m_busy = true;
    const bool success = m_manager->createDocument(path, content, &error); m_busy = false;
    if (!success && !QFileInfo::exists(QDir(m_manager->rootPath()).filePath(path))) storage.rollback(created);
    if (!success) { showError(error); return false; }
    m_current = m_manager->loadNode(path)->relativePath();
    m_loading = true; m_editor->setSource(content); m_loading = false;
    m_editor->document()->setModified(false); m_editor->setEnabled(true); m_document->setEnabled(true); refreshView(); refreshOutline(); refreshGraph();
    return true;
}
bool MainWindow::mayLeaveDocument() {
    if (m_current.isEmpty()) return true;
    const auto *node = m_manager->loadNode(m_current);
    if (node ? !node->isModified() : !m_editor->document()->isModified()) return true;
    const auto choice = QMessageBox::question(this, QStringLiteral("保存编辑"), QStringLiteral("%1 尚未保存。是否保存后继续？").arg(m_current), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (choice == QMessageBox::Cancel) return false;
    if (choice == QMessageBox::Save) return saveCurrent();
    if (node) { QString error; if (!m_manager->reloadDocument(m_current, &error)) { showError(error); return false; } }
    return true;
}
void MainWindow::createDocument() {
    if (!mayLeaveDocument()) return;
    const QString path = askDocumentPath(QStringLiteral("新建知识节点"), QStringLiteral("新节点.md")); if (path.isEmpty()) return;
    QString error; m_busy = true;
    const bool success = m_manager->createDocument(path, "# " + QFileInfo(path).completeBaseName() + "\n\n", &error); m_busy = false;
    if (!success) { showError(error); return; }
    refreshView(); openDocument(path);
}
void MainWindow::importDocument() {
    if (!mayLeaveDocument()) return;
    const QString source = QFileDialog::getOpenFileName(this, QStringLiteral("导入 Markdown"), {}, "Markdown (*.md *.MD)"); if (source.isEmpty()) return;
    const QString path = askDocumentPath(QStringLiteral("导入到知识库"), QFileInfo(source).fileName()); if (path.isEmpty()) return;
    QString error; m_busy = true;
    const bool success = m_manager->importDocument(source, path, &error); m_busy = false;
    if (!success) { showError(error); return; }
    refreshView(); openDocument(path);
}
void MainWindow::renameDocument() {
    if (!m_manager->loadNode(m_current) || !mayLeaveDocument()) return;
    const QString path = askDocumentPath(QStringLiteral("重命名文档"), m_current); if (path.isEmpty() || path == m_current) return;
    QString error; m_busy = true;
    const bool success = m_manager->renameDocument(m_current, path, &error); m_busy = false;
    if (!success) { showError(error); return; }
    m_current = m_manager->loadNode(path)->relativePath(); refreshView(); refreshGraph();
}
void MainWindow::deleteDocument() {
    if (!m_manager->loadNode(m_current)) return;
    if (QMessageBox::question(this, QStringLiteral("删除知识节点"), QStringLiteral("删除 %1？该文件及指向它的知识关系将被移除。当前未保存的编辑也会丢弃。").arg(m_current), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
    QString error; m_busy = true;
    const bool success = m_manager->deleteDocument(m_current, &error); m_busy = false;
    if (!success) { showError(error); return; }
    m_current.clear(); m_loading = true; m_editor->setSource({}); m_loading = false; m_editor->setEnabled(false); m_document->setEnabled(false);
    refreshView(); refreshOutline(); refreshGraph();
    if (!m_manager->allNodes().isEmpty()) openDocument(m_manager->allNodes().front()->relativePath());
}
void MainWindow::refreshDocuments() { QString error; if (!m_manager->scan(&error)) showError(error); else renderDocument(true); }
void MainWindow::refreshView() {
    m_documents->setDocuments(m_manager->allNodes()); m_documents->selectPath(m_current);
    const auto *node = m_manager->loadNode(m_current);
    if (node && !node->isModified() && node->content() != m_editor->source()) {
        const int position = m_editor->textCursor().position();
        m_loading = true; m_editor->setSource(node->content()); m_loading = false; m_editor->goToPosition(position); refreshOutline();
    }
    m_title->setText(node ? node->title() : m_current.isEmpty() ? QStringLiteral("你的知识，从一个节点开始") : m_current);
    setWindowTitle(m_current.isEmpty() ? "Tmd" : m_current + ((node ? node->isModified() : m_editor->document()->isModified()) ? " *" : "") + " — Tmd");
    QString notice;
    if (!m_current.isEmpty() && !node) notice = QStringLiteral("文件已被外部删除或移动。编辑内容仍保留，可另存为新文档。");
    else if (node && node->hasExternalChange()) notice = QStringLiteral("文件已被外部修改。已保留你的编辑；可重新加载外部内容，或保存时选择覆盖。");
    if (!m_manager->warnings().isEmpty()) notice += "\n" + m_manager->warnings().join('\n');
    m_notice->setText(notice.trimmed()); m_notice->setVisible(!notice.trimmed().isEmpty());
    m_reload->setVisible(node && node->hasExternalChange());
    refreshGraph(); if (m_documentAction->isChecked()) renderDocument();
}
void MainWindow::refreshOutline() {
    const auto headings = MarkdownParser::outline(m_editor->source()); m_outline->setOutline(headings);
    if (m_manager->loadNode(m_current)) m_title->setText(headings.isEmpty() ? QFileInfo(m_current).completeBaseName() : headings.front().text);
}
void MainWindow::refreshGraph() {
    m_graph->setGraph(m_manager, m_current);
}
void MainWindow::reloadCurrent() {
    const auto *node = m_manager->loadNode(m_current); if (!node) return;
    if (node->isModified() && QMessageBox::question(this, QStringLiteral("重新加载"), QStringLiteral("放弃当前编辑，读取外部修改后的文档？"), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
    QString error; if (!m_manager->reloadDocument(m_current, &error)) { showError(error); return; }
    m_loading = true; m_editor->setSource(m_manager->loadNode(m_current)->content()); m_loading = false;
    refreshView(); refreshOutline();
}
void MainWindow::setRelationsVisible(bool visible) { m_relationAction->setChecked(visible); }
void MainWindow::openKnowledgeTree() {
    if (!m_manager->loadNode(m_current)) return;
    if (auto *existing = findChild<RelationDialog *>("knowledgeTreeDialog")) { existing->raise(); existing->activateWindow(); return; }
    auto *dialog = new RelationDialog(m_manager, m_current, this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->open();
}

void MainWindow::setSourceMode(bool sourceMode) {
    m_documentAction->setChecked(!sourceMode); m_sourceAction->setChecked(sourceMode);
    if (!sourceMode) renderDocument();
    m_views->setCurrentWidget(sourceMode ? static_cast<QWidget *>(m_editor) : static_cast<QWidget *>(m_document));
    m_views->currentWidget()->setFocus();
    updateEditorStatus();
}
void MainWindow::renderDocument(bool force) {
    m_document->setMarkdownSource(QDir(m_manager->rootPath()).filePath(m_current), m_editor->source(), force);
}
void MainWindow::updateEditorStatus() {
    const QString path = m_current.isEmpty() ? m_manager->rootPath() : m_current;
    if (m_sourceAction->isChecked()) statusBar()->showMessage(QStringLiteral("%1    ·    原文    ·    第 %2 行，第 %3 列    ·    Ctrl+S 保存").arg(path).arg(m_editor->textCursor().blockNumber() + 1).arg(m_editor->textCursor().positionInBlock() + 1));
    else statusBar()->showMessage(QStringLiteral("%1    ·    文档    ·    可直接编辑、粘贴或拖入图片    ·    Ctrl+S 保存").arg(path));
}
void MainWindow::closeEvent(QCloseEvent *event) { if (mayLeaveDocument()) event->accept(); else event->ignore(); }

void MainWindow::insertImageReference(const QString &reference, const QString &alt) {
    QTextCursor cursor = m_editor->textCursor(); cursor.beginEditBlock();
    if (m_documentAction->isChecked() && cursor.position() > 0 && m_editor->source()[cursor.position() - 1] != '\n') cursor.insertText("\n");
    cursor.insertText(MarkdownImages::markup(reference, alt) + "\n"); cursor.endEditBlock();
    m_editor->setTextCursor(cursor); refreshOutline(); if (m_documentAction->isChecked()) renderDocument();
    m_views->currentWidget()->setFocus();
}
void MainWindow::insertClipboardImage(const QImage &image) {
    if (!m_manager->loadNode(m_current)) return;
    QString reference, error;
    if (!ImageStorage(m_manager->rootPath()).storeImage(m_current, image, reference, &error)) { showError(error); return; }
    if (m_imageMode == ImageMode::Absolute) reference = MarkdownImages::encodedPath(MarkdownImages::localPath(reference, QDir(m_manager->rootPath()).filePath(m_current)));
    insertImageReference(reference, QStringLiteral("粘贴的图片"));
    if (m_imageMode == ImageMode::Hosted) statusBar()->showMessage(QStringLiteral("截图已按相对路径保存；图床方式可粘贴图片链接。"), 6000);
}
void MainWindow::insertImageFiles(const QList<QUrl> &urls) {
    if (!m_manager->loadNode(m_current)) return;
    for (const auto &url : urls) {
        if (url.isLocalFile()) {
            QString reference, error;
            if (m_imageMode == ImageMode::Absolute) {
                QByteArray bytes; QImage image; QString extension;
                if (!ImageLoader::readLocal(url.toLocalFile(), {}, bytes, &error) || !ImageLoader::decode(bytes, image, extension, &error)) { showError(error); continue; }
                reference = MarkdownImages::encodedPath(url.toLocalFile());
            } else if (!ImageStorage(m_manager->rootPath()).storeFile(m_current, url.toLocalFile(), reference, nullptr, &error)) { showError(error); continue; }
            insertImageReference(reference, QFileInfo(url.toLocalFile()).fileName());
            if (m_imageMode == ImageMode::Hosted) statusBar()->showMessage(QStringLiteral("本地图片已按相对路径保存；图床方式可粘贴图片链接。"), 6000);
        } else if (MarkdownImages::isRemote(url.toString())) {
            if (m_imageMode == ImageMode::Hosted) { insertImageReference(url.toString(QUrl::FullyEncoded), QFileInfo(url.path()).fileName()); continue; }
            auto *loader = new ImageLoader(this); const QString document = m_current;
            QTextCursor cursor = m_editor->textCursor(); const ImageMode mode = m_imageMode;
            connect(loader, &ImageLoader::loaded, this, [this, loader, document, cursor, url, mode](const QByteArray &bytes, const QImage &, const QString &) mutable {
                loader->deleteLater(); if (m_current != document) return;
                QString reference, error;
                if (!ImageStorage(m_manager->rootPath()).store(document, bytes, reference, nullptr, &error)) { showError(error); return; }
                if (mode == ImageMode::Absolute) reference = MarkdownImages::encodedPath(MarkdownImages::localPath(reference, QDir(m_manager->rootPath()).filePath(document)));
                m_editor->setTextCursor(cursor); insertImageReference(reference, QFileInfo(url.path()).fileName());
            });
            connect(loader, &ImageLoader::failed, this, [this, loader](const QString &error) { loader->deleteLater(); showError(error); });
            loader->load(url.toString(), QDir(m_manager->rootPath()).filePath(m_current));
        }
    }
}
void MainWindow::exportDocument() {
    if (m_current.isEmpty() || m_exporting) return;
    QFileDialog dialog(this, QStringLiteral("导出 Markdown")); dialog.setOption(QFileDialog::DontUseNativeDialog);
    dialog.setAcceptMode(QFileDialog::AcceptSave); dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilter("Markdown (*.md)"); dialog.setDefaultSuffix("md"); dialog.selectFile(QFileInfo(m_current).fileName());
    auto *include = new QCheckBox(QStringLiteral("携带图片：复制到导出文件同级目录，并改写导出的图片路径"), &dialog); include->setChecked(true);
    auto *grid = qobject_cast<QGridLayout *>(dialog.layout()); grid->addWidget(include, grid->rowCount(), 0, 1, grid->columnCount());
    if (dialog.exec() != QDialog::Accepted) return;
    const QString target = dialog.selectedFiles().value(0); if (target.isEmpty()) return;
    auto *job = new MarkdownExportJob(this); auto *progress = new QProgressDialog(QStringLiteral("正在准备图片…"), QStringLiteral("取消"), 0, 0, this);
    progress->setWindowModality(Qt::WindowModal); progress->setMinimumDuration(0); m_exporting = true;
    connect(progress, &QProgressDialog::canceled, job, &MarkdownExportJob::cancel);
    connect(job, &MarkdownExportJob::progress, this, [progress](int done, int total) { progress->setMaximum(qMax(1, total)); progress->setValue(done); });
    connect(job, &MarkdownExportJob::finished, this, [this, job, progress](bool success, const QString &message) {
        m_exporting = false; progress->close(); progress->deleteLater(); job->deleteLater();
        if (!success && message != QStringLiteral("导出已取消。")) showError(message);
        else statusBar()->showMessage(message, 8000);
    });
    progress->show(); job->start(m_editor->source(), QDir(m_manager->rootPath()).filePath(m_current), target, include->isChecked());
}
