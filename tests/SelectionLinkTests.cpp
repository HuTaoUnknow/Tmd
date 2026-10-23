#include "core/library/DocumentLinkResolver.h"
#include "core/library/MarkdownFileIO.h"
#include "core/library/MarkdownManager.h"
#include "core/markdown/MarkdownSelectionEditing.h"
#include "core/settings/MarkdownSettings.h"
#include "ui/MainWindow.h"
#include "ui/common/Theme.h"
#include "ui/document/MarkdownDocumentView.h"
#include "ui/document/MarkdownEditor.h"
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QLabel>
#include <QInputMethodEvent>
#include <QMouseEvent>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class LinkCapture : public QObject {
    Q_OBJECT
public:
    QList<QUrl> urls;
public slots:
    void capture(const QUrl &url) { urls.append(url); }
};
class SelectionLinkTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_settings;
    static QString selected(QTextEdit *editor) { return editor->textCursor().selectedText().replace(QChar::ParagraphSeparator, '\n'); }
    static QString content(QTextEdit *editor) {
        if (auto *view = qobject_cast<MarkdownDocumentView *>(editor)) return view->markdownSource();
        return static_cast<MarkdownEditor *>(editor)->source();
    }
    static void select(QTextEdit *editor, int anchor, int caret) {
        if (auto *view = qobject_cast<MarkdownDocumentView *>(editor)) view->restoreSourceCursor(anchor, caret);
        else { auto cursor = editor->textCursor(); cursor.setPosition(anchor); cursor.setPosition(caret, QTextCursor::KeepAnchor); editor->setTextCursor(cursor); }
    }
    static void load(QTextEdit *editor, const QString &source) {
        if (auto *view = qobject_cast<MarkdownDocumentView *>(editor)) view->setMarkdownSource("test.md", source);
        else static_cast<MarkdownEditor *>(editor)->setSource(source);
        editor->document()->setModified(false);
    }
