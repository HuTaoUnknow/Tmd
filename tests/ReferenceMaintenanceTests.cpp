#include "core/library/MarkdownManager.h"
#include "core/library/MarkdownFileIO.h"
#include "core/library/LibraryReferences.h"
#include "core/library/LibraryFileIndex.h"
#include "core/markdown/MarkdownReferences.h"
#include "core/settings/MarkdownSettings.h"
#include "ui/MainWindow.h"
#include "ui/document/MarkdownEditor.h"
#include "ui/library/DocumentTreeBuilder.h"
#include <QDialog>
#include <QListWidget>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QPushButton>
#include <QPointer>
#include <QtTest>
#include <QtCore/qscopeguard.h>
#include <algorithm>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
bool put(const QString &path, const QString &text) { return QDir().mkpath(QFileInfo(path).absolutePath()) && MarkdownFileIO::writeFile(path,text); }
QString read(const QString &path) { QString content; MarkdownFileIO::readFile(path,content); return content; }
struct Library {
    QTemporaryDir temp;
    QString root() const { return temp.filePath("md_data"); }
    QString path(const QString &relative) const { return QDir(root()).filePath(relative); }
    QString photo(const QString &name) const { return temp.filePath("md_photo/"+name); }
    bool document(const QString &name,const QString &text) { return put(path(name),text); }
    bool image(const QString &name) { QDir().mkpath(QFileInfo(photo(name)).absolutePath()); QImage image(8,8,QImage::Format_ARGB32); image.fill(Qt::green); return image.save(photo(name)); }
};
QTreeWidgetItem *itemFor(QTreeWidget *tree,const QString &path) {
    QTreeWidgetItemIterator it(tree); while (*it) { if ((*it)->data(0,Qt::UserRole).toString()==path) return *it; ++it; } return nullptr;
}
}
class ReferenceMaintenanceTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
private slots:
    void initTestCase() {
        QVERIFY(settings.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    }
    void init() { QSettings().clear(); MarkdownSettingsStore::setStoragePath(settings.filePath(QString(QTest::currentTestFunction())+".txt")); }
    void parserKeepsOnlyRealDestinations() {
        const QString source="---\nname: '[meta](meta.md)'\n---\n"
            "[plain](target.md#part \"Title\") ![photo](../md_photo/a.png)\n"
            "[![linked](a.png)](outer.md) [ref][id] ![refphoto][pic]\n"
            "[id]: <folder/a b.md> \"Tooltip\"\n[pic]: ../md_photo/b.png\n"
            "`[inline](code.md)`\n```markdown\n[example](example.md)\n```\n"
            "<!-- [hidden](comment.md) -->\n<b>[literal](html.md)</b>\n\\[escaped](escape.md)\n";
        const auto refs=MarkdownReferences::parse(source);
        QStringList targets; for (const auto &ref : refs) { targets.append(ref.source); QCOMPARE(source.mid(ref.start,ref.length).startsWith('<'),false); }
        QCOMPARE(targets,QStringList({"target.md#part","../md_photo/a.png","a.png","outer.md","folder/a b.md","../md_photo/b.png","html.md"}));
        QCOMPARE(std::count_if(refs.cbegin(),refs.cend(),[](const auto &ref) { return ref.image; }),3);
    }
    void missingDeletionAndRestoration() {
        Library lib; QVERIFY(lib.document("a.md","[b](b.md)\n![p](../md_photo/p.png)\n[web](https://example.invalid/x)\n`[fake](missing.md)`"));
        QVERIFY(lib.document("b.md","# B unique body")); QVERIFY(lib.image("p.png")); MarkdownManager manager(lib.root()); QVERIFY(manager.scan());
        QVERIFY(!manager.loadNode("a.md")->hasMissingReferences()); QVERIFY(QFile::remove(lib.path("b.md"))); QVERIFY(QFile::remove(lib.photo("p.png")));
        QVERIFY(manager.scan()); QCOMPARE(manager.loadNode("a.md")->missingReferences().size(),2); QVERIFY(!manager.loadNode("a.md")->isModified());
        QVERIFY(lib.document("b.md","# Restored")); QVERIFY(lib.image("p.png")); QVERIFY(manager.scan()); QVERIFY(!manager.loadNode("a.md")->hasMissingReferences());
    }
    void multilineDestinationsKeepLayout() {
        Library lib; QVERIFY(lib.document("next/a.md","# A")); QVERIFY(lib.document("next/b.md","# B"));
        const QString text="[wrapped](\n <folder/a.md#one>\n \"Title\")\n[use][x]\n[x]:\n <folder/b.md> \"Tip\"";
        const auto refs=MarkdownReferences::parse(text); QCOMPARE(refs.size(),2);
        const QMap<QString,QString> moved{{LibraryFileIndex::key(lib.path("folder")),lib.path("next")}};
        const auto actual=LibraryReferences::rewrite(text,lib.root(),lib.path("doc.md"),lib.path("doc.md"),moved,{});
        QCOMPARE(actual,QString("[wrapped](\n <next/a.md#one>\n \"Title\")\n[use][x]\n[x]:\n <next/b.md> \"Tip\""));
    }
    void externalMovesUpdateBothEndsAndDefinitions() {
        Library lib; const QString text="[b][ref]\n![p](../../md_photo/p.png)\n\n[ref]: <../targets/b.md#part> \"Title\"\n\n```md\n[b](../targets/b.md)\n```";
        QVERIFY(lib.document("notes/a.md",text)); QVERIFY(lib.document("targets/b.md","# B original")); QVERIFY(lib.document("outside.md","[a](notes/a.md) [b](targets/b.md) ![p](../md_photo/p.png)")); QVERIFY(lib.image("p.png"));
        MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); QVERIFY(manager.createDirectory("new")); QVERIFY(manager.createDirectory("next"));
        QVERIFY(QFile::rename(lib.path("notes/a.md"),lib.path("new/a.md"))); QVERIFY(QFile::rename(lib.path("targets/b.md"),lib.path("next/b.md"))); QVERIFY(QFile::rename(lib.photo("p.png"),lib.photo("renamed.png")));
        QVERIFY(manager.scan()); const auto source=read(lib.path("new/a.md"));
        QVERIFY(source.contains("[ref]: <../next/b.md#part> \"Title\"")); QVERIFY(source.contains("../../md_photo/renamed.png")); QVERIFY(source.contains("[b](../targets/b.md)\n```"));
        QVERIFY(read(lib.path("outside.md")).contains("[a](new/a.md) [b](next/b.md) ![p](../md_photo/renamed.png)"));
        QVERIFY(!manager.loadNode("new/a.md")->hasMissingReferences()); QVERIFY(!manager.loadNode("outside.md")->hasMissingReferences());
    }
    void internalRenameUpdatesIncomingAndOutgoing() {
        Library lib; QVERIFY(lib.document("folder/a.md","[b](../b.md#part)")); QVERIFY(lib.document("b.md","# B")); QVERIFY(lib.document("outside.md","[a](folder/a.md \"Read A\")"));
        MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); QVERIFY(manager.createDirectory("next")); QString error;
        QVERIFY2(manager.renameDocument("folder/a.md",QString::fromUtf8("next/名字 空格.md"),&error),qPrintable(error));
        QVERIFY(read(lib.path("outside.md")).contains("next/%E5%90%8D%E5%AD%97%20%E7%A9%BA%E6%A0%BC.md \"Read A\""));
        QCOMPARE(read(lib.path(QString::fromUtf8("next/名字 空格.md"))),QString("[b](../b.md#part)"));
    }
    void batchMoveKeepsLinksBetweenMovedAndUnmovedDocuments() {
        Library lib; QVERIFY(lib.document("a.md","[b](b.md) [c](c.md)")); QVERIFY(lib.document("b.md","[a](a.md)")); QVERIFY(lib.document("c.md","[a](a.md)"));
        MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); QVERIFY(manager.createDirectory("dest")); QVERIFY(manager.moveDocuments({"a.md","b.md"},"dest"));
        QCOMPARE(read(lib.path("dest/a.md")),QString("[b](b.md) [c](../c.md)")); QCOMPARE(read(lib.path("dest/b.md")),QString("[a](a.md)")); QCOMPARE(read(lib.path("c.md")),QString("[a](dest/a.md)"));
    }
    void internalFolderMoveAndMissingImages() {
        Library lib; QVERIFY(lib.document("source/a.md","[b](../b.md) ![missing](gone.png)")); QVERIFY(lib.document("b.md","[a](source/a.md)"));
        MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); QVERIFY(manager.createDirectory("dest")); QVERIFY(manager.movePath("source","dest"));
        QCOMPARE(read(lib.path("dest/source/a.md")),QString("[b](../../b.md) ![missing](gone.png)"));
        QVERIFY(read(lib.path("b.md")).contains("dest/source/a.md")); QCOMPARE(manager.loadNode("dest/source/a.md")->missingReferences().size(),1);
    }
    void uniqueCopyDeleteMoveAndAmbiguousCopies() {
        Library lib; QVERIFY(lib.document("a.md","[target](target.md)")); QVERIFY(lib.document("target.md","unique target content")); MarkdownManager manager(lib.root()); QVERIFY(manager.scan());
        QVERIFY(QDir().mkpath(lib.path("new"))); QVERIFY(QFile::copy(lib.path("target.md"),lib.path("new/renamed.md"))); QVERIFY(QFile::remove(lib.path("target.md"))); QVERIFY(manager.scan());
        QCOMPARE(read(lib.path("a.md")),QString("[target](new/renamed.md)"));
        QVERIFY(QFile::copy(lib.path("new/renamed.md"),lib.path("one.md"))); QVERIFY(QFile::copy(lib.path("new/renamed.md"),lib.path("two.md"))); QVERIFY(QFile::remove(lib.path("new/renamed.md"))); QVERIFY(manager.scan());
        QCOMPARE(read(lib.path("a.md")),QString("[target](new/renamed.md)")); QVERIFY(manager.loadNode("a.md")->hasMissingReferences());
    }
    void movedDocumentDoesNotAdoptUnrelatedPictureAtNewLocation() {
        Library lib; QVERIFY(lib.document("source/a.md","![missing](gone.png)")); QVERIFY(lib.document("dest/keep.md","keep")); QVERIFY(put(lib.path("dest/gone.png"),"unrelated picture bytes"));
        MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); QVERIFY(manager.movePath("source/a.md","dest"));
        QCOMPARE(read(lib.path("dest/a.md")),QString("![missing](../source/gone.png)")); QVERIFY(manager.loadNode("dest/a.md")->hasMissingReferences());
    }
    void delayedRecoveryAndRestart() {
        Library lib; QVERIFY(lib.document("a.md","[b](b.md)")); QVERIFY(lib.document("b.md","specific content for recovery"));
        { MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); QVERIFY(QFile::rename(lib.path("b.md"),lib.temp.filePath("held.md"))); QVERIFY(manager.scan()); QVERIFY(manager.loadNode("a.md")->hasMissingReferences()); }
        QVERIFY(QDir().mkpath(lib.path("new"))); QVERIFY(QFile::rename(lib.temp.filePath("held.md"),lib.path("new/b.md")));
        { MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); QCOMPARE(read(lib.path("a.md")),QString("[b](new/b.md)")); QVERIFY(!manager.loadNode("a.md")->hasMissingReferences()); }
    }
    void explicitDeletionDoesNotRetargetToAnUnrelatedCopy() {
#ifdef Q_OS_WIN
        Library lib; QVERIFY(lib.document("a.md","[b](b.md)")); QVERIFY(lib.document("b.md","shared template body")); MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); QString error;
        QVERIFY2(manager.deleteDocument("b.md",&error),qPrintable(error)); QVERIFY(manager.loadNode("a.md")->hasMissingReferences());
        QVERIFY(manager.createDocument("else/b.md","shared template body")); QCOMPARE(read(lib.path("a.md")),QString("[b](b.md)")); QVERIFY(manager.loadNode("a.md")->hasMissingReferences());
