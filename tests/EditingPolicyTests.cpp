#include "ui/document/MarkdownDocumentView.h"
#include "ui/document/MarkdownEditor.h"
#include "ui/MainWindow.h"
#include "core/library/MarkdownManager.h"
#include "core/markdown/MarkdownTableEditing.h"
#include "core/markdown/MarkdownListEditing.h"
#include "core/markdown/MarkdownBlockSyntax.h"
#include "core/markdown/MarkdownParagraphEditing.h"
#include "core/markdown/MarkdownImport.h"
#include "core/markdown/MarkdownParser.h"
#include "core/markdown/MarkdownSourceMap.h"
#include "core/settings/MarkdownSettings.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QTextTable>
#include <QTextBlock>
#include <QTextList>
#include <QSettings>
#include <QFile>
#include <QClipboard>

class EditingPolicyTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
private slots:
    void initTestCase() {
        QVERIFY(settings.isValid()); MarkdownSettingsStore::setStoragePath(settings.filePath("settings.bin"));
        QCoreApplication::setOrganizationName("TmdPolicyTests"); QCoreApplication::setApplicationName("EditingPolicy");
        QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        QVERIFY(MarkdownSettingsStore::save(MarkdownSettings()));
    }
    void rectangularTablePreservesExtraContent() {
        const QString input = "Before\n\n| A | B |\n| :--- | ---: |\n| one | two | three |\n\nAfter";
        const auto result = MarkdownTableEditing::normalize(input);
        QVERIFY(result.contains("three")); QVERIFY(result.startsWith("Before\n\n")); QVERIFY(result.endsWith("\n\nAfter"));
        const auto tables = MarkdownTableEditing::tables(result); QCOMPARE(tables.size(), 1);
        QCOMPARE(tables[0].rows.size(), 2); QCOMPARE(tables[0].rows[0].size(), 3);
        QVERIFY(result.contains(":---")); QVERIFY(result.contains("---:"));
        QCOMPARE(MarkdownTableEditing::normalize(result), result);
    }
    void tableKeysAndExit() {
        MarkdownDocumentView view; view.resize(700, 400); view.show(); view.setFocus(); view.setMarkdownSource("keys.md", "");
        QTest::keyClicks(&view, "|"); QTest::keyClick(&view, Qt::Key_Space);
        QVERIFY(view.textCursor().currentTable()); QCOMPARE(view.textCursor().currentTable()->columns(), 1);
        QCOMPARE(view.textCursor().currentTable()->rows(), 1);
        QTest::keyClick(&view, Qt::Key_Tab, Qt::ShiftModifier);
        QVERIFY(view.textCursor().currentTable()); QCOMPARE(view.textCursor().currentTable()->columns(), 2);
        QTest::keyClick(&view, Qt::Key_Return, Qt::ShiftModifier);
        QVERIFY(view.textCursor().currentTable()); QCOMPARE(view.textCursor().currentTable()->rows(), 2);
        QTest::keyClicks(&view, "last"); QTest::keyClick(&view, Qt::Key_Tab);
        QVERIFY(!view.textCursor().currentTable()); QVERIFY(view.markdownSource().contains("last"));
    }
    void tableBackspaceProtectsCellsAndRemovesEmptyTable() {
        MarkdownDocumentView view; view.resize(700, 400); view.show(); view.setFocus();
        const QString input = "| A | B |\n| --- | --- |\n| first | second |";
        view.setMarkdownSource("back.md", input); const int at = input.indexOf("second"); view.restoreSourceCursor(at, at);
        QTest::keyClick(&view, Qt::Key_Backspace); QCOMPARE(view.markdownSource(), input); QCOMPARE(view.inlineCursorSourcePosition(), at);
        view.setMarkdownSource("empty.md", "|   |\n| --- |");
        const int emptyCaret = MarkdownTableEditing::tables(view.markdownSource()).front().rows[0][0].contentStart;
        view.restoreSourceCursor(emptyCaret, emptyCaret);
        auto cursor = view.textCursor(); auto *table = cursor.currentTable(); QVERIFY(table);
        view.setTextCursor(table->cellAt(0, 0).firstCursorPosition()); QTest::keyClick(&view, Qt::Key_Backspace);
        QVERIFY(!view.textCursor().currentTable()); QVERIFY(view.markdownSource().trimmed().isEmpty());
    }
    void gridPasteExpandsWithoutOverwritingOtherCells() {
        const QString input = "| A | B |\n| --- | --- |\n| keep | old |";
        const auto edit = MarkdownTableEditing::operate(input, 0, 1, 1, MarkdownTableEditing::Action::Paste, "one\ttwo\nthree\tfour");
        QVERIFY(edit.handled && edit.changed); const auto table = MarkdownTableEditing::tables(edit.source).front();
        QCOMPARE(table.rows.size(), 3); QCOMPARE(table.rows[0].size(), 3);
        QVERIFY(edit.source.contains("keep")); QVERIFY(!edit.source.contains("old")); QVERIFY(edit.source.contains("four"));
        const auto escaped = MarkdownTableEditing::operate(input, 0, 1, 0, MarkdownTableEditing::Action::Paste, "one\\|part\ttwo");
        QCOMPARE(MarkdownTableEditing::tables(escaped.source).front().rows.front().size(), 2);
        QVERIFY(escaped.source.contains("one\\|part"));
    }
    void ordinaryMultilinePasteStaysInsideOneCell() {
        const QString input = "| A | B |\n| --- | --- |\n| old | keep |";
        MarkdownDocumentView view; view.resize(700, 400); view.show(); view.setFocus(); view.setMarkdownSource("paste-cell.md", input);
        view.setTextCursor(view.document()->find("old")); QApplication::clipboard()->setText("    code\r\nnext | part");
        QTest::keyClick(&view, Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(view.markdownSource(), QString("| A | B |\n| --- | --- |\n|     code<br>next \\| part | keep |"));
        QVERIFY(view.textCursor().currentTable());
        QCOMPARE(view.textCursor().currentTable()->rows(), 2); QCOMPARE(view.textCursor().currentTable()->columns(), 2);
    }
    void emptyListEnterContinuesAndOutdentCleansEmptyPeers() {
        const QString input = "- parent\n    - ";
        const auto enter = MarkdownListEditing::newline(input, input.size(), input.size());
        QCOMPARE(enter.source, input + "\n    - ");
        const QString peers = "- parent\n    - \n    - \n    - \n    - content";
        const auto back = MarkdownListEditing::backspaceIndent(peers, QString("- parent\n    - ").size());
        QCOMPARE(back.source, QString("- parent\n- \n    - content"));
    }
    void parentAndDescendantsMoveTogether() {
        const QString input = "- previous\n- parent\n    - child\n        - leaf\n- after";
        const int caret = input.indexOf("parent"); const auto edit = MarkdownListEditing::indent(input, caret, caret, false);
        QCOMPARE(edit.source, QString("- previous\n    - parent\n        - child\n            - leaf\n- after"));
        const auto back = MarkdownListEditing::backspaceIndent(edit.source, edit.caret); QCOMPARE(back.source, input);
    }
    void restoredTaskBackspaceClearsWholePrefix() {
        const QString input = "before\n\n- [ ]\n\nafter"; const int caret = input.indexOf(']') + 1;
        const auto edit = MarkdownBlockSyntax::backspaceEmptyPrefix(input, caret);
        QVERIFY(edit.handled); QCOMPARE(edit.source, QString("before\n\n\n\nafter")); QCOMPARE(edit.caret, 8);
    }
    void selectedSubtreesIndentOnceAndKeepSelection() {
        const QString input = "- previous\n    - previous child\n- first\n    - child\n- second\n- after";
        const int begin = input.indexOf("first"), end = input.indexOf("- after");
        const auto edit = MarkdownListEditing::indent(input, begin, end, false);
        QCOMPARE(edit.source, QString("- previous\n    - previous child\n    - first\n        - child\n    - second\n- after"));
        QCOMPARE(edit.source.mid(edit.anchor, 5), QString("first"));
        QCOMPARE(edit.source.mid(edit.caret), QString("- after"));
        const auto restored = MarkdownListEditing::indent(edit.source, edit.caret, edit.anchor, true);
        QCOMPARE(restored.source, input); QCOMPARE(restored.anchor, end); QCOMPARE(restored.caret, begin);
    }
    void literalLinkSpellingHandlesTitlesAndEntities() {
        const QString input = "[label](https://example.com/?a=&#49;&amp;b=2 \"title ) remains\")";
        const auto regions = MarkdownSpelling::regions(input); QCOMPARE(regions.size(), 1);
        QCOMPARE(regions.front().end, input.size());
        QCOMPARE(MarkdownSpelling::target(input, regions.front()), QString("https://example.com/?a=1&b=2"));
        const QString label = "[a `]` b](https://example.com)";
        const auto nested = MarkdownSpelling::regions(label); QCOMPARE(nested.size(), 1); QCOMPARE(nested.front().end, label.size());
        const auto escapedImage = MarkdownSpelling::regions("\\![text](url)");
        QCOMPARE(escapedImage.size(), 2); QCOMPARE(escapedImage.back().kind, MarkdownSpelling::Kind::Link);
    }
    void selectedListTabPreservesHiddenBoundary() {
        MarkdownDocumentView view; view.resize(700, 400); view.show(); view.setFocus();
        const QString input = "- previous\n- first\n    - child\n- second\n- after";
        view.setMarkdownSource("selected-list.md", input);
        view.restoreSourceCursor(input.indexOf("first"), input.indexOf("- after"));
        QTest::keyClick(&view, Qt::Key_Tab);
        QCOMPARE(view.markdownSource(), QString("- previous\n    - first\n        - child\n    - second\n- after"));
        QTest::keyClick(&view, Qt::Key_Backtab, Qt::ShiftModifier); QCOMPARE(view.markdownSource(), input);
        QVERIFY(view.textCursor().hasSelection());
    }
    void splittingInlineFormatsLeavesUnpairedLiteralMarkers() {
        for (const QString &delimiter : {QString("**"), QString("*"), QString("_"), QString("~~"), QString("`")}) {
            MarkdownDocumentView view; view.resize(700, 400); view.show(); view.setFocus();
            view.setMarkdownSource("split-inline.md", delimiter + "AB" + delimiter);
            view.restoreSourceCursor(delimiter.size() + 1, delimiter.size() + 1); QTest::keyClick(&view, Qt::Key_Return);
            const QString expected = delimiter + "A\nB" + delimiter;
            QCOMPARE(view.markdownSource(), expected); QCOMPARE(view.toPlainText(), expected);
            const auto first = view.document()->find("A"); QVERIFY(first.charFormat().fontWeight() < QFont::Bold);
            QVERIFY(!first.charFormat().fontItalic()); QVERIFY(!first.charFormat().fontStrikeOut()); QVERIFY(!first.charFormat().fontFixedPitch());
            view.setMarkdownSource("split-inline.md", expected, true); QCOMPARE(view.toPlainText(), expected);
        }
    }
    void physicalBodyLinesAndSpacesSurviveReload() {
        MarkdownDocumentView view;
        const QString input = "first  \n    second\n\n\n  third \n";
        view.setMarkdownSource("space.md", input);
        QCOMPARE(view.toPlainText(), input); QCOMPARE(view.markdownSource(), input);
        view.setMarkdownSource("space.md", input, true); QCOMPARE(view.toPlainText(), input);
        for (const QString &heading : {QString("> ## title"), QString("> title\n> ---")}) {
            const QString quoted = heading + "\n\nbody";
            view.setMarkdownSource("quoted-heading-space.md", quoted);
            QCOMPARE(view.toPlainText(), QString("title\n\nbody")); QCOMPARE(view.markdownSource(), quoted);
        }
    }
    void ordinaryIndentDoesNotCreateHiddenBlocksOrBreakInlineStyle() {
        MarkdownDocumentView view;
        for (const QString &input : {QString("    # literal"), QString("\t# literal"), QString("\t- literal"),
            QString("    ```cpp\n    body\n    ```")}) {
            view.setMarkdownSource("ordinary-indent.md", input);
            QCOMPARE(view.toPlainText(), input); QCOMPARE(view.markdownSource(), input);
        }
        view.setMarkdownSource("indented-inline.md", "    **bold**");
        QCOMPARE(view.toPlainText(), QString("    bold"));
        QVERIFY(view.document()->find("bold").charFormat().fontWeight() >= QFont::Bold);
    }
    void setextOnlyAdjacentLineAndMinimumThree() {
        auto outline = MarkdownParser::outline("first\nsecond\n---"); QCOMPARE(outline.size(), 1); QCOMPARE(outline[0].text, QString("second"));
        QCOMPARE(outline[0].line, 1);
        QVERIFY(MarkdownParser::outline("title\n==").isEmpty());
        QVERIFY(MarkdownParser::outline("title\n--").isEmpty());
        QCOMPARE(MarkdownParser::outline("title\n===").front().level, 1);
    }
    void paragraphAndNestedQuoteKeys() {
        auto edit = MarkdownParagraphEditing::newline("## firstsecond", 8, 8);
        QCOMPARE(edit.source, QString("## first\nsecond")); QCOMPARE(edit.caret, 9);
        edit = MarkdownParagraphEditing::backspace("> > text", 4); QCOMPARE(edit.source, QString("> text"));
        edit = MarkdownParagraphEditing::newline("> > ", 4, 4); QCOMPARE(edit.source, QString("> "));
        edit = MarkdownParagraphEditing::backspace("title\n---\nafter", 0); QCOMPARE(edit.source, QString("title\nafter"));
        edit = MarkdownParagraphEditing::backspace("before\n\n---\n", 12); QCOMPARE(edit.source, QString("before\n\n"));
        edit = MarkdownParagraphEditing::newline("---", 3, 3); QCOMPARE(edit.source, QString("---\n"));
    }
    void importIndentedCodeAndPreserveOtherText() {
        const QString input = "    first();\n        second();\n\nBody";
        const auto converted = MarkdownImport::convertIndentedCode(input);
        QCOMPARE(converted, QString("```\nfirst();\n    second();\n```\n\nBody"));
        QCOMPARE(MarkdownImport::convertIndentedCode(converted), converted);
        QCOMPARE(MarkdownImport::convertIndentedCode("- parent\n    - child"), QString("- parent\n    - child"));
        QCOMPARE(MarkdownImport::paste("```\nline\n```", 5, input), input);
    }
    void htmlAndCommentsAreLiteral() {
        MarkdownDocumentView view;
        const QString input = "<b>text</b>\n\n<!-- first\nsecond -->\n<div>after</div>";
        view.setMarkdownSource("html.md", input); QCOMPARE(view.toPlainText(), input); QCOMPARE(view.markdownSource(), input);
    }
    void linkAndEntityExpandWithoutChangingSource() {
        MarkdownDocumentView view; view.resize(700, 400); view.show(); view.setFocus();
        const QString link = "[label](https://example.com) after";
        view.setMarkdownSource("link.md", link); QCOMPARE(view.toPlainText(), QString("label after"));
        view.restoreSourceCursor(3, 3); QTRY_COMPARE(view.toPlainText(), link); QCOMPARE(view.markdownSource(), link);
        QTest::keyClick(&view, Qt::Key_Backspace); QCOMPARE(view.markdownSource(), QString("[lbel](https://example.com) after"));
        view.setMarkdownSource("entity.md", "&amp; end"); QCOMPARE(view.toPlainText(), QString("& end"));
        view.restoreSourceCursor(2, 2); QTRY_COMPARE(view.toPlainText(), QString("&amp; end"));
        QTest::keyClick(&view, Qt::Key_Backspace); QCOMPARE(view.markdownSource(), QString("&mp; end"));
    }
    void metadataBodyVisibleAndLastCharacterDeletesRegion() {
        MarkdownDocumentView view; view.resize(700, 400); view.show(); view.setFocus();
        view.setMarkdownSource("meta.md", "---\na\n---"); QCOMPARE(view.toPlainText(), QString("a"));
        view.restoreSourceCursor(5, 5); QTest::keyClick(&view, Qt::Key_Backspace);
        QCOMPARE(view.markdownSource(), QString()); QVERIFY(view.toPlainText().isEmpty());
    }
    void arrowRevealsRealSetextMarkers() {
        MarkdownDocumentView view; view.resize(700, 400); view.show(); view.setFocus();
        const QString input = "title\n---"; view.setMarkdownSource("setext.md", input); view.restoreSourceCursor(5, 5);
        QTest::keyClick(&view, Qt::Key_Down); QCOMPARE(view.markdownSource(), input);
        QCOMPARE(view.textCursor().block().text(), QString("---")); QCOMPARE(view.inlineCursorSourcePosition(), 9);
    }
    void partialCrossBlockDeletionRetainsFence() {
        const QString input = "before\n```cpp\nAB\n```\nafter";
        MarkdownSourceMap map; const QString display = "before\nAB\nafter"; map.rebuild(input, display);
        int caret = 0; const auto result = map.edited(6, 2, {}, &caret);
        QCOMPARE(result, QString("before\n```cpp\nB\n```\nafter"));
        MarkdownDocumentView view; view.resize(700, 400); view.show(); view.setFocus(); view.setMarkdownSource("selection.md", input);
        view.restoreSourceCursor(6, input.indexOf("AB") + 1); QTest::keyClick(&view, Qt::Key_Delete);
        QCOMPARE(view.markdownSource(), result); QVERIFY(view.toPlainText().contains("\nB\n"));
    }
    void crossBlockDeletionOfWholeCodeDoesNotLeaveAnUnpairedFence() {
        const QString input = "before\n```cpp\nAB\n```\nafter";
        MarkdownDocumentView view; view.resize(700, 400); view.show(); view.setFocus(); view.setMarkdownSource("whole-code.md", input);
        view.restoreSourceCursor(6, input.indexOf("AB") + 2); QTest::keyClick(&view, Qt::Key_Delete);
        QCOMPARE(view.markdownSource(), QString("before\nafter")); QCOMPARE(view.toPlainText(), QString("before\nafter"));
    }
    void writtenOrderedNumbersRemainWritten() {
        MarkdownDocumentView view; view.setMarkdownSource("number.md", "1. first\n3. third");
        view.restoreSourceCursor(3, 3); QVERIFY(view.textCursor().block().textList());
        QCOMPARE(view.textCursor().block().textList()->format().start(), 1);
        view.restoreSourceCursor(12, 12); QVERIFY(view.textCursor().block().textList());
        QCOMPARE(view.textCursor().block().textList()->format().start(), 3);
    }
    void exitListHasOneBodyPositionAndOneMerge() {
        MarkdownDocumentView view; view.resize(700, 400); view.show(); view.setFocus();
        view.setMarkdownSource("exit.md", "- first\n- "); view.restoreSourceCursor(10, 10);
        QTest::keyClick(&view, Qt::Key_Backspace); QTest::keyClick(&view, Qt::Key_Backspace);
        QCOMPARE(view.markdownSource(), QString("- first\n\n")); QCOMPARE(view.toPlainText(), QString("first\n"));
        QTest::keyClick(&view, Qt::Key_Backspace); QCOMPARE(view.markdownSource(), QString("- first"));
    }
    void exitQuoteAndTypeBodyStaysOutside() {
        MarkdownDocumentView view; view.resize(700, 400); view.show(); view.setFocus();
        view.setMarkdownSource("quote.md", "> first\n> "); view.restoreSourceCursor(10, 10); QTest::keyClick(&view, Qt::Key_Return);
        QCOMPARE(view.toPlainText(), QString("first\n")); QTest::keyClicks(&view, "body");
        view.setMarkdownSource("quote.md", view.markdownSource(), true);
        QCOMPARE(view.textCursor().blockFormat().intProperty(QTextFormat::BlockQuoteLevel), 0);
        QCOMPARE(view.toPlainText(), QString("first\nbody"));
    }
    void emptyPeerCleanupDoesNotLeaveADeletedRow() {
        const QString input = "- parent\n    - \n    - ";
        QCOMPARE(MarkdownListEditing::backspaceIndent(input, 15).source, QString("- parent\n- "));
    }
    void enterCreatesFenceWithTypedOrNoLanguage() {
        for (const auto &language : {QString(), QString("cpp")}) {
            MarkdownEditor view; view.resize(700, 400); view.show(); view.setFocus(); view.setSource("```" + language);
            view.moveCursor(QTextCursor::End); QCoreApplication::processEvents(); QTest::keyClick(&view, Qt::Key_Return);
            QCOMPARE(view.source(), "```" + language + "\n\n```\n");
            QCOMPARE(view.textCursor().position(), 4 + language.size());
        }
    }
    void setextCreatesImmediatelyAndBackspaceEditsUnderline() {
        MarkdownDocumentView view; view.resize(700, 400); view.show(); view.setFocus();
        view.setMarkdownSource("typed-title.md", "title\n"); view.restoreSourceCursor(6, 6); QTest::keyClicks(&view, "---");
        QCOMPARE(view.toPlainText(), QString("title")); QCOMPARE(view.markdownSource(), QString("title\n---"));
        QTest::keyClick(&view, Qt::Key_Backspace); QCOMPARE(view.markdownSource(), QString("title\n--"));
    }
    void imageDeletionTakesTwoPressesAndDoubleClickShowsSource() {
        QTemporaryDir directory; QVERIFY(directory.isValid()); QFile image(directory.filePath("image.svg")); QVERIFY(image.open(QIODevice::WriteOnly));
        image.write("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"40\" height=\"20\"><rect width=\"40\" height=\"20\" fill=\"blue\"/></svg>"); image.close();
        MarkdownDocumentView view; view.resize(700, 400); view.show(); view.setFocus();
        const QString input = "![alt](image.svg)"; view.setMarkdownSource(directory.filePath("doc.md"), input); view.restoreSourceCursor(input.size(), input.size());
        QTest::keyClick(&view, Qt::Key_Backspace); QCOMPARE(view.markdownSource(), input); QVERIFY(view.textCursor().hasSelection());
        QTest::keyClick(&view, Qt::Key_Backspace); QVERIFY(view.markdownSource().isEmpty()); QVERIFY(QFile::exists(image.fileName()));
        view.setMarkdownSource(directory.filePath("doc.md"), input); view.restoreSourceCursor(0, 0);
        QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, view.cursorRect().center() + QPoint(8, 0));
        QTRY_COMPARE(view.toPlainText(), input); QCOMPARE(view.markdownSource(), input);
    }
    void taskClickTogglesSource() {
        MarkdownDocumentView view; view.resize(700, 400); view.show(); view.setFocus(); view.setMarkdownSource("task-click.md", "- [ ] task");
        view.restoreSourceCursor(6, 6); const auto point = view.cursorRect().center() - QPoint(14, 0);
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, point); QCOMPARE(view.markdownSource(), QString("- [x] task"));
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, point); QCOMPARE(view.markdownSource(), QString("- [ ] task"));
    }
    void tableInsertUndoRedoAndSourceSwitch() {
        QTemporaryDir directory; QVERIFY(directory.isValid()); MarkdownManager seed(directory.path());
        const QString before = "| A | B |\n| --- | --- |\n| one | two |"; QVERIFY(seed.createDocument("doc.md", before));
        MainWindow window(directory.path()); window.resize(1100, 700); window.show(); QVERIFY(window.openDocument("doc.md")); window.setSourceMode(false);
        auto *view = window.findChild<MarkdownDocumentView *>(); auto *editor = window.findChild<MarkdownEditor *>(); QVERIFY(view && editor);
        view->setFocus(); const int caret = before.indexOf("one"); view->restoreSourceCursor(caret, caret);
        QTest::keyClick(view, Qt::Key_Tab, Qt::ShiftModifier); const QString inserted = editor->source(); QVERIFY(inserted != before);
        QCOMPARE(view->markdownSource(), inserted); QCOMPARE(MarkdownTableEditing::tables(inserted).front().rows.front().size(), 3);
        QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); QCOMPARE(editor->source(), before);
        QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier); QCOMPARE(editor->source(), inserted);
        window.setSourceMode(true); window.setSourceMode(false); QCOMPARE(view->markdownSource(), inserted);
    }
    void loadingRaggedTableIsDirtyUntilSave() {
        QTemporaryDir directory; QVERIFY(directory.isValid()); MarkdownManager seed(directory.path());
        const QString before = "| A | B |\n| --- | --- |\n| one | two | three |";
        const QString after = "| A | B |   |\n| --- | --- | --- |\n| one | two | three |";
        QVERIFY(seed.createDocument("doc.md", before));
        MainWindow window(directory.path()); window.show(); QVERIFY(window.openDocument("doc.md"));
        auto *editor = window.findChild<MarkdownEditor *>(); QVERIFY(editor);
        QTRY_COMPARE(editor->source(), after); QVERIFY(editor->document()->isModified());
        const auto readFile = [&] { QFile file(directory.filePath("doc.md")); if (!file.open(QIODevice::ReadOnly)) return QString(); return QString::fromUtf8(file.readAll()); };
        QCOMPARE(readFile(), before);
        QTest::keyClick(&window, Qt::Key_S, Qt::ControlModifier); QTRY_COMPARE(readFile(), after);
        QVERIFY(!editor->document()->isModified());
    }
    void leavingCodeAddsOnlyOneBodyRow() {
        for (bool rendered : {true, false}) {
            MarkdownDocumentView document; MarkdownEditor editor;
            QTextEdit *view = rendered ? static_cast<QTextEdit *>(&document) : &editor;
            view->resize(700, 400); view->show(); view->setFocus();
            const QString input = "```cpp\nx\n```";
            if (rendered) { document.setMarkdownSource("code-exit.md", input); document.restoreSourceCursor(8, 8); }
            else { editor.setSource(input); auto cursor = editor.textCursor(); cursor.setPosition(8); editor.setTextCursor(cursor); }
            QTest::keyClick(view, Qt::Key_Down);
            QCOMPARE(rendered ? document.markdownSource() : editor.source(), input + '\n');
            QVERIFY(view->textCursor().block().text().isEmpty());
            QTest::keyClicks(view, "body"); QCOMPARE(rendered ? document.markdownSource() : editor.source(), input + "\nbody");
        }
    }
};
QTEST_MAIN(EditingPolicyTests)
#include "EditingPolicyTests.moc"