private slots:
    void initTestCase() {
        QVERIFY(m_settings.isValid()); QCoreApplication::setOrganizationName("TmdSelectionLinkTests");
        QCoreApplication::setApplicationName("IsolatedTests"); QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
        MarkdownSettingsStore::setStoragePath(m_settings.filePath("settings.json"));
        QVERIFY(MarkdownSettingsStore::save(MarkdownSettings())); applyTreeMdTheme(*qApp);
    }
    void cleanup() { QDesktopServices::unsetUrlHandler("file"); QDesktopServices::unsetUrlHandler("https"); }
    void linkResolutionKeepsLibraryBoundary() {
        QTemporaryDir temp; QVERIFY(temp.isValid()); const QString root = temp.filePath("md_data");
        QVERIFY(QDir().mkpath(root + "/folder"));
        QVERIFY(MarkdownFileIO::writeFile(root + "/folder/index.md", "index"));
        const QString internal = root + "/目标 文件.MD", sibling = root + "/folder/sibling.md", external = temp.filePath("external.md");
        for (const auto &path : {internal, sibling, external, root + "/manual.pdf"}) QVERIFY(MarkdownFileIO::writeFile(path, "fixture"));
        using Kind = DocumentLinkResolver::Kind;
        auto resolve = [&](const QUrl &url) { return DocumentLinkResolver::resolve(root, "folder/index.md", url); };
        for (const auto &link : {QUrl::fromLocalFile(internal), QUrl("../目标%20文件.MD"), QUrl("目标%20文件.MD"),
                                QUrl("md_data/目标%20文件.MD"), QUrl(QDir::toNativeSeparators(internal))}) {
            const auto target = resolve(link); QCOMPARE(target.kind, Kind::Document);
            QCOMPARE(QDir::cleanPath(target.path), QDir::cleanPath(internal));
        }
        QCOMPARE(resolve(QUrl("sibling.md")).path, sibling);
        const auto fragment = resolve(QUrl("../目标%20文件.MD#section")); QCOMPARE(fragment.kind, Kind::Document); QCOMPARE(fragment.fragment, QString("section"));
        QCOMPARE(resolve(QUrl("#local")).kind, Kind::Anchor);
        const auto outside = resolve(QUrl::fromLocalFile(external)); QCOMPARE(outside.kind, Kind::External); QCOMPARE(outside.url.toLocalFile(), external);
        const auto pdf = resolve(QUrl("../manual.pdf")); QCOMPARE(pdf.kind, Kind::External); QCOMPARE(pdf.url.toLocalFile(), root + "/manual.pdf");
        const QUrl web("https://example.com/page?q=1#topic"); QCOMPARE(resolve(web).kind, Kind::External); QCOMPARE(resolve(web).url, web);
        QCOMPARE(resolve(QUrl("mailto:someone@example.com")).kind, Kind::External);
        const auto missing = resolve(QUrl("missing.md")); QCOMPARE(missing.kind, Kind::Invalid); QVERIFY(!missing.error.isEmpty());
    }
    void sourceSelectionOperations() {
        const QString source = "前文 正文 后文"; const int start = source.indexOf("正文"), end = start + 2;
        for (const QString marker : {QString("*"), QString("**"), QString("***"), QString("_"), QString("__"), QString("~~"), QString("`"), QString("``"), QString("**_"), QString("**~~")}) {
            const auto edit = MarkdownSelectionEditing::wrap(source, end, start, marker);
            QVERIFY(edit.handled); QVERIFY(edit.changed);
            QCOMPARE(edit.source, source.left(start) + marker + "正文" + MarkdownSelectionEditing::closingMarker(marker) + source.mid(end));
            QCOMPARE(edit.anchor, end + marker.size()); QCOMPARE(edit.caret, start + marker.size());
        }
        for (const QString marker : {QString("~"), QString("~~~"), QString("*`"), QString("[]"), QString()})
            QVERIFY(!MarkdownSelectionEditing::wrap(source, start, end, marker).handled);
        QVERIFY(!MarkdownSelectionEditing::wrap(source, start, start, "**").handled);
        const QString code = "Before\n\n```cpp\nfirst\n  second\n```\n\nAfter"; const int caret = code.indexOf("second") + 2;
        const auto line = MarkdownSelectionEditing::selectCode(code, caret, caret, 0);
        QVERIFY(line.handled); QVERIFY(!line.changed); QCOMPARE(line.source.mid(line.anchor, line.caret - line.anchor), QString("  second"));
        const auto block = MarkdownSelectionEditing::selectCode(code, caret, caret, 1);
        QCOMPARE(block.source.mid(block.anchor, block.caret - block.anchor), QString("first\n  second"));
        const auto all = MarkdownSelectionEditing::selectCode(code, caret, caret, 2); QCOMPARE(all.anchor, 0); QCOMPARE(all.caret, code.size());
        QVERIFY(!MarkdownSelectionEditing::selectCode("```cpp\nunfinished", 9, 9, 0).handled);
        QVERIFY(!MarkdownSelectionEditing::selectCode(code, 0, 0, 0).handled);
    }
    void confirmWrap_data() {
        QTest::addColumn<bool>("rendered"); QTest::addColumn<QString>("marker"); QTest::addColumn<bool>("backwards");
        for (bool rendered : {false, true}) for (const QString marker : {QString("*"), QString("**"), QString("***"), QString("_"), QString("__"), QString("~~"), QString("`"), QString("``"), QString("**_"), QString("**~~")})
            for (bool backwards : {false, true}) QTest::newRow(qPrintable(QString("%1-%2-%3").arg(rendered).arg(marker).arg(backwards))) << rendered << marker << backwards;
    }
    void confirmWrap() {
        QFETCH(bool, rendered); QFETCH(QString, marker); QFETCH(bool, backwards);
        MarkdownEditor source; MarkdownDocumentView view; QTextEdit *editor = rendered ? static_cast<QTextEdit *>(&view) : &source;
        editor->resize(700, 400); editor->show(); const QString original = "前文 正文 后文"; load(editor, original);
        const int first = original.indexOf("正文"), last = first + 2; select(editor, backwards ? last : first, backwards ? first : last);
        QTest::keyClicks(editor, marker); QCOMPARE(content(editor), original); QVERIFY(!editor->document()->isModified());
        auto *preview = editor->viewport()->findChild<QLabel *>("selectionWrapPreview"); QVERIFY(preview); QVERIFY(preview->isVisible());
        QCOMPARE(selected(editor), QString("正文"));
        if (marker == "**") { QTest::qWait(600); QCOMPARE(content(editor), original); QVERIFY(preview->isVisible()); }
        QTest::keyClick(editor, Qt::Key_Return);
        QCOMPARE(content(editor), original.left(first) + marker + "正文" + MarkdownSelectionEditing::closingMarker(marker) + original.mid(last));
        QCOMPARE(selected(editor), QString("正文")); QVERIFY(!preview->isVisible());
        if (!rendered) { source.undo(); QCOMPARE(source.source(), original); QVERIFY(!source.document()->isUndoAvailable()); }
    }
    void cancelAndCorrectPendingMarkers_data() { QTest::addColumn<bool>("rendered"); QTest::newRow("source") << false; QTest::newRow("document") << true; }
    void mouseDragWrapKeepsOriginalText_data() {
        QTest::addColumn<bool>("rendered"); QTest::addColumn<bool>("backwards");
        for (bool rendered : {false, true}) for (bool backwards : {false, true})
            QTest::newRow(qPrintable(QString("%1-%2").arg(rendered).arg(backwards))) << rendered << backwards;
    }
    void mouseDragWrapKeepsOriginalText() {
        QFETCH(bool, rendered); QFETCH(bool, backwards); MarkdownEditor source; MarkdownDocumentView view;
        QTextEdit *editor = rendered ? static_cast<QTextEdit *>(&view) : &source; editor->resize(700, 350); editor->show();
        load(editor, "prefix body suffix"); QCoreApplication::processEvents();
        auto cursor = editor->document()->find("body"); const int first = cursor.selectionStart(), last = cursor.selectionEnd();
        cursor.setPosition(first); const auto a = editor->cursorRect(cursor).center(); cursor.setPosition(last); const auto b = editor->cursorRect(cursor).center();
        const auto from = backwards ? b : a, to = backwards ? a : b;
        QTest::mousePress(editor->viewport(), Qt::LeftButton, Qt::NoModifier, from); QTest::qWait(80);
        QMouseEvent move(QEvent::MouseMove, to, editor->viewport()->mapToGlobal(to), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(editor->viewport(), &move); QTest::mouseRelease(editor->viewport(), Qt::LeftButton, Qt::NoModifier, to);
        QCOMPARE(selected(editor), QString("body")); QTest::keyClicks(editor, "**"); QCOMPARE(content(editor), QString("prefix body suffix"));
        if (rendered && !backwards) QVERIFY(editor->grab().save(QCoreApplication::applicationDirPath() + "/selection-wrap-pending.png"));
        QTest::keyClick(editor, Qt::Key_Return); QCOMPARE(content(editor), QString("prefix **body** suffix")); QCOMPARE(selected(editor), QString("body"));
    }
    void cancelAndCorrectPendingMarkers() {
        QFETCH(bool, rendered); MarkdownEditor source; MarkdownDocumentView view;
        QTextEdit *editor = rendered ? static_cast<QTextEdit *>(&view) : &source; editor->resize(700, 400); editor->show();
        load(editor, "plain body tail"); select(editor, 6, 10); QTest::keyClicks(editor, "**"); QTest::keyClick(editor, Qt::Key_Escape);
        QCOMPARE(content(editor), QString("plain body tail")); QCOMPARE(selected(editor), QString("body"));
        QTest::keyClicks(editor, "***"); QTest::keyClick(editor, Qt::Key_Backspace); QTest::keyClick(editor, Qt::Key_Return);
        QCOMPARE(content(editor), QString("plain **body** tail"));
        load(editor, "body"); select(editor, 0, 4); QTest::keyClicks(editor, "~"); QTest::keyClick(editor, Qt::Key_Return);
        QCOMPARE(content(editor), QString("body")); QTest::keyClicks(editor, "~"); QTest::keyClick(editor, Qt::Key_Return); QCOMPARE(content(editor), QString("~~body~~"));
        load(editor, "body"); select(editor, 0, 4); QTest::keyClicks(editor, "*x"); QCOMPARE(content(editor), QString("x"));
        load(editor, "body"); select(editor, 0, 4); QTest::keyClicks(editor, "*");
        QTest::keyPress(editor, Qt::Key_Shift); QTest::keyRelease(editor, Qt::Key_Shift);
        QTest::keyClicks(editor, "*"); QTest::keyClick(editor, Qt::Key_Return); QCOMPARE(content(editor), QString("**body**"));
        load(editor, "body"); select(editor, 0, 4); QTest::keyClicks(editor, "*");
        QInputMethodEvent input; input.setCommitString("中"); QApplication::sendEvent(editor, &input);
        QCOMPARE(content(editor), QString("中")); QVERIFY(!editor->viewport()->findChild<QLabel *>("selectionWrapPreview")->isVisible());
        load(editor, "body"); select(editor, 0, 4); QTest::keyClicks(editor, "*"); load(editor, "new");
        QTest::keyClick(editor, Qt::Key_Return); QVERIFY(!content(editor).contains('*'));
    }
    void codeSelectAllStages_data() {
        QTest::addColumn<bool>("rendered"); QTest::addColumn<QString>("body");
        for (bool rendered : {false, true}) for (const QString body : {QString("first\n  second"), QString("only"), QString(), QString("\n")})
            QTest::newRow(qPrintable(QString("%1-%2").arg(rendered).arg(body))) << rendered << body;
    }
    void codeSelectAllStages() {
        QFETCH(bool, rendered); QFETCH(QString, body); MarkdownEditor source; MarkdownDocumentView view;
        QTextEdit *editor = rendered ? static_cast<QTextEdit *>(&view) : &source; editor->resize(700, 400); editor->show();
        const QString text = "Before\n\n```cpp\n" + body + "\n```\n\nAfter"; load(editor, text);
        const int bodyStart = text.indexOf("```cpp\n") + 7;
        const int caret = bodyStart + qMax(0, body.lastIndexOf('\n') + 1); select(editor, caret, caret);
        const auto undo = editor->document()->availableUndoSteps();
        QTest::keyClick(editor, Qt::Key_A, Qt::ControlModifier); QCOMPARE(selected(editor), body.section('\n', -1));
        QInputMethodEvent refresh; QApplication::sendEvent(editor, &refresh);
        QTest::keyClick(editor, Qt::Key_A, Qt::ControlModifier); QCOMPARE(selected(editor), body);
        QTest::keyClick(editor, Qt::Key_A, Qt::ControlModifier); QCOMPARE(content(editor), text);
        QTest::keyClick(editor, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text().replace("\r\n", "\n"), text);
        QVERIFY(!editor->document()->isModified()); QCOMPARE(editor->document()->availableUndoSteps(), undo);
        select(editor, caret, caret); QTest::keyClick(editor, Qt::Key_A, Qt::ControlModifier); QCOMPARE(selected(editor), body.section('\n', -1));
    }
    void wrappingUsesSharedUndo() {
        QTemporaryDir library; QVERIFY(library.isValid()); MarkdownManager seed(library.path());
        QVERIFY(seed.createDocument("doc.md", "before body after")); MainWindow window(library.path()); window.show(); QVERIFY(window.openDocument("doc.md"));
        auto *source = window.findChild<MarkdownEditor *>(); auto *view = window.findChild<MarkdownDocumentView *>();
        view->restoreSourceCursor(7, 11); QTest::keyClicks(view, "**"); QCOMPARE(source->source(), QString("before body after"));
        QTest::keyClick(view, Qt::Key_Return); QCOMPARE(source->source(), QString("before **body** after"));
        QTest::qWait(250); QCOMPARE(selected(view), QString("body"));
        QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); QCOMPARE(source->source(), QString("before body after"));
        QVERIFY(!source->document()->isUndoAvailable()); QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier); QCOMPARE(source->source(), QString("before **body** after"));
        QString disk; QVERIFY(MarkdownFileIO::readFile(library.filePath("doc.md"), disk)); QCOMPARE(disk, QString("before body after"));
    }
    void controlClickRoutesLinksAndKeepsEdits() {
        QTemporaryDir library; QVERIFY(library.isValid()); const QString root = library.filePath("md_data"); MarkdownManager seed(root);
        const QString index = "[local](../target%20name.md)\n\n[pdf](../manual.pdf)\n\n[outside](../../outside.md)\n\n[web](https://example.com/page?q=1)";
        QVERIFY(seed.createDocument("folder/index.md", index)); QVERIFY(seed.createDocument("target name.md", "# Target\n\ntext"));
        QVERIFY(MarkdownFileIO::writeFile(root + "/manual.pdf", "fixture")); QVERIFY(MarkdownFileIO::writeFile(library.filePath("outside.md"), "external"));
        MainWindow window(root); window.resize(1000, 700); window.show(); QVERIFY(window.openDocument("folder/index.md"));
        auto *source = window.findChild<MarkdownEditor *>(); auto *view = window.findChild<MarkdownDocumentView *>();
        auto click = [&](QTextEdit *editor, const QString &label, Qt::KeyboardModifiers modifiers) {
            auto cursor = editor->document()->find(label); QVERIFY(!cursor.isNull()); cursor.setPosition(cursor.selectionStart() + 1);
            QTest::mouseClick(editor->viewport(), Qt::LeftButton, modifiers, editor->cursorRect(cursor).center());
        };
        click(view, "local", Qt::NoModifier); QCOMPARE(source->source(), index);
        source->setSource(index + "\n\nunsaved"); window.setSourceMode(false); click(view, "local", Qt::ControlModifier);
        QCOMPARE(source->source(), QString("# Target\n\ntext")); QVERIFY(window.openDocument("folder/index.md")); QVERIFY(source->source().contains("unsaved"));
        LinkCapture capture; QDesktopServices::setUrlHandler("file", &capture, "capture"); QDesktopServices::setUrlHandler("https", &capture, "capture");
        click(view, "pdf", Qt::ControlModifier); QCOMPARE(capture.urls.size(), 1); QCOMPARE(capture.urls.last().toLocalFile(), root + "/manual.pdf");
        click(view, "outside", Qt::ControlModifier); QCOMPARE(capture.urls.size(), 2); QCOMPARE(capture.urls.last().toLocalFile(), library.filePath("outside.md"));
        click(view, "web", Qt::ControlModifier); QCOMPARE(capture.urls.size(), 3); QCOMPARE(capture.urls.last(), QUrl("https://example.com/page?q=1"));
        window.setSourceMode(true); click(source, "local", Qt::ControlModifier); QCOMPARE(source->source(), QString("# Target\n\ntext"));
        QVERIFY(window.openDocument("folder/index.md")); click(source, "web", Qt::ControlModifier); QCOMPARE(capture.urls.size(), 4);
    }
};
QTEST_MAIN(SelectionLinkTests)
#include "SelectionLinkTests.moc"
