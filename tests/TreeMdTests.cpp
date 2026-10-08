#include "core/library/MarkdownFileIO.h"
#include "core/library/MarkdownManager.h"
#include "core/markdown/MarkdownParser.h"
#include "core/markdown/MarkdownImages.h"
#include "core/images/ImageStorage.h"
#include "core/images/ImageLoader.h"
#include "core/images/MarkdownExportJob.h"
#include "ui/MainWindow.h"
#include "ui/document/MarkdownEditor.h"
#include "ui/document/MarkdownDocumentView.h"
#include "ui/knowledge/KnowledgeGraphWidget.h"
#include "ui/knowledge/KnowledgeNodeWidget.h"
#include "ui/library/OutlineSidebar.h"
#include "ui/library/DocumentSidebar.h"
#include "ui/common/Theme.h"
#include "ui/common/AppIcon.h"
#include "core/markdown/MarkdownSourceMap.h"
#include "ui/document/MarkdownTypography.h"
#include "core/knowledge/KnowledgeIndex.h"
#include "ui/knowledge/KnowledgeTreeCanvas.h"
#include "ui/knowledge/KnowledgeTreeLayout.h"
#include "ui/knowledge/RelationDialog.h"
#include <QAction>
#include <QFile>
#include <QGraphicsScene>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextLayout>
#include <algorithm>
#include <QMessageBox>
#include <QTimer>
#include <thread>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <QtTest>
#include <QBuffer>
#include <QClipboard>
#include <QMimeData>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDirIterator>
#include <QComboBox>
#include <QLineEdit>
#include <QDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QCheckBox>
#include <QMenu>
#include <QToolBar>
#include <QToolButton>
#include <QStackedWidget>
#include <QTextTable>
#include <QTextList>
#include <QTextFragment>
#include <QPointer>
#include <QFrame>
#include <QScrollBar>
#include <QWheelEvent>
#include <QLabel>
#include <QFontInfo>
#include <QFontDatabase>
#include <QAbstractTextDocumentLayout>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QPushButton>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QPainterPathStroker>
#include <QTreeWidgetItemIterator>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QListWidget>
#include <QInputDialog>
#include <QCompleter>
#include <QAbstractItemView>
#include "core/settings/MarkdownSettings.h"
#include "ui/settings/MarkdownSettingsDialog.h"
#include "ui/settings/ImageSettingsDialog.h"
#include <QFontComboBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QCryptographicHash>
#include <QTabWidget>
#include <QDataStream>

#ifdef Q_OS_WIN
static QString recycledFixture(const QString &original, QString *metadataPath = nullptr) {
    const QString absolute = QFileInfo(original).absoluteFilePath();
    const QString native = QDir::toNativeSeparators(absolute);
    const QByteArray needle(reinterpret_cast<const char *>(native.utf16()), native.size() * 2);
    const QDir recycle(absolute.left(3) + "$Recycle.Bin");
    for (const auto &user : recycle.entryList(QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot)) {
        const QDir records(recycle.filePath(user));
        for (const auto &record : records.entryList({"$I*"}, QDir::Files | QDir::Hidden | QDir::System)) {
            QFile info(records.filePath(record));
            if (info.open(QIODevice::ReadOnly) && info.readAll().contains(needle)) {
                if (metadataPath) *metadataPath = info.fileName();
                return records.filePath("$R" + record.mid(2));
            }
        }
    }
    return {};
}
#endif

class TreeMdTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_settingsDirectory;
private slots:
    void initTestCase();
    void init();
    void utf8ChineseAndAtomicIO();
    void transientWindowsLockKeepsAtomicSave();
    void outlineFencesAndSetext();
    void emptyAndLargeDocuments();
    void documentLifecycleAndPortableRelations();
    void uniqueRolesArePerPair();
    void externalChangeProtectsEdits();
    void externalDeletePrunesReferences();
    void externalRenameKeepsEditsAndRelations();
    void metadataErrorsPreserveState();
    void externalMetadataConflictRollsBack();
    void invalidPathsAndWindowsAliases();
    void watcherRecognizesNestedChanges();
    void editorPreservesMarkdownAndUndo();
    void graphShowsAllBranchesAndCardClick();
    void applicationEditingNavigationAndSource();
    void imageSyntaxPreservesCodeAndTitles();
    void imageImportMirrorsDirectoriesAndDeduplicates();
    void imageImportFailureRollsBack();
    void imageRelocationAndMetadataRollback();
    void imageExportLocalEmbeddedAndRemote();
    void imageExportFailureAndCancellation();
    void applicationImageDropPasteAndPathModes();
    void applicationExportOptions();
    void documentRenderInlineImagesAndSourceIntegrity();
    void documentRenderIgnoresStaleNetworkResults();
    void fileMenuLayoutAndModeSwitch();
    void renderedEditingPreservesSource();
    void renderedListsTablesAndUndo();
    void renderedParagraphsCodeAndDeletion();
    void headingsRequireSpaceWhenRendered();
    void headingBackspaceRestoresMarker();
    void emptyHeadingsKeepEditablePositions();
    void headingEditingUndoAndParagraphIntegrity();
    void markdownStylesAndOutlinePosition();
    void imagesHaveNoExcessSpacing();
    void fontZoomBoundsAndSourceIntegrity();
    void knowledgeIndexTraversesCyclesAndIncomingReferences();
    void treeRelationReplacementIsAtomic();
    void knowledgeTreeDropPreviewAndPersistence();
    void knowledgeTreeCardConnectionsAndRemoval();
    void knowledgeTreeDirectorySearchAndReindex();
    void reciprocalRolesPersistAndRemoveFromEitherSide();
    void legacyReciprocalMetadataAndConflictProtection();
    void reciprocalRolesUpdateBothInterfaces();
    void knowledgeIndexExpandsFiveLayersInBothDirections();
    void mainAndManagementTreesShareRangeAndLayout();
    void mainTreeNavigationDoesNotEditRelations();
    void nestedCardsFollowHierarchyAndSharedNodes();
    void nestedDropPreviewAndSmallCardPorts();
    void alternativeRoutesStayInSync();
    void legacyAlternativesAndDetailsStaySeparate();
    void learningRouteCardsAndConnectors();
    void referenceLayoutHasAlignedRouteAndParallelDetails();
    void detailBranchesReserveSpaceBesideRoutes();
    void skippedAndSharedConnectionsAvoidCards();
    void attachedDetailStaysBelowItsAlternativeAfterNewLinks();
    void responsivePanelsAndOneThirdTree();
    void sharedTreeZoomPersistsAcrossViews();
    void sidebarDragCyclesThroughAllModes();
    void sidebarHeaderMorphsWithoutGeometryJumps();
    void compactSearchOpensDocumentsAndRefreshes();
    void recyclePreservesOriginalNameAndContent();
    void directoryLifecycleAndRecycleRelations();
    void recycleFailurePreservesFilesAndRelations();
    void sidebarContextTargetsAndEmptyDirectories();
    void contextNewMarkdownNamesAndPreservesEdits();
    void popupMenusShareRoundedBlackTheme();
    void applicationContextActionsPreserveOtherEdits();
    void directoryDeleteProtectsUnsavedCurrentDocument();
    void fenceLanguageCompletionInSource();
    void fenceLanguageCompletionInDocument();
    void fencePopupKeyboardSelection_data();
    void fencePopupKeyboardSelection();
    void fenceCompletionRespectsCodeBoundaries();
    void fenceCompletionPreservesExistingBlocks();
    void emptyFencedBlocksKeepTypingInside();
    void inlineCodeCursorCanLeaveAndReenter();
    void inlineCodeMouseCanTypeAfter();
    void treeLayoutRemainsStableWhenCenterChanges();
    void settingsHexRoundTripAndValidation();
    void settingsMenuAndAppearanceApply();
    void settingsImportExportDialog();
    void imageConfigurationAndPaths();
};

void TreeMdTests::recyclePreservesOriginalNameAndContent() {
    QTemporaryDir dir; const QString original = dir.filePath(QStringLiteral("原来的名称.md"));
    const QString content = QStringLiteral("# 可还原\n中文内容\n"); QString error, trash;
    QVERIFY(MarkdownFileIO::writeFile(original, content));
    QVERIFY2(MarkdownFileIO::moveToTrash(original, &error, &trash), qPrintable(error));
    QVERIFY(!QFileInfo::exists(original));
#ifdef Q_OS_WIN
    QString record; const QString recycled = recycledFixture(original, &record);
    QVERIFY2(!recycled.isEmpty(), "The original Windows recycle record was not found.");
    QString recovered; QVERIFY(MarkdownFileIO::readFile(recycled, recovered)); QCOMPARE(recovered, content);
    QVERIFY(QDir().rename(recycled, original)); QVERIFY(QFile::remove(record));
#else
    if (!trash.isEmpty()) QVERIFY(QDir().rename(trash, original));
#endif
}
void TreeMdTests::directoryLifecycleAndRecycleRelations() {
    QTemporaryDir dir; const QString root = dir.filePath("md_data"); MarkdownManager manager(root); QString error;
    QVERIFY(manager.createDirectory("empty", &error)); QVERIFY(manager.directories().contains("empty"));
    QVERIFY(!manager.createDirectory("empty", &error)); QVERIFY(!manager.createDirectory("../escape", &error));
    QVERIFY(!manager.deleteDirectory(".", &error)); QVERIFY(!manager.deleteDirectory(root, &error));
    QVERIFY(!manager.deleteDirectory("empty/..", &error));
#ifdef Q_OS_WIN
    QVERIFY(!manager.createDirectory("CON", &error)); QVERIFY(!manager.createDirectory("folder.", &error));
#endif
    QVERIFY(manager.createDocument("branch/one.md", "# One")); QVERIFY(manager.createDocument("branch/deep/two.md", "# Two"));
    QVERIFY(manager.createDocument("branch-other/keep.md", "# Keep")); QVERIFY(manager.createDocument("outside.md", "# Outside"));
    QVERIFY(manager.addRelation("outside.md", "branch/one.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("branch/one.md", "branch/deep/two.md", NodeRelationType::Child));
    QVERIFY(MarkdownFileIO::writeFile(QDir(root).filePath("branch/other.txt"), "Other file"));
    const QString photo = dir.filePath("md_photo/branch/shared.png"); QVERIFY(QDir().mkpath(QFileInfo(photo).absolutePath()));
    QVERIFY(MarkdownFileIO::writeFile(photo, "Image kept for restoring documents"));
    const QString metadata = QDir(root).filePath(".tree-md-relations.json"); QString saved;
    QVERIFY(MarkdownFileIO::readFile(metadata, saved)); QVERIFY(MarkdownFileIO::writeFile(metadata, saved + "\n"));
    QVERIFY(!manager.deleteDirectory("branch", &error)); QVERIFY(QFileInfo::exists(QDir(root).filePath("branch/one.md")));
    QVERIFY(manager.scan());
    const QString original = QDir(root).filePath("branch");
    QVERIFY2(manager.deleteDirectory("branch", &error), qPrintable(error));
    QVERIFY(!QFileInfo::exists(original)); QVERIFY(!manager.loadNode("branch/one.md")); QVERIFY(!manager.loadNode("branch/deep/two.md"));
    QVERIFY(manager.loadNode("branch-other/keep.md")); QVERIFY(manager.loadNode("outside.md"));
    QVERIFY(manager.loadNode("outside.md")->relatedPaths(NodeRelationType::Next).isEmpty()); QVERIFY(QFileInfo::exists(photo));
    MarkdownManager reopened(root); QVERIFY(reopened.scan()); QCOMPARE(reopened.allNodes().size(), 2);
    QVERIFY(reopened.directories().contains("empty")); QVERIFY(!reopened.directories().contains("branch"));
#ifdef Q_OS_WIN
    QString record; const QString recycled = recycledFixture(original, &record); QVERIFY(!recycled.isEmpty());
    QVERIFY(QFileInfo::exists(QDir(recycled).filePath("one.md"))); QVERIFY(QFileInfo::exists(QDir(recycled).filePath("deep/two.md")));
    QVERIFY(QFileInfo::exists(QDir(recycled).filePath("other.txt")));
    QVERIFY(QDir().rename(recycled, original)); QVERIFY(QFile::remove(record));
#endif
}
void TreeMdTests::recycleFailurePreservesFilesAndRelations() {
#ifdef Q_OS_WIN
    QTemporaryDir dir; MarkdownManager manager(dir.path()); QString error;
    QVERIFY(manager.createDocument("keep.md", "# Keep")); QVERIFY(manager.createDocument("locked.md", "# Locked"));
    QVERIFY(manager.addRelation("keep.md", "locked.md", NodeRelationType::Next));
    QString before; QVERIFY(MarkdownFileIO::readFile(dir.filePath(".tree-md-relations.json"), before));
    const QString locked = QDir::toNativeSeparators(dir.filePath("locked.md"));
    const HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(locked.utf16()), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    QVERIFY(handle != INVALID_HANDLE_VALUE);
    const bool deleted = manager.deleteDocument("locked.md", &error); CloseHandle(handle);
    QVERIFY(!deleted); QVERIFY(QFileInfo::exists(locked)); QVERIFY(error.contains(QStringLiteral("回收站")));
    QCOMPARE(manager.loadNode("keep.md")->relatedPaths(NodeRelationType::Next), QStringList{"locked.md"});
    QString after; QVERIFY(MarkdownFileIO::readFile(dir.filePath(".tree-md-relations.json"), after)); QCOMPARE(after, before);
    MarkdownManager reopened(dir.path()); QVERIFY(reopened.scan());
    QCOMPARE(reopened.loadNode("keep.md")->relatedPaths(NodeRelationType::Next), QStringList{"locked.md"});
#else
    QSKIP("Windows file sharing locks are required.");
#endif
}
void TreeMdTests::sidebarContextTargetsAndEmptyDirectories() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    QVERIFY(manager.createDocument("A.md", "# A")); QVERIFY(manager.createDocument("folder/B.md", "# B"));
    QVERIFY(manager.createDirectory("empty"));
    DocumentSidebar sidebar; sidebar.resize(280, 600); sidebar.setDocuments(manager.allNodes(), manager.directories()); sidebar.show();
    auto *tree = sidebar.findChild<QTreeWidget *>("documentTree"); QVERIFY(tree);
    auto find = [&](int role, const QString &path) -> QTreeWidgetItem * {
        QTreeWidgetItemIterator it(tree); while (*it) { if ((*it)->data(0, role).toString() == path) return *it; ++it; } return nullptr;
    };
    auto *empty = find(Qt::UserRole + 2, "empty"); auto *other = find(Qt::UserRole, "folder/B.md"); QVERIFY(empty); QVERIFY(other); QVERIFY(!empty->isHidden());
    sidebar.selectPath("A.md"); QSignalSpy activated(&sidebar, &DocumentSidebar::documentActivated);
    QTest::mouseClick(tree->viewport(), Qt::RightButton, Qt::NoModifier, tree->visualItemRect(other).center()); QCOMPARE(activated.size(), 0);
    QSignalSpy renamed(&sidebar, &DocumentSidebar::renameDocumentRequested);
    QSignalSpy removed(&sidebar, &DocumentSidebar::deleteDocumentRequested);
    QSignalSpy created(&sidebar, &DocumentSidebar::createDirectoryRequested);
    QSignalSpy newDocument(&sidebar, &DocumentSidebar::createDocumentRequested);
    QSignalSpy removedFolder(&sidebar, &DocumentSidebar::deleteDirectoryRequested);
    auto openMenu = [&](QTreeWidgetItem *item) {
        const QPoint position = item ? tree->visualItemRect(item).center() : QPoint(tree->viewport()->width() / 2, tree->viewport()->height() - 5);
        QMetaObject::invokeMethod(tree, "customContextMenuRequested", Qt::DirectConnection, Q_ARG(QPoint, position));
        return sidebar.findChild<QMenu *>("documentContextMenu");
    };
    auto *menu = openMenu(other); QVERIFY(menu); QVERIFY(!menu->findChild<QAction *>("contextCreateDirectory"));
    menu->findChild<QAction *>("contextCreateDocument")->trigger(); QCOMPARE(newDocument.back().front().toString(), QString("folder"));
    for (auto *action : menu->actions()) { QVERIFY(!action->text().contains("...")); QVERIFY(!action->text().contains(QChar(0x2026))); }
    menu->findChild<QAction *>("contextRenameDocument")->trigger(); QCOMPARE(renamed.front().front().toString(), QString("folder/B.md"));
    menu->findChild<QAction *>("contextDeleteDocument")->trigger(); QCOMPARE(removed.front().front().toString(), QString("folder/B.md"));
    menu->hide(); QTest::qWait(1);
    menu = openMenu(empty); QVERIFY(menu); QVERIFY(!menu->findChild<QAction *>("contextRenameDocument"));
    menu->findChild<QAction *>("contextCreateDocument")->trigger(); QCOMPARE(newDocument.back().front().toString(), QString("empty"));
    menu->findChild<QAction *>("contextCreateDirectory")->trigger(); QCOMPARE(created.front().front().toString(), QString("empty"));
    menu->findChild<QAction *>("contextDeleteDirectory")->trigger(); QCOMPARE(removedFolder.front().front().toString(), QString("empty"));
    menu->hide(); QTest::qWait(1);
    menu = openMenu(nullptr); QVERIFY(menu); QVERIFY(!menu->findChild<QAction *>("contextDeleteDirectory"));
    menu->findChild<QAction *>("contextCreateDocument")->trigger(); QCOMPARE(newDocument.back().front().toString(), QString());
    menu->findChild<QAction *>("contextCreateDirectory")->trigger(); QCOMPARE(created.back().front().toString(), QString()); menu->hide();
    QTest::qWait(1); menu = openMenu(find(Qt::UserRole, "A.md")); QVERIFY(menu);
    menu->findChild<QAction *>("contextCreateDocument")->trigger(); QCOMPARE(newDocument.back().front().toString(), QString()); menu->hide();
}
void TreeMdTests::contextNewMarkdownNamesAndPreservesEdits() {
    QTemporaryDir dir; MarkdownManager initial(dir.path());
    QVERIFY(initial.createDocument("A.md", "# Keep")); QVERIFY(initial.createDocument("folder/B.md", "# Existing"));
    MainWindow window(dir.path()); window.show();
    auto *sidebar = window.findChild<DocumentSidebar *>(); auto *editor = window.findChild<MarkdownEditor *>(); auto *manager = window.findChild<MarkdownManager *>();
    QVERIFY(sidebar); QVERIFY(editor); QVERIFY(manager); QVERIFY(window.openDocument("A.md"));
    editor->setSource("# Keep\nunsaved edits");
    QString name; bool rejectName = false, completed = false; QMessageBox::StandardButton saveChoice = QMessageBox::Cancel;
    QStringList errors; QString inputLocation; int savePrompts = 0;
    QTimer answer; answer.setInterval(10);
    connect(&answer, &QTimer::timeout, &window, [&] {
        if (auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget())) {
            inputLocation = input->labelText(); input->setTextValue(name);
            if (rejectName) input->reject(); else input->accept();
        } else if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            if (box->standardButtons().testFlag(QMessageBox::Save)) { ++savePrompts; box->button(saveChoice)->click(); }
            else { errors.append(box->text()); box->button(QMessageBox::Ok)->click(); }
        } else { completed = true; answer.stop(); }
    });
    auto request = [&](const QString &parent, const QString &value, bool reject = false) {
        name = value; rejectName = reject; completed = false;
        sidebar->createDocumentRequested(parent); answer.start();
    };
    request("folder", "Cancelled", true); QTRY_VERIFY(completed);
    QVERIFY(!QFileInfo::exists(dir.filePath("folder/Cancelled.md"))); QCOMPARE(savePrompts, 0);
    QCOMPARE(editor->source(), QString("# Keep\nunsaved edits")); QVERIFY(manager->loadNode("A.md")->isModified());
    request("folder", "B"); QTRY_VERIFY(completed);
    QCOMPARE(errors.size(), 1); QVERIFY(errors.back().contains(QStringLiteral("已存在"))); QCOMPARE(savePrompts, 0);
    QString content; QVERIFY(MarkdownFileIO::readFile(dir.filePath("folder/B.md"), content)); QCOMPARE(content, QString("# Existing"));
    request("folder", "../Escaped"); QTRY_VERIFY(completed);
    QCOMPARE(errors.size(), 2); QVERIFY(!QFileInfo::exists(dir.filePath("Escaped.md"))); QCOMPARE(savePrompts, 0);
    request("folder", "New"); QTRY_VERIFY(completed);
    QCOMPARE(savePrompts, 1); QVERIFY(!QFileInfo::exists(dir.filePath("folder/New.md")));
    QCOMPARE(editor->source(), QString("# Keep\nunsaved edits")); QVERIFY(manager->loadNode("A.md")->isModified());
    saveChoice = QMessageBox::Save;
    request("folder", QStringLiteral("新文档")); QTRY_VERIFY(completed);
    QVERIFY(inputLocation.contains("folder")); QVERIFY(manager->loadNode(QStringLiteral("folder/新文档.md")));
    QCOMPARE(editor->source(), QStringLiteral("# 新文档\n\n"));
    QVERIFY(MarkdownFileIO::readFile(dir.filePath("A.md"), content)); QCOMPARE(content, QString("# Keep\nunsaved edits"));
    request({}, "Root.MD"); QTRY_VERIFY(completed);
    QVERIFY(inputLocation.contains("md_data")); QVERIFY(QFileInfo::exists(dir.filePath("Root.MD"))); QVERIFY(!QFileInfo::exists(dir.filePath("Root.MD.md")));
    QCOMPARE(editor->source(), QString("# Root\n\n")); window.close();
}
void TreeMdTests::popupMenusShareRoundedBlackTheme() {
    QTemporaryDir dir; MarkdownManager manager(dir.path()); QVERIFY(manager.createDocument("README.md", "# Menu"));
    MainWindow window(dir.path()); window.show();
    const QString captures = qEnvironmentVariable("TMD_UI_CHECK_DIRECTORY");
    QMenu futureMenu(&window); futureMenu.setObjectName("futureMenu"); futureMenu.addAction(QStringLiteral("后续菜单"));
    QList<QMenu *> menus{window.findChild<QMenu *>("fileMenu"), window.findChild<QMenu *>("settingsMenu"), window.findChild<QMenu *>("saveCopyMenu"), &futureMenu};
    auto *sidebar = window.findChild<DocumentSidebar *>(); auto *tree = sidebar->findChild<QTreeWidget *>("documentTree");
    for (int index = 0; index <= menus.size(); ++index) {
        QMenu *menu = nullptr;
        if (index == menus.size()) {
            QMetaObject::invokeMethod(tree, "customContextMenuRequested", Qt::DirectConnection, Q_ARG(QPoint, QPoint(20, tree->viewport()->height() - 5)));
            menu = sidebar->findChild<QMenu *>("documentContextMenu");
        } else menu = menus[index];
        QVERIFY(menu); menu->popup(window.mapToGlobal(QPoint(12, 60))); QTest::qWait(20);
        QVERIFY(menu->isVisible());
        for (auto *action : menu->actions()) { QVERIFY(!action->text().contains("...")); QVERIFY(!action->text().contains(QChar(0x2026))); }
        const QImage rendered = menu->grab().toImage(); QVERIFY(!rendered.isNull());
        QCOMPARE(rendered.pixelColor(0, 0).alpha(), 0); QCOMPARE(rendered.pixelColor(rendered.width() - 1, 0).alpha(), 0);
        QCOMPARE(rendered.pixelColor(0, rendered.height() - 1).alpha(), 0); QCOMPARE(rendered.pixelColor(rendered.width() - 1, rendered.height() - 1).alpha(), 0);
        QCOMPARE(rendered.pixelColor(qRound(3 * rendered.devicePixelRatio()), rendered.height() / 2), QColor(Qt::black));
        if (!captures.isEmpty()) { QVERIFY(QDir().mkpath(captures)); QVERIFY(rendered.save(QDir(captures).filePath(menu->objectName() + ".png"))); }
        menu->hide();
    }
    window.close();
}
void TreeMdTests::applicationContextActionsPreserveOtherEdits() {
    QTemporaryDir dir; MarkdownManager initial(dir.path());
    QVERIFY(initial.createDocument("A.md", "# Keep")); QVERIFY(initial.createDocument("folder/B.md", "# Target"));
    MainWindow window(dir.path()); window.show(); QVERIFY(window.openDocument("A.md"));
    auto *sidebar = window.findChild<DocumentSidebar *>(); auto *editor = window.findChild<MarkdownEditor *>(); auto *manager = window.findChild<MarkdownManager *>();
    QVERIFY(sidebar); QVERIFY(editor); QVERIFY(manager); editor->setSource("# Keep\nunsaved edits");
    QTimer answer; answer.setInterval(10);
    connect(&answer, &QTimer::timeout, &window, [&] {
        if (auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget())) { input->setTextValue("folder/C.md"); input->accept(); answer.stop(); }
        else if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            qWarning() << "Unexpected rename dialog:" << box->windowTitle() << box->text();
            auto *button = box->button(QMessageBox::Cancel); if (!button) button = box->button(QMessageBox::Ok);
            if (button) button->click(); answer.stop();
        }
    }); answer.start();
    sidebar->renameDocumentRequested("folder/B.md"); QTRY_VERIFY(manager->loadNode("folder/C.md"));
    QCOMPARE(editor->source(), QString("# Keep\nunsaved edits")); QVERIFY(manager->loadNode("A.md")->isModified());
    QString questioned;
    disconnect(&answer, nullptr, &window, nullptr);
    connect(&answer, &QTimer::timeout, &window, [&] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) { questioned = box->text(); box->button(QMessageBox::Yes)->click(); answer.stop(); }
    }); answer.start();
    sidebar->deleteDocumentRequested("folder/C.md"); QTRY_VERIFY(!manager->loadNode("folder/C.md"));
    QVERIFY(questioned.contains("folder/C.md")); QCOMPARE(editor->source(), QString("# Keep\nunsaved edits"));
    disconnect(&answer, nullptr, &window, nullptr);
    connect(&answer, &QTimer::timeout, &window, [&] {
        if (auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget())) { input->setTextValue("new-empty"); input->accept(); answer.stop(); }
        else if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) { qWarning() << "Unexpected folder dialog:" << box->text(); box->button(QMessageBox::Ok)->click(); answer.stop(); }
    }); answer.start(); sidebar->createDirectoryRequested({}); QTRY_VERIFY(manager->directories().contains("new-empty"));
    answer.start(); sidebar->createDirectoryRequested("folder"); QTRY_VERIFY(manager->directories().contains("folder/new-empty"));
    QCOMPARE(editor->source(), QString("# Keep\nunsaved edits"));
    disconnect(&answer, nullptr, &window, nullptr);
    connect(&answer, &QTimer::timeout, &window, [&] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            if (auto *yes = box->button(QMessageBox::Yes)) yes->click();
            else { qWarning() << "Unexpected recycle dialog:" << box->text(); if (auto *ok = box->button(QMessageBox::Ok)) ok->click(); }
        }
    });
    answer.start(); sidebar->deleteDirectoryRequested("folder"); QTRY_VERIFY(!QFileInfo::exists(dir.filePath("folder")));
    answer.stop();
    QCOMPARE(editor->source(), QString("# Keep\nunsaved edits")); QVERIFY(manager->loadNode("A.md"));
    window.hide();
}
void TreeMdTests::directoryDeleteProtectsUnsavedCurrentDocument() {
    QTemporaryDir dir; MarkdownManager initial(dir.path());
    QVERIFY(initial.createDocument("A.md", "# Keep")); QVERIFY(initial.createDocument("active/B.md", "# Active"));
    QVERIFY(initial.createDocument("active/deep/C.md", "# Nested"));
    MainWindow window(dir.path()); window.show(); QVERIFY(window.openDocument("active/B.md"));
    auto *sidebar = window.findChild<DocumentSidebar *>(); auto *editor = window.findChild<MarkdownEditor *>(); auto *manager = window.findChild<MarkdownManager *>();
    QVERIFY(sidebar); QVERIFY(editor); QVERIFY(manager); editor->setSource("# Active\nsaved before recycling");
    QTimer answer; answer.setInterval(10); int prompts = 0;
    connect(&answer, &QTimer::timeout, &window, [&] { if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) { ++prompts; box->button(QMessageBox::Cancel)->click(); answer.stop(); } });
    answer.start(); sidebar->deleteDirectoryRequested("active"); QTRY_COMPARE(prompts, 1);
    QVERIFY(QFileInfo::exists(dir.filePath("active/B.md"))); QVERIFY(manager->loadNode("active/B.md")->isModified());
    disconnect(&answer, nullptr, &window, nullptr);
    connect(&answer, &QTimer::timeout, &window, [&] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            ++prompts;
            if (box->standardButtons().testFlag(QMessageBox::Save)) box->button(QMessageBox::Save)->click();
            else { box->button(QMessageBox::Yes)->click(); answer.stop(); }
        }
    }); answer.start(); sidebar->deleteDirectoryRequested("active"); QTRY_VERIFY(!QFileInfo::exists(dir.filePath("active")));
    QVERIFY(!manager->loadNode("active/B.md")); QVERIFY(!manager->loadNode("active/deep/C.md")); QVERIFY(manager->loadNode("A.md"));
    QCOMPARE(editor->source(), QString("# Keep"));