#else
        QSKIP("Windows Recycle Bin identity");
#endif
    }
    void pendingTextIsNotSavedByReferenceRepair() {
        Library lib; QVERIFY(lib.document("a.md","[b](b.md)\nsaved")); QVERIFY(lib.document("b.md","# Unique B")); MarkdownManager manager(lib.root()); QVERIFY(manager.scan());
        manager.updateContent("a.md","[b](b.md)\nsaved\nUNSAVED"); QVERIFY(QFile::rename(lib.path("b.md"),lib.path("renamed.md"))); QVERIFY(manager.scan());
        QCOMPARE(read(lib.path("a.md")),QString("[b](renamed.md)\nsaved")); QCOMPARE(manager.loadNode("a.md")->content(),QString("[b](renamed.md)\nsaved\nUNSAVED")); QVERIFY(manager.loadNode("a.md")->isModified());
        QVERIFY(manager.saveDocument("a.md")); QVERIFY(!manager.loadNode("a.md")->isModified());
    }
    void externalEditsArePreservedSeparatelyFromPendingText() {
        Library lib; QVERIFY(lib.document("a.md","[b](b.md)\nbaseline")); QVERIFY(lib.document("b.md","# Unique B")); MarkdownManager manager(lib.root()); QVERIFY(manager.scan());
        manager.updateContent("a.md","[b](b.md)\nLOCAL"); QVERIFY(lib.document("a.md","[b](b.md)\nEXTERNAL")); QVERIFY(QFile::rename(lib.path("b.md"),lib.path("moved.md"))); QVERIFY(manager.scan());
        QCOMPARE(read(lib.path("a.md")),QString("[b](moved.md)\nEXTERNAL")); QCOMPARE(manager.loadNode("a.md")->content(),QString("[b](moved.md)\nLOCAL")); QVERIFY(manager.loadNode("a.md")->hasExternalChange()); QVERIFY(!manager.saveDocument("a.md"));
    }
    void saveThenMoveBeforeWatcherRetainsIdentity() {
        Library lib; QVERIFY(lib.document("a.md","[b](b.md)")); QVERIFY(lib.document("b.md","old"));
        { MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); manager.updateContent("b.md","new content immediately before closing"); QVERIFY(manager.saveDocument("b.md")); }
        QVERIFY(QFile::rename(lib.path("b.md"),lib.path("renamed.md"))); MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); QCOMPARE(read(lib.path("a.md")),QString("[b](renamed.md)"));
    }
    void persistentIdentitySupportsInPlaceEditAndMove() {
        Library lib; QVERIFY(lib.document("a.md","[b](b.md)")); QVERIFY(lib.document("b.md","target before edit"));
        { MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); }
        { QFile file(lib.path("b.md")); QVERIFY(file.open(QIODevice::Append)); QVERIFY(file.write(" edited")>0); }
        QVERIFY(QFile::rename(lib.path("b.md"),lib.path("renamed.md"))); MarkdownManager manager(lib.root()); QVERIFY(manager.scan());
        QCOMPARE(read(lib.path("a.md")),QString("[b](renamed.md)")); QCOMPARE(read(lib.path("renamed.md")),QString("target before edit edited"));
    }
    void duplicateNamesAreRejectedEverywhere() {
        Library lib; QVERIFY(lib.document("one/A.md","# A")); QVERIFY(lib.document("other.md","# Other")); MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); QString error;
        QVERIFY(!manager.createDocument("two/a.MD","new",&error)); QVERIFY(error.contains(QStringLiteral("同名"))); QVERIFY(!QFileInfo::exists(lib.path("two/a.MD")));
        QVERIFY(!manager.renameDocument("other.md","two/a.md",&error)); QVERIFY(manager.loadNode("other.md"));
        QVERIFY(put(lib.temp.filePath("source.md"),"external")); QVERIFY(!manager.importDocument(lib.temp.filePath("source.md"),"two/A.md",&error));
        QVERIFY(!manager.copyDocument("other.md","two/a.md","copy",&error));
        QVERIFY(lib.document("two/a.MD","outside-created duplicate")); QVERIFY(manager.scan()); QCOMPARE(manager.duplicateDocuments().size(),1); QCOMPARE(manager.allNodes().size(),3);
        QVERIFY(manager.renameDocument("two/a.MD","two/Unique.md",&error)); QVERIFY(manager.duplicateDocuments().isEmpty());
    }
    void watcherSeesImageRemovalAndMoves() {
        Library lib; QVERIFY(lib.document("a.md","![p](../md_photo/p.png) [b](b.md)")); QVERIFY(lib.document("b.md","watch target unique")); QVERIFY(lib.image("p.png"));
        MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); manager.enableWatching();
        QVERIFY(QFile::rename(lib.photo("p.png"),lib.photo("new.png")));
        QTRY_VERIFY_WITH_TIMEOUT(manager.loadNode("a.md")->content().contains("../md_photo/new.png"),5000);
        QVERIFY(QFile::remove(lib.photo("new.png"))); QTRY_VERIFY_WITH_TIMEOUT(manager.loadNode("a.md")->hasMissingReferences(),5000);
        QVERIFY(QFile::rename(lib.path("b.md"),lib.path("new.md"))); QTRY_VERIFY_WITH_TIMEOUT(manager.loadNode("a.md")->content().contains("[b](new.md)"),5000);
    }
    void absoluteImagesAndOtherFileLinksKeepFragmentsAndTitles() {
        Library lib; QVERIFY(lib.document("files/keep.md","# Keep")); QVERIFY(put(lib.path("files/report.pdf"),"pdf fixture")); QVERIFY(lib.image("p.png"));
        const QString photo=QUrl::fromLocalFile(lib.photo("p.png")).toString(QUrl::FullyEncoded);
        QVERIFY(lib.document("a.md","[pdf](files/report.pdf#page=2 \"Report\") ![p](<"+photo+">)"));
        MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); QVERIFY(QDir(lib.root()).rename("files","renamed")); QVERIFY(QFile::rename(lib.photo("p.png"),lib.photo("q.png"))); QVERIFY(manager.scan());
        const auto actual=read(lib.path("a.md")); QVERIFY(actual.contains("[pdf](renamed/report.pdf#page=2 \"Report\")")); QVERIFY(actual.contains(QUrl::fromLocalFile(lib.photo("q.png")).toString(QUrl::FullyEncoded))); QVERIFY(!manager.loadNode("a.md")->hasMissingReferences());
    }
    void imageDirectoryInsideLibraryMovesWithoutExtraWatchHandles() {
        Library lib; auto config=MarkdownSettingsStore::current(); config.imageMode=1; config.absolutePhotoPath=lib.path("source/photos"); QVERIFY(MarkdownSettingsStore::save(config));
        QVERIFY(QDir().mkpath(config.absolutePhotoPath)); QImage photo(8,8,QImage::Format_ARGB32); photo.fill(Qt::red); QVERIFY(photo.save(lib.path("source/photos/p.png")));
        const auto original=lib.path("source/photos/p.png"); QVERIFY(lib.document("source/a.md","![p](<"+original+">)")); MarkdownManager manager(lib.root()); QVERIFY(manager.scan()); manager.enableWatching(); QVERIFY(manager.createDirectory("dest")); QString error;
        QVERIFY2(manager.movePath("source","dest",&error),qPrintable(error)); QVERIFY(!manager.loadNode("dest/source/a.md")->hasMissingReferences());
        QVERIFY(QFile::rename(lib.path("dest/source/photos/p.png"),lib.path("dest/source/photos/new.png")));
        QTRY_VERIFY_WITH_TIMEOUT(manager.loadNode("dest/source/a.md")->content().contains("/photos/new.png"),5000);
    }
    void failedAutomaticWriteKeepsPendingRepairAndRetries() {
#ifdef Q_OS_WIN
        Library lib; QVERIFY(lib.document("a.md","[b](b.md)\nkeep text")); QVERIFY(lib.document("b.md","unique b")); MarkdownManager manager(lib.root()); QVERIFY(manager.scan());
        const auto native=QDir::toNativeSeparators(lib.path("a.md")); const DWORD flags=GetFileAttributesW(reinterpret_cast<LPCWSTR>(native.utf16())); QVERIFY(flags!=INVALID_FILE_ATTRIBUTES);
        QVERIFY(SetFileAttributesW(reinterpret_cast<LPCWSTR>(native.utf16()),flags|FILE_ATTRIBUTE_READONLY));
        const auto restore=qScopeGuard([&] { SetFileAttributesW(reinterpret_cast<LPCWSTR>(native.utf16()),flags); });
        QVERIFY(QFile::rename(lib.path("b.md"),lib.path("new.md"))); QVERIFY(manager.scan());
        QCOMPARE(read(lib.path("a.md")),QString("[b](b.md)\nkeep text")); QCOMPARE(manager.loadNode("a.md")->content(),QString("[b](new.md)\nkeep text")); QVERIFY(manager.loadNode("a.md")->isModified()); QVERIFY(!manager.warnings().isEmpty());
        QVERIFY(SetFileAttributesW(reinterpret_cast<LPCWSTR>(native.utf16()),flags)); QVERIFY(manager.scan()); QCOMPARE(read(lib.path("a.md")),QString("[b](new.md)\nkeep text")); QVERIFY(!manager.loadNode("a.md")->isModified());
#else
        QSKIP("Windows read-only replacement");
#endif
    }
    void sidebarShowsRedEvenForSavedDocumentsAndWhiteForPending() {
        Library lib; QVERIFY(lib.document("a.md","[missing](missing.md)")); QVERIFY(lib.document("good.md","good")); MainWindow window(lib.root()); window.show(); QVERIFY(window.openDocument("a.md")); QTest::qWait(60);
        auto *manager=window.findChild<MarkdownManager *>(); auto *tree=window.findChild<QTreeWidget *>("documentTree"); QVERIFY(manager); QVERIFY(tree);
        auto *item=itemFor(tree,"a.md"); QVERIFY(item); QVERIFY(!item->data(0,DocumentTreeBuilder::ModifiedRole).toBool()); QVERIFY(!item->data(0,DocumentTreeBuilder::MissingReferencesRole).toStringList().isEmpty());
        const QImage image=tree->viewport()->grab().toImage(); int red=0; for (int y=0;y<image.height();++y) for (int x=0;x<image.width();++x) { const auto c=image.pixelColor(x,y); if (c.red()>180 && c.green()<150 && c.blue()<150) ++red; } QVERIFY(red>20);
        manager->updateContent("a.md","pending valid text"); item=itemFor(tree,"a.md"); QVERIFY(item->data(0,DocumentTreeBuilder::ModifiedRole).toBool()); QVERIFY(item->data(0,DocumentTreeBuilder::MissingReferencesRole).toStringList().isEmpty());
    }
    void currentPendingEditorIsPatchedWithoutLosingUndo() {
        Library lib; QVERIFY(lib.document("a.md","[b](b.md)\nbody")); QVERIFY(lib.document("b.md","# Unique B")); MainWindow window(lib.root()); window.show(); window.setSourceMode(true); QVERIFY(window.openDocument("a.md"));
        auto *editor=window.findChild<MarkdownEditor *>(); auto *manager=window.findChild<MarkdownManager *>(); QVERIFY(editor); QVERIFY(manager);
        editor->moveCursor(QTextCursor::End); editor->insertPlainText(" PENDING"); QVERIFY(editor->document()->isUndoAvailable());
        QVERIFY(QFile::rename(lib.path("b.md"),lib.path("new.md"))); QVERIFY(manager->scan());
        QCOMPARE(editor->source(),QString("[b](new.md)\nbody PENDING")); QVERIFY(editor->document()->isUndoAvailable()); QCOMPARE(read(lib.path("a.md")),QString("[b](new.md)\nbody"));
    }
    void externalDuplicatePromptsForRename() {
        Library lib; QVERIFY(lib.document("one/a.md","# One")); QVERIFY(lib.document("two/A.md","# Two")); MainWindow window(lib.root()); window.show(); QCoreApplication::processEvents();
        QPointer<QDialog> dialog=window.findChild<QDialog *>("duplicateDocumentsDialog"); QVERIFY(dialog); QVERIFY(dialog->isVisible()); auto *list=dialog->findChild<QListWidget *>("duplicateDocumentList"); QVERIFY(list); QCOMPARE(list->count(),2);
        auto *manager=window.findChild<MarkdownManager *>(); QVERIFY(manager->renameDocument("two/A.md","two/unique.md")); QCoreApplication::processEvents(); QVERIFY(!dialog || !dialog->isVisible());
    }
    void twoManagersDoNotRepeatAMoveOrLosePendingText() {
        Library lib; QVERIFY(lib.document("notes/a.md","[b](../b.md)\nbody")); QVERIFY(lib.document("b.md","# B unique"));
        MarkdownManager first(lib.root()), second(lib.root()); QVERIFY(first.scan()); QVERIFY(second.scan());
        second.updateContent("notes/a.md","[b](../b.md)\nbody\nPENDING");
        QVERIFY(QDir().mkpath(lib.path("deep/nested"))); QVERIFY(QFile::rename(lib.path("notes/a.md"),lib.path("deep/nested/a.md")));
        QVERIFY(first.scan()); QCOMPARE(read(lib.path("deep/nested/a.md")),QString("[b](../../b.md)\nbody"));
        QVERIFY(second.scan()); const auto *node=second.loadNode("deep/nested/a.md"); QVERIFY(node);
        QCOMPARE(node->content(),QString("[b](../../b.md)\nbody\nPENDING")); QVERIFY(node->isModified()); QVERIFY(!node->hasExternalChange());
        QVERIFY(first.scan()); QVERIFY(second.scan()); QCOMPARE(read(lib.path("deep/nested/a.md")),QString("[b](../../b.md)\nbody"));
        QVERIFY(second.saveDocument("deep/nested/a.md")); QCOMPARE(read(lib.path("deep/nested/a.md")),QString("[b](../../b.md)\nbody\nPENDING"));
    }
    void twoManagersKeepPendingIncomingLinksInSync() {
        Library lib; QVERIFY(lib.document("a.md","[b](b.md)\nbody")); QVERIFY(lib.document("b.md","unique b"));
        MarkdownManager first(lib.root()), second(lib.root()); QVERIFY(first.scan()); QVERIFY(second.scan());
        second.updateContent("a.md","[b](b.md)\nbody\nPENDING"); QVERIFY(first.renameDocument("b.md","renamed.md"));
        QVERIFY(second.scan()); QCOMPARE(second.loadNode("a.md")->content(),QString("[b](renamed.md)\nbody\nPENDING"));
        QVERIFY(!second.loadNode("a.md")->hasExternalChange()); QCOMPARE(read(lib.path("a.md")),QString("[b](renamed.md)\nbody"));
        QVERIFY(second.saveDocument("a.md"));
    }
    void inventoryDoesNotRefreshItselfWhenPhotoRootContainsTheLibrary() {
        Library lib; auto config=MarkdownSettingsStore::current(); config.imageMode=1; config.absolutePhotoPath=lib.temp.path(); QVERIFY(MarkdownSettingsStore::save(config));
        QVERIFY(lib.document("a.md","# A")); MarkdownManager manager(lib.root()); QVERIFY(manager.scan());
        const auto snapshot=read(lib.path(".tree-md-files.json")); QVERIFY(!snapshot.isEmpty());
        QVERIFY(manager.scan()); QCOMPARE(read(lib.path(".tree-md-files.json")),snapshot);
    }
    void cursorBetweenRepairedLinksKeepsItsTextPosition() {
        const QString before="[a](a.md)\ntext CARET body\n[b](old-long-b.md)";
        const QString after="[a](nested/much-longer-a.md)\ntext CARET body\n[b](b.md)";
        QCOMPARE(LibraryReferences::mapPosition(before,after,before.indexOf("CARET")),after.indexOf("CARET"));
        QCOMPARE(LibraryReferences::mapPosition(before,after,before.size()),after.size());
        QCOMPARE(LibraryReferences::mapPosition(before,after,0),0);
    }
};
QTEST_MAIN(ReferenceMaintenanceTests)
#include "ReferenceMaintenanceTests.moc"
