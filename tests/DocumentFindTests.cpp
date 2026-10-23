#include "ui/document/DocumentFindDialog.h"
#include "ui/document/MarkdownDocumentView.h"
#include "ui/document/MarkdownEditor.h"
#include "ui/document/SearchHighlightOverlay.h"
#include "ui/MainWindow.h"
#include "ui/common/Theme.h"
#include "core/library/MarkdownManager.h"
#include "core/settings/MarkdownSettings.h"
#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QScrollBar>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextLayout>
#include <QToolButton>
#include <QtTest>

class DocumentFindTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_settings;
private slots:
    void initTestCase() {
        QVERIFY(m_settings.isValid());
        QCoreApplication::setOrganizationName("TmdFindTests"); QCoreApplication::setApplicationName("DocumentFind");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
        MarkdownSettingsStore::setStoragePath(m_settings.filePath("settings.json"));
        QVERIFY(MarkdownSettingsStore::save(MarkdownSettings()));
        applyTreeMdTheme(*qApp);
    }
    void liveHighlightsAndNavigation() {
        QTextEdit editor; editor.resize(800, 500); editor.show(); editor.setPlainText("alpha ALPHA alpha\nother");
        auto cursor = editor.textCursor(); cursor.setPosition(0); editor.setTextCursor(cursor);
        editor.document()->setModified(false); const int undo = editor.document()->availableUndoSteps();
        DocumentFindDialog finder(&editor); finder.setEditor(&editor); finder.openSearch();
        auto *input = finder.findChild<QLineEdit *>("documentFindInput");
        auto *status = finder.findChild<QLabel *>("documentFindStatus"); QVERIFY(input); QVERIFY(status);
        QTest::keyClicks(input, "alpha");
        QCOMPARE(status->text(), QString("1/3")); QCOMPARE(editor.extraSelections().size(), 3);
        input->setCursorPosition(2); QTest::keyClick(input, Qt::Key_F, Qt::ControlModifier);
        QCOMPARE(input->selectedText(), QString("alpha"));
        const auto first = editor.extraSelections();
        QVERIFY(first[0].format.property(SearchHighlightOverlay::Background) != first[1].format.property(SearchHighlightOverlay::Background));
        QTest::keyClick(input, Qt::Key_Down); QCOMPARE(status->text(), QString("2/3"));
        QTest::keyClick(input, Qt::Key_Up); QCOMPARE(status->text(), QString("1/3"));
        QTest::keyClick(input, Qt::Key_Return); QCOMPARE(status->text(), QString("2/3"));
        QTest::keyClick(input, Qt::Key_Return, Qt::ShiftModifier); QCOMPARE(status->text(), QString("1/3"));
        QTest::mouseClick(finder.findChild<QToolButton *>("documentFindPrevious"), Qt::LeftButton);
        QCOMPARE(status->text(), QString("3/3"));
        QTest::mouseClick(finder.findChild<QToolButton *>("documentFindNext"), Qt::LeftButton);
        QCOMPARE(status->text(), QString("1/3"));
        QCOMPARE(editor.toPlainText(), QString("alpha ALPHA alpha\nother"));
        QCOMPARE(editor.textCursor().position(), 0); QVERIFY(!editor.document()->isModified());
        QCOMPARE(editor.document()->availableUndoSteps(), undo);
        input->setText("missing"); QCOMPARE(status->text(), QString::fromUtf8("未找到"));
        QVERIFY(editor.extraSelections().isEmpty()); QVERIFY(!finder.findChild<QToolButton *>("documentFindNext")->isEnabled());
        input->clear(); QCOMPARE(status->text(), QString("0/0"));
        QTest::keyClicks(input, "alpha"); QTest::keyClick(input, Qt::Key_Escape);
        QVERIFY(!finder.isVisible()); QVERIFY(editor.extraSelections().isEmpty());
        finder.openSearch(); QCOMPARE(status->text(), QString("1/3"));
        QCOMPARE(finder.findChildren<QLineEdit *>().size(), 2);
    }
    void replaceHoldsSelectionUntilSearchOrClose() {
        QTextEdit editor; editor.resize(800, 500); editor.show(); editor.setPlainText("cat cat cat");
        DocumentFindDialog finder(&editor); finder.setEditor(&editor); finder.openSearch();
        auto *query = finder.findChild<QLineEdit *>("documentFindInput");
        auto *replacement = finder.findChild<QLineEdit *>("documentReplaceInput");
        auto *status = finder.findChild<QLabel *>("documentFindStatus");
        query->setText("cat"); QVERIFY(!replacement->isVisible());
        QTest::keyClick(query, Qt::Key_Tab); QVERIFY(replacement->isVisible());
        QVERIFY(finder.findChild<QToolButton *>("documentReplaceMode")->isChecked());
        replacement->setText("kitten"); QTest::keyClick(replacement, Qt::Key_Return);
        QCOMPARE(editor.toPlainText(), QString("kitten cat cat"));
        QCOMPARE(editor.textCursor().selectedText(), QString("kitten")); QCOMPARE(status->text(), QString::fromUtf8("已替换"));
        QTest::qWait(250); QCOMPARE(editor.textCursor().selectedText(), QString("kitten"));
        const int undo = editor.document()->availableUndoSteps();
        QTest::keyClick(replacement, Qt::Key_Return); QCOMPARE(editor.toPlainText(), QString("kitten cat cat"));
        QCOMPARE(editor.document()->availableUndoSteps(), undo);
        replacement->setText("dog"); QCOMPARE(editor.textCursor().selectedText(), QString("kitten"));
        QTest::keyClick(replacement, Qt::Key_Return); QCOMPARE(editor.toPlainText(), QString("dog cat cat"));
        QCOMPARE(editor.textCursor().selectedText(), QString("dog"));
        QTest::keyClick(replacement, Qt::Key_Down); QVERIFY(!editor.textCursor().hasSelection()); QCOMPARE(status->text(), QString("1/2"));
        QTest::keyClick(replacement, Qt::Key_Return); QCOMPARE(editor.toPlainText(), QString("dog dog cat"));
        QCOMPARE(editor.textCursor().selectedText(), QString("dog"));
        QTest::keyClick(replacement, Qt::Key_Tab); QVERIFY(!replacement->isVisible());
        QCOMPARE(editor.textCursor().selectedText(), QString("dog"));
        query->setText("dog"); QVERIFY(!editor.textCursor().hasSelection()); QCOMPARE(status->text(), QString("1/2"));
        QTest::keyClick(query, Qt::Key_Tab); QTest::keyClick(replacement, Qt::Key_Return);
        QVERIFY(editor.textCursor().hasSelection());
        QTest::mouseClick(finder.findChild<QToolButton *>("documentFindClose"), Qt::LeftButton);
        QVERIFY(!finder.isVisible()); QVERIFY(!editor.textCursor().hasSelection()); QVERIFY(editor.extraSelections().isEmpty());
        editor.undo(); QCOMPARE(editor.toPlainText(), QString("dog cat cat"));
        editor.undo(); QCOMPARE(editor.toPlainText(), QString("kitten cat cat"));
        editor.undo(); QCOMPARE(editor.toPlainText(), QString("cat cat cat"));
    }
    void renderedReplacementPreservesMarkdown_data() {
        QTest::addColumn<QString>("source"); QTest::addColumn<QString>("expected");
        for (const QString source : {QString("**cat**"), QString("*cat*"), QString("`cat`"), QString("# cat"),
             QString("> cat"), QString("- cat"), QString("```cpp\ncat\n```\n"),
             QString("| cat |\n| --- |"), QString("[cat](https://example.com)")}) {
            auto expected = source; expected.replace("cat", "dog");
            QTest::newRow(qPrintable(source)) << source << expected;
        }
    }
    void renderedReplacementPreservesMarkdown() {
        QFETCH(QString, source); QFETCH(QString, expected);
        MarkdownDocumentView view; view.resize(800, 500); view.show(); view.setMarkdownSource("", source);
        DocumentFindDialog finder(&view); finder.setEditor(&view); finder.openSearch();
        auto *query = finder.findChild<QLineEdit *>("documentFindInput");
        auto *replacement = finder.findChild<QLineEdit *>("documentReplaceInput");
        query->setText("cat"); QTest::keyClick(query, Qt::Key_Tab); replacement->setText("dog");
        QSignalSpy edited(&view, &MarkdownDocumentView::sourceEdited);
        QTest::keyClick(replacement, Qt::Key_Return); QCOMPARE(view.markdownSource(), expected);
        QCOMPARE(view.textCursor().selectedText(), QString("dog")); QCOMPARE(edited.size(), 1);
        QTest::qWait(250); QCOMPARE(view.textCursor().selectedText(), QString("dog"));
        QTest::keyClick(replacement, Qt::Key_Return); QCOMPARE(edited.size(), 1);
        auto range = view.textCursor(); range.setPosition(range.selectionStart());
        const auto start = static_cast<QTextEdit *>(&view)->cursorRect(range);
        const auto end = static_cast<QTextEdit *>(&view)->cursorRect(view.textCursor());
        const auto pixels = view.viewport()->grab().toImage(); int bright = 0;
        for (int y = start.top(); y <= start.bottom(); ++y) for (int x = start.left(); x < end.left(); ++x) {
            const auto color = pixels.pixelColor(x, y);
            if (color.red() > 220 && color.green() > 220 && color.blue() > 220) ++bright;
        }
        QVERIFY2(bright > 5, "Rounded highlights must redraw readable text at its native position");
        if (source.startsWith("```")) QVERIFY(view.viewport()->grab().save(QCoreApplication::applicationDirPath() + "/find-replace-code.png"));
        if (source.startsWith('|')) QVERIFY(view.viewport()->grab().save(QCoreApplication::applicationDirPath() + "/find-replace-table.png"));
        finder.close(); QVERIFY(!view.textCursor().hasSelection()); QCOMPARE(view.markdownSource(), expected);
    }
    void emptyReplacementAndReadOnly() {
        QTextEdit editor; editor.setPlainText("cat cat");
        DocumentFindDialog finder(&editor); finder.setEditor(&editor); finder.openSearch();
        auto *query = finder.findChild<QLineEdit *>("documentFindInput");
        auto *replacement = finder.findChild<QLineEdit *>("documentReplaceInput");
        query->setText("cat"); QTest::keyClick(query, Qt::Key_Tab);
        editor.setReadOnly(true); QTest::keyClick(replacement, Qt::Key_Return); QCOMPARE(editor.toPlainText(), QString("cat cat"));
        editor.setReadOnly(false); QTest::keyClick(replacement, Qt::Key_Return); QCOMPARE(editor.toPlainText(), QString(" cat"));
        QCOMPARE(finder.findChild<QLabel *>("documentFindStatus")->text(), QString::fromUtf8("已替换"));
        QTest::keyClick(replacement, Qt::Key_Return); QCOMPARE(editor.toPlainText(), QString(" cat"));
        QTest::keyClick(replacement, Qt::Key_Down); QTest::keyClick(replacement, Qt::Key_Return); QCOMPARE(editor.toPlainText(), QString(" "));
        finder.close(); editor.undo(); QCOMPARE(editor.toPlainText(), QString(" cat"));
    }
    void renderedUndoRedoAndDocumentSwitch() {
        QTemporaryDir library; QVERIFY(library.isValid()); MarkdownManager seed(library.path());
        QVERIFY(seed.createDocument("one.md", "**cat** and cat")); QVERIFY(seed.createDocument("two.md", "cat next"));
        MainWindow window(library.path()); window.resize(1100, 800); window.show(); QVERIFY(window.openDocument("one.md"));
        auto *view = window.findChild<MarkdownDocumentView *>(); auto *source = window.findChild<MarkdownEditor *>();
        auto *finder = window.findChild<DocumentFindDialog *>(); finder->openSearch();
        auto *query = finder->findChild<QLineEdit *>("documentFindInput");
        auto *replacement = finder->findChild<QLineEdit *>("documentReplaceInput");
        query->setText("cat"); QTest::keyClick(query, Qt::Key_Tab); replacement->setText("kitten");
        QTest::keyClick(replacement, Qt::Key_Return); QCOMPARE(source->source(), QString("**kitten** and cat"));
        QTest::qWait(300); QCOMPARE(view->textCursor().selectedText(), QString("kitten"));
        QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); QTRY_COMPARE(source->source(), QString("**cat** and cat"));
        QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier); QTRY_COMPARE(source->source(), QString("**kitten** and cat"));
        QTest::qWait(300);
        auto held = view->extraSelections().last(); QVERIFY(held.format.boolProperty(SearchHighlightOverlay::Held));
        QCOMPARE(held.cursor.selectedText(), QString("kitten"));
        replacement->setText("dog"); QTest::keyClick(replacement, Qt::Key_Return);
        QCOMPARE(source->source(), QString("**dog** and cat"));
        QVERIFY(window.openDocument("two.md")); QTest::qWait(250);
        QCOMPARE(finder->findChild<QLabel *>("documentFindStatus")->text(), QString("1/1"));
        QTest::keyClick(replacement, Qt::Key_Return); QCOMPARE(source->source(), QString("dog next"));
        finder->close(); QVERIFY(!view->textCursor().hasSelection());
        window.setSourceMode(true); finder->openSearch(); query->setText("dog"); replacement->setText("fox");
        QTest::keyClick(replacement, Qt::Key_Return); QCOMPARE(source->source(), QString("fox next"));
        QCOMPARE(source->textCursor().selectedText(), QString("fox"));
        finder->close(); source->undo(); QCOMPARE(source->source(), QString("dog next"));
    }
    void roundedHighlightsAndWindow() {
        QTextEdit editor; editor.resize(700, 350); editor.setPlainText("  roundedword other"); editor.show();
        DocumentFindDialog finder(&editor); finder.setEditor(&editor); finder.openSearch();
        auto *query = finder.findChild<QLineEdit *>("documentFindInput"); query->setText("roundedword");
        auto checkRounded = [&] {
            QCoreApplication::processEvents();
            auto cursor = editor.extraSelections().last().cursor; cursor.setPosition(cursor.selectionStart());
            const auto native = editor.cursorRect(cursor); const int left = native.left() - 2;
            const auto shot = editor.viewport()->grab().toImage();
            // Antialiased corners retain the page color, unlike the flat top edge.
            const auto corner = shot.pixelColor(left, native.top()); const auto edge = shot.pixelColor(left + 8, native.top());
            QVERIFY(corner != edge); QCOMPARE(edge, QColor("#b76c20"));
        };
        checkRounded(); QTest::keyClick(query, Qt::Key_Tab);
        auto *replacement = finder.findChild<QLineEdit *>("documentReplaceInput"); replacement->setText("replacedword");
        QTest::keyClick(replacement, Qt::Key_Return); checkRounded();
        QVERIFY(editor.viewport()->grab().save(QCoreApplication::applicationDirPath() + "/find-replace-rounded-selection.png"));
        const auto window = finder.grab().toImage(); QVERIFY(window.hasAlphaChannel()); QCOMPARE(window.pixelColor(0, 0).alpha(), 0);
        QCOMPARE(window.pixelColor(window.width() / 2, 3), QApplication::palette().color(QPalette::Window));
        QVERIFY(finder.grab().save(QCoreApplication::applicationDirPath() + "/find-replace-dialog.png"));
    }
    void wrappedHighlightsKeepTextAndFollowScrolling() {
        QTextEdit editor; editor.resize(230, 180); editor.show();
        const QString query = "a long match that wraps across several visual lines with words";
        editor.setPlainText(query + QString("\nother row").repeated(35) + '\n' + query);
        DocumentFindDialog finder(&editor); finder.setEditor(&editor); finder.openSearch();
        auto *input = finder.findChild<QLineEdit *>("documentFindInput"); input->setText(query);
        QCoreApplication::processEvents();
        const auto block = editor.document()->begin(); const auto *layout = block.layout();
        QVERIFY(layout->lineCount() > 1); const auto pixels = editor.viewport()->grab().toImage();
        for (int i = 0; i < layout->lineCount(); ++i) {
            QTextCursor cursor(block); cursor.setPosition(block.position() + layout->lineAt(i).textStart());
            const auto native = editor.cursorRect(cursor); int bright = 0;
            for (int y = native.top(); y <= native.bottom(); ++y) for (int x = 0; x < pixels.width(); ++x) {
                const auto color = pixels.pixelColor(x, y);
                if (color.red() > 220 && color.green() > 220 && color.blue() > 220) ++bright;
            }
            QVERIFY(bright > 5);
        }
        QTest::keyClick(input, Qt::Key_Down); QVERIFY(editor.verticalScrollBar()->value() > 0);
        auto *layer = editor.viewport()->findChild<QWidget *>("documentSearchHighlights"); QVERIFY(layer);
        QCOMPARE(layer->geometry(), editor.viewport()->rect());
        QVERIFY(editor.viewport()->grab().save(QCoreApplication::applicationDirPath() + "/find-replace-wrapped.png"));
    }
    void highlightKeepsRenderedDelimiterCaretVisible() {
        MarkdownDocumentView view; view.resize(600, 300); view.show(); view.setMarkdownSource("", "**word**");
        DocumentFindDialog finder(&view); finder.setEditor(&view); finder.openSearch();
        finder.findChild<QLineEdit *>("documentFindInput")->setText("word");
        view.restoreSourceCursor(2, 2); QApplication::setActiveWindow(&view); view.setFocus();
        QCoreApplication::processEvents(); QVERIFY(view.hasFocus()); QCOMPARE(view.cursorWidth(), 0);
        const auto caret = view.cursorRect(); QVERIFY(caret.width() > 0);
        const auto shot = view.viewport()->grab().toImage();
        QCOMPARE(shot.pixelColor(caret.center()), view.palette().color(QPalette::Text));
    }
    void literalUnicodeAndWhitespace_data() {
        QTest::addColumn<QString>("text"); QTest::addColumn<QString>("query"); QTest::addColumn<int>("count");
        QTest::newRow("literal") << "a.b axb a.b [x]" << "a.b" << 2;
        QTest::newRow("brackets") << "[x] x [x]" << "[x]" << 2;
        QTest::newRow("chinese") << QString::fromUtf8("中文查找，中文标题\n中文正文") << QString::fromUtf8("中文") << 3;
        QTest::newRow("spaces") << "a  b a b a  b" << "  " << 2;
        QTest::newRow("overlap") << "aaaaa" << "aa" << 2;
    }
    void literalUnicodeAndWhitespace() {
        QFETCH(QString, text); QFETCH(QString, query); QFETCH(int, count);
        QTextEdit editor; editor.setPlainText(text);
        DocumentFindDialog finder(&editor); finder.setEditor(&editor); finder.openSearch();
        finder.findChild<QLineEdit *>("documentFindInput")->setText(query);
        QCOMPARE(editor.extraSelections().size(), count);
        for (const auto &selection : editor.extraSelections()) QCOMPARE(selection.cursor.selectedText(), query);
    }
    void updatesAfterEditsAndPreservesOtherOverlays() {
        QTextEdit editor; editor.setPlainText("match\nmatch");
        QTextEdit::ExtraSelection existing; existing.cursor = QTextCursor(editor.document());
        existing.cursor.setPosition(0); existing.cursor.setPosition(1, QTextCursor::KeepAnchor);
        existing.format.setUnderlineStyle(QTextCharFormat::WaveUnderline); editor.setExtraSelections({existing});
        DocumentFindDialog finder(&editor); finder.setEditor(&editor); finder.openSearch();
        auto *input = finder.findChild<QLineEdit *>("documentFindInput"); input->setText("match");
        QCOMPARE(editor.extraSelections().size(), 3);
        QTextCursor cursor(editor.document()); cursor.movePosition(QTextCursor::End); cursor.insertText(" match");
        QTRY_COMPARE(editor.extraSelections().size(), 4);
        editor.undo(); QTRY_COMPARE(editor.extraSelections().size(), 3);
        finder.close(); QCOMPARE(editor.extraSelections().size(), 1);
        QCOMPARE(editor.extraSelections().first().format.underlineStyle(), QTextCharFormat::WaveUnderline);
    }
    void navigationRevealsOffscreenMatches() {
        QTextEdit editor; editor.resize(700, 320); editor.show();
        editor.setPlainText("needle\n" + QString("ordinary row\n").repeated(120) + "needle");
        DocumentFindDialog finder(&editor); finder.setEditor(&editor); finder.openSearch();
        auto *input = finder.findChild<QLineEdit *>("documentFindInput"); input->setText("needle");
        QTest::keyClick(input, Qt::Key_Return);
        const auto match = editor.extraSelections().last().cursor;
        QCoreApplication::processEvents(); const auto rect = editor.cursorRect(match);
        QVERIFY(editor.verticalScrollBar()->value() > 0);
        QVERIFY(rect.top() >= 0 && rect.bottom() <= editor.viewport()->height());
        QTextCursor edit(editor.document()); edit.setPosition(0); edit.insertText("x");
        const int scroll = editor.verticalScrollBar()->value();
        QTest::qWait(30); QCOMPARE(editor.extraSelections().size(), 2);
        QCOMPARE(editor.verticalScrollBar()->value(), scroll);
    }
    void shortcutSwitchingAndDocumentReplacement() {
        QTemporaryDir library; QVERIFY(library.isValid()); MarkdownManager seed(library.path());
        QVERIFY(seed.createDocument("one.md", "## needle\n\n**needle** and needle\n"));
        QVERIFY(seed.createDocument("two.md", "needle once"));
        MainWindow window(library.path()); window.resize(1100, 800); window.show(); QVERIFY(window.openDocument("one.md"));
        auto *view = window.findChild<MarkdownDocumentView *>(); auto *source = window.findChild<MarkdownEditor *>();
        auto *finder = window.findChild<DocumentFindDialog *>(); QVERIFY(view); QVERIFY(source); QVERIFY(finder);
        view->setFocus(); window.activateWindow(); QCoreApplication::processEvents();
        QTest::keyClick(view, Qt::Key_F, Qt::ControlModifier); QTRY_VERIFY(finder->isVisible());
        auto *input = finder->findChild<QLineEdit *>("documentFindInput"); input->setText("needle");
        input->setCursorPosition(2); QTest::keyClick(input, Qt::Key_F, Qt::ControlModifier);
        QCOMPARE(input->selectedText(), QString("needle"));
        QCOMPARE(view->extraSelections().size(), 3); QVERIFY(source->extraSelections().isEmpty());
        const QString original = source->source(); const int undo = source->document()->availableUndoSteps();
        input->setText("##"); QVERIFY(view->extraSelections().isEmpty());
        window.setSourceMode(true); QTRY_COMPARE(source->extraSelections().size(), 1); QVERIFY(view->extraSelections().isEmpty());
        input->setText("needle"); QCOMPARE(source->extraSelections().size(), 3);
        QVERIFY(window.openDocument("two.md")); QTRY_COMPARE(source->extraSelections().size(), 1);
        QVERIFY(window.openDocument("one.md")); QTRY_COMPARE(source->extraSelections().size(), 3);
        QCOMPARE(source->source(), original); QCOMPARE(source->document()->availableUndoSteps(), undo);
        QVERIFY(!source->document()->isModified());
        finder->close(); QVERIFY(source->extraSelections().isEmpty());
        source->setFocus(); window.activateWindow(); QCoreApplication::processEvents();
        QTest::keyClick(source, Qt::Key_F, Qt::ControlModifier); QTRY_VERIFY(finder->isVisible());
        QCOMPARE(source->extraSelections().size(), 3); finder->close();
        window.setSourceMode(false); source->setFocus();
        window.findChild<QAction *>("documentFindAction")->trigger(); QTRY_VERIFY(finder->isVisible());
        QCOMPARE(window.findChildren<DocumentFindDialog *>().size(), 1);
        QCOMPARE(view->extraSelections().size(), 3);
        window.grab().save(QCoreApplication::applicationDirPath() + "/find-replace-main-window.png");
        finder->close();
    }
    void noDocumentDoesNotOpen() {
        QTemporaryDir library; QVERIFY(library.isValid()); MainWindow window(library.path());
        auto *finder = window.findChild<DocumentFindDialog *>(); QVERIFY(finder);
        window.findChild<QAction *>("documentFindAction")->trigger(); QVERIFY(!finder->isVisible());
    }
};
QTEST_MAIN(DocumentFindTests)
#include "DocumentFindTests.moc"