#ifdef Q_OS_WIN
    QString record; const QString recycled = recycledFixture(dir.filePath("active"), &record); QVERIFY(!recycled.isEmpty());
    QString content; QVERIFY(MarkdownFileIO::readFile(QDir(recycled).filePath("B.md"), content)); QCOMPARE(content, QString("# Active\nsaved before recycling"));
    QVERIFY(QDir().rename(recycled, dir.filePath("active"))); QVERIFY(QFile::remove(record));
#endif
    window.hide();
}

void TreeMdTests::initTestCase() {
    QVERIFY(m_settingsDirectory.isValid());
    QCoreApplication::setOrganizationName("TreeMdTests"); QCoreApplication::setApplicationName("TreeMdTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDirectory.path());
}
void TreeMdTests::init() { QSettings().clear(); MarkdownSettingsStore::setStoragePath(m_settingsDirectory.filePath(QString(QTest::currentTestFunction()) + ".txt")); }

void TreeMdTests::responsivePanelsAndOneThirdTree() {
    QTemporaryDir dir; MarkdownManager manager(dir.path()); QVERIFY(manager.createDocument("README.md", "# Current\n## Heading\n"));
    MainWindow window(dir.path()); window.resize(1200, 800); window.show(); QTest::qWait(40);
    auto *columns = window.findChild<QSplitter *>("documentColumns");
    auto *vertical = window.findChild<QSplitter *>("documentTreeSplitter");
    auto *outline = window.findChild<OutlineSidebar *>(); auto *outlineAction = window.findChild<QAction *>("outlineAction");
    QVERIFY(columns); QVERIFY(vertical); QVERIFY(outline); QVERIFY(outlineAction);
    auto *sidebar = window.findChild<DocumentSidebar *>(); QVERIFY(sidebar);
    QVERIFY(!outline->isVisible()); QVERIFY(!outlineAction->isChecked());
    const int fullWidth = columns->sizes()[0]; QVERIFY(fullWidth >= DocumentSidebar::compactThreshold);
    window.setRelationsVisible(true); QTest::qWait(40);
    auto checkThird = [&] {
        const auto sizes = vertical->sizes();
        QVERIFY(qAbs(sizes[1] * 3 - sizes[0] - sizes[1]) <= 3);
    };
    checkThird(); vertical->setSizes({200, 500}); window.setRelationsVisible(false); window.setRelationsVisible(true); QTest::qWait(30); checkThird();
    window.resize(1200, 620); window.setRelationsVisible(false); window.setRelationsVisible(true); QTest::qWait(30); checkThird();
    window.resize(1000, 1200); QTest::qWait(30); QVERIFY(outline->isVisible()); QVERIFY(outlineAction->isChecked());
    QCOMPARE(sidebar->displayMode(), DocumentSidebar::DisplayMode::Compact); QCOMPARE(columns->sizes()[0], DocumentSidebar::compactWidth);
    outlineAction->setChecked(false); window.resize(1100, 800); QTest::qWait(30); QVERIFY(!outline->isVisible());
    window.resize(1000, 1200); QTest::qWait(30); QVERIFY(!outline->isVisible());
    outlineAction->setChecked(true); window.resize(1000, 1000); QTest::qWait(30); QVERIFY(!outline->isVisible());
    outlineAction->setChecked(true); window.resize(1200, 900); QTest::qWait(30); QVERIFY(outline->isVisible());
    window.resize(640, 959); QTest::qWait(30); QVERIFY(sidebar->isVisible()); QCOMPARE(sidebar->displayMode(), DocumentSidebar::DisplayMode::Compact);
    window.resize(640, 960); QTest::qWait(30); QVERIFY(sidebar->isHidden()); QVERIFY(!outline->isVisible()); QVERIFY(!outlineAction->isChecked());
    outlineAction->trigger(); QTest::qWait(30); QVERIFY(outline->isVisible()); QVERIFY(sidebar->isHidden());
    window.resize(640, 1000); QTest::qWait(30); QVERIFY(outline->isVisible()); QVERIFY(sidebar->isHidden());
    outlineAction->trigger(); QVERIFY(!outline->isVisible()); window.resize(1200, 800); QTest::qWait(30); QVERIFY(sidebar->isVisible());
    QCOMPARE(sidebar->displayMode(), DocumentSidebar::DisplayMode::Expanded);
    window.close();
}
void TreeMdTests::sidebarDragCyclesThroughAllModes() {
    QTemporaryDir dir; MarkdownManager manager(dir.path()); QVERIFY(manager.createDocument("README.md", "# Current"));
    MainWindow window(dir.path()); window.show(); QTest::qWait(40);
    auto *columns = window.findChild<QSplitter *>("documentColumns"); auto *sidebar = window.findChild<DocumentSidebar *>();
    auto *tree = window.findChild<QTreeWidget *>("documentTree"); auto *button = window.findChild<QToolButton *>("quickDocumentSearchButton");
    QVERIFY(columns); QVERIFY(sidebar); QVERIFY(tree); QVERIFY(button); QCOMPARE(sidebar->displayMode(), DocumentSidebar::DisplayMode::Expanded);
    auto dragTo = [&](int x) {
        auto *handle = columns->handle(1); QVERIFY(handle->isVisible());
        QTest::mousePress(handle, Qt::LeftButton, Qt::NoModifier, handle->rect().center());
        const QPoint global = columns->mapToGlobal(QPoint(x, handle->y() + handle->height() / 2));
        QMouseEvent move(QEvent::MouseMove, handle->mapFromGlobal(global), global, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(handle, &move); QTest::mouseRelease(handle, Qt::LeftButton, Qt::NoModifier, handle->mapFromGlobal(global));
        QCoreApplication::processEvents();
    };
    auto *search = sidebar->findChild<QLineEdit *>("documentSearch"); auto *icon = sidebar->findChild<QLabel *>("compactBrandIcon");
    QVERIFY(search); QVERIFY(icon); search->setText("Current");
    dragTo(190); QVERIFY(tree->isVisible()); const int normalSearchHeight = search->height();
    dragTo(170); QCOMPARE(sidebar->displayMode(), DocumentSidebar::DisplayMode::Compact);
    QVERIFY(qAbs(columns->sizes()[0] - 170) <= columns->handleWidth());
    QVERIFY(!tree->isVisible()); QVERIFY(search->isVisible()); QVERIFY(!button->isVisible());
    dragTo(110); QVERIFY(qAbs(columns->sizes()[0] - 110) <= columns->handleWidth()); QVERIFY(search->height() > normalSearchHeight);
    dragTo(80); QVERIFY(search->isVisible()); QCOMPARE(search->width(), search->height());
    dragTo(64); QVERIFY(button->isVisible()); QVERIFY(icon->isVisible()); const int fullIcon = icon->width();
    QCOMPARE(button->size(), icon->size()); QVERIFY(!search->isVisible());
    dragTo(50); QVERIFY(button->isVisible()); QVERIFY(icon->width() < fullIcon); QCOMPARE(button->size(), icon->size());
    dragTo(42); QVERIFY(button->isVisible()); QVERIFY(icon->width() >= 24);
    dragTo(18); QCOMPARE(columns->sizes()[0], 0); QCOMPARE(sidebar->displayMode(), DocumentSidebar::DisplayMode::Hidden);
    QVERIFY(!button->isVisible()); QVERIFY(!sidebar->findChild<QLabel *>("compactBrandIcon")->isVisible());
    dragTo(250); QCOMPARE(sidebar->displayMode(), DocumentSidebar::DisplayMode::Expanded); QVERIFY(tree->isVisible()); QVERIFY(!button->isVisible());
    QCOMPARE(search->text(), QString("Current")); QCOMPARE(tree->currentItem()->data(0, Qt::UserRole).toString(), QString("README.md")); window.close();
}
void TreeMdTests::sidebarHeaderMorphsWithoutGeometryJumps() {
    QTemporaryDir dir; MarkdownManager manager(dir.path()); QVERIFY(manager.createDocument("notes/Current.md", "# Current"));
    QWidget host; host.resize(600, 400);
    DocumentSidebar sidebar(&host); sidebar.setDocuments(manager.allNodes(), manager.directories()); sidebar.resize(240, 360); host.show(); sidebar.show();
    auto *search = sidebar.findChild<QLineEdit *>("documentSearch"); auto *tree = sidebar.findChild<QTreeWidget *>("documentTree");
    auto *button = sidebar.findChild<QToolButton *>("quickDocumentSearchButton"); auto *icon = sidebar.findChild<QLabel *>("compactBrandIcon");
    QVERIFY(search); QVERIFY(tree); QVERIFY(button); QVERIFY(icon);
    search->setText("Current"); QRect previousSearch = search->geometry(), previousIcon;
    int previousDiameter = 44;
    const QString captures = qEnvironmentVariable("TMD_UI_CHECK_DIRECTORY");
    for (int w = 238; w >= DocumentSidebar::hiddenThreshold; w -= 2) {
        sidebar.resize(w, 360); QCoreApplication::processEvents();
        if (search->isVisible()) {
            QVERIFY(qAbs(search->x() - previousSearch.x()) <= 4); QVERIFY(qAbs(search->y() - previousSearch.y()) <= 2);
            QVERIFY(qAbs(search->width() - previousSearch.width()) <= 4); QVERIFY(qAbs(search->height() - previousSearch.height()) <= 2);
            QVERIFY(sidebar.rect().contains(search->geometry())); previousSearch = search->geometry();
            if (w <= DocumentSidebar::circleThreshold) QCOMPARE(search->width(), search->height());
        } else {
            QVERIFY(button->isVisible()); QVERIFY(icon->isVisible()); QCOMPARE(button->size(), icon->size());
            QCOMPARE(button->width(), button->height()); QVERIFY(icon->width() <= previousDiameter); QVERIFY(icon->width() >= 24);
            QVERIFY(sidebar.rect().contains(button->geometry())); QVERIFY(sidebar.rect().contains(icon->geometry()));
            if (!previousIcon.isNull()) QVERIFY(qAbs(icon->width() - previousIcon.width()) <= 3);
            previousIcon = icon->geometry(); previousDiameter = icon->width();
        }
        QCOMPARE(tree->isVisible(), w >= DocumentSidebar::compactThreshold); QCOMPARE(search->text(), QString("Current"));
        if (!captures.isEmpty() && QList<int>{220, 200, 180, 160, 120, 100, 80, 66, 64, 52, 40, 36}.contains(w)) {
            QVERIFY(QDir().mkpath(captures)); QVERIFY(sidebar.grab().save(QDir(captures).filePath(QString("sidebar-%1.png").arg(w))));
        }
    }
    sidebar.resize(DocumentSidebar::hiddenThreshold - 1, 360); QCoreApplication::processEvents();
    QVERIFY(!button->isVisible()); QVERIFY(!icon->isVisible()); QCOMPARE(sidebar.displayMode(), DocumentSidebar::DisplayMode::Hidden);
    sidebar.resize(240, 360); QCoreApplication::processEvents(); QVERIFY(tree->isVisible()); QVERIFY(search->isVisible());
    QCOMPARE(search->text(), QString("Current"));
}
void TreeMdTests::compactSearchOpensDocumentsAndRefreshes() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    QVERIFY(manager.createDocument("README.md", "# Start\n")); QVERIFY(manager.createDocument("folder/alpha.md", "# Alpha title\n"));
    QVERIFY(manager.createDocument("other.md", QStringLiteral("# 中文标题\n")));
    MainWindow window(dir.path()); window.resize(780, 1000); window.show(); QTest::qWait(40);
    auto *sidebar = window.findChild<DocumentSidebar *>(); auto *button = window.findChild<QToolButton *>("quickDocumentSearchButton");
    QVERIFY(sidebar); QVERIFY(button); QVERIFY(button->isVisible()); QTest::mouseClick(button, Qt::LeftButton); QTest::qWait(30);
    auto *popup = window.findChild<QDialog *>("quickDocumentPopup"); auto *search = window.findChild<QLineEdit *>("quickDocumentSearch");
    auto *results = window.findChild<QListWidget *>("quickDocumentResults"); QVERIFY(popup); QVERIFY(search); QVERIFY(results); QVERIFY(popup->isVisible());
    search->setText("missing"); QCOMPARE(results->count(), 0); QTest::keyClick(search, Qt::Key_Return); QVERIFY(popup->isVisible());
    search->setText("FOLDER ALPHA"); QCOMPARE(results->count(), 1); QCOMPARE(results->item(0)->data(Qt::UserRole).toString(), QString("folder/alpha.md"));
    QTest::keyClick(search, Qt::Key_Return); QTRY_VERIFY(!popup->isVisible());
    QTRY_COMPARE(window.findChild<MarkdownEditor *>()->source(), QString("# Alpha title\n"));
    button->click(); search->setText(QStringLiteral("中文标题")); QCOMPARE(results->count(), 1);
    search->setText("alpha"); auto *liveManager = window.findChild<MarkdownManager *>(); QVERIFY(liveManager);
    QVERIFY(liveManager->renameDocument("folder/alpha.md", "folder/renamed.md")); QTRY_COMPARE(results->item(0)->data(Qt::UserRole).toString(), QString("folder/renamed.md"));
    QVERIFY(liveManager->createDocument("new.md", "# Alpha new\n")); QTRY_COMPARE(results->count(), 2);
    search->clear(); QCOMPARE(results->count(), 4); const int row = results->currentRow(); QTest::keyClick(search, Qt::Key_Down);
    QCOMPARE(results->currentRow(), qMin(row + 1, 3)); QTest::keyClick(search, Qt::Key_Up); QCOMPARE(results->currentRow(), row);
    QTest::keyClick(search, Qt::Key_Escape); QVERIFY(!popup->isVisible());
    window.resize(640, 960); QTest::qWait(30); QVERIFY(sidebar->isHidden());
    window.findChild<QAction *>("quickDocumentSearchAction")->trigger(); QTest::qWait(30); QVERIFY(popup->isVisible()); QVERIFY(sidebar->isHidden());
    QTest::keyClick(search, Qt::Key_Escape); window.close();
}
void TreeMdTests::sharedTreeZoomPersistsAcrossViews() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    for (const auto &path : {"README.md", "next.md", "isolated.md"}) QVERIFY(manager.createDocument(path, "# " + QString(path)));
    QVERIFY(manager.addRelation("README.md", "next.md", NodeRelationType::Next));
    MainWindow window(dir.path()); window.show(); window.setRelationsVisible(true); QTest::qWait(40);
    auto *graph = window.findChild<KnowledgeGraphWidget *>(); auto *control = window.findChild<QSpinBox *>("knowledgeTreeZoom");
    QVERIFY(graph); QVERIFY(control); QCOMPARE(control->value(), 60);
    control->setValue(60); QCOMPARE(graph->transform().m11(), .6);
    window.resize(1100, 1200); QTest::qWait(30); QCOMPARE(graph->transform().m11(), .6);
    QVERIFY(window.openDocument("isolated.md")); QCOMPARE(graph->visibleNodeCount(), 1); QCOMPARE(graph->transform().m11(), .6);
    QVERIFY(window.openDocument("README.md")); QCOMPARE(graph->transform().m11(), .6);
    window.setRelationsVisible(false); window.setRelationsVisible(true); QTest::qWait(30); QCOMPARE(graph->transform().m11(), .6);
    window.openKnowledgeTree(); auto *dialog = window.findChild<RelationDialog *>(); QVERIFY(dialog);
    auto *canvas = dialog->findChild<KnowledgeTreeCanvas *>(); auto *otherControl = dialog->findChild<QSpinBox *>("knowledgeTreeZoom");
    QVERIFY(canvas); QVERIFY(otherControl); QTest::qWait(30); QCOMPARE(canvas->transform().m11(), .6); QCOMPARE(otherControl->value(), 60);
    otherControl->setValue(125); QCOMPARE(graph->zoomPercent(), 125); QCOMPARE(control->value(), 125); QCOMPARE(graph->transform().m11(), 1.25);
    const QPoint point = canvas->viewport()->rect().center();
    QWheelEvent wheel(point, canvas->viewport()->mapToGlobal(point), {}, {0, -120}, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(canvas->viewport(), &wheel);
    QCOMPARE(canvas->zoomPercent(), 112); QCOMPARE(control->value(), 112); QCOMPARE(otherControl->value(), 112);
    QCOMPARE(QSettings().value("knowledgeTree/zoomPercent").toInt(), 112);
    dialog->close(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); window.close();
    MainWindow reopened(dir.path()); reopened.show(); reopened.setRelationsVisible(true); QTest::qWait(30);
    auto *reopenedGraph = reopened.findChild<KnowledgeGraphWidget *>(); QCOMPARE(reopenedGraph->zoomPercent(), 112);
    QCOMPARE(reopenedGraph->transform().m11(), 1.12); reopened.close();
    QSettings().setValue("knowledgeTree/zoomPercent", "invalid"); KnowledgeTreeCanvas invalid; QCOMPARE(invalid.zoomPercent(), 60);
    QSettings().setValue("knowledgeTree/zoomPercent", 999); KnowledgeTreeCanvas outside; QCOMPARE(outside.zoomPercent(), 60);
}

namespace {
QByteArray png(QColor color = Qt::green) {
    QImage image(32, 24, QImage::Format_ARGB32); image.fill(color);
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer, "PNG"); return bytes;
}
bool bytesFile(const QString &path, const QByteArray &bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
int assetCount(const QString &root) {
    int count = 0; QDirIterator iterator(root, {"image_*"}, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) { iterator.next(); ++count; } return count;
}
void imageServer(QTcpServer &server, const QByteArray &bytes) {
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server, bytes] {
        while (server.hasPendingConnections()) {
            auto *socket = server.nextPendingConnection();
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, bytes] {
                socket->readAll(); socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: " + QByteArray::number(bytes.size()) + "\r\nConnection: close\r\n\r\n" + bytes); socket->disconnectFromHost();
            });
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
}
}
void TreeMdTests::imageSyntaxPreservesCodeAndTitles() {
    const QString body = "![nested [alt]](photos/a(b).png \"keep title\")\n![space](<a b.png>)\n![ref][PIC]\n![PIC][]\n[PIC]: <folder/ref.png> 'caption'\n"
        "`![code](bad.png)`\n````md\n![code](bad2.png)\n```\n![code](bad3.png)\n````\n    ![code](bad4.png)\n\\![escaped](bad5.png)\n"
        "<img src=\"html.png\" alt=\"html\">\n![data](data:image/png;base64,AAAA)\n";
    const auto images = MarkdownImages::parse(body);
    QCOMPARE(images.size(), 6); QCOMPARE(images[0].source, QString("photos/a(b).png")); QCOMPARE(images[0].alt, QString("nested [alt]"));
    QCOMPARE(images[1].source, QString("a b.png")); QCOMPARE(images[2].start, images[3].start);
    const QString rewritten = MarkdownImages::replace(body, images, {{"photos/a(b).png", "new.png"}, {"folder/ref.png", "ref-new.png"}});
    QVERIFY(rewritten.contains("(new.png \"keep title\")")); QVERIFY(rewritten.contains("[PIC]: <ref-new.png> 'caption'"));
    QVERIFY(rewritten.contains("![code](bad3.png)")); QVERIFY(rewritten.contains("![data](data:image/png;base64,AAAA)"));
    const QString path = QStringLiteral("../图片/空 格(1).png");
    QVERIFY(!MarkdownImages::encodedPath(path).contains(' '));
    QCOMPARE(MarkdownImages::localPath(MarkdownImages::encodedPath(path), "C:/data/test.md"), QStringLiteral("C:/图片/空 格(1).png"));
}
void TreeMdTests::imageImportMirrorsDirectoriesAndDeduplicates() {
    QTemporaryDir dir; const QString root = dir.filePath("md_data"); MarkdownManager manager(root); ImageStorage storage(root); QString error;
    QVERIFY(storage.ensureRoot(&error)); QVERIFY(QFileInfo(storage.photoRoot()).isDir());
    QVERIFY(manager.createDocument("empty/deep/no-images.md", "# Text\n")); QVERIFY(!QFileInfo::exists(dir.filePath("md_photo/empty")));
    const QByteArray bytes = png(); const QString local = dir.filePath(QStringLiteral("source/图片 空格.png")); QVERIFY(bytesFile(local, bytes));
    const QString source = dir.filePath("source/doc.md");
    const QString content = "# Import\n![local](<" + MarkdownImages::encodedPath(QFileInfo(local).fileName()) + "> \"caption\")\n![inline](data:image/png;base64," + QString::fromLatin1(bytes.toBase64()) + ")\n![host](https://example.test/image.png)\n";
    QVERIFY(MarkdownFileIO::writeFile(source, content)); QVERIFY2(manager.importDocument(source, "notes/deep/doc.md", &error), qPrintable(error));
    const QString imported = manager.loadNode("notes/deep/doc.md")->content(); const auto images = MarkdownImages::parse(imported);
    QCOMPARE(images.size(), 3); QCOMPARE(images[0].source, images[1].source); QCOMPARE(assetCount(storage.photoRoot()), 1);
    QVERIFY(images[0].source.startsWith("../../../md_photo/notes/deep/")); QVERIFY(imported.contains("\"caption\""));
    QCOMPARE(images[2].source, QString("https://example.test/image.png"));
    QString original; QVERIFY(MarkdownFileIO::readFile(source, original)); QCOMPARE(original, content);
    const QString imagePath = MarkdownImages::localPath(images[0].source, manager.loadNode("notes/deep/doc.md")->filePath());
    QFile image(imagePath); QVERIFY(image.open(QIODevice::ReadOnly)); QCOMPARE(image.readAll(), bytes);
    QString reference; QVERIFY(!storage.store("../outside.md", bytes, reference, nullptr, &error));
}
void TreeMdTests::imageImportFailureRollsBack() {
    QTemporaryDir dir; const QString root = dir.filePath("md_data"); MarkdownManager manager(root); ImageStorage storage(root);
    QVERIFY(bytesFile(dir.filePath("input/ok.png"), png()));
    QVERIFY(MarkdownFileIO::writeFile(dir.filePath("input/a.md"), "![ok](ok.png)\n![missing](missing.png)\n"));
    QString error; QVERIFY(!manager.importDocument(dir.filePath("input/a.md"), "failed/a.md", &error));
    QVERIFY(!error.isEmpty()); QVERIFY(!QFileInfo::exists(dir.filePath("md_data/failed/a.md"))); QCOMPARE(assetCount(storage.photoRoot()), 0);
    QVERIFY(!QFileInfo::exists(dir.filePath("md_photo/failed"))); QVERIFY(QFileInfo::exists(dir.filePath("input/ok.png")));
}
void TreeMdTests::imageRelocationAndMetadataRollback() {
    QTemporaryDir dir; const QString root = dir.filePath("md_data"); MarkdownManager manager(root); ImageStorage storage(root);
    QString reference, error; QVERIFY(storage.store("one/doc.md", png(), reference, nullptr, &error));
    const QString body = "# Test\n" + MarkdownImages::markup(reference) + "\n![absolute](<C:/external/pic.png>)\n";
    QVERIFY(manager.createDocument("one/doc.md", body));
    QVERIFY2(manager.renameDocument("one/doc.md", "two/deep/doc.md", &error), qPrintable(error));
    auto *node = manager.loadNode("two/deep/doc.md"); const auto images = MarkdownImages::parse(node->content());
    QVERIFY(QFileInfo::exists(MarkdownImages::localPath(images[0].source, node->filePath()))); QVERIFY(images[0].source.contains("md_photo/two/deep/"));
    QCOMPARE(images[1].source, QString("C:/external/pic.png"));
    QVERIFY(MarkdownFileIO::writeFile(dir.filePath("md_data/.tree-md-relations.json"), "not json"));
    QVERIFY(!manager.renameDocument("two/deep/doc.md", "rollback/doc.md", &error));
    QVERIFY(!QFileInfo::exists(dir.filePath("md_photo/rollback"))); QVERIFY(QFileInfo::exists(dir.filePath("md_data/two/deep/doc.md")));
    QString restored; QVERIFY(MarkdownFileIO::readFile(node->filePath(), restored)); QCOMPARE(restored, node->content());
}
void TreeMdTests::imageExportLocalEmbeddedAndRemote() {
    QTemporaryDir dir; QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost)); imageServer(server, png(Qt::blue));
    QVERIFY(bytesFile(dir.filePath("source/local.png"), png(Qt::green)));
    const QString remote = QString("http://127.0.0.1:%1/image.png").arg(server.serverPort());
    const QString body = "# Export\n![local](local.png \"title\")\n![remote](" + remote + ")\n![data](data:image/png;base64," + QString::fromLatin1(png(Qt::red).toBase64()) + ")\n`![example](missing.png)`\n";
    const QString source = dir.filePath("source/a.md"), target = dir.filePath("export/a.md"); QVERIFY(MarkdownFileIO::writeFile(source, body));
    MarkdownExportJob job; QSignalSpy done(&job, &MarkdownExportJob::finished); job.start(body, source, target, true);
    QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 5000); QVERIFY2(done[0][0].toBool(), qPrintable(done[0][1].toString()));
    QString exported, original; QVERIFY(MarkdownFileIO::readFile(target, exported)); const auto images = MarkdownImages::parse(exported); QCOMPARE(images.size(), 3);
    for (const auto &image : images) { QVERIFY(!image.source.contains('/')); QVERIFY(QFileInfo::exists(MarkdownImages::localPath(image.source, target))); }
    QVERIFY(exported.contains("\"title\"")); QVERIFY(exported.contains("`![example](missing.png)`")); QCOMPARE(assetCount(dir.filePath("export")), 3);
    QVERIFY(MarkdownFileIO::readFile(source, original)); QCOMPARE(original, body);
    done.clear(); const QString plain = dir.filePath("plain/a.md"); job.start(body, source, plain, false);
    QTRY_COMPARE(done.size(), 1); QVERIFY(done[0][0].toBool()); QVERIFY(MarkdownFileIO::readFile(plain, exported)); QCOMPARE(exported, body); QCOMPARE(assetCount(dir.filePath("plain")), 0);
}
void TreeMdTests::imageExportFailureAndCancellation() {
    QTemporaryDir dir; const QString source = dir.filePath("source.md"), target = dir.filePath("out/a.md");
    QVERIFY(QDir().mkpath(QFileInfo(target).absolutePath()));
    QVERIFY(MarkdownFileIO::writeFile(target, "existing")); MarkdownExportJob job; QSignalSpy done(&job, &MarkdownExportJob::finished);
    job.start("![missing](missing.png)", source, target, true); QTRY_COMPARE(done.size(), 1); QVERIFY(!done[0][0].toBool());
    QString preserved; QVERIFY(MarkdownFileIO::readFile(target, preserved)); QCOMPARE(preserved, QString("existing"));
    QVERIFY(bytesFile(dir.filePath("ok.png"), png())); QDir().mkpath(dir.filePath("cannot-replace.md"));
    done.clear(); job.start("![ok](ok.png)", source, dir.filePath("cannot-replace.md"), true); QTRY_COMPARE(done.size(), 1);
    QVERIFY(!done[0][0].toBool()); QCOMPARE(assetCount(dir.path()), 0);
    done.clear(); job.start("# unchanged", source, source, false); QCOMPARE(done.size(), 1); QVERIFY(!done[0][0].toBool());
    QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
    done.clear(); job.start(QString("![remote](http://127.0.0.1:%1/pending.png)").arg(server.serverPort()), source, target, true);
    QTRY_VERIFY(server.hasPendingConnections()); job.cancel(); QCOMPARE(done.size(), 1); QVERIFY(!done[0][0].toBool());
    QVERIFY(MarkdownFileIO::readFile(target, preserved)); QCOMPARE(preserved, QString("existing"));
}
void TreeMdTests::applicationImageDropPasteAndPathModes() {
    QTemporaryDir dir; const QString root = dir.filePath("md_data"); MarkdownManager manager(root);
    QVERIFY(manager.createDocument("deep/doc.md", "# Images\n\n")); QVERIFY(bytesFile(dir.filePath("input/pic.png"), png()));
    MainWindow window(root); window.show(); window.setSourceMode(true);
    auto *editor = window.findChild<MarkdownEditor *>(); auto *rendered = window.findChild<MarkdownDocumentView *>(); QVERIFY(editor); QVERIFY(rendered);
    const QString original = editor->source();
    QMimeData mime; mime.setUrls({QUrl::fromLocalFile(dir.filePath("input/pic.png"))});
    QDragEnterEvent enter(QPoint(60, 70), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(editor->viewport(), &enter); QVERIFY(enter.isAccepted());
    QDropEvent drop(QPointF(60, 70), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier); QApplication::sendEvent(editor->viewport(), &drop);
    QTRY_COMPARE(MarkdownImages::parse(editor->source()).size(), 1);
    const auto image = MarkdownImages::parse(editor->source()).front(); QVERIFY(image.source.contains("../../md_photo/deep/"));
    QVERIFY(QFileInfo::exists(dir.filePath("input/pic.png"))); QCOMPARE(assetCount(dir.filePath("md_photo")), 1);
    editor->undo(); QCOMPARE(editor->source(), original);
    QImage clipboard(16, 16, QImage::Format_ARGB32); clipboard.fill(Qt::red); QApplication::clipboard()->setImage(clipboard); editor->paste();
    QCOMPARE(MarkdownImages::parse(editor->source()).size(), 1); QCOMPARE(assetCount(dir.filePath("md_photo")), 2); editor->undo(); QCOMPARE(editor->source(), original);
    QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost)); imageServer(server, png(Qt::yellow));
    const QString hosted = QString("http://127.0.0.1:%1/image?token=test").arg(server.serverPort());
    for (int mode : {0, 1, 2}) {
        window.findChild<QAction *>("imageSettingsAction")->trigger(); auto *settings = window.findChild<ImageSettingsDialog *>(); QVERIFY(settings);
        settings->findChild<QTabWidget *>("imageStorageModes")->setCurrentIndex(mode); settings->findChild<QPushButton *>("applyImageSettings")->click(); settings->reject();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); QCOMPARE(MarkdownSettingsStore::current().imageMode, quint8(mode));
        if (mode == 2) QApplication::clipboard()->setText(hosted);
        else { auto *input = new QMimeData; input->setUrls(mime.urls()); QApplication::clipboard()->setMimeData(input); }
        editor->paste(); const auto images = MarkdownImages::parse(editor->source()); QCOMPARE(images.size(), 1);
        if (mode == 0) QVERIFY(images[0].source.contains("md_photo/deep/"));
        if (mode == 1) QCOMPARE(MarkdownImages::localPath(images[0].source, dir.filePath("md_data/deep/doc.md")), QDir::fromNativeSeparators(dir.filePath("input/pic.png")));
        if (mode == 2) QCOMPARE(images[0].source, hosted);
        editor->undo(); QCOMPARE(editor->source(), original);
    }
    window.findChild<QAction *>("imageSettingsAction")->trigger(); auto *imageSettings = window.findChild<ImageSettingsDialog *>(); QVERIFY(imageSettings);
    imageSettings->findChild<QTabWidget *>("imageStorageModes")->setCurrentIndex(0); imageSettings->findChild<QPushButton *>("applyImageSettings")->click(); imageSettings->reject();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); window.setSourceMode(false);
    clipboard.fill(Qt::blue); QApplication::clipboard()->setImage(clipboard); rendered->setFocus(); QTest::keyClick(rendered, Qt::Key_V, Qt::ControlModifier);
    QTRY_COMPARE(rendered->loadedImageCount(), 1); QCOMPARE(MarkdownImages::parse(editor->source()).size(), 1);
    QTest::keyClick(rendered, Qt::Key_Z, Qt::ControlModifier); QTRY_COMPARE(editor->source(), original); QTRY_COMPARE(rendered->loadedImageCount(), 0);
    QDragEnterEvent documentEnter(QPoint(60, 70), Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(rendered->viewport(), &documentEnter); QVERIFY(documentEnter.isAccepted());
    QDropEvent documentDrop(QPointF(60, 70), Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier); documentDrop.setDropAction(Qt::MoveAction);
    QApplication::sendEvent(rendered->viewport(), &documentDrop); QCOMPARE(documentDrop.dropAction(), Qt::CopyAction);
    QTRY_COMPARE(rendered->loadedImageCount(), 1); QVERIFY(QFileInfo::exists(dir.filePath("input/pic.png")));
    editor->undo(); QCOMPARE(editor->source(), original); QApplication::clipboard()->clear(); window.close();
}
void TreeMdTests::applicationExportOptions() {
    QTemporaryDir dir; const QString root = dir.filePath("md_data"); MarkdownManager manager(root); ImageStorage storage(root); QString reference;
    QVERIFY(storage.store("doc.md", png(), reference)); QVERIFY(manager.createDocument("doc.md", "# Export\n" + MarkdownImages::markup(reference) + "\n"));
    MainWindow window(root); window.show(); auto *action = window.findChild<QAction *>("exportAction"); QVERIFY(action);
    for (bool include : {true, false}) {
        const QString target = dir.filePath(include ? "with/doc.md" : "without/doc.md"); QDir().mkpath(QFileInfo(target).absolutePath());
        QTimer::singleShot(50, &window, [&, target, include] {
            auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget()); QVERIFY(dialog);
            auto *checkbox = dialog->findChild<QCheckBox *>(); QVERIFY(checkbox); QVERIFY(checkbox->isChecked()); checkbox->setChecked(include);
            dialog->selectFile(target); QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
        });
        action->trigger(); QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(target), 5000);
        QCOMPARE(assetCount(QFileInfo(target).absolutePath()), include ? 1 : 0);
    }
    window.close();
}

