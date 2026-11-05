#include "core/library/MarkdownManager.h"
#include "core/library/ExternalDocuments.h"
#include "core/library/FileWatcher.h"
#include "core/library/MarkdownFileIO.h"
#include "core/library/LibraryReferences.h"
#include "core/knowledge/KnowledgeIndex.h"
#include "core/settings/MarkdownSettings.h"
#include "ui/MainWindow.h"
#include "ui/common/Theme.h"
#include "ui/document/MarkdownEditor.h"
#include "ui/document/MarkdownDocumentView.h"
#include "ui/library/DocumentFileTree.h"
#include "ui/library/DocumentTreeBuilder.h"
#include "ui/library/DocumentSidebar.h"
#include <QAction>
#include <QAbstractButton>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QMenu>
#include <QMimeData>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QElapsedTimer>
#include <QScrollBar>
#include <QTreeWidgetItemIterator>
#include <QtTest>

namespace {
bool put(const QString &path, const QString &text) {
    return QDir().mkpath(QFileInfo(path).absolutePath()) && MarkdownFileIO::writeFile(path, text);
}
QString read(const QString &path) { QString text; MarkdownFileIO::readFile(path, text); return text; }
bool capture(QWidget *widget, const QString &name) {
    const auto folder=qEnvironmentVariable("TMD_EXTERNAL_QA_DIR");
    return folder.isEmpty() || (QDir().mkpath(folder) && widget->grab().save(QDir(folder).filePath(name+".png")));
}
struct Library {
    QTemporaryDir temp;
    QString root() const { return temp.filePath("md_data"); }
    QString outside(const QString &name = "external.md") const { return temp.filePath("outside/" + name); }
    bool init() { return put(root() + "/local.md", "# Local\n\nKeep this library document.\n"); }
};
QTreeWidgetItem *itemFor(QTreeWidget *tree, const QString &path) {
    QTreeWidgetItemIterator it(tree);
    while (*it) { if ((*it)->data(0, Qt::UserRole).toString() == path) return *it; ++it; }
    return nullptr;
}
class Answer : public QTimer {
public:
    explicit Answer(int result) {
        setInterval(5);
        connect(this, &QTimer::timeout, this, [this, result] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box) return;
            if (box->objectName()=="externalDocumentBackupPrompt")
                capture(box,QString(QTest::currentTestFunction())+"-prompt");
            // Static QMessageBox helpers inspect clickedButton(), not only
            // QDialog::result(). Click the real button as a user would.
            if (auto *button=box->button(static_cast<QMessageBox::StandardButton>(result))) {
                ++count; button->click();
            }
        });
        start();
    }
    int count = 0;
};
}
class ExternalDocumentTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
private slots:
    void initTestCase() {
        QVERIFY(settings.isValid()); QCoreApplication::setOrganizationName("TmdExternalDocumentTests");
        QCoreApplication::setApplicationName("IsolatedTests"); QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        applyTreeMdTheme(*qApp);
    }
    void init() {
        QSettings().clear(); MarkdownSettingsStore::setStoragePath(settings.filePath(QString(QTest::currentTestFunction()) + ".txt"));
    }
    void externalEditingPreservesOriginalLocationAndSource() {
        Library lib; QVERIFY(lib.init());
        const auto path = lib.outside(QStringLiteral("含空格 & 符号.MD"));
        const QString source = "# Outside\n\n    typed indentation\n\n`literal`\n";
        QVERIFY(put(path, source));
        MainWindow window(lib.root()); window.show(); Answer no(QMessageBox::No);
        QVERIFY(window.openFile(QUrl::fromLocalFile(path).toString())); QCOMPARE(no.count, 1);
        auto *manager = window.findChild<MarkdownManager *>();
        auto *editor = window.findChild<MarkdownEditor *>(); auto *view = window.findChild<MarkdownDocumentView *>();
        const auto absolute = MarkdownFileIO::normalizedPath(path);
        QCOMPARE(editor->source(), source); QCOMPARE(view->markdownSource(), source);
        QCOMPARE(manager->allNodes().size(), 1); QCOMPARE(manager->externalNodes().size(), 1);
        QVERIFY(manager->loadNode(absolute)->isExternalDocument());
        QVERIFY(!window.findChild<QAction *>("renameAction")->isEnabled());
        QVERIFY(!window.findChild<QAction *>("deleteAction")->isEnabled());
        auto *tree = window.findChild<DocumentFileTree *>(); auto *item = itemFor(tree, absolute); QVERIFY(item);
        QVERIFY(item->text(0).contains(path)); QCOMPARE(item->parent()->text(0), QStringLiteral("外部文档"));
        QVERIFY(!item->flags().testFlag(Qt::ItemIsDragEnabled));
        std::unique_ptr<QMimeData> mime(tree->mimeData({item})); QVERIFY(!mime->hasFormat("application/x-tmd-library-item"));
        editor->moveCursor(QTextCursor::End); editor->insertPlainText("appended\n");
        QVERIFY(manager->modifiedDocuments().contains(absolute));
        window.findChild<QAction *>("saveAction")->trigger();
        QCOMPARE(read(path), source + "appended\n"); QCOMPARE(manager->modifiedDocuments().size(), 0);
        QCOMPARE(read(lib.root()+"/local.md"), QString("# Local\n\nKeep this library document.\n"));
        QVERIFY(!QFileInfo::exists(lib.root()+"/"+QFileInfo(path).fileName()));
        QVERIFY(KnowledgeIndex::build(*manager, absolute).members.isEmpty());
        QString error; QVERIFY(!manager->addRelation(absolute, "local.md", NodeRelationType::Child, &error));
        QVERIFY(!manager->setRelation("local.md", absolute, NodeRelationType::Child, &error));
    }
    void backupCopiesToRootWithoutOverwritingNamesAndRebasesImages() {
        Library lib; QVERIFY(lib.init()); QVERIFY(put(lib.root()+"/nested/report.md", "existing"));
        const auto path = lib.outside("report.md");
        QImage image(12,12,QImage::Format_RGB32); image.fill(Qt::green);
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath())); QVERIFY(image.save(lib.temp.filePath("outside/picture.png")));
        QVERIFY(put(lib.temp.filePath("outside/linked.md"), "linked"));
        const auto body = QString("# Report\n\n![picture](picture.png)\n\n[linked](linked.md)\n\n![missing](missing.png)\n");
        QVERIFY(put(path, body));
        MainWindow window(lib.root()); window.show(); Answer yes(QMessageBox::Yes);
        QVERIFY(window.openFile(path)); QCOMPARE(yes.count,1);
        auto *manager=window.findChild<MarkdownManager *>();
        QCOMPARE(manager->externalNodes().size(),0); QCOMPARE(manager->allNodes().size(),3);
        auto *node=manager->loadNode(QStringLiteral("report（备份2）.md")); QVERIFY(node);
        QCOMPARE(read(path),body); QCOMPARE(read(lib.root()+"/nested/report.md"),QString("existing"));
        QCOMPARE(QFileInfo(node->filePath()).absolutePath(),MarkdownFileIO::normalizedPath(lib.root()));
        QVERIFY(QFile::remove(lib.temp.filePath("outside/picture.png")));
        const auto missing=LibraryReferences::missing(node->content(),lib.root(),node->filePath());
        QCOMPARE(missing.size(),1); QVERIFY(missing.front().contains("missing.png"));
        QVERIFY(window.findChild<MarkdownEditor *>()->source().contains("linked.md"));
    }
    void cancelledAndInternalOpenDoNotCreateExternalRecords() {
        Library lib; QVERIFY(lib.init()); QVERIFY(put(lib.outside(),"outside"));
        MainWindow window(lib.root()); window.show(); Answer cancel(QMessageBox::Cancel);
        QVERIFY(!window.openFile(lib.outside())); QCOMPARE(cancel.count,1);
        QVERIFY(window.openFile(lib.root()+"/local.md")); QCOMPARE(cancel.count,1);
        auto *manager=window.findChild<MarkdownManager *>(); QCOMPARE(manager->externalNodes().size(),0);
        QVERIFY(!QFileInfo::exists(lib.root()+"/.tree-md-external.json"));
    }
    void originalRelativeImagesAndLineEndingsRemainEditable() {
        Library lib; QVERIFY(lib.init()); const auto path=lib.outside(QStringLiteral("外部文档 示例.md"));
        QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
        QImage image(120,70,QImage::Format_RGB32); image.fill(QColor("#4e9364"));
        QVERIFY(image.save(lib.temp.filePath("outside/picture.png")));
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
        const auto original=QStringLiteral("# 外部文档\r\n\r\n正文与相对路径图片。\r\n\r\n![图片](picture.png)\r\n").toUtf8();
        QCOMPARE(file.write(original),original.size()); file.close();
        MainWindow window(lib.root()); window.resize(1250,800); window.show(); Answer no(QMessageBox::No);
        QVERIFY(window.openFile(path));
        auto *view=window.findChild<MarkdownDocumentView *>();
        QTRY_COMPARE_WITH_TIMEOUT(view->loadedImageCount(),1,5000); QVERIFY(view->imageErrors().isEmpty());
        QVERIFY(capture(&window,"external-document-normal"));
        auto *manager=window.findChild<MarkdownManager *>(); manager->updateContent("local.md","unsaved library edit");
        auto *editor=window.findChild<MarkdownEditor *>(); editor->moveCursor(QTextCursor::End); editor->insertPlainText("extra\n");
        window.findChild<QAction *>("saveAction")->trigger();
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),original+QByteArray("extra\r\n")); file.close();
        QVERIFY(manager->loadNode("local.md")->isModified()); QVERIFY(!read(lib.root()+"/local.md").contains("unsaved library edit"));
        QVERIFY(QFile::remove(path));
        QTRY_VERIFY_WITH_TIMEOUT(manager->loadNode(path)->isSourceMissing(),5000);
        QVERIFY(capture(&window,"external-document-missing"));
    }
    void backupUsesPendingExternalBufferAndSaveAsRebasesAssets() {
        Library lib; QVERIFY(lib.init()); const auto path=lib.outside("draft.md");
        QVERIFY(put(path,"disk text\n"));
        QImage image(12,12,QImage::Format_RGB32); image.fill(Qt::blue);
        QVERIFY(image.save(lib.temp.filePath("outside/picture.png")));
        MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); QVERIFY(manager.openExternalDocument(path));
        const QString pending="# Pending\n\n![image](picture.png)\n"; manager.updateContent(path,pending);
        QString error; QVERIFY(manager.copyDocument(path,"saved-as.md",pending,&error));
        QCOMPARE(read(path),QString("disk text\n")); QVERIFY(manager.loadNode(path)->isModified());
        QVERIFY(QFile::remove(path));
        const auto backup=manager.backupExternalDocument(path,&error); QVERIFY2(!backup.isEmpty(),qPrintable(error));
        QVERIFY(manager.loadNode(backup)->content().contains("# Pending"));
        QVERIFY(!manager.loadNode(backup)->hasMissingReferences());
        QVERIFY(!manager.loadNode("saved-as.md")->hasMissingReferences()); QVERIFY(!QFileInfo::exists(path));
        QCOMPARE(manager.externalNodes().size(),1); QVERIFY(manager.loadNode(path)->isModified());
    }
    void watcherMarksMissingRetainsEditsAndRecovers() {
        Library lib; QVERIFY(lib.init()); const auto path=lib.outside(); QVERIFY(put(path,"# Original\n"));
        MainWindow window(lib.root()); window.show(); Answer no(QMessageBox::No); QVERIFY(window.openFile(path));
        auto *manager=window.findChild<MarkdownManager *>(); const auto absolute=MarkdownFileIO::normalizedPath(path);
        auto *editor=window.findChild<MarkdownEditor *>(); auto *tree=window.findChild<DocumentFileTree *>();
        editor->moveCursor(QTextCursor::End); editor->insertPlainText("pending");
        QVERIFY(QFile::rename(path,path+".moved"));
        QTRY_VERIFY_WITH_TIMEOUT(manager->loadNode(absolute)->isSourceMissing(),5000);
        QVERIFY(editor->source().contains("pending")); QVERIFY(!QFileInfo::exists(path));
        auto *item=itemFor(tree,absolute); QVERIFY(item);
        QVERIFY(!item->data(0,DocumentTreeBuilder::MissingReferencesRole).toStringList().isEmpty());
        QCOMPARE(item->foreground(0).color(),QColor("#ef6464"));
        QString error; QVERIFY(!manager->saveDocument(absolute,&error)); QVERIFY(!QFileInfo::exists(path));
        QVERIFY(QFile::rename(path+".moved",path));
        QTRY_VERIFY_WITH_TIMEOUT(!manager->loadNode(absolute)->isSourceMissing(),5000);
        QVERIFY(editor->source().contains("pending"));
        QVERIFY(manager->saveDocument(absolute,&error)); QVERIFY(read(path).contains("pending"));
        const auto folder=QFileInfo(path).absolutePath(), moved=folder+"-moved";
        QVERIFY2(QDir().rename(folder,moved),"An open external document must not lock its parent folder");
        QTRY_VERIFY_WITH_TIMEOUT(manager->loadNode(absolute)->isSourceMissing(),5000);
        QVERIFY(editor->source().contains("pending"));
        QVERIFY(QDir().rename(moved,folder));
        QTRY_VERIFY_WITH_TIMEOUT(!manager->loadNode(absolute)->isSourceMissing(),5000);
    }
    void externalChangesAndSaveAllProtectPendingText() {
        Library lib; QVERIFY(lib.init()); const auto path=lib.outside(); QVERIFY(put(path,"old\n"));
        MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); QVERIFY(manager.openExternalDocument(path));
        const auto absolute=MarkdownFileIO::normalizedPath(path); manager.updateContent(absolute,"pending\n");
        QVERIFY(put(path,"another program\n")); manager.findChild<ExternalDocuments *>()->refresh();
        QCOMPARE(manager.loadNode(absolute)->content(),QString("pending\n")); QVERIFY(manager.loadNode(absolute)->hasExternalChange());
        const auto all=manager.saveAllModified(); QVERIFY(!all.success()); QCOMPARE(read(path),QString("another program\n"));
        QString error; QVERIFY(manager.saveDocument(absolute,&error,true)); QCOMPARE(read(path),QString("pending\n"));
        QVERIFY(put(path,"clean update\n")); manager.findChild<ExternalDocuments *>()->refresh();
        QCOMPARE(manager.loadNode(absolute)->content(),QString("clean update\n")); QVERIFY(!manager.loadNode(absolute)->isModified());
    }
    void recordsSurviveRestartAndConcurrentOpens() {
        Library lib; QVERIFY(lib.init()); const auto a=lib.outside("same.md"), b=lib.temp.filePath("other/same.md");
        QVERIFY(put(a,"a")); QVERIFY(put(b,"b"));
        MarkdownManager first(lib.root()),second(lib.root()); QVERIFY(first.scan()); QVERIFY(second.scan());
        QVERIFY(first.openExternalDocument(a)); QVERIFY(second.openExternalDocument(b));
        MarkdownManager third(lib.root()); QCOMPARE(third.externalNodes().size(),2);
        QVERIFY(third.scan()); QVERIFY(third.duplicateDocuments().isEmpty());
        QVERIFY(QFile::remove(a));
        MarkdownManager fourth(lib.root()); QCOMPARE(fourth.externalNodes().size(),2);
        QVERIFY(fourth.loadNode(a)->isSourceMissing());
        QString error; QVERIFY(fourth.forgetExternalDocument(a,&error)); QVERIFY(QFileInfo::exists(b));
        MarkdownManager fifth(lib.root()); QCOMPARE(fifth.externalNodes().size(),1);
    }
    void invalidSourcesAndCorruptRecordsAreNotOverwritten() {
        Library lib; QVERIFY(lib.init()); const auto path=lib.outside(); QVERIFY(put(path,"valid"));
        const auto record=lib.root()+"/.tree-md-external.json"; QVERIFY(put(record,"corrupt"));
        MarkdownManager manager(lib.root()); QString error;
        QVERIFY(!manager.openExternalDocument(path,&error)); QCOMPARE(read(record),QString("corrupt"));
        QVERIFY(!manager.openExternalDocument(lib.root()+"/local.md",&error));
        QVERIFY(!manager.openExternalDocument(lib.outside("no.md"),&error));
        QVERIFY(put(lib.outside("notes.txt"),"text")); QVERIFY(!manager.openExternalDocument(lib.outside("notes.txt"),&error));
    }
    void dirtyMissingRecordCanBeDiscardedWithoutRecreatingFile() {
        Library lib; QVERIFY(lib.init()); const auto path=lib.outside(); QVERIFY(put(path,"baseline"));
        MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); QVERIFY(manager.openExternalDocument(path));
        manager.updateContent(path,"pending"); QVERIFY(QFile::remove(path));
        QString error; QVERIFY(!manager.forgetExternalDocument(path,&error));
        QVERIFY(manager.forgetExternalDocument(path,&error,true)); QVERIFY(!QFileInfo::exists(path));
        QVERIFY(manager.externalNodes().isEmpty());
    }
    void contextRemovalOfCurrentExternalDocumentKeepsUiResponsive_data() {
        QTest::addColumn<bool>("missing");
        QTest::newRow("present")<<false;
        QTest::newRow("moved-and-red")<<true;
    }
    void watcherCancellationRetainsBuffersUntilCompletion() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        QTimer heartbeat; heartbeat.setInterval(10); QSignalSpy beats(&heartbeat,&QTimer::timeout); heartbeat.start();
        for(int i=0;i<40;++i) {
            auto watcher=std::make_unique<FileWatcher>(directory.path(),nullptr,false);
            QVERIFY(put(directory.filePath("change.txt"),QString::number(i)));
            QElapsedTimer elapsed; elapsed.start();
            watcher->suspend();watcher.reset();
            QVERIFY2(elapsed.elapsed()<500,"Cancelling a directory watch must not wait for kernel I/O on the GUI thread");
            QTest::qWait(10);
        }
        QTRY_VERIFY_WITH_TIMEOUT(beats.size()>=20,1500);
        FileWatcher active(directory.path(),nullptr,false);QSignalSpy changes(&active,&FileWatcher::changed);
        QVERIFY(put(directory.filePath("change.txt"),"watching still works"));
        QTRY_VERIFY_WITH_TIMEOUT(!changes.isEmpty(),3000);
    }
    void contextRemovalOfCurrentExternalDocumentKeepsUiResponsive() {
        QFETCH(bool,missing);
        Library lib; QVERIFY(lib.init()); const auto path=lib.outside();
        for(int i=0;i<45;++i)QVERIFY(put(lib.root()+QString("/folder/note%1.md").arg(i),"# Note\n"));
        QVERIFY(put(path,"# External\n\nKeep the original.\n"));
        MainWindow window(lib.root()); window.show(); Answer no(QMessageBox::No);
        QVERIFY(window.openFile(path)); no.stop();
        auto *manager=window.findChild<MarkdownManager *>();
        auto *tree=window.findChild<DocumentFileTree *>();
        auto *sidebar=window.findChild<DocumentSidebar *>();
        const auto retained=path+".moved";
        if(missing){
            QVERIFY(QFile::rename(path,retained));
            QTRY_VERIFY_WITH_TIMEOUT(manager->loadNode(path)->isSourceMissing(),5000);
        }
        auto *item=itemFor(tree,MarkdownFileIO::normalizedPath(path)); QVERIFY(item);
        tree->scrollToItem(item);tree->verticalScrollBar()->setValue(tree->verticalScrollBar()->maximum());
        tree->scrollToItem(item);QTest::qWait(50);
        const QPoint point=tree->visualItemRect(item).center();
        QVERIFY(QMetaObject::invokeMethod(tree,"customContextMenuRequested",Qt::DirectConnection,Q_ARG(QPoint,point)));
        auto *menu=sidebar->findChild<QMenu *>("documentContextMenu"); QVERIFY(menu);
        auto *forget=menu->findChild<QAction *>("contextForgetExternal"); QVERIFY(forget);
        qInfo()<<"Removing current external document through context action";
        forget->trigger(); menu->hide();
        QTRY_VERIFY_WITH_TIMEOUT(manager->externalNodes().isEmpty(),3000);
        QCOMPARE(read(missing?retained:path),QString("# External\n\nKeep the original.\n"));
        if(missing)QVERIFY(!QFileInfo::exists(path));
        QCOMPARE(window.findChild<MarkdownEditor *>()->source(),manager->allNodes().front()->content());
        QSignalSpy refreshes(manager,&MarkdownManager::documentsChanged);
        QTest::qWait(3500);
        qInfo()<<"Idle library refreshes:"<<refreshes.size();
        QVERIFY2(refreshes.size()<=2,"Removing an external record must not start a perpetual library refresh loop");
        QVERIFY(window.openDocument("local.md"));
        QVERIFY(window.close());
        MainWindow reopened(lib.root());reopened.show();QTest::qWait(2200);
        QVERIFY(reopened.findChild<MarkdownManager *>()->externalNodes().isEmpty());
        QVERIFY(reopened.close());
    }
    void viewportDropUsesSameBackupQuestionAndSavePrompt() {
        Library lib; QVERIFY(lib.init()); const auto path=lib.outside(QStringLiteral("拖入 文件.md")); QVERIFY(put(path,"# Dropped\n"));
        MainWindow window(lib.root()); window.resize(1200,800); window.show();
        auto *view=window.findChild<MarkdownDocumentView *>();
        QMimeData mime; mime.setUrls({QUrl::fromLocalFile(path)}); Answer no(QMessageBox::No);
        QDragEnterEvent enter(QPoint(15,15),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(view->viewport(),&enter); QVERIFY(enter.isAccepted());
        QDropEvent drop(QPointF(15,15),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(view->viewport(),&drop); QVERIFY(drop.isAccepted());
        auto *editor=window.findChild<MarkdownEditor *>(); QTRY_COMPARE(editor->source(),QString("# Dropped\n"));
        QCOMPARE(no.count,1); no.stop(); editor->moveCursor(QTextCursor::End); editor->insertPlainText("pending");
        Answer cancel(QMessageBox::Cancel); QVERIFY(!window.close()); QCOMPARE(cancel.count,1); cancel.stop();
        Answer save(QMessageBox::Save); QVERIFY(window.close()); QCOMPARE(save.count,1); QVERIFY(read(path).contains("pending"));
    }
    void removingDirtyMissingRecordHonoursCancelAndDiscard() {
        Library lib; QVERIFY(lib.init()); const auto path=lib.outside();
        QVERIFY(put(path,"# External\n"));
        MainWindow window(lib.root());window.show();Answer no(QMessageBox::No);
        QVERIFY(window.openFile(path));no.stop();
        auto *editor=window.findChild<MarkdownEditor *>();editor->moveCursor(QTextCursor::End);editor->insertPlainText("pending edit");
        const auto retained=path+".moved";QVERIFY(QFile::rename(path,retained));
        auto *manager=window.findChild<MarkdownManager *>();
        QTRY_VERIFY_WITH_TIMEOUT(manager->loadNode(path)->isSourceMissing(),5000);
        auto *sidebar=window.findChild<DocumentSidebar *>();
        Answer cancel(QMessageBox::Cancel);sidebar->forgetExternalRequested(path);
        QTRY_VERIFY(cancel.count>0);cancel.stop();
        QVERIFY(manager->loadNode(path));QVERIFY(editor->source().contains("pending edit"));QVERIFY(!QFileInfo::exists(path));
        Answer discard(QMessageBox::Discard);sidebar->forgetExternalRequested(path);
        QTRY_VERIFY_WITH_TIMEOUT(manager->externalNodes().isEmpty(),3000);discard.stop();
        QCOMPARE(read(retained),QString("# External\n"));QVERIFY(!QFileInfo::exists(path));
        QVERIFY(window.close());
    }
    void executableAcceptsPositionalMarkdownWithSpaces() {
        Library lib; QVERIFY(lib.init()); const auto path=lib.root()+QStringLiteral("/从快捷方式 打开.md"); QVERIFY(put(path,"# Positional shortcut document\n"));
        const auto qa=qEnvironmentVariable("TMD_EXTERNAL_QA_DIR");
        const auto screenshot=qa.isEmpty() ? lib.temp.filePath("startup.png") : QDir(qa).filePath("startup-positional.png");
        QProcess child; child.setProgram(QCoreApplication::applicationDirPath()+"/tree_md.exe");
        child.setArguments({"--data-dir",lib.root(),"--smoke-test","--screenshot",screenshot,path});
        child.start(); QVERIFY(child.waitForStarted(5000)); QVERIFY(child.waitForFinished(20000));
        QCOMPARE(child.exitCode(),0); QVERIFY(QFileInfo::exists(screenshot));
        QVERIFY(!QFileInfo::exists(lib.root()+"/.tree-md-external.json"));
        const auto external=lib.outside(QStringLiteral("快捷方式 外部文件.md")); QVERIFY(put(external,"# External CLI\n"));
        const auto prompt=qa.isEmpty() ? lib.temp.filePath("external-startup.png") : QDir(qa).filePath("startup-external-prompt.png");
        child.setArguments({"--data-dir",lib.root(),"--smoke-test","--screenshot",prompt,external});
        child.start(); QVERIFY(child.waitForStarted(5000)); QVERIFY(child.waitForFinished(20000));
        QCOMPARE(child.exitCode(),0); QVERIFY(QFileInfo::exists(prompt));
        QVERIFY(!QFileInfo::exists(lib.root()+"/.tree-md-external.json"));
    }
    void denseTablesOpenPromptlyAndKeepCellEditsInPlace() {
        Library lib; QVERIFY(lib.init());
        QString source=QStringLiteral("# 大型表格\n\n");
        for (int table=0; table<4; ++table) {
            source+=QStringLiteral("## 表格 %1\n\n| A | B | C | D | E |\n| --- | --- | --- | --- | --- |\n").arg(table);
            for (int row=0; row<50; ++row) {
                source+='|';
                for (int column=0; column<5; ++column)
                    source+=QStringLiteral(" 入口 T%1R%2C%3 用户输入与服务端校验；`token` 和 <input>，保留普通空格与文字 |")
                        .arg(table).arg(row).arg(column);
                source+='\n';
            }
            source+='\n';
        }
        source+=QStringLiteral("表格结束，普通正文仍可编辑。\n");
        const auto path=lib.outside("dense-tables.md"); QVERIFY(put(path,source));
        MainWindow window(lib.root()); window.resize(1250,800); window.show(); Answer no(QMessageBox::No);
        QElapsedTimer elapsed; elapsed.start(); QVERIFY(window.openFile(path));
        qInfo() << "Dense table open time (ms):" << elapsed.elapsed();
        QVERIFY2(elapsed.elapsed()<5000,"Opening dense tables must not trigger Windows unresponsive detection");
        auto *view=window.findChild<MarkdownDocumentView *>(); auto *editor=window.findChild<MarkdownEditor *>();
        for (const auto &marker : {QString("T0R0C0"),QString("T2R25C2"),QString("T3R49C4")}) {
            const int position=source.indexOf(marker); QVERIFY(position>=0);
            const auto cursor=view->cursorForSourceRange(position,position+marker.size());
            QCOMPARE(cursor.selectedText(),marker);
            const auto replacement=marker+"-edited";
            QVERIFY(view->replaceDisplayText(cursor.selectionStart(),cursor.selectionEnd()-cursor.selectionStart(),replacement));
            source.replace(position,marker.size(),replacement); QCOMPARE(editor->source(),source);
        }
        window.findChild<QAction *>("saveAction")->trigger(); QCOMPARE(read(path),source);
    }
};
QTEST_MAIN(ExternalDocumentTests)
#include "ExternalDocumentTests.moc"