void TreeMdTests::utf8ChineseAndAtomicIO() {
    QTemporaryDir directory; QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("中文文档.md"));
    const QString original = QStringLiteral("# 中文标题\n\n你好，世界 🌳\n");
    QString read, error;
    QVERIFY2(MarkdownFileIO::writeFile(path, original, &error), qPrintable(error));
    QVERIFY(MarkdownFileIO::readFile(path, read, &error)); QCOMPARE(read, original);
    QFile invalid(path); QVERIFY(invalid.open(QIODevice::WriteOnly)); invalid.write("\xff\xfe"); invalid.close();
    read = "keep"; QVERIFY(!MarkdownFileIO::readFile(path, read, &error)); QCOMPARE(read, QString("keep")); QVERIFY(!error.isEmpty());
    QFile windows(path); QVERIFY(windows.open(QIODevice::WriteOnly)); windows.write("# Windows\r\n\r\n"); windows.close();
    QVERIFY(MarkdownFileIO::writeFile(path, "# Windows\nupdated\n"));
    QVERIFY(windows.open(QIODevice::ReadOnly)); QCOMPARE(windows.readAll(), QByteArray("# Windows\r\nupdated\r\n")); windows.close();
    QVERIFY(!MarkdownFileIO::writeFile(directory.path(), "cannot replace directory", &error));
    QVERIFY(MarkdownFileIO::readFile(path, read)); QCOMPARE(read, QString("# Windows\nupdated\n"));
}
void TreeMdTests::outlineFencesAndSetext() {
    const QString text = QStringLiteral("# 中文 123\n````cpp\n# hidden\n```\n## still hidden\n````\n## **Visible** ###\n~~~\n# hidden too\n~~~\nSetext\n======\n    # indented code\n#not-heading\n###\n");
    const auto headings = MarkdownParser::outline(text);
    QCOMPARE(headings.size(), 3); QCOMPARE(headings[0].text, QStringLiteral("中文 123"));
    QCOMPARE(headings[1].text, QString("Visible")); QCOMPARE(headings[1].level, 2);
    QCOMPARE(headings[2].text, QString("Setext")); QCOMPARE(headings[2].line, 10); QCOMPARE(headings[2].level, 1);
    QCOMPARE(text.mid(headings[1].position, 2), QString("##"));
    QVERIFY(MarkdownParser::outline("#\n##\n###\n#word\n").isEmpty());
    QCOMPARE(MarkdownParser::outline("# \n##\t\n").size(), 2);
}
void TreeMdTests::transientWindowsLockKeepsAtomicSave() {
#ifdef Q_OS_WIN
    QTemporaryDir directory; const QString path = directory.filePath("locked.md");
    QVERIFY(MarkdownFileIO::writeFile(path, "old content"));
    const HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    QVERIFY(handle != INVALID_HANDLE_VALUE);
    std::thread release([handle] { Sleep(75); CloseHandle(handle); });
    QString error;
    const bool success = MarkdownFileIO::writeFile(path, "new content", &error);
    release.join(); QVERIFY2(success, qPrintable(error));
    QString content; QVERIFY(MarkdownFileIO::readFile(path, content)); QCOMPARE(content, QString("new content"));
#endif
}
void TreeMdTests::emptyAndLargeDocuments() {
    QTemporaryDir directory; MarkdownManager manager(directory.path()); QString error;
    QVERIFY(manager.createDocument(QStringLiteral("空.md"), {}, &error));
    QCOMPARE(manager.loadNode(QStringLiteral("空.md"))->title(), QStringLiteral("空"));
    QVERIFY(MarkdownParser::outline({}).isEmpty());
    QString large = "# Large\n";
    large.reserve(2500000);
    for (int i = 0; i < 45000; ++i) large += "A paragraph with Unicode 中文 and **important** content.\n";
    QVERIFY(manager.createDocument("large.md", large, &error));
    QCOMPARE(manager.loadNode("large.md")->content(), large); QCOMPARE(MarkdownParser::outline(large).size(), 1);
}
void TreeMdTests::documentLifecycleAndPortableRelations() {
    QTemporaryDir directory; MarkdownManager manager(directory.path()); QString error;
    QVERIFY(manager.createDocument("A.md", "# A\n"));
    QVERIFY(manager.createDocument(QStringLiteral("目录/B.md"), "# B\n"));
    QVERIFY(manager.createDocument("C.md", "# C\n"));
    QVERIFY(manager.addRelation("A.md", QStringLiteral("目录/B.md"), NodeRelationType::Previous));
    QVERIFY(manager.addRelation(QStringLiteral("目录/B.md"), "C.md", NodeRelationType::Child));
    QVERIFY2(manager.renameDocument(QStringLiteral("目录/B.md"), QStringLiteral("新目录/重命名.md"), &error), qPrintable(error));
    QCOMPARE(manager.loadNode("A.md")->relatedPaths(NodeRelationType::Previous), QStringList{QStringLiteral("新目录/重命名.md")});
    QCOMPARE(manager.loadNode(QStringLiteral("新目录/重命名.md"))->relatedPaths(NodeRelationType::Child), QStringList{"C.md"});
    QString metadata; QVERIFY(MarkdownFileIO::readFile(directory.filePath(".tree-md-relations.json"), metadata));
    QVERIFY(!metadata.contains(directory.path())); QVERIFY(!metadata.contains("D:"));
    MarkdownManager reopened(directory.path()); QVERIFY(reopened.scan());
    QCOMPARE(reopened.loadNode("A.md")->relatedPaths(NodeRelationType::Previous), QStringList{QStringLiteral("新目录/重命名.md")});
    QTemporaryDir moved;
    QVERIFY(QDir(moved.path()).mkpath(QStringLiteral("新目录")));
    QVERIFY(QFile::copy(directory.filePath("A.md"), moved.filePath("A.md")));
    QVERIFY(QFile::copy(directory.filePath("C.md"), moved.filePath("C.md")));
    QVERIFY(QFile::copy(directory.filePath(QStringLiteral("新目录/重命名.md")), moved.filePath(QStringLiteral("新目录/重命名.md"))));
    QVERIFY(QFile::copy(directory.filePath(".tree-md-relations.json"), moved.filePath(".tree-md-relations.json")));
    MarkdownManager portable(moved.path()); QVERIFY(portable.scan());
    QCOMPARE(portable.loadNode("A.md")->relatedPaths(NodeRelationType::Previous).size(), 1);
    QVERIFY(reopened.deleteDocument(QStringLiteral("新目录/重命名.md")));
    QVERIFY(!reopened.loadNode(QStringLiteral("新目录/重命名.md"))); QVERIFY(reopened.loadNode("A.md")->relatedPaths(NodeRelationType::Previous).isEmpty());
    QVERIFY(reopened.scan());
    QVERIFY(reopened.importDocument(moved.filePath("C.md"), QStringLiteral("导入.md"))); QCOMPARE(reopened.loadNode(QStringLiteral("导入.md"))->content(), QString("# C\n"));
}
void TreeMdTests::uniqueRolesArePerPair() {
    QTemporaryDir directory; MarkdownManager manager(directory.path()); QString error;
    for (const QString path : {"A.md", "B.md", "C.md"}) QVERIFY(manager.createDocument(path, "# " + path));
    QVERIFY(manager.addRelation("A.md", "B.md", NodeRelationType::Previous));
    QVERIFY(!manager.addRelation("A.md", "B.md", NodeRelationType::Parent, &error)); QVERIFY(!error.isEmpty());
    QVERIFY(manager.addRelation("C.md", "B.md", NodeRelationType::Next));
    QVERIFY(!manager.addRelation("A.md", "A.md", NodeRelationType::Child));
    QVERIFY(!manager.addRelation("A.md", "missing.md", NodeRelationType::Next));
    QVERIFY(manager.removeRelation("A.md", "B.md")); QVERIFY(manager.addRelation("A.md", "B.md", NodeRelationType::Parent));
}
void TreeMdTests::externalChangeProtectsEdits() {
    QTemporaryDir directory; MarkdownManager manager(directory.path()); QString error;
    QVERIFY(manager.createDocument("A.md", "# A\noriginal")); manager.updateContent("A.md", "# A\nmy edits");
    QVERIFY(MarkdownFileIO::writeFile(directory.filePath("A.md"), "# A\nexternal"));
    QVERIFY(manager.scan()); QVERIFY(manager.loadNode("A.md")->hasExternalChange()); QCOMPARE(manager.loadNode("A.md")->content(), QString("# A\nmy edits"));
    QVERIFY(!manager.saveDocument("A.md", &error));
    QString disk; QVERIFY(MarkdownFileIO::readFile(directory.filePath("A.md"), disk)); QCOMPARE(disk, QString("# A\nexternal"));
    QVERIFY(manager.saveDocument("A.md", &error, true)); QVERIFY(!manager.loadNode("A.md")->isModified());
    QVERIFY(MarkdownFileIO::writeFile(directory.filePath("A.md"), "# New external\n")); QVERIFY(manager.scan());
    QCOMPARE(manager.loadNode("A.md")->title(), QString("New external"));
}
void TreeMdTests::externalDeletePrunesReferences() {
    QTemporaryDir directory; MarkdownManager manager(directory.path());
    QVERIFY(manager.createDocument("A.md", "# A")); QVERIFY(manager.createDocument("B.md", "# B"));
    QVERIFY(manager.addRelation("A.md", "B.md", NodeRelationType::Next));
    QVERIFY(QFile::remove(directory.filePath("B.md"))); QVERIFY(manager.scan());
    QVERIFY(manager.loadNode("A.md")->relatedPaths(NodeRelationType::Next).isEmpty());
    MarkdownManager fresh(directory.path()); QVERIFY(fresh.scan()); QVERIFY(fresh.loadNode("A.md")->relatedPaths(NodeRelationType::Next).isEmpty());
}
void TreeMdTests::externalRenameKeepsEditsAndRelations() {
    QTemporaryDir directory; MarkdownManager manager(directory.path());
    QVERIFY(manager.createDocument("A.md", "# A")); QVERIFY(manager.createDocument("B.md", "# B"));
    QVERIFY(manager.addRelation("A.md", "B.md", NodeRelationType::Next)); manager.updateContent("B.md", "# B\nunsaved");
    QSignalSpy spy(&manager, &MarkdownManager::documentRenamed);
    QVERIFY(QFile::rename(directory.filePath("B.md"), directory.filePath(QStringLiteral("新名字.md"))));
    QVERIFY(manager.scan()); QCOMPARE(spy.size(), 1); QVERIFY(!manager.loadNode("B.md"));
    QCOMPARE(manager.loadNode(QStringLiteral("新名字.md"))->content(), QString("# B\nunsaved"));
    QCOMPARE(manager.loadNode("A.md")->relatedPaths(NodeRelationType::Next), QStringList{QStringLiteral("新名字.md")});
    QVERIFY(manager.saveDocument(QStringLiteral("新名字.md")));
}
void TreeMdTests::metadataErrorsPreserveState() {
    QTemporaryDir directory; MarkdownManager manager(directory.path()); QString error;
    QVERIFY(manager.createDocument("A.md", "# A")); manager.updateContent("A.md", "# A\nunsaved");
    const QString metadata = directory.filePath(".tree-md-relations.json");
    QVERIFY(MarkdownFileIO::writeFile(metadata, "invalid-json")); QVERIFY(!manager.scan(&error));
    QCOMPARE(manager.loadNode("A.md")->content(), QString("# A\nunsaved"));
    QString disk; QVERIFY(MarkdownFileIO::readFile(metadata, disk)); QCOMPARE(disk, QString("invalid-json"));
}
void TreeMdTests::externalMetadataConflictRollsBack() {
    QTemporaryDir directory; MarkdownManager manager(directory.path()); QString error;
    QVERIFY(manager.createDocument("A.md", "# A")); QVERIFY(manager.createDocument("B.md", "# B"));
    QVERIFY(manager.addRelation("A.md", "B.md", NodeRelationType::Previous));
    QString metadata; const QString path = directory.filePath(".tree-md-relations.json"); QVERIFY(MarkdownFileIO::readFile(path, metadata));
    QVERIFY(MarkdownFileIO::writeFile(path, metadata + "\n"));
    QVERIFY(!manager.renameDocument("B.md", "renamed.md", &error)); QVERIFY(QFileInfo::exists(directory.filePath("B.md"))); QVERIFY(!QFileInfo::exists(directory.filePath("renamed.md")));
    QVERIFY(!manager.deleteDocument("B.md", &error)); QVERIFY(QFileInfo::exists(directory.filePath("B.md")));
    QVERIFY(!manager.removeRelation("A.md", "B.md", &error));
    QCOMPARE(manager.loadNode("A.md")->relatedPaths(NodeRelationType::Previous), QStringList{"B.md"});
    QVERIFY(manager.scan()); QVERIFY(manager.renameDocument("B.md", "renamed.md"));
}
void TreeMdTests::invalidPathsAndWindowsAliases() {
    QTemporaryDir directory; MarkdownManager manager(directory.path()); QString error;
    QVERIFY(!manager.createDocument("../escaped.md", "x", &error)); QVERIFY(!manager.createDocument("A.txt", "x", &error));
    QVERIFY(manager.createDocument("A.md", "# A")); QVERIFY(manager.createDocument("B.md", "# B"));
#ifdef Q_OS_WIN
    QVERIFY(manager.loadNode("a.MD")); QVERIFY(!manager.addRelation("A.md", "a.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("A.md", "B.md", NodeRelationType::Next)); QVERIFY(!manager.addRelation("a.md", "b.md", NodeRelationType::Parent));
#endif
}
void TreeMdTests::watcherRecognizesNestedChanges() {
    QTemporaryDir directory; MarkdownManager manager(directory.path()); QVERIFY(manager.scan()); manager.enableWatching();
    QSignalSpy spy(&manager, &MarkdownManager::documentsChanged);
    QVERIFY(QDir(directory.path()).mkpath(QStringLiteral("新目录")));
    QVERIFY(MarkdownFileIO::writeFile(directory.filePath(QStringLiteral("新目录/外部.md")), "# Outside"));
    QTRY_VERIFY_WITH_TIMEOUT(manager.loadNode(QStringLiteral("新目录/外部.md")) != nullptr, 5000); QVERIFY(spy.size() >= 1);
    QVERIFY(MarkdownFileIO::writeFile(directory.filePath(QStringLiteral("新目录/外部.md")), "# Updated"));
    QTRY_COMPARE_WITH_TIMEOUT(manager.loadNode(QStringLiteral("新目录/外部.md"))->title(), QString("Updated"), 5000);
}
void TreeMdTests::editorPreservesMarkdownAndUndo() {
    MarkdownEditor editor; const QString source = QStringLiteral("# 标题\n\n非\u00a0断行\u00a0空格\n\n**bold** *italic* `code` [link](https://example.test)\n\n```cpp\n# not a heading\n```\n");
    editor.setSource(source); QCOMPARE(editor.source(), source); editor.show(); editor.setFocus();
    QTextCursor cursor = editor.textCursor(); cursor.movePosition(QTextCursor::End); editor.setTextCursor(cursor);
    QTest::keyClicks(&editor, "edited"); QCOMPARE(editor.source(), source + "edited"); editor.undo(); QCOMPARE(editor.source(), source);
    QCOMPARE(editor.document()->firstBlock().blockFormat().headingLevel(), 0);
    QVERIFY(editor.document()->firstBlock().layout()->formats().isEmpty());
}
void TreeMdTests::graphShowsAllBranchesAndCardClick() {
    QTemporaryDir directory; MarkdownManager manager(directory.path());
    QVERIFY(manager.createDocument("current.md", "# Current"));
    for (int i = 0; i < 14; ++i) {
        const QString previous = QString("p%1.md").arg(i), next = QString("n%1.md").arg(i);
        QVERIFY(manager.createDocument(previous, "# Previous")); QVERIFY(manager.createDocument(next, "# Next"));
        QVERIFY(manager.addRelation("current.md", previous, NodeRelationType::Previous)); QVERIFY(manager.addRelation("current.md", next, NodeRelationType::Next));
    }
    QVERIFY(manager.createDocument("alternative.md", "# Alternative")); QVERIFY(manager.createDocument("detail.md", "# Detail"));
    QVERIFY(manager.addRelation("current.md", "alternative.md", NodeRelationType::Parent)); QVERIFY(manager.addRelation("current.md", "detail.md", NodeRelationType::Child));
    KnowledgeGraphWidget graph; graph.resize(1100, 350); graph.show();
    graph.setGraph(&manager, "current.md"); QCOMPARE(graph.visibleNodeCount(), 31);
    QTest::qWait(30);
    QSignalSpy spy(&graph, &KnowledgeGraphWidget::nodeActivated);
    for (auto *item : graph.scene()->items()) if (auto *card = dynamic_cast<KnowledgeNodeWidget *>(item)) {
        const QPoint point = graph.mapFromScene(card->mapToScene(card->boundingRect().center()));
        if (!graph.viewport()->rect().contains(point)) continue;
        QTest::mouseClick(graph.viewport(), Qt::LeftButton, Qt::NoModifier, point); break;
    }
    QCOMPARE(spy.size(), 1);
}
void TreeMdTests::applicationEditingNavigationAndSource() {
    QTemporaryDir directory; MarkdownManager manager(directory.path());
    QVERIFY(manager.createDocument("README.md", "# Start\n\n## Section\n")); QVERIFY(manager.createDocument("B.md", "# B\n"));
    QVERIFY(manager.addRelation("README.md", "B.md", NodeRelationType::Next));
    MainWindow window(directory.path()); window.show();
    window.setSourceMode(true);
    // Unexpected modal errors must fail visibly, rather than block a headless test forever.
    QTimer modalWatchdog;
    connect(&modalWatchdog, &QTimer::timeout, &window, [] {
        if (auto *dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            QTest::qFail(qPrintable(QStringLiteral("Unexpected dialog: ") + dialog->text()), __FILE__, __LINE__);
            dialog->reject();
        }
    });
    modalWatchdog.start(2000);
    window.activateWindow(); QVERIFY(QTest::qWaitForWindowActive(&window));
    auto *editor = window.findChild<MarkdownEditor *>(); auto *graph = window.findChild<KnowledgeGraphWidget *>(); auto *outline = window.findChild<OutlineSidebar *>();
    QVERIFY(editor); QVERIFY(graph); QVERIFY(outline); QCOMPARE(editor->source(), QString("# Start\n\n## Section\n")); QCOMPARE(outline->topLevelItemCount(), 1); QVERIFY(!graph->isVisible());
    QVERIFY(!window.findChild<QAction *>("parentAction")); QVERIFY(!window.findChild<QAction *>("childAction"));
    QTextCursor cursor = editor->textCursor(); cursor.movePosition(QTextCursor::End); editor->setTextCursor(cursor); editor->setFocus();
    QTest::keyClicks(editor, "new text"); QTest::keyClick(editor, Qt::Key_S, Qt::ControlModifier);
    QString disk; QTRY_VERIFY(MarkdownFileIO::readFile(directory.filePath("README.md"), disk) && disk.endsWith("new text"));
    const int position = editor->textCursor().position(); window.setSourceMode(false);
    auto *rendered = window.findChild<MarkdownDocumentView *>(); QTRY_VERIFY(rendered->isVisible());
    QVERIFY(!rendered->toPlainText().contains("# Start")); QVERIFY(rendered->toPlainText().contains("new text"));
    QTest::keyClick(rendered, Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_VERIFY(editor->isVisible()); QCOMPARE(editor->textCursor().position(), position);
    window.setRelationsVisible(true); QVERIFY(graph->isVisible());
    QSignalSpy spy(graph, &KnowledgeGraphWidget::nodeActivated);
    for (auto *item : graph->scene()->items()) if (auto *card = dynamic_cast<KnowledgeNodeWidget *>(item)) {
        if (card->toolTip().endsWith("B.md")) { QTest::mouseClick(graph->viewport(), Qt::LeftButton, Qt::NoModifier, graph->mapFromScene(card->mapToScene(card->boundingRect().center()))); break; }
    }
    QCOMPARE(spy.size(), 1); QTRY_COMPARE(editor->source(), QString("# B\n")); QCOMPARE(outline->topLevelItem(0)->text(0), QString("B"));
    QVERIFY(QFile::rename(directory.filePath("B.md"), directory.filePath(QStringLiteral("外部重命名.md"))));
    QTRY_VERIFY_WITH_TIMEOUT(window.windowTitle().contains(QStringLiteral("外部重命名.md")), 5000);
    QCOMPARE(editor->source(), QString("# B\n"));
    QVERIFY(QFile::remove(directory.filePath(QStringLiteral("外部重命名.md"))));
    QTRY_COMPARE_WITH_TIMEOUT(graph->visibleNodeCount(), 0, 5000);
    QCOMPARE(editor->source(), QString("# B\n"));
    window.close();
}

void TreeMdTests::documentRenderInlineImagesAndSourceIntegrity() {
    QTemporaryDir dir; QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost)); imageServer(server, png(Qt::blue));
    QVERIFY(bytesFile(dir.filePath(QStringLiteral("相对 图.png")), png(Qt::green)));
    const QString local = MarkdownImages::encodedPath(QStringLiteral("相对 图.png"));
    const QString absolute = MarkdownImages::encodedPath(dir.filePath(QStringLiteral("相对 图.png")));
    const QString remote = QString("http://127.0.0.1:%1/image.png").arg(server.serverPort());
    const QString source = "# Title\n\n**bold** and *italic*\n\n- one\n- two\n\n| A | B |\n|---|---|\n| 1 | 2 |\n\n```md\n# literal\n![example](missing.png)\n```\n\n"
        "![relative](<" + local + ">)\n\n![absolute](<" + absolute + ">)\n\n![remote](" + remote + ")\n\n![inline](data:image/png;base64," + QString::fromLatin1(png(Qt::red).toBase64()) + ")\n\n![again][photo]\n\n[photo]: <" + local + ">\n";
    MarkdownEditor editor; editor.setSource(source); MarkdownDocumentView view; view.resize(700, 600); view.show();
    QSignalSpy settled(&view, &MarkdownDocumentView::imagesSettled); view.setMarkdownSource(dir.filePath("doc.md"), editor.source());
    QTRY_COMPARE_WITH_TIMEOUT(view.loadedImageCount(), 4, 5000); QVERIFY(view.imageErrors().isEmpty()); QCOMPARE(settled.size(), 1);
    QCOMPARE(editor.source(), source); QVERIFY(!editor.document()->isModified());
    QVERIFY(view.toPlainText().contains("bold and italic")); QVERIFY(!view.toPlainText().contains("**bold**")); QVERIFY(view.toPlainText().contains("# literal"));
    QVERIFY(view.toPlainText().contains("![example](missing.png)")); QCOMPARE(view.document()->firstBlock().blockFormat().headingLevel(), 1);
    bool table = false, list = false, bold = false; int images = 0;
    for (auto *frame : view.document()->rootFrame()->childFrames()) if (dynamic_cast<QTextTable *>(frame)) table = true;
    for (auto block = view.document()->begin(); block.isValid(); block = block.next()) {
        if (block.textList()) list = true;
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment(); if (!fragment.isValid()) continue;
            if (fragment.text().contains("bold") && fragment.charFormat().fontWeight() >= QFont::Bold) bold = true;
            if (!fragment.charFormat().isImageFormat()) continue;
            ++images; const auto format = fragment.charFormat().toImageFormat();
            QVERIFY(format.width() > 0); QVERIFY(format.width() <= view.viewport()->width() - 64 + 1);
            const QImage image = qvariant_cast<QImage>(view.document()->resource(QTextDocument::ImageResource, QUrl(format.name())));
            QCOMPARE(image.size(), QSize(32, 24));
        }
    }
    QCOMPARE(images, 5); QVERIFY(table); QVERIFY(list); QVERIFY(bold);
    view.resize(180, 300); QCoreApplication::processEvents(); QCOMPARE(editor.source(), source);
}
void TreeMdTests::documentRenderIgnoresStaleNetworkResults() {
    QTemporaryDir dir; QVERIFY(bytesFile(dir.filePath("current.png"), png(Qt::green)));
    QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost)); bool connected = false;
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto *socket = server.nextPendingConnection(); connected = true;
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        QTimer::singleShot(180, &server, [socket = QPointer<QTcpSocket>(socket)] {
            if (!socket || socket->state() != QAbstractSocket::ConnectedState) return;
            const QByteArray bytes = png(Qt::blue);
            socket->write("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(bytes.size()) + "\r\n\r\n" + bytes); socket->disconnectFromHost();
        });
    });
    MarkdownDocumentView view; view.show();
    view.setMarkdownSource(dir.filePath("old.md"), QString("# Old\n![old](http://127.0.0.1:%1/slow.png)").arg(server.serverPort()));
    QTRY_VERIFY(connected); view.setMarkdownSource(dir.filePath("new.md"), "# Current\n![current](current.png)");
    QTRY_COMPARE(view.loadedImageCount(), 1); QTest::qWait(220);
    QCOMPARE(view.loadedImageCount(), 1); QVERIFY(view.imageErrors().isEmpty()); QVERIFY(view.toPlainText().startsWith("Current"));
    const QImage image = qvariant_cast<QImage>(view.document()->resource(QTextDocument::ImageResource, QUrl("tree-md-image:/1")));
    QCOMPARE(image.pixelColor(0, 0), QColor(Qt::green));
}
void TreeMdTests::fileMenuLayoutAndModeSwitch() {
    QTemporaryDir dir; MarkdownManager manager(dir.filePath("md_data"));
    const QString source = "# Title\n\n**bold**\n\n## Section\nParagraph\n"; QVERIFY(manager.createDocument("doc.md", source));
    MainWindow window(manager.rootPath()); window.show(); QCoreApplication::processEvents();
    auto *toolbar = window.findChild<QToolBar *>("mainToolbar"); auto *menu = window.findChild<QMenu *>("fileMenu"); auto *spring = window.findChild<QWidget *>("headerSpring");
    auto *editor = window.findChild<MarkdownEditor *>(); auto *view = window.findChild<MarkdownDocumentView *>(); QVERIFY(toolbar); QVERIFY(menu); QVERIFY(spring); QVERIFY(editor); QVERIFY(view);
    QVERIFY(spring->width() > 500); QCOMPARE(toolbar->actions().size(), 6);
    auto *frame = window.findChild<QFrame *>("documentModeFrame"); QVERIFY(frame);
    QCOMPARE(frame->findChildren<QToolButton *>().size(), 2);
    const QStringList names = {"newAction", "importAction", "saveAction", "renameAction", "deleteAction", "refreshAction"};
    for (const auto &name : names) { auto *a = window.findChild<QAction *>(name); QVERIFY(a); QVERIFY(menu->actions().contains(a)); QVERIFY(!toolbar->actions().contains(a)); }
    QCOMPARE(menu->actions()[4]->isSeparator(), true);
    auto *copy = window.findChild<QMenu *>("saveCopyMenu"); auto *imageMode = window.findChild<QAction *>("imageSettingsAction"); QVERIFY(copy); QVERIFY(imageMode); QCOMPARE(copy->actions().size(), 2);
    QVERIFY(imageMode->text().contains(QStringLiteral("图片保存方式"))); QCOMPARE(MarkdownSettingsStore::current().imageMode, quint8(0)); QVERIFY(!imageMode->menu());
    QVERIFY(!window.findChild<QAction *>("insertImageAction")); QVERIFY(!window.findChild<QWidget *>("imagePreview")); QVERIFY(!window.findChild<QWidget *>("sourceView"));
    QVERIFY(view->isVisible()); QVERIFY(!editor->isVisible()); QCOMPARE(editor->source(), source);
    window.findChild<QAction *>("sourceModeAction")->trigger(); QVERIFY(editor->isVisible()); QVERIFY(!view->isVisible());
    QVERIFY(!window.findChild<QAction *>("documentModeAction")->isChecked());
    QVERIFY(editor->document()->firstBlock().layout()->formats().isEmpty());
    QTextCursor cursor = editor->textCursor(); cursor.movePosition(QTextCursor::End); editor->setTextCursor(cursor); QTest::keyClicks(editor, "**unsaved**");
    const int position = editor->textCursor().position(); const QString edited = editor->source();
    for (int i = 0; i < 4; ++i) { window.setSourceMode(false); QVERIFY(view->toPlainText().contains("unsaved")); window.setSourceMode(true); QCOMPARE(editor->source(), edited); QCOMPARE(editor->textCursor().position(), position); }
    editor->undo(); QCOMPARE(editor->source(), source); window.close();
}

void TreeMdTests::renderedEditingPreservesSource() {
    const QString source = QStringLiteral("# 标题\n\nprefix **bold** suffix &amp; 非\u00a0断行\n\n[link](https://example.test/a?b=1 \"title\") and `inline`\n\n```cpp\nint value = 1; // original\n```\n\n<span>HTML</span>\n\n![ref][photo]\n\n[photo]: ./unchanged.png \"original title\"\n");
    MarkdownDocumentView view; view.resize(750, 600); view.show(); view.setMarkdownSource("doc.md", source); QVERIFY(!view.isReadOnly());
    QSignalSpy spy(&view, &MarkdownDocumentView::sourceEdited);
    QTextCursor cursor = view.document()->find("bold"); QVERIFY(!cursor.isNull()); cursor.setPosition(cursor.selectionEnd()); cursor.insertText("er");
    QString expected = source; expected.replace("**bold**", "**bolder**"); QCOMPARE(view.markdownSource(), expected); QCOMPARE(spy.size(), 1);
    QTest::qWait(500); QCOMPARE(view.markdownSource(), expected); QVERIFY(view.toPlainText().contains("bolder"));
    cursor = view.document()->find("link"); QVERIFY(!cursor.isNull()); cursor.setPosition(cursor.selectionStart() + 2); cursor.insertText("X");
    expected.replace("[link]", "[liXnk]"); QCOMPARE(view.markdownSource(), expected);
    cursor = view.document()->find("inline"); QVERIFY(!cursor.isNull()); cursor.setPosition(cursor.selectionEnd()); cursor.insertText("!");
    expected.replace("`inline`", "`inline!`"); QCOMPARE(view.markdownSource(), expected);
    cursor = view.document()->find("value"); QVERIFY(!cursor.isNull()); cursor.insertText("answer");
    expected.replace("int value", "int answer"); QCOMPARE(view.markdownSource(), expected);
    cursor = view.document()->find("HTML"); QVERIFY(!cursor.isNull()); cursor.setPosition(cursor.selectionEnd()); cursor.insertText(" edited");
    expected.replace("HTML</span>", "HTML edited</span>"); QCOMPARE(view.markdownSource(), expected);
    QVERIFY(view.markdownSource().contains(QChar(0xa0))); QVERIFY(view.markdownSource().endsWith("[photo]: ./unchanged.png \"original title\"\n"));
    MarkdownSourceMap map; map.rebuild("a **bold** z", "a bold z"); int position;
    QCOMPARE(map.edited(0, 3, {}, &position), QString("**old** z"));
    map.rebuild("a **bold** z", "a bold z"); QCOMPARE(map.edited(2, 4, {}, &position), QString("a  z"));
    map.rebuild("# A\n\n## B", "A\nB"); QCOMPARE(map.edited(1, 1, {}, &position), QString("# AB"));
    map.rebuild("a **bold** z", "a bold z"); QCOMPARE(map.edited(2, 4, "new", &position), QString("a **new** z"));
    view.selectAll(); view.textCursor().removeSelectedText(); QCOMPARE(view.markdownSource(), QString());
    view.textCursor().insertText(QStringLiteral("重新开始")); QCOMPARE(view.markdownSource(), QStringLiteral("重新开始"));
}
void TreeMdTests::renderedListsTablesAndUndo() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    const QString source = "# Title\n\n- one\n- two\n\n| A | B |\n|---|---|\n| cell | value |\n\n```cpp\nint keep = 2;\n```\n";
    QVERIFY(manager.createDocument("doc.md", source)); MainWindow window(dir.path()); window.show(); window.activateWindow(); QVERIFY(QTest::qWaitForWindowActive(&window));
    auto *view = window.findChild<MarkdownDocumentView *>(); auto *editor = window.findChild<MarkdownEditor *>();
    auto cursor = view->document()->find("cell"); QVERIFY(!cursor.isNull()); cursor.setPosition(cursor.selectionEnd()); view->setTextCursor(cursor); view->setFocus();
    QTest::keyClicks(view, "x"); QString changed = source; changed.replace("cell", "cellx"); QCOMPARE(editor->source(), changed);
    QTest::keyClick(view, Qt::Key_S, Qt::ControlModifier); QString disk;
    QTRY_VERIFY(MarkdownFileIO::readFile(dir.filePath("doc.md"), disk) && disk == changed);
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); QTRY_COMPARE(editor->source(), source);
    QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier); QTRY_COMPARE(editor->source(), changed);
    QTRY_VERIFY(view->toPlainText().contains("cellx"));
    cursor = view->document()->find("one"); cursor.setPosition(cursor.selectionEnd()); view->setTextCursor(cursor); view->setFocus();
    QTest::keyClick(view, Qt::Key_Return); QTest::keyClicks(view, "next");
    changed.replace("- one\n- two", "- one\n- next\n- two"); QCOMPARE(editor->source(), changed);
    window.setSourceMode(true); QCOMPARE(editor->source(), changed); window.setSourceMode(false); QVERIFY(view->toPlainText().contains("next"));
    // Plain paste in the rendered page follows the same source edit path.
    cursor = view->document()->find("next"); view->setTextCursor(cursor); QApplication::clipboard()->setText("pasted");
    QTest::keyClick(view, Qt::Key_V, Qt::ControlModifier); changed.replace("- next", "- pasted"); QCOMPARE(editor->source(), changed);
    QTest::keyClick(view, Qt::Key_S, Qt::ControlModifier); QTRY_VERIFY(MarkdownFileIO::readFile(dir.filePath("doc.md"), disk) && disk == changed); window.close();
}
void TreeMdTests::renderedParagraphsCodeAndDeletion() {
    MarkdownDocumentView view; view.resize(700, 540); view.show(); view.setMarkdownSource("doc.md", "# Title\n\nParagraph\n\n```cpp\nint value;\n```\n");
    auto cursor = view.document()->find("Paragraph"); cursor.setPosition(cursor.selectionEnd()); view.setTextCursor(cursor); view.setFocus();
    QTest::keyClick(&view, Qt::Key_Return); QTest::qWait(500); QVERIFY(view.textCursor().block().text().isEmpty());
    QTest::keyClicks(&view, "New"); QVERIFY(view.markdownSource().contains("Paragraph\n\nNew"));
    QTest::qWait(500); cursor = view.document()->find("New"); QVERIFY(!cursor.isNull()); QCOMPARE(cursor.block().text(), QString("New"));
    cursor.setPosition(cursor.selectionStart()); view.setTextCursor(cursor); QTest::keyClick(&view, Qt::Key_Backspace);
    QVERIFY(view.markdownSource().contains("ParagraphNew")); QVERIFY(view.markdownSource().contains("```cpp\nint value;\n```"));
    cursor = view.document()->find("int value;"); cursor.setPosition(cursor.selectionStart() + 4); view.setTextCursor(cursor);
    QTest::keyClick(&view, Qt::Key_Return); QTest::keyClicks(&view, "long "); QVERIFY(view.markdownSource().contains("```cpp\nint \nlong value;\n```"));
    view.setMarkdownSource("doc.md", "one\ntwo\n"); cursor = view.document()->find("one two"); QVERIFY(!cursor.isNull());
    cursor.setPosition(cursor.selectionStart() + 3); cursor.setPosition(cursor.position() + 1, QTextCursor::KeepAnchor); cursor.removeSelectedText(); QCOMPARE(view.markdownSource(), QString("onetwo\n"));
    view.setMarkdownSource("doc.md", "before ![photo][ref] after\n\n[ref]: unchanged.png\n");
    cursor = view.document()->find(QString(QChar::ObjectReplacementCharacter)); QVERIFY(!cursor.isNull()); cursor.removeSelectedText();
    QCOMPARE(view.markdownSource(), QString("before  after\n\n[ref]: unchanged.png\n"));
}
void TreeMdTests::headingsRequireSpaceWhenRendered() {
    MarkdownDocumentView view; view.resize(700, 500); view.show(); view.setFocus();
    for (int level = 1; level <= 6; ++level) {
        const QString hashes(level, '#'); view.setMarkdownSource("headings.md", QString());
        QTest::keyClicks(&view, hashes); QTest::qWait(550);
        QCOMPARE(view.toPlainText(), hashes); QCOMPARE(view.markdownSource(), hashes);
        QCOMPARE(view.textCursor().blockFormat().headingLevel(), 0); QVERIFY(MarkdownParser::outline(view.markdownSource()).isEmpty());
        QTest::keyClick(&view, Qt::Key_Space); QTRY_COMPARE(view.textCursor().blockFormat().headingLevel(), level);
        QCOMPARE(view.markdownSource(), hashes + " "); QCOMPARE(view.toPlainText(), QString());
        QTest::keyClicks(&view, "Title"); QCOMPARE(view.markdownSource(), hashes + " Title");
        QCOMPARE(view.currentCharFormat().fontPointSize(), MarkdownTypography::headingPointSize(level, 100));
    }
    for (const QString &source : {QString("#word"), QString("> #"), QString("- ##"), QString("    #"),
        QString("```text\n#\n- ```\n##\n```"), QString("> ```text\n> #\n> ```"),
        QString("- ```text\n  #\n  ```"), QString("<pre>\n#\n</pre>")}) {
        view.setMarkdownSource("headings.md", source); QCOMPARE(view.markdownSource(), source);
        for (auto block = view.document()->begin(); block.isValid(); block = block.next()) QCOMPARE(block.blockFormat().headingLevel(), 0);
        QVERIFY(view.toPlainText().contains('#'));
    }
}
void TreeMdTests::headingBackspaceRestoresMarker() {
    for (const QString &prefix : {QString(), QString("Before\n\n"), QString("  "), QString("> "), QString("- "), QString("Before\n\n> ")}) {
        const QString source = prefix + "### **Title**\n\nAfter";
        MarkdownDocumentView view; view.resize(700, 500); view.show(); view.setMarkdownSource("headings.md", source); view.setFocus();
        auto cursor = view.document()->find("Title"); QVERIFY(!cursor.isNull()); cursor.setPosition(cursor.selectionStart()); view.setTextCursor(cursor);
        QCOMPARE(view.textCursor().blockFormat().headingLevel(), 3); QTest::keyClick(&view, Qt::Key_Backspace);
        QCOMPARE(view.markdownSource(), prefix + "###**Title**\n\nAfter"); QCOMPARE(view.textCursor().blockFormat().headingLevel(), 0);
        QVERIFY(view.toPlainText().contains("###Title")); QVERIFY(view.toPlainText().endsWith("After"));
        QTest::keyClick(&view, Qt::Key_Space); QTRY_COMPARE(view.textCursor().blockFormat().headingLevel(), 3);
        QCOMPARE(view.markdownSource(), source);
        cursor = view.document()->find("Title"); cursor.setPosition(cursor.selectionStart() + 2); view.setTextCursor(cursor);
        QTest::keyClick(&view, Qt::Key_Backspace); QCOMPARE(view.markdownSource(), prefix + "### **Ttle**\n\nAfter");
    }
}
void TreeMdTests::emptyHeadingsKeepEditablePositions() {
    for (const QString &source : {QString("# "), QString("Before\n\n## \n\nAfter"), QString("# \n\n## \n\nAfter"), QString("> ## "), QString("- ## ")}) {
        MarkdownDocumentView view; view.resize(700, 500); view.show(); view.setMarkdownSource("headings.md", source); view.setFocus();
        QList<QTextBlock> headings;
        for (auto block = view.document()->begin(); block.isValid(); block = block.next()) if (block.blockFormat().headingLevel()) headings.append(block);
        const auto markers = MarkdownParser::headingMarkers(source); QCOMPARE(headings.size(), markers.size()); QVERIFY(!headings.isEmpty());
        const auto target = markers.last(); view.setTextCursor(QTextCursor(headings.last()));
        QTest::keyClick(&view, Qt::Key_Backspace); QString raw = source; raw.remove(target.markerStart + target.level, target.contentStart - target.markerStart - target.level);
        QCOMPARE(view.markdownSource(), raw); QVERIFY(view.toPlainText().contains(QString(target.level, '#')));
        QTest::keyClick(&view, Qt::Key_Space); QTRY_COMPARE(view.textCursor().blockFormat().headingLevel(), target.level);
        QCOMPARE(view.markdownSource(), source); QTest::keyClicks(&view, "Title");
        QString expected = source; expected.insert(target.contentStart, "Title"); QCOMPARE(view.markdownSource(), expected);
    }
}
void TreeMdTests::headingEditingUndoAndParagraphIntegrity() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    const QString source = "Before\n\n## Title\n\nAfter"; QVERIFY(manager.createDocument("doc.md", source));
    MainWindow window(dir.path()); window.show(); window.activateWindow(); QVERIFY(QTest::qWaitForWindowActive(&window));
    auto *view = window.findChild<MarkdownDocumentView *>(); auto *editor = window.findChild<MarkdownEditor *>();
    auto cursor = view->document()->find("Title"); cursor.setPosition(cursor.selectionStart()); view->setTextCursor(cursor); view->setFocus();
    QTest::keyClick(view, Qt::Key_Backspace); const QString changed = "Before\n\n##Title\n\nAfter"; QCOMPARE(editor->source(), changed);
    QCOMPARE(view->textCursor().blockFormat().headingLevel(), 0);
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); QTRY_COMPARE(editor->source(), source);
    QCOMPARE(view->document()->find("Title").block().blockFormat().headingLevel(), 2);
    QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier); QTRY_COMPARE(editor->source(), changed);
    QCOMPARE(view->document()->find("Title").block().blockFormat().headingLevel(), 0);
    QTest::keyClick(view, Qt::Key_S, Qt::ControlModifier); QString disk;
    QTRY_VERIFY(MarkdownFileIO::readFile(dir.filePath("doc.md"), disk) && disk == changed);
    window.setSourceMode(true); QCOMPARE(editor->source(), changed); window.setSourceMode(false); QCOMPARE(view->markdownSource(), changed);
    window.close();
}
void TreeMdTests::markdownStylesAndOutlinePosition() {
    QString source = "# Title\n\nA `inline` **bold** *italic* ~~strike~~ [link](https://example.test).\n\n```cpp\nint count = 42;\nreturn count;\n```\n\n> Quote\n\n- [x] done\n- [ ] pending\n\n| A | B |\n|:---|---:|\n| 1 | 2 |\n\n";
    for (int i = 0; i < 50; ++i) source += QString("Paragraph before %1.\n\n").arg(i);
    const int target = source.size(); source += "## Target\n\n";
    for (int i = 0; i < 50; ++i) source += QString("Paragraph after %1.\n\n").arg(i);
    MarkdownDocumentView view; view.resize(720, 540); view.show(); view.setMarkdownSource("doc.md", source); QCoreApplication::processEvents();
    const QColor base = view.palette().color(QPalette::Base);
    QVERIFY(MarkdownTypography::codeBackground(true).lightness() > base.lightness());
    QVERIFY(MarkdownTypography::codeBackground().lightness() > base.lightness());
    bool inlineCode = false, strike = false, quote = false, task = false; int codeLines = 0;
    for (auto block = view.document()->begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().nonBreakableLines()) { ++codeLines; QCOMPARE(block.blockFormat().background().color(), MarkdownTypography::codeBackground()); }
        if (block.text() == "Quote") { quote = true; QCOMPARE(block.blockFormat().background().color(), QColor("#27312b")); }
        if (block.blockFormat().marker() == QTextBlockFormat::MarkerType::Checked) task = true;
        for (auto it = block.begin(); !it.atEnd(); ++it) { const auto fragment = it.fragment(); if (!fragment.isValid()) continue;
            if (fragment.text().contains("inline")) { inlineCode = true; QCOMPARE(fragment.charFormat().background().color(), MarkdownTypography::codeBackground(true)); }
            if (fragment.text().contains("strike") && fragment.charFormat().fontStrikeOut()) strike = true;
        }
    }
    QVERIFY(inlineCode); QVERIFY(strike); QVERIFY(quote); QVERIFY(task); QCOMPARE(codeLines, 2);
    view.goToSourcePosition(target); QCOMPARE(view.textCursor().block().text(), QString("Target"));
    QVERIFY(qAbs(view.cursorRect().top() - view.viewport()->height() * 0.28) < 25);
    const int finalHeading = source.size(); source += "## Last heading\n";
    view.setMarkdownSource("doc.md", source); view.goToSourcePosition(finalHeading);
    QCOMPARE(view.textCursor().block().text(), QString("Last heading"));
    QVERIFY(view.cursorRect().top() >= 0 && view.cursorRect().bottom() <= view.viewport()->height());
    QCOMPARE(view.document()->rootFrame()->frameFormat().bottomMargin(), qreal(24));
    MarkdownEditor editor; editor.resize(720, 540); editor.show(); editor.setSource(source); editor.goToPosition(target); QCoreApplication::processEvents();
    QVERIFY(qAbs(editor.cursorRect().top() - editor.viewport()->height() * 0.28) < 25);
    QVERIFY(!treeMdIcon().isNull()); QCOMPARE(qApp->windowIcon().cacheKey(), treeMdIcon().cacheKey());
    const QImage icon = treeMdIcon().pixmap(128, 128).toImage(); QVERIFY(icon.pixelColor(0, 0).alpha() < 10); QCOMPARE(icon.pixelColor(64, 20), QColor(Qt::white));
}

void TreeMdTests::imagesHaveNoExcessSpacing() {
    QTemporaryDir dir; QImage image(320, 240, QImage::Format_ARGB32); image.fill(Qt::green); QVERIFY(image.save(dir.filePath("image.png")));
    MarkdownDocumentView view; view.resize(700, 500); view.show();
    const QString source = "# Title\n\n![image](image.png)\n\nAfter image\n"; QSignalSpy edits(&view, &MarkdownDocumentView::sourceEdited);
    view.setMarkdownSource(dir.filePath("doc.md"), source); QTRY_COMPARE(view.loadedImageCount(), 1);
    auto verify = [&] {
        QTextBlock picture;
        for (auto block = view.document()->begin(); block.isValid(); block = block.next()) if (block.text().contains(QChar::ObjectReplacementCharacter)) picture = block;
        QVERIFY(picture.isValid()); QCOMPARE(picture.blockFormat().lineHeight(), qreal(100));
        const QRectF bounds = view.document()->documentLayout()->blockBoundingRect(picture);
        QVERIFY2(bounds.height() <= image.height() + 25, qPrintable(QString::number(bounds.height())));
        const QRectF following = view.document()->documentLayout()->blockBoundingRect(picture.next());
        QVERIFY(following.top() - bounds.bottom() <= 20);
        const QRectF last = view.document()->documentLayout()->blockBoundingRect(view.document()->lastBlock());
        const qreal normalSpacing = 32 + QFontMetricsF(view.font()).height() * 0.6;
        QVERIFY(view.document()->size().height() - last.bottom() <= normalSpacing);
    };
    verify(); view.resize(700, 1000); QCoreApplication::processEvents(); verify();
    view.setZoomPercent(200); verify(); QCOMPARE(view.markdownSource(), source); QVERIFY(edits.isEmpty()); QCOMPARE(view.loadedImageCount(), 1);
    view.setMarkdownSource(dir.filePath("doc.md"), "# Title\n\n![image](image.png)\n");
    QCOMPARE(view.loadedImageCount(), 1);
    const auto last = view.document()->lastBlock(); QVERIFY(last.text().contains(QChar::ObjectReplacementCharacter));
    const QRectF lastImage = view.document()->documentLayout()->blockBoundingRect(last);
    QVERIFY(view.document()->size().height() - lastImage.bottom() <= 32);
}
void TreeMdTests::fontZoomBoundsAndSourceIntegrity() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    const QString source = "# Title\n\nBody **bold** and `inline`.\n\n```cpp\nint value = 1;\n```\n";
    QVERIFY(manager.createDocument("README.md", source)); QVERIFY(manager.createDocument("folder/doc.md", "# Nested\n"));
    MainWindow window(dir.path()); window.show(); window.activateWindow(); QVERIFY(QTest::qWaitForWindowActive(&window));
    auto *view = window.findChild<MarkdownDocumentView *>(); auto *editor = window.findChild<MarkdownEditor *>(); auto *label = window.findChild<QLabel *>("editorZoom");
    QCOMPARE(view->zoomPercent(), 100); QCOMPARE(editor->zoomPercent(), 100); QCOMPARE(label->text(), QString("100%"));
    QVERIFY(QFontDatabase::families().contains("Open Sans")); QVERIFY(QFontDatabase::families().contains("DejaVu Sans Mono"));
    QCOMPARE(QFontInfo(view->font()).family(), QString("Open Sans")); QCOMPARE(QFontInfo(editor->font()).family(), QString("DejaVu Sans Mono"));
    auto wheel = [](QTextEdit *target, int delta, Qt::KeyboardModifiers modifiers = Qt::ControlModifier) {
        const QPoint point(80, 80); QWheelEvent event(point, target->viewport()->mapToGlobal(point), {}, QPoint(0, delta), Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
        QApplication::sendEvent(target->viewport(), &event);
    };
    QSignalSpy edits(view, &MarkdownDocumentView::sourceEdited); const qreal headingBase = view->document()->firstBlock().begin().fragment().charFormat().fontPointSize();
    wheel(view, 120); QCOMPARE(view->zoomPercent(), 110); QCOMPARE(editor->zoomPercent(), 110); QCOMPARE(label->text(), QString("110%"));
    for (int i = 0; i < 20; ++i) wheel(view, 120);
    QCOMPARE(view->zoomPercent(), 200); QCOMPARE(editor->zoomPercent(), 200);
    QCOMPARE(view->document()->firstBlock().begin().fragment().charFormat().fontPointSize(), headingBase * 2);
    window.setSourceMode(true); editor->setFocus(); for (int i = 0; i < 30; ++i) wheel(editor, -120);
    QCOMPARE(editor->zoomPercent(), 50); QCOMPARE(view->zoomPercent(), 50); QCOMPARE(label->text(), QString("50%"));
    wheel(editor, 120, Qt::NoModifier); QCOMPARE(editor->zoomPercent(), 50);
    QCOMPARE(editor->source(), source); QCOMPARE(view->markdownSource(), source); QVERIFY(edits.isEmpty()); QVERIFY(!editor->document()->isModified()); QVERIFY(!window.windowTitle().contains(" *"));
    window.findChild<QAction *>("resetZoomAction")->trigger(); QCOMPARE(view->zoomPercent(), 100); QCOMPARE(editor->zoomPercent(), 100);
    auto cursor = editor->textCursor(); cursor.movePosition(QTextCursor::End); editor->setTextCursor(cursor); QTest::keyClicks(editor, "edited");
    wheel(editor, 120); editor->undo(); QCOMPARE(editor->source(), source);
    window.setSourceMode(false); QCOMPARE(view->zoomPercent(), 110); QCOMPARE(label->text(), QString("110%"));
    const auto *tree = window.findChild<QTreeWidget *>("documentTree"); QVERIFY(tree);
    QTreeWidgetItemIterator iterator(const_cast<QTreeWidget *>(tree)); while (*iterator) { QVERIFY((*iterator)->icon(0).isNull()); ++iterator; }
    QVERIFY(window.findChild<QToolButton *>("fileButton")->icon().isNull()); window.close();
}

void TreeMdTests::knowledgeIndexTraversesCyclesAndIncomingReferences() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    for (const auto &path : {"A.md", "B.md", "C.md", "D.md", "E.md", "outside.md"}) QVERIFY(manager.createDocument(path, "# " + QString(path)));
    QVERIFY(manager.addRelation("A.md", "B.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("B.md", "C.md", NodeRelationType::Child));
    QVERIFY(manager.addRelation("C.md", "D.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("D.md", "A.md", NodeRelationType::Previous));
    QVERIFY(manager.addRelation("A.md", "C.md", NodeRelationType::Parent));
    QVERIFY(manager.addRelation("E.md", "D.md", NodeRelationType::Parent));
    const auto index = KnowledgeIndex::build(manager, "C.md");
    QCOMPARE(index.paths.size(), 5); QCOMPARE(index.edges.size(), 5); QCOMPARE(index.root, QString("C.md"));
    QVERIFY(index.members.contains("C.md")); QVERIFY(index.paths.contains("E.md")); QVERIFY(!index.paths.contains("outside.md"));
    QCOMPARE(index.members.size(), index.paths.size()); QVERIFY(KnowledgeIndex::build(manager, "missing.md").paths.isEmpty());
    QString previous = "E.md";
    for (int i = 0; i < 24; ++i) {
        const QString next = QString("chain/%1.md").arg(i); QVERIFY(manager.createDocument(next, "# chain"));
        QVERIFY(manager.addRelation(previous, next, NodeRelationType::Next)); previous = next;
    }
    const auto limited = KnowledgeIndex::build(manager, "A.md"); QVERIFY(!limited.paths.contains(previous));
    for (int depth : limited.depths) QVERIFY(depth <= 5);
    const auto longIndex = KnowledgeIndex::build(manager, "A.md", 32); QCOMPARE(longIndex.paths.size(), 29); QVERIFY(longIndex.depths.value(previous) >= 24);
}
void TreeMdTests::treeRelationReplacementIsAtomic() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    QVERIFY(manager.createDocument("A.md", "# A\n")); QVERIFY(manager.createDocument("B.md", "# B\n"));
    QVERIFY(manager.addRelation("A.md", "B.md", NodeRelationType::Previous));
    QVERIFY(manager.addRelation("B.md", "A.md", NodeRelationType::Next));
    QVERIFY(manager.setRelation("B.md", "A.md", NodeRelationType::Parent));
    QVERIFY(manager.loadNode("A.md")->relatedPaths(NodeRelationType::Previous).isEmpty());
    QCOMPARE(manager.loadNode("B.md")->relatedPaths(NodeRelationType::Parent), QStringList{"A.md"});
    QVERIFY(manager.setRelation("B.md", "A.md", NodeRelationType::Child));
    QVERIFY(manager.loadNode("B.md")->relatedPaths(NodeRelationType::Parent).isEmpty());
    QString error; QVERIFY(!manager.setRelation("A.md", "a.MD", NodeRelationType::Next, &error));
    QVERIFY(!manager.setRelation("A.md", "missing.md", NodeRelationType::Next));
    MarkdownManager reopened(dir.path()); QVERIFY(reopened.scan()); QCOMPARE(reopened.loadNode("B.md")->relatedPaths(NodeRelationType::Child), QStringList{"A.md"});
    QVERIFY(MarkdownFileIO::writeFile(dir.filePath(".tree-md-relations.json"), "invalid external data"));
    QVERIFY(!manager.setRelation("B.md", "A.md", NodeRelationType::Next, &error)); QVERIFY(!error.isEmpty());
    QCOMPARE(manager.loadNode("B.md")->relatedPaths(NodeRelationType::Child), QStringList{"A.md"});
    QVERIFY(manager.loadNode("B.md")->relatedPaths(NodeRelationType::Next).isEmpty());
    QString disk; QVERIFY(MarkdownFileIO::readFile(dir.filePath("A.md"), disk)); QCOMPARE(disk, QString("# A\n"));
}
void TreeMdTests::knowledgeTreeDropPreviewAndPersistence() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    for (const auto &path : {"A.md", "notes/B.md", "notes/C.md", "upper.md", "lower.md"}) QVERIFY(manager.createDocument(path, "# " + QString(path)));
    QVERIFY(manager.addRelation("notes/B.md", "notes/C.md", NodeRelationType::Next));
    RelationDialog dialog(&manager, "A.md"); dialog.show(); QTest::qWait(50);
    auto *canvas = dialog.findChild<KnowledgeTreeCanvas *>(); QVERIFY(canvas); QCOMPARE(canvas->index().paths, QStringList{"A.md"});
    QMimeData mime; mime.setData(KnowledgeTreeCanvas::mimeType(), "notes/B.md");
    const auto root = canvas->nodeRect("A.md");
    const QPointF positions[] = {{root.left() - 70, root.center().y()}, {root.right() + 70, root.center().y()}, {root.center().x(), root.top() - 70}, {root.center().x(), root.bottom() + 70}};
    for (int i = 0; i < 4; ++i) {
        const QPoint point = canvas->mapFromScene(positions[i]);
        QDragEnterEvent enter(point, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier); QApplication::sendEvent(canvas->viewport(), &enter); QVERIFY(enter.isAccepted());
        QDragMoveEvent move(point, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier); QApplication::sendEvent(canvas->viewport(), &move); QVERIFY(move.isAccepted());
        const auto preview = canvas->preview(); QVERIFY(preview.valid); QCOMPARE(preview.source, QString("A.md")); QCOMPARE(preview.target, QString("notes/B.md")); QCOMPARE(static_cast<int>(preview.type), i);
        QVERIFY(manager.loadNode("A.md")->relatedPaths(static_cast<NodeRelationType>(i)).isEmpty());
        QDragLeaveEvent leave; QApplication::sendEvent(canvas->viewport(), &leave); QVERIFY(!canvas->preview().valid);
    }
    const QPoint point = canvas->mapFromScene(positions[1]);
    QDragEnterEvent enter(point, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier); QApplication::sendEvent(canvas->viewport(), &enter);
    QDropEvent drop(point, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier); QApplication::sendEvent(canvas->viewport(), &drop); QVERIFY(drop.isAccepted());
    QTRY_COMPARE(canvas->index().paths.size(), 3); QCOMPARE(manager.loadNode("A.md")->relatedPaths(NodeRelationType::Next), QStringList{"notes/B.md"});
    auto *tree = dialog.findChild<QTreeWidget *>("availableDocumentTree"); QVERIFY(tree);
    QStringList available; QTreeWidgetItemIterator it(tree); while (*it) { const auto path = (*it)->data(0, Qt::UserRole).toString(); if (!path.isEmpty()) available.append(path); ++it; }
    QVERIFY(!available.contains("notes/B.md")); QVERIFY(!available.contains("notes/C.md")); QCOMPARE(available.size(), 2);
    QDragEnterEvent duplicate(point, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier); QApplication::sendEvent(canvas->viewport(), &duplicate); QVERIFY(!duplicate.isAccepted());
    MarkdownManager reopened(dir.path()); QVERIFY(reopened.scan()); QCOMPARE(KnowledgeIndex::build(reopened, "notes/C.md").paths.size(), 3);
    dialog.close();
}
void TreeMdTests::knowledgeTreeCardConnectionsAndRemoval() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    for (const auto &path : {"A.md", "B.md", "C.md", "D.md"}) QVERIFY(manager.createDocument(path, "# " + QString(path)));
    QVERIFY(manager.addRelation("A.md", "B.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("A.md", "C.md", NodeRelationType::Child));
    QVERIFY(manager.addRelation("A.md", "D.md", NodeRelationType::Parent));
    RelationDialog dialog(&manager, "A.md"); dialog.show(); QTest::qWait(50); auto *canvas = dialog.findChild<KnowledgeTreeCanvas *>(); QVERIFY(canvas);
    auto move = [&](QPoint point) {
        QMouseEvent event(QEvent::MouseMove, point, canvas->viewport()->mapToGlobal(point), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(canvas->viewport(), &event);
    };
    auto start = [&] { const auto rect = canvas->nodeRect("B.md"); return canvas->mapFromScene(QPointF(rect.right() - 4, rect.center().y())); };
    const QPoint destination = canvas->mapFromScene(canvas->nodeRect("C.md").center());
    const auto sourceRect = canvas->nodeRect("B.md");
    std::array<QStringList, 4> originalRoles;
    for (int i = 0; i < 4; ++i) originalRoles[i] = manager.loadNode("B.md")->relatedPaths(static_cast<NodeRelationType>(i));
    const QPointF ports[] = {{sourceRect.left() + 4, sourceRect.center().y()}, {sourceRect.right() - 4, sourceRect.center().y()}, {sourceRect.center().x(), sourceRect.top() + 4}, {sourceRect.center().x(), sourceRect.bottom() - 4}};
    for (int i = 0; i < 4; ++i) {
        QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, canvas->mapFromScene(ports[i])); move(destination);
        QVERIFY(canvas->preview().valid); QCOMPARE(static_cast<int>(canvas->preview().type), i);
        QTest::keyClick(canvas, Qt::Key_Escape); QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, destination);
        QCOMPARE(manager.loadNode("B.md")->relatedPaths(static_cast<NodeRelationType>(i)), originalRoles[i]);
    }
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, start());
    const auto self = canvas->mapFromScene(sourceRect.center()); move(self); QVERIFY(!canvas->preview().valid);
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, self);
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, start()); move(destination);
    QVERIFY(canvas->preview().valid); QCOMPARE(canvas->preview().type, NodeRelationType::Next); QCOMPARE(canvas->preview().source, QString("B.md"));
    QTest::keyClick(canvas, Qt::Key_Escape); QVERIFY(!canvas->preview().valid); QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, destination);
    QVERIFY(manager.loadNode("B.md")->relatedPaths(NodeRelationType::Next).isEmpty());
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, start()); move(destination);
    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, destination);
    QTRY_COMPARE(manager.loadNode("B.md")->relatedPaths(NodeRelationType::Next), QStringList{"C.md"});
    QVERIFY(canvas->index().edges.size() == 4);
    // Select the actual newly created connector and remove it without removing either document.
    QGraphicsPathItem *connector = nullptr;
    for (auto *item : canvas->scene()->items()) if (auto *path = dynamic_cast<QGraphicsPathItem *>(item))
        if (path->toolTip().startsWith("B.md") && path->toolTip().endsWith("C.md")) { connector = path; break; }
    QVERIFY(connector); QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, canvas->mapFromScene(connector->path().pointAtPercent(.5)));
    QVERIFY(canvas->hasSelectedEdge()); auto *remove = dialog.findChild<QPushButton *>("removeTreeRelationButton"); QVERIFY(remove->isEnabled());
    QTest::mouseClick(remove, Qt::LeftButton); QTRY_VERIFY(manager.loadNode("B.md")->relatedPaths(NodeRelationType::Next).isEmpty());
    QVERIFY(manager.loadNode("B.md")); QVERIFY(manager.loadNode("C.md")); QCOMPARE(canvas->index().paths.size(), 4);
    dialog.close();
}
void TreeMdTests::knowledgeTreeDirectorySearchAndReindex() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    QVERIFY(manager.createDocument("A.md", "# Alpha")); QVERIFY(manager.createDocument("folder/B.md", "# Beta"));
    QVERIFY(manager.createDocument("folder/nested/C.md", "# Gamma")); QVERIFY(manager.createDocument("other/D.md", "# Delta"));
    QVERIFY(manager.addRelation("folder/B.md", "folder/nested/C.md", NodeRelationType::Next));
    RelationDialog dialog(&manager, "A.md"); dialog.show(); QTest::qWait(30); QCOMPARE(dialog.windowTitle(), QStringLiteral("知识树"));
    auto *tree = dialog.findChild<QTreeWidget *>("availableDocumentTree"); auto *search = dialog.findChild<QLineEdit *>("knowledgeTreeSearch");
    QVERIFY(tree); QVERIFY(search); QCOMPARE(tree->topLevelItemCount(), 2);
    auto *folder = tree->topLevelItem(0); QCOMPARE(folder->text(0), QString("folder")); QVERIFY(!(folder->flags() & Qt::ItemIsDragEnabled));
    QCOMPARE(folder->childCount(), 2); QVERIFY(folder->icon(0).isNull());
    search->setText("Beta"); QVERIFY(!folder->isHidden()); QVERIFY(tree->topLevelItem(1)->isHidden()); search->clear();
    dialog.setCurrentNode("folder/B.md"); auto *canvas = dialog.findChild<KnowledgeTreeCanvas *>(); QCOMPARE(canvas->index().paths.size(), 2);
    QVERIFY(manager.setRelation("folder/B.md", "A.md", NodeRelationType::Parent)); QCOMPARE(canvas->index().paths.size(), 3);
    QVERIFY(manager.renameDocument("folder/B.md", "renamed/B.md")); QCOMPARE(canvas->index().root, QString("renamed/B.md"));
    QVERIFY(manager.removeRelation("renamed/B.md", "A.md")); QCOMPARE(canvas->index().paths.size(), 2);
    QVERIFY(manager.deleteDocument("renamed/B.md")); QVERIFY(!canvas->index().root.isEmpty()); dialog.close();
    MainWindow window(dir.path()); window.show(); window.setRelationsVisible(true);
    QCOMPARE(window.findChild<QAction *>("outlineAction")->text(), QStringLiteral("目录"));
    QCOMPARE(window.findChild<OutlineSidebar *>()->headerItem()->text(0), QStringLiteral("目录"));
    auto *toolbar = window.findChild<QToolBar *>("relationToolbar"); QVERIFY(toolbar);
    QStringList labels; for (auto *action : toolbar->actions()) if (!action->text().isEmpty()) labels.append(action->text());
    QCOMPARE(labels, QStringList({QStringLiteral("完善知识树")}));
    auto *manageButton = toolbar->widgetForAction(window.findChild<QAction *>("manageTreeAction"));
    QVERIFY(manageButton->geometry().center().x() > toolbar->width() * .7); window.close();
}

void TreeMdTests::reciprocalRolesPersistAndRemoveFromEitherSide() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    QVERIFY(manager.createDocument("A.md", "# A\n")); QVERIFY(manager.createDocument("B.md", "# B\n"));
    for (int i = 0; i < 4; ++i) {
        const auto type = static_cast<NodeRelationType>(i), inverse = MarkdownRelation::inverse(type);
        QVERIFY(manager.setRelation("A.md", "B.md", type));
        for (int role = 0; role < 5; ++role) {
            QCOMPARE(manager.loadNode("A.md")->relatedPaths(static_cast<NodeRelationType>(role)), role == i ? QStringList{"B.md"} : QStringList{});
            QCOMPARE(manager.loadNode("B.md")->relatedPaths(static_cast<NodeRelationType>(role)), role == static_cast<int>(inverse) ? QStringList{"A.md"} : QStringList{});
        }
        // Adding the already implied inverse is idempotent, not a second connection.
        QVERIFY(manager.addRelation("B.md", "A.md", inverse));
        QString metadata; QVERIFY(MarkdownFileIO::readFile(dir.filePath(".tree-md-relations.json"), metadata));
        QCOMPARE(QJsonDocument::fromJson(metadata.toUtf8()).object().value("relations").toArray().size(), 1);
        QCOMPARE(KnowledgeIndex::build(manager, "B.md").edges.size(), 1);
        MarkdownManager reopened(dir.path()); QVERIFY(reopened.scan());
        QCOMPARE(reopened.loadNode("A.md")->relatedPaths(type), QStringList{"B.md"});
        QCOMPARE(reopened.loadNode("B.md")->relatedPaths(inverse), QStringList{"A.md"});
        QVERIFY(reopened.removeRelation("B.md", "A.md"));
        QVERIFY(reopened.loadNode("A.md")->relatedPaths(type).isEmpty()); QVERIFY(reopened.loadNode("B.md")->relatedPaths(inverse).isEmpty());
        QVERIFY(manager.scan()); QCOMPARE(KnowledgeIndex::build(manager, "A.md").paths.size(), 1);
    }
    QVERIFY(manager.addRelation("A.md", "B.md", NodeRelationType::Next));
    QVERIFY(manager.setRelation("B.md", "A.md", NodeRelationType::Child));
    QCOMPARE(manager.loadNode("A.md")->relatedPaths(NodeRelationType::DetailOf), QStringList{"B.md"});
    QCOMPARE(manager.loadNode("B.md")->relatedPaths(NodeRelationType::Child), QStringList{"A.md"});
    QVERIFY(manager.loadNode("A.md")->relatedPaths(NodeRelationType::Next).isEmpty()); QVERIFY(manager.loadNode("B.md")->relatedPaths(NodeRelationType::Previous).isEmpty());
    const QString path = dir.filePath(".tree-md-relations.json"); QString disk; QVERIFY(MarkdownFileIO::readFile(path, disk));
    QVERIFY(MarkdownFileIO::writeFile(path, disk + "\n"));
    QVERIFY(!manager.setRelation("B.md", "A.md", NodeRelationType::Next)); QVERIFY(!manager.removeRelation("B.md", "A.md"));
    QCOMPARE(manager.loadNode("A.md")->relatedPaths(NodeRelationType::DetailOf), QStringList{"B.md"});
    QCOMPARE(manager.loadNode("B.md")->relatedPaths(NodeRelationType::Child), QStringList{"A.md"});
}
void TreeMdTests::legacyReciprocalMetadataAndConflictProtection() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    QVERIFY(manager.createDocument("A.md", "# A\n")); QVERIFY(manager.createDocument("B.md", "# B\n"));
    const QString path = dir.filePath(".tree-md-relations.json");
    const QString single = R"({"version":1,"relations":[{"source":"B.md","target":"A.md","type":"next"}]})";
    QVERIFY(MarkdownFileIO::writeFile(path, single)); QVERIFY(manager.scan());
    QCOMPARE(manager.loadNode("A.md")->relatedPaths(NodeRelationType::Previous), QStringList{"B.md"});
    QCOMPARE(manager.loadNode("B.md")->relatedPaths(NodeRelationType::Next), QStringList{"A.md"});
    QString disk; QVERIFY(MarkdownFileIO::readFile(path, disk)); QCOMPARE(disk, single);
    const QString doubled = R"({"version":1,"relations":[{"source":"A.md","target":"B.md","type":"previous"},{"source":"B.md","target":"A.md","type":"next"}]})";
    QVERIFY(MarkdownFileIO::writeFile(path, doubled)); QVERIFY(manager.scan());
    QCOMPARE(manager.loadNode("A.md")->relatedPaths(NodeRelationType::Previous).size(), 1);
    QCOMPARE(manager.loadNode("B.md")->relatedPaths(NodeRelationType::Next).size(), 1);
    const auto index = KnowledgeIndex::build(manager, "A.md"); QCOMPARE(index.paths.size(), 2); QCOMPARE(index.edges.size(), 1);
    QCOMPARE(index.edges.front().source, QString("B.md")); QCOMPARE(index.edges.front().target, QString("A.md")); QCOMPARE(index.edges.front().type, NodeRelationType::Next);
    QVERIFY(MarkdownFileIO::readFile(path, disk)); QCOMPARE(disk, doubled);
    manager.updateContent("A.md", "# A\nunsaved");
    const QString conflicting = R"({"version":1,"relations":[{"source":"A.md","target":"B.md","type":"next"},{"source":"B.md","target":"A.md","type":"next"}]})";
    QVERIFY(MarkdownFileIO::writeFile(path, conflicting)); QString error; QVERIFY(!manager.scan(&error)); QVERIFY(error.contains(QStringLiteral("双向关系不一致")));
    QCOMPARE(manager.loadNode("A.md")->content(), QString("# A\nunsaved"));
    QCOMPARE(manager.loadNode("A.md")->relatedPaths(NodeRelationType::Previous), QStringList{"B.md"});
    QCOMPARE(manager.loadNode("B.md")->relatedPaths(NodeRelationType::Next), QStringList{"A.md"});
    QVERIFY(MarkdownFileIO::readFile(path, disk)); QCOMPARE(disk, conflicting);
    QVERIFY(!manager.setRelation("B.md", "A.md", NodeRelationType::Parent));
}
void TreeMdTests::reciprocalRolesUpdateBothInterfaces() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    QVERIFY(manager.createDocument("README.md", "# Foundation\n")); QVERIFY(manager.createDocument("B.md", "# Followup\n"));
    QVERIFY(manager.addRelation("README.md", "B.md", NodeRelationType::Next));
    MainWindow window(dir.path()); window.show(); window.setRelationsVisible(true); QVERIFY(window.openDocument("B.md"));
    auto *graph = window.findChild<KnowledgeGraphWidget *>(); QVERIFY(graph); QCOMPARE(graph->visibleNodeCount(), 2);
    KnowledgeNodeWidget *foundation = nullptr, *followup = nullptr;
    for (auto *item : graph->scene()->items()) if (auto *card = dynamic_cast<KnowledgeNodeWidget *>(item)) {
        if (card->path() == "README.md") foundation = card; if (card->path() == "B.md") followup = card;
    }
    QVERIFY(foundation); QVERIFY(followup); QVERIFY(foundation->pos().x() < followup->pos().x());
    auto *liveManager = window.findChild<MarkdownManager *>(); QVERIFY(liveManager);
    QVERIFY(liveManager->setRelation("B.md", "README.md", NodeRelationType::Child));
    QCOMPARE(graph->visibleNodeCount(), 2);
    foundation = nullptr; followup = nullptr;
    for (auto *item : graph->scene()->items()) if (auto *card = dynamic_cast<KnowledgeNodeWidget *>(item)) {
        if (card->path() == "README.md") foundation = card; if (card->path() == "B.md") followup = card;
    }
    QVERIFY(foundation); QVERIFY(followup); QVERIFY(foundation->pos().y() > followup->pos().y());
    RelationDialog dialog(liveManager, "B.md", &window); dialog.show(); QTest::qWait(40);
    auto *canvas = dialog.findChild<KnowledgeTreeCanvas *>(); QCOMPARE(canvas->index().edges.size(), 1);
    QGraphicsPathItem *line = nullptr;
    for (auto *item : canvas->scene()->items()) if (auto *candidate = dynamic_cast<QGraphicsPathItem *>(item)) { line = candidate; break; }
    QVERIFY(line); QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, canvas->mapFromScene(line->path().pointAtPercent(.5)));
    QVERIFY(canvas->hasSelectedEdge()); QTest::keyClick(canvas, Qt::Key_Delete);
    QTRY_VERIFY(liveManager->loadNode("README.md")->relatedPaths(NodeRelationType::DetailOf).isEmpty());
    QVERIFY(liveManager->loadNode("B.md")->relatedPaths(NodeRelationType::Child).isEmpty()); QCOMPARE(canvas->index().paths.size(), 1); QCOMPARE(graph->visibleNodeCount(), 1);
    auto *tree = dialog.findChild<QTreeWidget *>("availableDocumentTree"); QCOMPARE(tree->topLevelItemCount(), 1); QCOMPARE(tree->topLevelItem(0)->data(0, Qt::UserRole).toString(), QString("README.md"));
    dialog.close(); window.close();
}

void TreeMdTests::knowledgeIndexExpandsFiveLayersInBothDirections() {
    QTemporaryDir dir; MarkdownManager manager(dir.path()); QVERIFY(manager.createDocument("root.md", "# Root"));
    QString previous = "root.md", next = "root.md";
    for (int i = 1; i <= 7; ++i) {
        const QString p = QString("previous/p%1.md").arg(i), n = QString("next/n%1.md").arg(i);
        QVERIFY(manager.createDocument(p, "# Previous")); QVERIFY(manager.createDocument(n, "# Next"));
        QVERIFY(manager.addRelation(previous, p, NodeRelationType::Previous)); QVERIFY(manager.addRelation(next, n, NodeRelationType::Next));
        previous = p; next = n;
    }
    auto index = KnowledgeIndex::build(manager, "root.md"); QCOMPARE(index.paths.size(), 11); QCOMPARE(index.edges.size(), 10);
    QCOMPARE(index.depths.value("root.md"), 0);
    for (int i = 1; i <= 7; ++i) {
        const QString p = QString("previous/p%1.md").arg(i), n = QString("next/n%1.md").arg(i);
        QCOMPARE(index.paths.contains(p), i <= 5); QCOMPARE(index.paths.contains(n), i <= 5);
        if (i <= 5) { QCOMPARE(index.depths.value(p), i); QCOMPARE(index.depths.value(n), i); }
    }
    QVERIFY(manager.createDocument("shared.md", "# Shared")); QVERIFY(manager.createDocument("outside.md", "# Outside"));
    QVERIFY(manager.addRelation("previous/p1.md", "shared.md", NodeRelationType::Child));
    QVERIFY(manager.addRelation("next/n1.md", "shared.md", NodeRelationType::Child));
    QVERIFY(manager.addRelation("previous/p2.md", "shared.md", NodeRelationType::Child));
    for (int i = 0; i < 14; ++i) {
        const QString branch = QString("branch/%1.md").arg(i); QVERIFY(manager.createDocument(branch, "# Branch"));
        QVERIFY(manager.addRelation("root.md", branch, NodeRelationType::Next));
    }
    index = KnowledgeIndex::build(manager, "root.md"); QCOMPARE(index.paths.size(), 26); QCOMPARE(index.edges.size(), 27);
    QCOMPARE(index.paths.count("shared.md"), 1); QVERIFY(!index.paths.contains("outside.md"));
    QSet<QString> uniqueEdges;
    for (const auto &edge : index.edges) {
        QVERIFY(index.paths.contains(edge.source)); QVERIFY(index.paths.contains(edge.target));
        uniqueEdges.insert(edge.source + '|' + edge.target);
    }
    QCOMPARE(uniqueEdges.size(), index.edges.size()); for (int depth : index.depths) QVERIFY(depth <= 5);
    const auto recentered = KnowledgeIndex::build(manager, "previous/p5.md"); QVERIFY(recentered.paths.contains("previous/p7.md"));
    for (int depth : recentered.depths) QVERIFY(depth <= 5);
    QCOMPARE(KnowledgeIndex::build(manager, "root.md", 0).paths, QStringList{"root.md"});
}
void TreeMdTests::mainAndManagementTreesShareRangeAndLayout() {
    QTemporaryDir dir; MarkdownManager manager(dir.path()); QVERIFY(manager.createDocument("README.md", "# Current"));
    QString previous = "README.md", next = "README.md";
    for (int i = 1; i <= 6; ++i) {
        const QString p = QString("previous/p%1.md").arg(i), n = QString("next/n%1.md").arg(i);
        QVERIFY(manager.createDocument(p, "# Previous")); QVERIFY(manager.createDocument(n, "# Next"));
        QVERIFY(manager.addRelation(previous, p, NodeRelationType::Previous)); QVERIFY(manager.addRelation(next, n, NodeRelationType::Next)); previous = p; next = n;
    }
    for (int i = 0; i < 11; ++i) {
        const QString path = QString("branch/%1.md").arg(i); QVERIFY(manager.createDocument(path, "# Branch")); QVERIFY(manager.addRelation("README.md", path, NodeRelationType::Next));
    }
    QVERIFY(manager.createDocument("up.md", "# Up")); QVERIFY(manager.createDocument("down.md", "# Down"));
    QVERIFY(manager.addRelation("README.md", "up.md", NodeRelationType::Parent)); QVERIFY(manager.addRelation("README.md", "down.md", NodeRelationType::Child));
    MainWindow window(dir.path()); window.show(); window.setRelationsVisible(true); QTest::qWait(40);
    QCOMPARE(window.findChild<QAction *>("relationsAction")->text(), QStringLiteral("知识树"));
    auto *graph = window.findChild<KnowledgeGraphWidget *>(); QVERIFY(graph); QCOMPARE(graph->visibleNodeCount(), 24);
    QVERIFY(graph->index().paths.contains("previous/p5.md")); QVERIFY(graph->index().paths.contains("next/n5.md"));
    QVERIFY(!graph->index().paths.contains("previous/p6.md")); QVERIFY(!graph->index().paths.contains("next/n6.md"));
    window.openKnowledgeTree(); auto *dialog = window.findChild<RelationDialog *>("knowledgeTreeDialog"); QVERIFY(dialog); QVERIFY(dialog->isVisible());
    auto *canvas = dialog->findChild<KnowledgeTreeCanvas *>(); QVERIFY(canvas);
    auto compareViews = [&] {
        QCOMPARE(graph->index().paths, canvas->index().paths); QCOMPARE(graph->index().members, canvas->index().members);
        QCOMPARE(graph->index().depths, canvas->index().depths); QCOMPARE(graph->index().edges.size(), canvas->index().edges.size());
        for (const auto &path : graph->index().paths) QCOMPARE(graph->nodeRect(path), canvas->nodeRect(path));
    };
    compareViews(); auto *available = dialog->findChild<QTreeWidget *>("availableDocumentTree"); QVERIFY(available);
    QStringList remaining;
    { QTreeWidgetItemIterator it(available); while (*it) { if (!(*it)->data(0, Qt::UserRole).toString().isEmpty()) remaining.append((*it)->data(0, Qt::UserRole).toString()); ++it; } }
    QCOMPARE(remaining.size(), 2); QVERIFY(remaining.contains("previous/p6.md")); QVERIFY(remaining.contains("next/n6.md"));
    dialog->close(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCOMPARE(graph->visibleNodeCount(), 24);
    window.openKnowledgeTree(); dialog = window.findChild<RelationDialog *>("knowledgeTreeDialog"); QVERIFY(dialog); canvas = dialog->findChild<KnowledgeTreeCanvas *>(); compareViews();
    QVERIFY(!dialog->findChild<QPushButton *>("centerTreeButton")); QVERIFY(!dialog->findChild<QPushButton *>("fitTreeButton"));
    dialog->close(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete); window.close();
}
void TreeMdTests::mainTreeNavigationDoesNotEditRelations() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    for (const auto &path : {"root.md", "B.md", "C.md", "outside.md"}) QVERIFY(manager.createDocument(path, "# " + QString(path)));
    QVERIFY(manager.addRelation("root.md", "B.md", NodeRelationType::Next)); QVERIFY(manager.addRelation("B.md", "C.md", NodeRelationType::Next));
    KnowledgeGraphWidget graph; graph.resize(1100, 400); graph.show(); graph.setGraph(&manager, "root.md"); QTest::qWait(40);
    QSignalSpy navigation(&graph, &KnowledgeGraphWidget::nodeActivated), edits(&graph, &KnowledgeTreeCanvas::relationRequested);
    const QPoint point = graph.mapFromScene(graph.nodeRect("C.md").center()); QTest::mouseClick(graph.viewport(), Qt::LeftButton, Qt::NoModifier, point);
    QCOMPARE(navigation.size(), 1); QCOMPARE(navigation.first().first().toString(), QString("C.md"));
    const QPoint origin = graph.mapFromScene(graph.nodeRect("B.md").center()), destination = origin + QPoint(60, 50);
    QTest::mousePress(graph.viewport(), Qt::LeftButton, Qt::NoModifier, origin);
    QMouseEvent move(QEvent::MouseMove, destination, graph.viewport()->mapToGlobal(destination), Qt::NoButton, Qt::LeftButton, Qt::NoModifier); QApplication::sendEvent(graph.viewport(), &move);
    QTest::mouseRelease(graph.viewport(), Qt::LeftButton, Qt::NoModifier, destination); QCOMPARE(navigation.size(), 1); QVERIFY(edits.isEmpty());
    QMimeData mime; mime.setData(KnowledgeTreeCanvas::mimeType(), "outside.md");
    QDragEnterEvent enter(origin, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier); QApplication::sendEvent(graph.viewport(), &enter); QVERIFY(!enter.isAccepted());
    QTest::keyClick(&graph, Qt::Key_Delete); QVERIFY(edits.isEmpty()); QCOMPARE(graph.visibleNodeCount(), 3);
    QString metadata; QVERIFY(MarkdownFileIO::readFile(dir.filePath(".tree-md-relations.json"), metadata));
    QCOMPARE(QJsonDocument::fromJson(metadata.toUtf8()).object().value("relations").toArray().size(), 2);
    graph.close();
}

void TreeMdTests::nestedCardsFollowHierarchyAndSharedNodes() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    for (const auto &path : {"upper2.md", "upper1.md", "root.md", "child1.md", "child2.md", "child3.md", "child4.md", "child5.md", "peer.md"})
        QVERIFY(manager.createDocument(path, "# " + QString(path)));
    const QStringList chain = {"upper2.md", "upper1.md", "root.md", "child1.md", "child2.md", "child3.md", "child4.md", "child5.md"};
    for (int i = 0; i + 1 < chain.size(); ++i) QVERIFY(manager.addRelation(chain[i], chain[i + 1], NodeRelationType::Child));
    QVERIFY(manager.addRelation("root.md", "peer.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("peer.md", "child2.md", NodeRelationType::Child));
    KnowledgeGraphWidget graph; graph.setGraph(&manager, "root.md"); QCOMPARE(graph.visibleNodeCount(), 9);
    auto check = [&] {
        const auto layout = KnowledgeTreeLayout::build(graph.index());
        for (const auto &edge : graph.index().edges) if (edge.type == NodeRelationType::Child) {
            const auto parent = graph.nodeRect(edge.source), child = graph.nodeRect(edge.target);
            QVERIFY(parent != child); QVERIFY(child.height() <= 100);
        }
        for (int i = 3; i + 1 < chain.size(); ++i) {
            QVERIFY(graph.nodeRect(chain[i]).width() >= graph.nodeRect(chain[i + 1]).width());
            QVERIFY(KnowledgeNodeWidget::titlePointSize(layout.levels.value(chain[i])) >= KnowledgeNodeWidget::titlePointSize(layout.levels.value(chain[i + 1])));
        }
        QVERIFY(graph.nodeRect("upper2.md").bottom() < graph.nodeRect("upper1.md").top());
        QVERIFY(graph.nodeRect("upper1.md").bottom() < graph.nodeRect("root.md").top());
        for (int i = 0; i < graph.index().paths.size(); ++i) for (int j = i + 1; j < graph.index().paths.size(); ++j)
            QVERIFY(!graph.nodeRect(graph.index().paths[i]).intersects(graph.nodeRect(graph.index().paths[j])));
        for (auto *item : graph.scene()->items()) if (auto *line = dynamic_cast<QGraphicsPathItem *>(item)) {
            bool matched = false;
            for (const auto &edge : graph.index().edges) if (edge.type == NodeRelationType::Child) {
                const auto parent = graph.nodeRect(edge.source), child = graph.nodeRect(edge.target);
                if (QLineF(line->path().pointAtPercent(0), {parent.center().x(), parent.bottom()}).length() < .01 &&
                    QLineF(line->path().pointAtPercent(1), {child.center().x(), child.top()}).length() < .01) matched = true;
            }
            if (line->data(0).toInt() == static_cast<int>(NodeRelationType::Child)) QVERIFY(matched);
        }
    };
    check();
    QCOMPARE(graph.index().paths.count("child2.md"), 1); QCOMPARE(graph.index().edges.size(), 9);
    graph.setGraph(&manager, "child1.md"); check();
    // Vertical cycles retain all links and a finite, non-overlapping layout.
    QVERIFY(manager.addRelation("child3.md", "root.md", NodeRelationType::Child)); graph.setGraph(&manager, "root.md");
    QCOMPARE(graph.visibleNodeCount(), 9); QCOMPARE(graph.index().edges.size(), 10);
    for (const auto &path : graph.index().paths) { QVERIFY(graph.nodeRect(path).isValid()); QVERIFY(graph.nodeRect(path).height() < 400); }
}
void TreeMdTests::nestedDropPreviewAndSmallCardPorts() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    for (const auto &path : {"root.md", "child.md", "outside.md", "peer.md"}) QVERIFY(manager.createDocument(path, "# " + QString(path)));
    QVERIFY(manager.addRelation("root.md", "child.md", NodeRelationType::Child));
    QVERIFY(manager.addRelation("root.md", "peer.md", NodeRelationType::Next));
    RelationDialog dialog(&manager, "root.md"); dialog.show(); QTest::qWait(40);
    QVERIFY(!dialog.findChild<QPushButton *>("centerTreeButton")); QVERIFY(!dialog.findChild<QPushButton *>("fitTreeButton"));
    auto *canvas = dialog.findChild<KnowledgeTreeCanvas *>(); QVERIFY(canvas);
    const auto child = canvas->nodeRect("child.md"); QMimeData mime; mime.setData(KnowledgeTreeCanvas::mimeType(), "outside.md");
    const QPoint point = canvas->mapFromScene({child.center().x(), child.bottom() + 55});
    QDragEnterEvent enter(point, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier); QApplication::sendEvent(canvas->viewport(), &enter); QVERIFY(enter.isAccepted());
    QDragMoveEvent move(point, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier); QApplication::sendEvent(canvas->viewport(), &move);
    QVERIFY(canvas->preview().valid); QCOMPARE(canvas->preview().source, QString("child.md")); QCOMPARE(canvas->preview().type, NodeRelationType::Child);
    bool smallerGhost = false;
    for (auto *item : canvas->scene()->items()) if (auto *frame = dynamic_cast<QGraphicsRectItem *>(item))
        if (frame->rect().width() < child.width()) smallerGhost = true;
    QVERIFY(smallerGhost); QVERIFY(manager.loadNode("child.md")->relatedPaths(NodeRelationType::Child).isEmpty());
    QDropEvent drop(point, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier); QApplication::sendEvent(canvas->viewport(), &drop); QVERIFY(drop.isAccepted());
    QTRY_VERIFY(manager.loadNode("child.md")->relatedPaths(NodeRelationType::Child).contains("outside.md"));
    const auto small = canvas->nodeRect("outside.md"); QVERIFY(small.width() < canvas->nodeRect("child.md").width());
    const QPoint start = canvas->mapFromScene({small.center().x(), small.bottom() - 4});
    const QPoint end = canvas->mapFromScene(canvas->nodeRect("peer.md").center());
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, start);
    QMouseEvent drag(QEvent::MouseMove, end, canvas->viewport()->mapToGlobal(end), Qt::NoButton, Qt::LeftButton, Qt::NoModifier); QApplication::sendEvent(canvas->viewport(), &drag);
    QVERIFY(canvas->preview().valid); QCOMPARE(canvas->preview().source, QString("outside.md")); QCOMPARE(canvas->preview().type, NodeRelationType::Child);
    QTest::keyClick(canvas, Qt::Key_Escape); QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, end);
    QVERIFY(manager.loadNode("outside.md")->relatedPaths(NodeRelationType::Child).isEmpty()); dialog.close();
}

void TreeMdTests::alternativeRoutesStayInSync() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    for (const auto &path : {"P.md", "A.md", "B.md", "C.md", "N.md", "N2.md"}) QVERIFY(manager.createDocument(path, "# " + QString(path)));
    QVERIFY(manager.addRelation("P.md", "A.md", NodeRelationType::Next)); QVERIFY(manager.addRelation("A.md", "N.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("A.md", "B.md", NodeRelationType::Parent)); QVERIFY(manager.addRelation("B.md", "C.md", NodeRelationType::Parent));
    for (const auto &path : {"A.md", "B.md", "C.md"}) {
        QCOMPARE(manager.loadNode(path)->relatedPaths(NodeRelationType::Previous), QStringList{"P.md"});
        QCOMPARE(manager.loadNode(path)->relatedPaths(NodeRelationType::Next), QStringList{"N.md"});
    }
    QCOMPARE(manager.loadNode("N.md")->relatedPaths(NodeRelationType::Previous), QStringList({"A.md", "B.md", "C.md"}));
    QVERIFY(!manager.setRelation("A.md", "C.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("B.md", "N2.md", NodeRelationType::Next));
    QString metadata; const QString relationFile = dir.filePath(".tree-md-relations.json"); QVERIFY(MarkdownFileIO::readFile(relationFile, metadata));
    const auto object = QJsonDocument::fromJson(metadata.toUtf8()).object(); QCOMPARE(object.value("version").toInt(), 2);
    QCOMPARE(object.value("relations").toArray().size(), 5); // Implied routes are not duplicated on disk.
    QVERIFY(manager.removeRelation("C.md", "N.md"));
    for (const auto &path : {"A.md", "B.md", "C.md"}) QCOMPARE(manager.loadNode(path)->relatedPaths(NodeRelationType::Next), QStringList{"N2.md"});
    QVERIFY(manager.setRelation("C.md", "N2.md", NodeRelationType::Previous));
    for (const auto &path : {"A.md", "B.md", "C.md"}) {
        QCOMPARE(manager.loadNode(path)->relatedPaths(NodeRelationType::Previous), QStringList({"N2.md", "P.md"}));
        QVERIFY(manager.loadNode(path)->relatedPaths(NodeRelationType::Next).isEmpty());
    }
    MarkdownManager reopened(dir.path()); QVERIFY(reopened.scan());
    QCOMPARE(reopened.loadNode("B.md")->relatedPaths(NodeRelationType::Previous), manager.loadNode("A.md")->relatedPaths(NodeRelationType::Previous));
    QVERIFY(manager.removeRelation("A.md", "B.md")); QCOMPARE(manager.loadNode("A.md")->relatedPaths(NodeRelationType::Previous), QStringList{"P.md"});
    QCOMPARE(manager.loadNode("B.md")->relatedPaths(NodeRelationType::Previous), QStringList{"N2.md"});
    QVERIFY(manager.renameDocument("A.md", "folder/A.md")); QCOMPARE(manager.loadNode("P.md")->relatedPaths(NodeRelationType::Next), QStringList{"folder/A.md"});
    // Merging contradictory routes fails atomically.
    QVERIFY(manager.setRelation("B.md", "P.md", NodeRelationType::Next)); QVERIFY(MarkdownFileIO::readFile(relationFile, metadata));
    QString error; QVERIFY(!manager.setRelation("folder/A.md", "B.md", NodeRelationType::Parent, &error)); QVERIFY(!error.isEmpty());
    QString after; QVERIFY(MarkdownFileIO::readFile(relationFile, after)); QCOMPARE(after, metadata);
}
void TreeMdTests::legacyAlternativesAndDetailsStaySeparate() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    for (const auto &path : {"P.md", "A.md", "B.md", "C.md"}) QVERIFY(manager.createDocument(path, "# " + QString(path)));
    const QString legacy = R"({"version":1,"relations":[{"source":"P.md","target":"A.md","type":"next"},{"source":"A.md","target":"B.md","type":"parent"},{"source":"B.md","target":"A.md","type":"child"},{"source":"A.md","target":"C.md","type":"child"}]})";
    const QString file = dir.filePath(".tree-md-relations.json"); QVERIFY(MarkdownFileIO::writeFile(file, legacy)); QVERIFY(manager.scan());
    QCOMPARE(manager.loadNode("A.md")->relatedPaths(NodeRelationType::Parent), QStringList{"B.md"});
    QCOMPARE(manager.loadNode("B.md")->relatedPaths(NodeRelationType::Parent), QStringList{"A.md"});
    QCOMPARE(manager.loadNode("B.md")->relatedPaths(NodeRelationType::Previous), QStringList{"P.md"});
    QCOMPARE(manager.loadNode("A.md")->relatedPaths(NodeRelationType::Child), QStringList{"C.md"});
    QCOMPARE(manager.loadNode("C.md")->relatedPaths(NodeRelationType::DetailOf), QStringList{"A.md"});
    QVERIFY(manager.loadNode("C.md")->relatedPaths(NodeRelationType::Parent).isEmpty());
    QString disk; QVERIFY(MarkdownFileIO::readFile(file, disk)); QCOMPARE(disk, legacy);
    QVERIFY(manager.addRelation("B.md", "P.md", NodeRelationType::Previous)); QVERIFY(MarkdownFileIO::readFile(file, disk)); QCOMPARE(disk, legacy);
    QVERIFY(manager.setRelation("C.md", "A.md", NodeRelationType::DetailOf));
    QVERIFY(MarkdownFileIO::readFile(file, disk));
    QCOMPARE(QJsonDocument::fromJson(disk.toUtf8()).object().value("version").toInt(), 2);
    MarkdownManager reopened(dir.path()); QVERIFY(reopened.scan()); QCOMPARE(reopened.loadNode("C.md")->relatedPaths(NodeRelationType::DetailOf), QStringList{"A.md"});
}
void TreeMdTests::learningRouteCardsAndConnectors() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    for (const auto &path : {"P.md", "A.md", "B.md", "B2.md", "N.md", "detail.md", "practice.md"}) QVERIFY(manager.createDocument(path, "# " + QString(path)));
    QVERIFY(manager.addRelation("P.md", "A.md", NodeRelationType::Next)); QVERIFY(manager.addRelation("A.md", "N.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("A.md", "B.md", NodeRelationType::Parent)); QVERIFY(manager.addRelation("B.md", "B2.md", NodeRelationType::Parent));
    QVERIFY(manager.addRelation("A.md", "detail.md", NodeRelationType::Child)); QVERIFY(manager.addRelation("detail.md", "practice.md", NodeRelationType::Child));
    KnowledgeGraphWidget graph; graph.resize(1300, 650); graph.show(); graph.setGraph(&manager, "A.md"); QTest::qWait(30);
    const auto main = graph.nodeRect("A.md"), alternative = graph.nodeRect("B.md"), other = graph.nodeRect("B2.md"), detail = graph.nodeRect("detail.md"), practice = graph.nodeRect("practice.md");
    QCOMPARE(alternative.bottom(), main.top()); QCOMPARE(other.bottom(), alternative.top());
    QVERIFY(alternative.width() < main.width()); QVERIFY(alternative.width() > main.width() * .9);
    QVERIFY(detail.width() <= main.width() * .66); QVERIFY(detail.height() < main.height());
    QCOMPARE(detail.top() - main.bottom(), 60.0); QCOMPARE(practice.top() - detail.bottom(), 39.6);
    QCOMPARE(graph.nodeRect("P.md").center().y(), main.center().y()); QCOMPARE(graph.nodeRect("N.md").center().y(), main.center().y());
    QCOMPARE(graph.index().paths.size(), 7); QCOMPARE(graph.index().edges.size(), 6);
    int routes = 0, details = 0;
    for (auto *item : graph.scene()->items()) if (auto *line = dynamic_cast<QGraphicsPathItem *>(item)) {
        if (line->data(0).toInt() == static_cast<int>(NodeRelationType::Next)) { ++routes; QCOMPARE(line->pen().style(), Qt::SolidLine); QVERIFY(line->pen().widthF() > 3); }
        if (line->data(0).toInt() == static_cast<int>(NodeRelationType::Child)) { ++details; QCOMPARE(line->pen().style(), Qt::SolidLine); QVERIFY(line->pen().widthF() < 2); }
    }
    QCOMPARE(routes, 2); QCOMPARE(details, 2);
    QSignalSpy navigate(&graph, &KnowledgeGraphWidget::nodeActivated);
    QTest::mouseClick(graph.viewport(), Qt::LeftButton, Qt::NoModifier, graph.mapFromScene(alternative.center()));
    QCOMPARE(navigate.size(), 1); QCOMPARE(navigate.first().first().toString(), QString("B.md"));
    graph.setGraph(&manager, "B.md"); QCOMPARE(graph.nodeRect("A.md"), main); QCOMPARE(graph.nodeRect("B.md"), alternative); QCOMPARE(graph.nodeRect("B2.md"), other);
    QCOMPARE(manager.loadNode("A.md")->relatedPaths(NodeRelationType::Previous), manager.loadNode("B.md")->relatedPaths(NodeRelationType::Previous));
    for (int i = 0; i < graph.index().paths.size(); ++i) for (int j = i + 1; j < graph.index().paths.size(); ++j)
        QVERIFY(!graph.nodeRect(graph.index().paths[i]).intersects(graph.nodeRect(graph.index().paths[j])));
    graph.close();
}

void TreeMdTests::referenceLayoutHasAlignedRouteAndParallelDetails() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    for (const auto &path : {"P.md", "A.md", "N.md", "U.md", "D1.md", "D2.md"}) QVERIFY(manager.createDocument(path, "# " + QString(path)));
    QVERIFY(manager.addRelation("P.md", "A.md", NodeRelationType::Next)); QVERIFY(manager.addRelation("A.md", "N.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("A.md", "U.md", NodeRelationType::Parent));
    QVERIFY(manager.addRelation("A.md", "D1.md", NodeRelationType::Child)); QVERIFY(manager.addRelation("A.md", "D2.md", NodeRelationType::Child));
    RelationDialog dialog(&manager, "A.md"); dialog.show(); QTest::qWait(40);
    auto *canvas = dialog.findChild<KnowledgeTreeCanvas *>(); QVERIFY(canvas);
    const auto main = canvas->nodeRect("A.md"), upper = canvas->nodeRect("U.md"), first = canvas->nodeRect("D1.md"), second = canvas->nodeRect("D2.md");
    QCOMPARE(canvas->nodeRect("P.md").center().y(), main.center().y()); QCOMPARE(canvas->nodeRect("N.md").center().y(), main.center().y());
    QVERIFY(canvas->nodeRect("P.md").right() < main.left()); QVERIFY(main.right() < canvas->nodeRect("N.md").left());
    QCOMPARE(upper.bottom(), main.top()); QCOMPARE(upper.center().x(), main.center().x());
    QCOMPARE(first.top(), second.top()); QVERIFY(first.right() < second.left()); QVERIFY(first.top() > main.bottom());
    QCOMPARE((first.center().x() + second.center().x()) / 2, main.center().x());
    QVERIFY(first.width() < main.width() * .7); QVERIFY(first.height() < main.height());
    int arrows = 0;
    for (auto *item : canvas->scene()->items()) if (auto *line = dynamic_cast<QGraphicsPathItem *>(item)) {
        if (line->data(0).toInt() == static_cast<int>(NodeRelationType::Next)) {
            QCOMPARE(line->path().elementCount(), 2); QCOMPARE(line->path().pointAtPercent(0).y(), line->path().pointAtPercent(1).y());
        }
        if (line->data(0).toInt() == static_cast<int>(NodeRelationType::Child)) {
            ++arrows; QCOMPARE(line->path().elementCount(), 4);
            QCOMPARE(line->path().pointAtPercent(0), QPointF(main.center().x(), main.bottom()));
            const auto end = line->path().pointAtPercent(1);
            QVERIFY(end == QPointF(first.center().x(), first.top()) || end == QPointF(second.center().x(), second.top()));
            const auto a = line->path().elementAt(1), b = line->path().elementAt(2);
            QCOMPARE(a.x, main.center().x()); QCOMPARE(b.x, end.x());
            QCOMPARE(a.y, b.y); QVERIFY(a.y > main.bottom()); QVERIFY(a.y < end.y());
        }
    }
    QCOMPARE(arrows, 2);
    // Touching alternatives remain removable without drawing a side bracket.
    QTest::mouseClick(canvas->viewport(), Qt::LeftButton, Qt::NoModifier, canvas->mapFromScene({upper.center().x() + upper.width() * .23, upper.bottom()}));
    QVERIFY(canvas->hasSelectedEdge()); QTest::keyClick(canvas, Qt::Key_Delete);
    QTRY_VERIFY(manager.loadNode("A.md")->relatedPaths(NodeRelationType::Parent).isEmpty());
    QVERIFY(manager.loadNode("U.md")); QCOMPARE(manager.allNodes().size(), 6); dialog.close();
}
void TreeMdTests::detailBranchesReserveSpaceBesideRoutes() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    for (const auto &path : {"Owner.md", "P.md", "Q.md", "H.md", "A.md", "N.md", "U.md", "D1.md", "D2.md", "D3.md", "D4.md", "C1.md", "C2.md", "C3.md"})
        QVERIFY(manager.createDocument(path, "# " + QString(path)));
    QVERIFY(manager.addRelation("H.md", "P.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("P.md", "A.md", NodeRelationType::Next)); QVERIFY(manager.addRelation("Q.md", "A.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("A.md", "N.md", NodeRelationType::Next)); QVERIFY(manager.addRelation("A.md", "U.md", NodeRelationType::Parent));
    QVERIFY(manager.addRelation("Owner.md", "A.md", NodeRelationType::Child));
    QVERIFY(manager.addRelation("A.md", "D1.md", NodeRelationType::Child)); QVERIFY(manager.addRelation("A.md", "D2.md", NodeRelationType::Child));
    for (const auto &path : {"C1.md", "C2.md", "C3.md"}) QVERIFY(manager.addRelation("D1.md", path, NodeRelationType::Child));
    QVERIFY(manager.addRelation("D2.md", "D3.md", NodeRelationType::Next)); QVERIFY(manager.addRelation("D2.md", "D4.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("Q.md", "C2.md", NodeRelationType::Child)); // Shared detail, only one card.
    KnowledgeGraphWidget graph;
    auto verify = [&](const QString &root) {
        graph.setGraph(&manager, root); const auto paths = graph.index().paths;
        QCOMPARE(graph.index().paths.count("C2.md"), 1);
        const auto layout = KnowledgeTreeLayout::build(graph.index()); QCOMPARE(layout.rects.size(), paths.size());
        for (int i = 0; i < paths.size(); ++i) {
            QVERIFY(graph.nodeRect(paths[i]).isValid());
            for (int j = i + 1; j < paths.size(); ++j) QVERIFY2(!graph.nodeRect(paths[i]).intersects(graph.nodeRect(paths[j])), qPrintable(paths[i] + " overlaps " + paths[j]));
        }
        QCOMPARE(layout.rects, KnowledgeTreeLayout::build(graph.index()).rects); // Stable after refresh.
    };
    verify("A.md");
    const auto main = graph.nodeRect("A.md"); QCOMPARE(main.width(), KnowledgeNodeWidget::cardSize(1, false).width());
    for (const auto &path : {"H.md", "P.md", "N.md"}) QCOMPARE(graph.nodeRect(path).center().y(), main.center().y());
    QVERIFY(graph.nodeRect("Owner.md").bottom() < graph.nodeRect("U.md").top());
    QCOMPARE(graph.nodeRect("D1.md").top(), graph.nodeRect("D2.md").top());
    QCOMPARE(graph.nodeRect("C1.md").top(), graph.nodeRect("C3.md").top());
    verify("D1.md"); verify("P.md"); verify("U.md");
    QVERIFY(manager.addRelation("N.md", "H.md", NodeRelationType::Next)); // Route cycle.
    QVERIFY(manager.addRelation("C3.md", "Owner.md", NodeRelationType::Child)); // Detail cycle.
    verify("A.md");
}

void TreeMdTests::skippedAndSharedConnectionsAvoidCards() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    for (const auto &path : {"P.md", "A.md", "N.md", "U.md", "D.md"}) QVERIFY(manager.createDocument(path, "# " + QString(path)));
    QVERIFY(manager.addRelation("P.md", "A.md", NodeRelationType::Next)); QVERIFY(manager.addRelation("A.md", "N.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("P.md", "N.md", NodeRelationType::Next)); // Skips the middle card.
    QVERIFY(manager.addRelation("A.md", "U.md", NodeRelationType::Parent)); QVERIFY(manager.addRelation("A.md", "D.md", NodeRelationType::Child));
    QVERIFY(manager.addRelation("N.md", "D.md", NodeRelationType::Child)); QVERIFY(manager.addRelation("D.md", "P.md", NodeRelationType::Child));
    KnowledgeGraphWidget graph; graph.setGraph(&manager, "A.md");
    QPainterPathStroker stroke; stroke.setWidth(.5); int checked = 0, detours = 0;
    for (auto *item : graph.scene()->items()) if (auto *line = dynamic_cast<QGraphicsPathItem *>(item)) {
        if (line->data(0).toInt() == static_cast<int>(NodeRelationType::Parent)) continue;
        ++checked; if (line->path().elementCount() > 4) ++detours;
        const auto ink = stroke.createStroke(line->path());
        for (const auto &path : graph.index().paths) QVERIFY2(!ink.intersects(graph.nodeRect(path).adjusted(2, 2, -2, -2)), qPrintable(path + " obscures a connection"));
    }
    QCOMPARE(checked, 6); QVERIFY(detours > 0);
}

void TreeMdTests::attachedDetailStaysBelowItsAlternativeAfterNewLinks() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    for (const auto &path : {"root.md", "Prometheus.md", "Curl.md", "backup.md", "future.md"}) QVERIFY(manager.createDocument(path, "# " + QString(path)));
    QVERIFY(manager.addRelation("root.md", "Prometheus.md", NodeRelationType::Child));
    QVERIFY(manager.addRelation("root.md", "backup.md", NodeRelationType::Child));
    QVERIFY(manager.addRelation("Curl.md", "Prometheus.md", NodeRelationType::Parent));
    KnowledgeGraphWidget graph;
    auto check = [&] {
        graph.setGraph(&manager, "root.md"); const auto layout = KnowledgeTreeLayout::build(graph.index());
        QCOMPARE(layout.routeAnchors.value("Curl.md"), QString("Prometheus.md"));
        const auto root = graph.nodeRect("root.md"), prometheus = graph.nodeRect("Prometheus.md"), curl = graph.nodeRect("Curl.md"), backup = graph.nodeRect("backup.md");
        QCOMPARE(prometheus.top(), backup.top()); QCOMPARE(curl.bottom(), prometheus.top()); QCOMPARE(curl.center().x(), prometheus.center().x());
        QVERIFY(curl.top() > root.bottom()); QVERIFY(curl.width() < prometheus.width());
        QCOMPARE(graph.index().paths.count("Curl.md"), 1); QVERIFY(!prometheus.intersects(backup));
    };
    check();
    QVERIFY(manager.addRelation("Curl.md", "root.md", NodeRelationType::Child)); check(); // New owner backlink used to flip the stack.
    QVERIFY(manager.addRelation("Curl.md", "future.md", NodeRelationType::Next)); check(); // A route on the peer used to win the main-card score.
    QVERIFY(manager.removeRelation("Curl.md", "root.md")); check();
}

void TreeMdTests::settingsHexRoundTripAndValidation() {
    MarkdownSettings defaults, decoded; QString error;
    const QString text = MarkdownSettingsStore::encode(defaults);
    QVERIFY(text.size() < 768); QVERIFY(QRegularExpression("^[0-9A-F\\s]+$").match(text).hasMatch());
    QVERIFY2(MarkdownSettingsStore::decode(text, decoded, &error), qPrintable(error));
    QCOMPARE(MarkdownSettingsStore::encode(decoded), text);
    for (int image = 0; image < 3; ++image) for (quint16 flags : {quint16(0), quint16(255), quint16(511)}) {
        auto custom = defaults; custom.imageMode = image; custom.flags = flags;
        custom.fonts[1] = QStringLiteral("微软雅黑"); custom.colors[MarkdownSettings::InlineBackground] = qRgb(83, 86, 89);
        for (int i = 0; i < MarkdownSettings::NumberCount; ++i) custom.numbers[i] = flags ? markdownNumberSpecs()[i].maximum : markdownNumberSpecs()[i].minimum;
        QVERIFY(MarkdownSettingsStore::decode(MarkdownSettingsStore::encode(custom), decoded, &error));
        QCOMPARE(decoded.fonts, custom.fonts); QCOMPARE(decoded.flags, flags); QCOMPARE(decoded.imageMode, image);
        QVERIFY(decoded.numbers == custom.numbers); QVERIFY(decoded.colors == custom.colors);
    }
    const QString unchanged = MarkdownSettingsStore::encode(decoded);
    for (const auto &invalid : {QString("oops"), text.left(30), text + "00", QString(text).replace(5, 1, "0"), QString(40000, '0')}) {
        QVERIFY(!MarkdownSettingsStore::decode(invalid, decoded, &error)); QVERIFY(!error.isEmpty());
        QCOMPARE(MarkdownSettingsStore::encode(decoded), unchanged);
    }
    auto invalid = defaults; invalid.fonts[0] = QString("a\nb"); QVERIFY(!MarkdownSettingsStore::validate(invalid));
    invalid = defaults; invalid.numbers[MarkdownSettings::ListIndent] = 0; QVERIFY(!MarkdownSettingsStore::validate(invalid));
    invalid = defaults; invalid.imageMode = 3; QVERIFY(!MarkdownSettingsStore::validate(invalid));
    QString compact = text; compact.remove(QRegularExpression("\\s"));
    QByteArray bytes = QByteArray::fromHex(compact.toLatin1()); bytes[5] = 99;
    bytes.chop(4); bytes += QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).left(4);
    QVERIFY(!MarkdownSettingsStore::decode(QString::fromLatin1(bytes.toHex()), decoded, &error)); QVERIFY(error.contains(QStringLiteral("版本")));
    QTemporaryDir dir; const QString exported = dir.filePath("settings.txt");
    QVERIFY(MarkdownSettingsStore::write(exported, defaults, &error)); QVERIFY(MarkdownSettingsStore::read(exported, decoded, &error));
    QCOMPARE(MarkdownSettingsStore::encode(decoded), text);
    auto saved = defaults; saved.numbers[MarkdownSettings::BodySize] = 165; saved.imageMode = 2;
    QVERIFY(MarkdownSettingsStore::save(saved, &error)); const QString stored = MarkdownSettingsStore::storagePath();
    MarkdownSettingsStore::setStoragePath(stored); QCOMPARE(MarkdownSettingsStore::current().numbers[MarkdownSettings::BodySize], quint16(165));
    QCOMPARE(MarkdownSettingsStore::current().imageMode, quint8(2));
}
void TreeMdTests::settingsMenuAndAppearanceApply() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    const QString source = "# Title\n\nBody `inline`\n\n- first\n- second\n\n```cpp\nint keep;\n```\n";
    QVERIFY(manager.createDocument("README.md", source));
    MainWindow window(dir.path()); window.show(); window.activateWindow(); QVERIFY(QTest::qWaitForWindowActive(&window));
    auto *file = window.findChild<QMenu *>("fileMenu"), *settings = window.findChild<QMenu *>("settingsMenu");
    QVERIFY(file); QVERIFY(settings); auto *image = window.findChild<QAction *>("imageSettingsAction"); QVERIFY(image);
    QVERIFY(settings->actions().contains(image)); QVERIFY(!file->actions().contains(image));
    auto *about = window.findChild<QAction *>("aboutAction"); QVERIFY(settings->actions().contains(about)); QVERIFY(!file->actions().contains(about));
    QVERIFY(window.findChild<QToolButton *>("fileButton")->styleSheet().contains("image: none"));
    QVERIFY(window.findChild<QToolButton *>("settingsButton")->styleSheet().contains("image: none"));
    window.findChild<QAction *>("markdownSettingsAction")->trigger(); auto *dialog = window.findChild<MarkdownSettingsDialog *>(); QVERIFY(dialog);
    for (int i = 0; i < 5; ++i) { auto *section = dialog->findChild<QToolButton *>(QString("markdownSettingsSection%1").arg(i)); QVERIFY(section); QVERIFY(!section->isChecked()); QCOMPARE(section->arrowType(), Qt::DownArrow); QVERIFY(dialog->findChild<QWidget *>(QString("markdownSettingsSectionBody%1").arg(i))->isHidden()); }
    dialog->findChild<QToolButton *>("markdownSettingsSection0")->click(); QCoreApplication::processEvents();
    QVERIFY(!dialog->findChild<QWidget *>("markdownSettingsSectionBody0")->isHidden());
    QCOMPARE(dialog->findChild<QPushButton *>("importMarkdownSettings")->text(), QStringLiteral("导入配置")); QCOMPARE(dialog->findChild<QPushButton *>("exportMarkdownSettings")->text(), QStringLiteral("导出配置"));
    QCOMPARE(dialog->findChild<QFontComboBox *>("markdownSettingFont0")->currentText(), QString("Open Sans"));
    QVERIFY(dialog->findChild<QFontComboBox *>("markdownSettingFont0")->mapTo(dialog, QPoint()).y()
        < dialog->findChild<QDoubleSpinBox *>("markdownSettingNumber0")->mapTo(dialog, QPoint()).y());
    dialog->findChild<QDoubleSpinBox *>(QString("markdownSettingNumber%1").arg(MarkdownSettings::BodySize))->setValue(14.5);
    dialog->findChild<QDoubleSpinBox *>(QString("markdownSettingNumber%1").arg(MarkdownSettings::H1))->setValue(32);
    dialog->findChild<QDoubleSpinBox *>(QString("markdownSettingNumber%1").arg(MarkdownSettings::ListIndent))->setValue(28);
    dialog->findChild<QCheckBox *>(QString("markdownSettingFlag%1").arg(MarkdownSettings::WrapCode))->setChecked(true);
    dialog->findChild<QPushButton *>("applyMarkdownSettings")->click();
    auto *view = window.findChild<MarkdownDocumentView *>(); auto *editor = window.findChild<MarkdownEditor *>();
    QCOMPARE(view->markdownSource(), source); QCOMPARE(editor->source(), source);
    QCOMPARE(view->font().pointSizeF(), qreal(14.5)); QCOMPARE(view->document()->indentWidth(), qreal(28));
    QCOMPARE(view->document()->find("Title").charFormat().fontPointSize(), qreal(32));
    auto code = view->document()->find("int keep;").block(); QVERIFY(!code.blockFormat().nonBreakableLines());
    QVERIFY(code.blockFormat().hasProperty(QTextFormat::BlockCodeFence));
    view->setZoomPercent(150); QCOMPARE(view->document()->find("Title").charFormat().fontPointSize(), qreal(48));
    QCOMPARE(view->markdownSource(), source); QString disk; QVERIFY(MarkdownFileIO::readFile(dir.filePath("README.md"), disk)); QCOMPARE(disk, source);
    dialog->reject(); QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    window.setSourceMode(true); editor->moveCursor(QTextCursor::End); QTest::keyClicks(editor, "draft"); const QString draft = editor->source();
    window.findChild<QAction *>("markdownSettingsAction")->trigger(); dialog = window.findChild<MarkdownSettingsDialog *>(); QVERIFY(dialog);
    dialog->findChild<QDoubleSpinBox *>(QString("markdownSettingNumber%1").arg(MarkdownSettings::CodeSize))->setValue(13);
    dialog->findChild<QPushButton *>("applyMarkdownSettings")->click(); QCOMPARE(editor->source(), draft); QVERIFY(manager.loadNode("README.md"));
    QVERIFY(editor->document()->isUndoAvailable()); editor->undo(); QCOMPARE(editor->source(), source);
    dialog->reject(); window.hide();
}
void TreeMdTests::settingsImportExportDialog() {
    QTemporaryDir dir; MarkdownManager manager(dir.filePath("md_data")); QVERIFY(manager.createDocument("README.md", "# Keep\n\nText"));
    MainWindow window(manager.rootPath()); window.show();
    window.findChild<QAction *>("markdownSettingsAction")->trigger(); auto *dialog = window.findChild<MarkdownSettingsDialog *>(); QVERIFY(dialog);
    const bool native = QCoreApplication::testAttribute(Qt::AA_DontUseNativeDialogs); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
    const auto restore = qScopeGuard([native] { QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs, native); });
    const QString exported = dir.filePath("exported.txt"), imported = dir.filePath("imported.txt"), broken = dir.filePath("broken.txt");
    auto custom = MarkdownSettingsStore::current(); custom.numbers[MarkdownSettings::BodySize] = 155;
    custom.colors[MarkdownSettings::BlockBackground] = qRgb(72, 72, 72); custom.imageMode = 1;
    QVERIFY(MarkdownSettingsStore::write(imported, custom)); QVERIFY(MarkdownFileIO::writeFile(broken, "invalid"));
    QString selected, warning; QTimer answer; answer.setInterval(50); QSet<QFileDialog *> answered;
    bool timedOut = false; QTimer deadline; deadline.setSingleShot(true);
    connect(&deadline, &QTimer::timeout, &window, [&] {
        timedOut = true;
        for (auto *widget : QApplication::topLevelWidgets()) if (auto *modal = qobject_cast<QDialog *>(widget)) modal->reject();
    });
    connect(&answer, &QTimer::timeout, &window, [&] {
        if (auto *file = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
            if (answered.contains(file)) return;
            answered.insert(file);
            auto *name = file->findChild<QLineEdit *>("fileNameEdit"); QVERIFY(name);
            name->setText(selected); QMetaObject::invokeMethod(file, "accept", Qt::QueuedConnection);
        } else if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            warning = box->text(); if (auto *ok = box->button(QMessageBox::Ok)) ok->click(); else box->reject();
        }
    });
    selected = exported; answer.start(); deadline.start(5000); dialog->findChild<QPushButton *>("exportMarkdownSettings")->click(); answer.stop(); deadline.stop(); QVERIFY(!timedOut);
    MarkdownSettings decoded; QString decodeError; QVERIFY2(MarkdownSettingsStore::read(exported, decoded, &decodeError), qPrintable(decodeError + " " + warning)); QCOMPARE(decoded.numbers[MarkdownSettings::BodySize], quint16(120));
    selected = imported; answered.clear(); answer.start(); deadline.start(5000); dialog->findChild<QPushButton *>("importMarkdownSettings")->click(); answer.stop(); deadline.stop(); QVERIFY(!timedOut);
    QCOMPARE(MarkdownSettingsStore::current().numbers[MarkdownSettings::BodySize], quint16(120));
    QCOMPARE(dialog->findChild<QDoubleSpinBox *>(QString("markdownSettingNumber%1").arg(MarkdownSettings::BodySize))->value(), qreal(15.5));
    dialog->findChild<QPushButton *>("applyMarkdownSettings")->click(); QCOMPARE(MarkdownSettingsStore::current().numbers[MarkdownSettings::BodySize], quint16(155));
    QCOMPARE(window.findChild<MarkdownDocumentView *>()->font().pointSizeF(), qreal(15.5));
    QCOMPARE(MarkdownSettingsStore::current().imageMode, quint8(1));
    QCOMPARE(window.findChild<MarkdownDocumentView *>()->markdownSource(), QString("# Keep\n\nText"));
    selected = broken; answered.clear(); answer.start(); deadline.start(5000); dialog->findChild<QPushButton *>("importMarkdownSettings")->click(); answer.stop(); deadline.stop(); QVERIFY(!timedOut); QVERIFY(!warning.isEmpty());
    QCOMPARE(dialog->findChild<QDoubleSpinBox *>(QString("markdownSettingNumber%1").arg(MarkdownSettings::BodySize))->value(), qreal(15.5));
    QCOMPARE(MarkdownSettingsStore::current().numbers[MarkdownSettings::BodySize], quint16(155));
    dialog->findChild<QPushButton *>("resetMarkdownSettings")->click(); dialog->reject();
    QCOMPARE(MarkdownSettingsStore::current().numbers[MarkdownSettings::BodySize], quint16(155)); window.close();
}

void TreeMdTests::imageConfigurationAndPaths() {
    QTemporaryDir dir; const QString root = dir.filePath("md_data"); MarkdownManager manager(root);
    QVERIFY(manager.createDocument("folder/doc.md", "# Keep\n\nBody")); auto settings = MarkdownSettingsStore::current();
    settings.relativePhotoPath = "../custom images"; settings.absolutePhotoPath = dir.filePath("absolute images");
    settings.hostedLinks = {{QStringLiteral("示例图片"), "https://example.test/a.png"}, {"Second", "https://example.test/b.png"}}; settings.defaultHostedLink = 1;
    settings.uploadEndpoint = "https://example.test/upload"; settings.uploadFileField = "photo"; settings.uploadUrlPath = "result.links.0.url";
    settings.uploadAuthKind = 2; settings.uploadAuthName = "key"; settings.uploadAuthPrefix.clear(); settings.uploadToken = "fake-test-key"; settings.uploadFields["format"] = "json";
    MarkdownSettings decoded; QString error; QVERIFY(MarkdownSettingsStore::decode(MarkdownSettingsStore::encode(settings), decoded, &error));
    QCOMPARE(decoded.relativePhotoPath, settings.relativePhotoPath); QCOMPARE(decoded.absolutePhotoPath, settings.absolutePhotoPath); QCOMPARE(decoded.hostedLinks.size(), 2);
    QCOMPARE(decoded.hostedLinks[0].name, settings.hostedLinks[0].name); QCOMPARE(decoded.defaultHostedLink, 1); QCOMPARE(decoded.uploadFields, settings.uploadFields);
    QCOMPARE(decoded.uploadEndpoint, settings.uploadEndpoint); QVERIFY(decoded.uploadToken.isEmpty());
#ifdef Q_OS_WIN
    QVERIFY2(MarkdownSettingsStore::save(settings, &error), qPrintable(error)); const QString path = MarkdownSettingsStore::storagePath();
    MarkdownSettingsStore::setStoragePath(path); QCOMPARE(MarkdownSettingsStore::current().uploadToken, settings.uploadToken);
#else
    settings.uploadToken.clear(); QVERIFY(MarkdownSettingsStore::save(settings, &error));
#endif
    ImageStorage storage(root); QString reference; QVERIFY(storage.store("folder/doc.md", png(), reference, nullptr, &error));
    QVERIFY(reference.contains("../../custom%20images/folder/")); QVERIFY(QFileInfo::exists(MarkdownImages::localPath(reference, dir.filePath("md_data/folder/doc.md"))));
    QVERIFY(!QFileInfo::exists(dir.filePath("md_photo"))); QVERIFY(ImageStorage(root, ImageStorage::absolutePhotoRoot(root)).store("folder/doc.md", png(Qt::red), reference));
    QVERIFY(MarkdownImages::localPath(reference, dir.filePath("md_data/folder/doc.md")).startsWith(QDir::fromNativeSeparators(settings.absolutePhotoPath)));
    auto invalid = settings; invalid.relativePhotoPath = dir.path(); QVERIFY(!MarkdownSettingsStore::validate(invalid)); invalid = settings; invalid.hostedLinks[0].url = "file:///bad"; QVERIFY(!MarkdownSettingsStore::validate(invalid));
    QByteArray legacy("TMDP"); QDataStream stream(&legacy, QIODevice::Append); stream.setByteOrder(QDataStream::BigEndian); MarkdownSettings old;
    stream << quint16(1) << quint16(old.flags | (1 << 10)); for (auto number : old.numbers) stream << number;
    for (auto color : old.colors) stream << quint8(qRed(color)) << quint8(qGreen(color)) << quint8(qBlue(color));
    for (const auto &font : old.fonts) { const auto utf8 = font.toUtf8(); stream << quint16(utf8.size()); stream.writeRawData(utf8.constData(), utf8.size()); }
    legacy += QCryptographicHash::hash(legacy, QCryptographicHash::Sha256).left(4); QVERIFY(MarkdownSettingsStore::decode(QString::fromLatin1(legacy.toHex()), decoded, &error));
    QCOMPARE(decoded.imageMode, quint8(1)); QCOMPARE(decoded.relativePhotoPath, QString("../md_photo")); QVERIFY(decoded.uploadEndpoint.isEmpty());
    // Manage saved URLs without making a network request to an image host.
    ImageSettingsDialog dialog(root); dialog.show(); auto *tabs = dialog.findChild<QTabWidget *>("imageStorageModes"); QVERIFY(tabs); QCOMPARE(tabs->count(), 3);
    tabs->setCurrentIndex(2); auto *links = dialog.findChild<QListWidget *>("hostedImageLinks"); QCOMPARE(links->count(), 2);
    QCoreApplication::processEvents(); const QString previewDir = qEnvironmentVariable("TMD_UI_CHECK_DIRECTORY");
    if (!previewDir.isEmpty()) QVERIFY(dialog.grab().save(QDir(previewDir).filePath("image-host-settings.png")));
    dialog.findChild<QPushButton *>("newHostedImageLink")->click(); dialog.findChild<QLineEdit *>("hostedImageName")->setText("Third"); dialog.findChild<QLineEdit *>("hostedImageUrl")->setText("https://example.test/c.png");
    dialog.findChild<QPushButton *>("saveHostedImageLink")->click(); QCOMPARE(links->count(), 3); dialog.findChild<QPushButton *>("defaultHostedImageLink")->click();
    dialog.findChild<QPushButton *>("applyImageSettings")->click(); QCOMPARE(MarkdownSettingsStore::current().defaultHostedLink, 2);
    dialog.findChild<QLineEdit *>("hostedImageName")->setText("Renamed"); dialog.findChild<QPushButton *>("saveHostedImageLink")->click();
    QSignalSpy insert(&dialog, &ImageSettingsDialog::imageLinkRequested); dialog.findChild<QPushButton *>("insertHostedImageLink")->click(); QCOMPARE(insert.size(), 1);
    QCOMPARE(insert[0][0].toString(), QString("https://example.test/c.png")); QCOMPARE(insert[0][1].toString(), QString("Renamed"));
    ImageSettingsDialog remove(root); remove.findChild<QTabWidget *>("imageStorageModes")->setCurrentIndex(2); remove.findChild<QPushButton *>("deleteHostedImageLink")->click();
    remove.findChild<QPushButton *>("applyImageSettings")->click(); QCOMPARE(MarkdownSettingsStore::current().hostedLinks.size(), 2); QCOMPARE(MarkdownSettingsStore::current().defaultHostedLink, 0);
    remove.findChild<QLineEdit *>("relativeImageDirectory")->setText("../cancelled"); remove.reject(); QCOMPARE(MarkdownSettingsStore::current().relativePhotoPath, settings.relativePhotoPath);
    // Absolute mode must copy newly pasted local files to the configured folder.
    auto local = MarkdownSettingsStore::current(); local.imageMode = 1; local.uploadEndpoint.clear(); QVERIFY(MarkdownSettingsStore::save(local));
    QVERIFY(QDir().mkpath(dir.filePath("input"))); QVERIFY(MarkdownFileIO::writeFile(dir.filePath("input/pic.png"), QString()));
    QFile file(dir.filePath("input/pic.png")); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(png(Qt::yellow)); file.close();
    MainWindow window(root); window.show(); window.setSourceMode(true); auto *editor = window.findChild<MarkdownEditor *>();
    auto *mime = new QMimeData; mime->setUrls({QUrl::fromLocalFile(file.fileName())}); QApplication::clipboard()->setMimeData(mime); editor->paste();
    const auto images = MarkdownImages::parse(editor->source()); QCOMPARE(images.size(), 1); QVERIFY(images[0].source.contains("absolute%20images/folder/")); QVERIFY(QDir::isAbsolutePath(MarkdownImages::localPath(images[0].source, dir.filePath("md_data/folder/doc.md"))));
    QVERIFY(QFileInfo::exists(file.fileName())); editor->undo(); QApplication::clipboard()->clear(); window.close();
}

void TreeMdTests::fenceLanguageCompletionInSource() {
    MarkdownEditor editor; editor.resize(700, 450); editor.show(); editor.activateWindow(); QVERIFY(QTest::qWaitForWindowActive(&editor)); editor.setFocus();
    editor.setSource("# Keep\n\n"); editor.moveCursor(QTextCursor::End);
    auto *completer = editor.findChild<QCompleter *>("fenceLanguageCompleter"); QVERIFY(completer);
    QTest::keyClicks(&editor, "```"); QTRY_VERIFY(completer->popup()->isVisible());
    QVERIFY(completer->completionCount() > 20);
    QVERIFY(completer->completionModel()->index(1, 0).data().toString().contains("Python"));
    QTest::keyClicks(&editor, "py"); QTRY_COMPARE(completer->completionCount(), 1);
    QTest::keyClick(&editor, Qt::Key_Tab); QTRY_COMPARE(editor.source(), QString("# Keep\n\n```python\n\n```\n\n"));
    QVERIFY(!completer->popup()->isVisible());
    QVERIFY(editor.textCursor().block().text().isEmpty());
    editor.undo(); QCOMPARE(editor.source(), QString("# Keep\n\n```py"));
    editor.redo(); QCOMPARE(editor.source(), QString("# Keep\n\n```python\n\n```\n\n"));
    QTextCursor body(editor.document()); body.setPosition(editor.source().indexOf("python") + 7); editor.setTextCursor(body);
    QTest::keyClicks(&editor, "print(1)"); QCoreApplication::processEvents();
    QCOMPARE(editor.source(), QString("# Keep\n\n```python\nprint(1)\n```\n\n")); QVERIFY(!completer->popup()->isVisible());
    QTest::keyClick(&editor, Qt::Key_Down); QCOMPARE(editor.textCursor().position(), editor.source().size());
    QTest::keyClicks(&editor, "After"); QCOMPARE(editor.source(), QString("# Keep\n\n```python\nprint(1)\n```\n\nAfter"));
    editor.setSource("```ja"); editor.moveCursor(QTextCursor::End); QTRY_VERIFY(completer->popup()->isVisible());
    QCOMPARE(completer->completionCount(), 2); QTest::keyClick(&editor, Qt::Key_Down);
    QTest::keyClick(&editor, Qt::Key_Return); QCOMPARE(editor.source(), QString("```javascript\n\n```\n\n"));
}
void TreeMdTests::fenceLanguageCompletionInDocument() {
    QTemporaryDir dir; MarkdownManager manager(dir.path()); QVERIFY(manager.createDocument("README.md", "# Keep\n\nBody"));
    MainWindow window(dir.path()); window.show(); window.activateWindow(); QVERIFY(QTest::qWaitForWindowActive(&window));
    auto *view = window.findChild<MarkdownDocumentView *>(); auto *editor = window.findChild<MarkdownEditor *>();
    auto *completer = view->findChild<QCompleter *>("fenceLanguageCompleter"); QVERIFY(completer);
    view->setFocus(); view->moveCursor(QTextCursor::End); QTest::keyClick(view, Qt::Key_Return);
    QTest::keyClicks(view, "```sq"); QTRY_VERIFY(completer->popup()->isVisible()); QTRY_COMPARE(completer->completionCount(), 1);
    // Rendering must not hide the language header while the user chooses a type.
    QTest::qWait(550); QVERIFY(view->toPlainText().contains("```sq")); QVERIFY(completer->popup()->isVisible());
    QTest::keyClick(view, Qt::Key_Tab); QTRY_COMPARE(editor->source(), QString("# Keep\n\nBody\n\n```sql\n\n```\n\n"));
    QCOMPARE(view->markdownSource(), editor->source()); QVERIFY(!completer->popup()->isVisible());
    QVERIFY(view->textCursor().blockFormat().hasProperty(QTextFormat::BlockCodeFence));
    QTest::keyClicks(view, "SELECT 1;");
    QTRY_COMPARE(editor->source(), QString("# Keep\n\nBody\n\n```sql\nSELECT 1;\n```\n\n"));
    QTest::qWait(550); QVERIFY(view->toPlainText().contains("SELECT 1;")); QVERIFY(!completer->popup()->isVisible());
    const auto code = view->document()->find("SELECT 1;").block(); QVERIFY(code.blockFormat().hasProperty(QTextFormat::BlockCodeFence));
    QVERIFY(code.blockFormat().background().color().lightness() > view->palette().color(QPalette::Base).lightness());
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); QVERIFY(editor->source().startsWith("# Keep\n\nBody\n\n```sql\n"));
    QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier); QTRY_VERIFY(editor->source().contains("SELECT 1;\n```\n\n"));
    QTest::keyClick(view, Qt::Key_Down); QVERIFY(!view->textCursor().blockFormat().hasProperty(QTextFormat::BlockCodeFence));
    QTest::keyClicks(view, "After"); QTRY_COMPARE(editor->source(), QString("# Keep\n\nBody\n\n```sql\nSELECT 1;\n```\n\nAfter"));
    const QString captures = qEnvironmentVariable("TMD_UI_CHECK_DIRECTORY");
    if (!captures.isEmpty()) { QVERIFY(QDir().mkpath(captures)); QVERIFY(view->grab().save(QDir(captures).filePath("fenceCodeDocument.png"))); }
    window.hide();
}
void TreeMdTests::fencePopupKeyboardSelection_data() {
    QTest::addColumn<bool>("rendered"); QTest::addColumn<int>("confirmKey");
    QTest::newRow("source-return") << false << int(Qt::Key_Return);
    QTest::newRow("source-enter") << false << int(Qt::Key_Enter);
    QTest::newRow("document-return") << true << int(Qt::Key_Return);
    QTest::newRow("document-enter") << true << int(Qt::Key_Enter);
}
void TreeMdTests::fencePopupKeyboardSelection() {
    QFETCH(bool, rendered); QFETCH(int, confirmKey);
    MarkdownEditor editor; MarkdownDocumentView document;
    QTextEdit *target = rendered ? static_cast<QTextEdit *>(&document) : &editor;
    target->resize(700, 450); target->show(); target->activateWindow(); QVERIFY(QTest::qWaitForWindowActive(target)); target->setFocus();
    if (rendered) document.setMarkdownSource("keyboard.md", QString()); else editor.setSource(QString());
    auto source = [&] { return rendered ? document.markdownSource() : editor.source(); };
    auto *completer = target->findChild<QCompleter *>("fenceLanguageCompleter"); QVERIFY(completer);
    QTest::keyClicks(target, "```"); QTRY_VERIFY(completer->popup()->isVisible());
    auto *popup = completer->popup(); const int caret = target->textCursor().position();
    const int rows = completer->completionModel()->rowCount(); QVERIFY(rows > 20);
    // Native popup key delivery must not reach the editor's newline handler.
    QTest::keyClick(popup, Qt::Key_Down); QCOMPARE(popup->currentIndex().row(), 1);
    QTest::keyClick(popup, Qt::Key_Up); QCOMPARE(popup->currentIndex().row(), 0);
    QTest::keyClick(popup, Qt::Key_Up); QCOMPARE(popup->currentIndex().row(), rows - 1);
    QTest::keyClick(popup, Qt::Key_Down); QCOMPARE(popup->currentIndex().row(), 0);
    for (int row = 0; row < 9; ++row) QTest::keyClick(popup, Qt::Key_Down);
    QTest::qWait(550); QCOMPARE(popup->currentIndex().row(), 9);
    QVERIFY(popup->viewport()->rect().contains(popup->visualRect(popup->currentIndex()).center()));
    QCOMPARE(source(), QString("```")); QCOMPARE(target->textCursor().position(), caret);
    const QString language = popup->currentIndex().data(Qt::UserRole).toString(); QVERIFY(!language.isEmpty());
    QTest::keyClick(popup, Qt::Key(confirmKey));
    QCOMPARE(source(), "```" + language + "\n\n```\n\n"); QVERIFY(!popup->isVisible()); QVERIFY(target->hasFocus());
    QTest::keyClicks(target, "value"); QCOMPARE(source(), "```" + language + "\nvalue\n```\n\n");
    QTest::keyClick(target, Qt::Key_Down); QTest::keyClicks(target, "After");
    QCOMPARE(source(), "```" + language + "\nvalue\n```\n\nAfter");

    // Esc returns to text entry, and typing a prefix still filters candidates.
    if (rendered) document.setMarkdownSource("keyboard.md", QString()); else editor.setSource(QString());
    QTest::keyClicks(target, "```ja"); QTRY_COMPARE(completer->completionCount(), 2); QTRY_VERIFY(popup->isVisible());
    QTest::keyClick(popup, Qt::Key_Escape); QVERIFY(!popup->isVisible()); QCOMPARE(source(), QString("```ja"));
    QTest::keyClick(target, Qt::Key_Space, Qt::ControlModifier); QTRY_VERIFY(popup->isVisible());
    QTest::keyClick(popup, Qt::Key_Down); QTest::keyClick(popup, Qt::Key(confirmKey));
    QCOMPARE(source(), QString("```javascript\n\n```\n\n"));
}
void TreeMdTests::fenceCompletionRespectsCodeBoundaries() {
    MarkdownEditor editor; editor.resize(700, 450); editor.show(); editor.activateWindow(); QVERIFY(QTest::qWaitForWindowActive(&editor)); editor.setFocus();
    auto *completer = editor.findChild<QCompleter *>("fenceLanguageCompleter"); QVERIFY(completer);
    for (const auto &source : {"Inline ```", "    ```", "```python\nvalue\n```", "~~~bash\n```", "```\n```py"}) {
        editor.setSource(source); editor.moveCursor(QTextCursor::End); QCoreApplication::processEvents();
        QVERIFY2(!completer->popup()->isVisible(), source);
    }
    editor.setSource("```python\nx\n```\n\n  ```C"); editor.moveCursor(QTextCursor::End);
    QTRY_VERIFY(completer->popup()->isVisible()); QVERIFY(completer->completionCount() >= 3);
    QTest::keyClick(&editor, Qt::Key_Escape); QTRY_VERIFY(!completer->popup()->isVisible());
    QCOMPARE(editor.source(), QString("```python\nx\n```\n\n  ```C"));
    QTest::keyClick(&editor, Qt::Key_Space, Qt::ControlModifier); QTRY_VERIFY(completer->popup()->isVisible());
    QTest::keyClick(&editor, Qt::Key_Tab); QVERIFY(editor.source().endsWith("  ```cpp\n\n  ```\n\n"));
    editor.setSource("~~~rust"); editor.moveCursor(QTextCursor::End); QTRY_VERIFY(completer->popup()->isVisible());
    QTest::keyClick(&editor, Qt::Key_Escape); QTest::keyClick(&editor, Qt::Key_Return);
    QTest::keyClicks(&editor, "```sql"); QCoreApplication::processEvents(); QVERIFY(!completer->popup()->isVisible());
    editor.setSource("```unknown-language"); editor.moveCursor(QTextCursor::End); QCoreApplication::processEvents();
    QVERIFY(!completer->popup()->isVisible()); QCOMPARE(editor.source(), QString("```unknown-language"));
}

void TreeMdTests::fenceCompletionPreservesExistingBlocks() {
    MarkdownEditor editor; editor.resize(700, 450); editor.show(); editor.activateWindow(); QVERIFY(QTest::qWaitForWindowActive(&editor)); editor.setFocus();
    auto *completer = editor.findChild<QCompleter *>("fenceLanguageCompleter"); QVERIFY(completer);
    const QList<QPair<QString, QString>> cases = {
        {"```py\nprint(1)\n```\n\nAfter", "```python\nprint(1)\n```\n\nAfter"},
        {"```py\n```", "```python\n\n```\n\n"},
        {"```py\nprint(1)", "```python\nprint(1)\n```\n\n"},
        {"  ````py\n```\nkeep", "  ````python\n```\nkeep\n  ````\n\n"},
        {"~~~ru\ntext\n~~~", "~~~rust\ntext\n~~~\n\n"},
        {"````py\nx\n`````\nAfter", "````python\nx\n`````\n\nAfter"},
        {"```py\n\n```\n\n```sql\nSELECT 1;\n```\n\nAfter", "```python\n\n```\n\n```sql\nSELECT 1;\n```\n\nAfter"}
    };
    for (const auto &entry : cases) {
        editor.setSource(entry.first); QTextCursor cursor(editor.document()); cursor.setPosition(entry.first.indexOf('\n')); editor.setTextCursor(cursor);
        QTRY_VERIFY(completer->popup()->isVisible()); QTest::keyClick(&editor, Qt::Key_Tab); QCOMPARE(editor.source(), entry.second);
        editor.undo(); QCOMPARE(editor.source(), entry.first); editor.redo(); QCOMPARE(editor.source(), entry.second);
        const auto fences = MarkdownParser::codeFences(editor.source()); QVERIFY(!fences.isEmpty()); QVERIFY(fences.front().closed());
        QVERIFY(fences.front().closeStart > fences.front().bodyStart);
        QCOMPARE(fences.size(), MarkdownParser::codeFences(entry.first).size());
    }
}
void TreeMdTests::emptyFencedBlocksKeepTypingInside() {
    for (const QString &prefix : {QString(), QString("# Before\n\n")}) {
        const QString source = prefix + "```python\n\n```\n\n";
        MarkdownDocumentView view; view.resize(700, 500); view.show(); view.setMarkdownSource("empty.md", source);
        QTextBlock code;
        for (auto block = view.document()->begin(); block.isValid(); block = block.next())
            if (block.blockFormat().hasProperty(QTextFormat::BlockCodeFence)) { code = block; break; }
        QVERIFY(code.isValid()); QVERIFY(code.text().isEmpty()); view.setTextCursor(QTextCursor(code)); QTest::keyClicks(&view, "print(1)");
        QCOMPARE(view.markdownSource(), prefix + "```python\nprint(1)\n```\n\n");
        QTest::keyClick(&view, Qt::Key_Return); QTest::keyClicks(&view, "print(2)");
        QCOMPARE(view.markdownSource(), prefix + "```python\nprint(1)\nprint(2)\n```\n\n");
        QTest::qWait(550); view.moveCursor(QTextCursor::End);
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, view.cursorRect().center() + QPoint(8, 6));
        QVERIFY(!view.textCursor().blockFormat().hasProperty(QTextFormat::BlockCodeFence)); QTest::keyClicks(&view, "After");
        QCOMPARE(view.markdownSource(), prefix + "```python\nprint(1)\nprint(2)\n```\n\nAfter");
    }
    MarkdownDocumentView unclosed; unclosed.setMarkdownSource("unclosed.md", "~~~bash\n```\n\n");
    QVERIFY(unclosed.document()->lastBlock().blockFormat().hasProperty(QTextFormat::BlockCodeFence));
    QTemporaryDir dir; MarkdownManager manager(dir.path()); QVERIFY(manager.createDocument("README.md", "Body"));
    MainWindow window(dir.path()); window.show(); window.activateWindow(); QVERIFY(QTest::qWaitForWindowActive(&window));
    auto *view = window.findChild<MarkdownDocumentView *>(); auto *editor = window.findChild<MarkdownEditor *>();
    view->setFocus(); view->moveCursor(QTextCursor::End); QTest::keyClick(view, Qt::Key_Return); QTest::keyClicks(view, "```py");
    auto *completer = view->findChild<QCompleter *>("fenceLanguageCompleter"); QTRY_VERIFY(completer->popup()->isVisible());
    const QString before = editor->source(); QTest::keyClick(view, Qt::Key_Tab); const QString completed = editor->source();
    QVERIFY(completed.endsWith("```python\n\n```\n\n"));
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); QCOMPARE(editor->source(), before);
    QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier); QCOMPARE(editor->source(), completed); window.hide();
}

void TreeMdTests::inlineCodeCursorCanLeaveAndReenter() {
    MarkdownDocumentView view; view.resize(700, 450); view.show(); view.setMarkdownSource("inline.md", "Start `code`");
    QTextCursor cursor = view.document()->find("code"); cursor.setPosition(cursor.selectionEnd()); view.setTextCursor(cursor);
    QTest::keyClicks(&view, "X"); QCOMPARE(view.markdownSource(), QString("Start `codeX`"));
    QTest::keyClick(&view, Qt::Key_Right); QVERIFY(!view.currentCharFormat().fontFixedPitch());
    QTest::keyClick(&view, Qt::Key_Left); QVERIFY(view.currentCharFormat().fontFixedPitch());
    QTest::keyClicks(&view, "Y"); QCOMPARE(view.markdownSource(), QString("Start `codeXY`"));
    QTest::keyClick(&view, Qt::Key_Right); QVERIFY(!view.currentCharFormat().fontFixedPitch());
    // A delayed render must keep the caret outside the closing backtick.
    QTest::qWait(550); QVERIFY(!view.currentCharFormat().fontFixedPitch());
    QTest::keyClicks(&view, "-after"); QCOMPARE(view.markdownSource(), QString("Start `codeXY`-after"));
    QVERIFY(!view.document()->find("after").charFormat().fontFixedPitch());
    cursor = view.document()->find("codeXY"); cursor.setPosition(cursor.selectionStart()); view.setTextCursor(cursor);
    QTest::keyClick(&view, Qt::Key_Right); QVERIFY(view.currentCharFormat().fontFixedPitch());
    QTest::keyClick(&view, Qt::Key_Left); QVERIFY(!view.currentCharFormat().fontFixedPitch());
    QTest::keyClicks(&view, "before "); QCOMPARE(view.markdownSource(), QString("Start before `codeXY`-after"));
}
void TreeMdTests::inlineCodeMouseCanTypeAfter() {
    for (const QString &source : {QString("`code`"), QString("Start `code`"), QString("Start `code` tail"), QString("# Heading `code`")}) {
        MarkdownDocumentView view; view.resize(700, 450); view.show(); view.setMarkdownSource("inline.md", source);
        QTextCursor cursor = view.document()->find("code"); cursor.setPosition(cursor.selectionEnd()); view.setTextCursor(cursor);
        const QRect end = view.cursorRect(); QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(end.left() + 1, end.center().y()));
        QVERIFY(!view.currentCharFormat().fontFixedPitch()); QTest::keyClicks(&view, "after");
        QString expected = source; expected.replace("`code`", "`code`after"); QCOMPARE(view.markdownSource(), expected);
        view.setMarkdownSource("inline.md", source);
        cursor = view.document()->find("code"); cursor.setPosition(cursor.selectionStart()); view.setTextCursor(cursor);
        const QRect begin = view.cursorRect(); QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(begin.left() + 1, begin.center().y()));
        QVERIFY(view.currentCharFormat().fontFixedPitch()); QTest::keyClicks(&view, "X");
        expected = source; expected.replace("`code`", "`Xcode`"); QCOMPARE(view.markdownSource(), expected);
    }
}

void TreeMdTests::treeLayoutRemainsStableWhenCenterChanges() {
    QTemporaryDir dir; MarkdownManager manager(dir.path());
    for (const auto &path : {"Http.md", "Docker.md", "MySQL.md", "Web.md", "SQL.md", "Curl.md", "Other.md", "Prometheus.md", "Backup.md"})
        QVERIFY(manager.createDocument(path, "# " + QString(path)));
    QVERIFY(manager.addRelation("Http.md", "Web.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("Web.md", "SQL.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("MySQL.md", "SQL.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("Docker.md", "SQL.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("SQL.md", "Other.md", NodeRelationType::Next));
    QVERIFY(manager.addRelation("SQL.md", "Curl.md", NodeRelationType::Parent));
    QVERIFY(manager.addRelation("SQL.md", "Prometheus.md", NodeRelationType::Child));
    QVERIFY(manager.addRelation("SQL.md", "Backup.md", NodeRelationType::Child));
    KnowledgeGraphWidget graph; graph.setGraph(&manager, "SQL.md");
    QMap<QString, QRectF> original;
    for (const auto &path : graph.index().paths) original[path] = graph.nodeRect(path);
    auto routes = [](const KnowledgeTreeCanvas &canvas) {
        QMap<QString, QPainterPath> result;
        for (auto *item : canvas.scene()->items()) if (auto *line = dynamic_cast<QGraphicsPathItem *>(item)) result[line->toolTip()] = line->path();
        return result;
    };
    const auto originalRoutes = routes(graph);
    KnowledgeTreeCanvas overview; overview.resize(1000, 650); overview.setGraph(&manager, "MySQL.md"); overview.show(); QCoreApplication::processEvents();
    for (const auto &path : overview.index().paths)
        QVERIFY(overview.viewport()->rect().contains(overview.mapFromScene(overview.nodeRect(path)).boundingRect()));
    const QPoint fixedSqlPosition = overview.mapFromScene(overview.nodeRect("SQL.md").center());
    overview.setGraph(&manager, "SQL.md"); QCOMPARE(overview.mapFromScene(overview.nodeRect("SQL.md").center()), fixedSqlPosition);
    RelationDialog dialog(&manager, "SQL.md"); dialog.show(); QCoreApplication::processEvents();
    auto *managed = dialog.findChild<KnowledgeTreeCanvas *>(); QVERIFY(managed);
    managed->centerOn(managed->mapToScene(managed->viewport()->rect().center()) + QPointF(40, 0));
    const QPoint managedSqlPosition = managed->mapFromScene(managed->nodeRect("SQL.md").center());
    dialog.setCurrentNode("MySQL.md"); QCOMPARE(managed->mapFromScene(managed->nodeRect("SQL.md").center()), managedSqlPosition);
    for (const auto &center : {"MySQL.md", "Http.md", "Curl.md", "Prometheus.md", "SQL.md"}) {
        graph.setGraph(&manager, center); QCOMPARE(graph.index().paths.size(), original.size());
        for (const auto &path : graph.index().paths) QCOMPARE(graph.nodeRect(path), original.value(path));
        QCOMPARE(routes(graph), originalRoutes);
        KnowledgeTreeCanvas independent; independent.setGraph(&manager, center);
        for (const auto &path : graph.index().paths) QCOMPARE(independent.nodeRect(path), original.value(path));
    }
    const auto sql = original.value("SQL.md"), mysql = original.value("MySQL.md"), curl = original.value("Curl.md");
    QVERIFY(mysql.right() < sql.left()); QVERIFY(sql.left() - mysql.right() < 200);
    QCOMPARE(curl.bottom(), sql.top()); QCOMPARE(curl.center().x(), sql.center().x());
    QCOMPARE(original.value("Prometheus.md").top(), original.value("Backup.md").top());
    QVERIFY(original.value("Prometheus.md").top() > sql.bottom());
    const auto stored = manager.explicitRelations();
    for (int i = 0; i < 12; ++i) {
        const QString path = QString("long/%1.md").arg(i); QVERIFY(manager.createDocument(path, "# Route"));
        QVERIFY(manager.addRelation(i ? QString("long/%1.md").arg(i - 1) : "Other.md", path, NodeRelationType::Next));
    }
    graph.setGraph(&manager, "SQL.md"); original.clear();
    for (const auto &path : graph.index().paths) original[path] = graph.nodeRect(path);
    graph.setGraph(&manager, "long/3.md");
    for (const auto &path : graph.index().paths) if (original.contains(path)) QCOMPARE(graph.nodeRect(path), original.value(path));
    QVERIFY(!graph.index().paths.contains("long/9.md"));
    KnowledgeTreeCanvas reopened; reopened.setGraph(&manager, "long/3.md");
    for (const auto &path : graph.index().paths) QCOMPARE(reopened.nodeRect(path), graph.nodeRect(path));
    for (auto it = stored.cbegin(); it != stored.cend(); ++it)
        for (auto target = it->cbegin(); target != it->cend(); ++target) QCOMPARE(manager.explicitRelations().value(it.key()).value(target.key()), target.value());
}

int main(int argc, char **argv) {
    QApplication application(argc, argv); applyTreeMdTheme(application);
    TreeMdTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "TreeMdTests.moc"

