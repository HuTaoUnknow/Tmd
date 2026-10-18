#include "ui/document/MarkdownDocumentView.h"
#include "ui/document/MarkdownEditor.h"
#include "core/settings/MarkdownSettings.h"
#include "application/editing/EditCommandRouter.h"
#include <QApplication>
#include <QInputMethodEvent>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QtTest>
#include <memory>

class EditingHeadingsAuditTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void headingSpacesAndChineseInput_data();
    void headingSpacesAndChineseInput();
    void quoteEnterKeepsSeparateParagraphs_data();
    void quoteEnterKeepsSeparateParagraphs();
    void emptyQuoteEnterLeavesContainer_data();
    void emptyQuoteEnterLeavesContainer();
    void headingEnterAndShiftEnterAlwaysStartBody_data();
    void headingEnterAndShiftEnterAlwaysStartBody();
    void paragraphEnterAndShiftEnterAgree_data();
    void paragraphEnterAndShiftEnterAgree();
    void quotedHeadingImportKeepsContainer_data();
    void quotedHeadingImportKeepsContainer();
    void headingMarkerBackspace_data();
    void headingMarkerBackspace();
    void headingMarkerArrowNavigation();
private:
    std::unique_ptr<QTemporaryDir> m_settings;
};

void EditingHeadingsAuditTests::headingMarkerBackspace_data() {
    QTest::addColumn<QString>("initial"); QTest::addColumn<QString>("expected");
    QTest::addColumn<int>("caret"); QTest::addColumn<int>("expectedCaret");
    QTest::addColumn<bool>("rendered");
    const auto add = [&](const char *name, const QString &before, const QString &sourcePrefix, const QString &documentPrefix,
                         const QString &indent = QString(), const QString &body = "Title") {
        for (int level : {1, 3, 6}) for (bool rendered : {false, true}) {
            const QString heading = indent + QString(level, '#') + ' ' + body + "\n\nAfter";
            const QString prefix = rendered ? documentPrefix : sourcePrefix;
            QTest::newRow(qPrintable(QString("%1-h%2-%3").arg(name).arg(level).arg(rendered ? "doc" : "source")))
                << before + heading << prefix + heading << int(before.size() + indent.size())
                << int(prefix.size() + indent.size()) << rendered;
        }
    };
    add("first-line", "", "", "");
    add("leading-blank", "\n", "", "");
    add("two-leading-blanks", "\n\n", "\n", "\n");
    add("body-adjacent", "Body\n", "Body", "Body");
    add("body-hidden-separator", "Body\n\n", "Body\n", "Body");
    add("body-visible-blank", "Body\n\n\n", "Body\n\n", "Body\n\n");
    add("body-spaces", "Body  \n", "Body  ", "Body  ");
    add("indented-heading", "Body\n", "Body", "Body", "  ");
    add("empty-heading", "Body\n", "Body", "Body", "", "");
    add("bullet", "- item\n", "- item\n", "- item\n");
    add("ordered", "2. item\n", "2. item\n", "2. item\n");
    add("task", "- [ ] item\n", "- [ ] item\n", "- [ ] item\n");
    add("list-continuation", "- item\ncontinuation\n", "- item\ncontinuation\n", "- item\ncontinuation\n");
    add("list-hidden-separator", "- item\n\n", "- item\n", "- item\n\n");
    add("list-visible-blank", "- item\n\n\n", "- item\n\n", "- item\n\n");
    add("quote", "> quote\n", "> quote\n", "> quote\n");
    add("heading", "## Previous\n", "## Previous\n", "## Previous\n");
    add("setext", "Previous\n===\n", "Previous\n===\n", "Previous\n===\n");
    add("rule", "***\n", "***\n", "***\n");
    add("code", "```cpp\nx\n```\n", "```cpp\nx\n```\n", "```cpp\nx\n```\n");
    add("code-hidden-separator", "```cpp\nx\n```\n\n", "```cpp\nx\n```\n", "```cpp\nx\n```\n\n");
    add("table", "| A |\n| --- |\n", "| A |\n| --- |\n", "| A |\n| --- |\n");
    add("image", "![alt](missing.png)\n", "![alt](missing.png)\n", "![alt](missing.png)\n");
    add("definition", "[ref]: https://example.com\n", "[ref]: https://example.com\n", "[ref]: https://example.com\n");
    add("html", "<div>body</div>\n\n", "<div>body</div>\n", "<div>body</div>\n\n");
    add("metadata", "---\ntitle: meta\n---\n", "---\ntitle: meta\n---\n", "---\ntitle: meta\n---\n");
}
void EditingHeadingsAuditTests::headingMarkerBackspace() {
    QFETCH(QString, initial); QFETCH(QString, expected); QFETCH(int, caret); QFETCH(int, expectedCaret); QFETCH(bool, rendered);
    const auto mode = rendered ? tmd::ViewMode::Rendered : tmd::ViewMode::Source;
    const auto edit = tmd::EditCommandRouter::dispatch({initial, {caret, caret}, tmd::EditCommand::Backspace, mode}).edit;
    QVERIFY(edit.handled); QCOMPARE(edit.source, expected); QCOMPARE(edit.caret, expectedCaret);
    QCOMPARE(edit.changed, initial != expected);
    MarkdownDocumentView document; MarkdownEditor source;
    QTextEdit *view = rendered ? static_cast<QTextEdit *>(&document) : &source;
    view->resize(740, 420); view->show(); view->setFocus();
    if (rendered) { document.setMarkdownSource("heading-marker.md", initial); document.restoreSourceCursor(caret, caret); }
    else { source.setSource(initial); auto cursor = source.textCursor(); cursor.setPosition(caret); source.setTextCursor(cursor); }
    QTest::keyClick(view, Qt::Key_Backspace);
    QCOMPARE(rendered ? document.markdownSource() : source.source(), expected);
    QCOMPARE(rendered ? document.inlineCursorSourcePosition() : source.textCursor().position(), expectedCaret);
    QVERIFY(!view->textCursor().hasSelection());
    if (rendered) {
        document.setMarkdownSource("heading-marker.md", expected, true);
        QCOMPARE(document.inlineCursorSourcePosition(), expectedCaret);
        if (expected.indexOf('#') > 0 && !expected.left(expected.indexOf('#')).contains('\n')
            && !expected.left(expected.indexOf('#')).trimmed().isEmpty()) {
            QCOMPARE(view->textCursor().blockFormat().headingLevel(), 0);
            QVERIFY(document.toPlainText().contains("#"));
        }
    } else if (initial != expected) {
        QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); QCOMPARE(source.source(), initial);
        QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier); QCOMPARE(source.source(), expected);
    }
}

void EditingHeadingsAuditTests::headingMarkerArrowNavigation() {
    const QString initial = "Body\n\n### Title\n\nAfter";
    MarkdownDocumentView view; view.resize(740, 420); view.show(); view.setFocus();
    view.setMarkdownSource("heading-arrows.md", initial);
    view.restoreSourceCursor(initial.indexOf("Title"), initial.indexOf("Title"));
    for (int i = 0; i < 4; ++i) QTest::keyClick(&view, Qt::Key_Left);
    QCOMPARE(view.inlineCursorSourcePosition(), initial.indexOf("###"));
    QTest::keyClick(&view, Qt::Key_Backspace);
    QCOMPARE(view.markdownSource(), QString("Body### Title\n\nAfter"));
    QCOMPARE(view.inlineCursorSourcePosition(), 4); QCOMPARE(view.textCursor().blockFormat().headingLevel(), 0);
    QTest::qWait(550); QCOMPARE(view.toPlainText(), QString("Body### Title\nAfter"));
    // Inside the hash prefix, Backspace still edits one character normally.
    view.setMarkdownSource("heading-arrows.md", initial);
    view.restoreSourceCursor(initial.indexOf("###") + 2, initial.indexOf("###") + 2);
    QTest::keyClick(&view, Qt::Key_Backspace);
    QCOMPARE(view.markdownSource(), QString("Body\n\n## Title\n\nAfter"));
    QCOMPARE(view.textCursor().blockFormat().headingLevel(), 2);
    for (const QString prefix : {QString("> "), QString("- ")}) {
        view.setMarkdownSource("nested-heading.md", prefix + "### Title");
        view.restoreSourceCursor(prefix.size(), prefix.size()); QTest::keyClick(&view, Qt::Key_Backspace);
        QCOMPARE(view.markdownSource(), prefix + "### Title");
        QCOMPARE(view.inlineCursorSourcePosition(), prefix.size());
    }
}

void EditingHeadingsAuditTests::initTestCase() {
    QCoreApplication::setOrganizationName("TmdEditingAudit");
    QCoreApplication::setApplicationName("HeadingQuoteRegression");
    m_settings = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settings->isValid());
    MarkdownSettingsStore::setStoragePath(m_settings->path() + "/settings.bin");
    QVERIFY(MarkdownSettingsStore::save(MarkdownSettings()));
}

void EditingHeadingsAuditTests::headingSpacesAndChineseInput_data() {
    QTest::addColumn<int>("level");
    QTest::addColumn<bool>("rendered");
    QTest::addColumn<QString>("before");
    for (int level = 1; level <= 6; ++level)
        for (bool rendered : {false, true})
            for (const QString &before : {QString(), QString("Before\n")})
                QTest::newRow(qPrintable(QString("h%1-%2-%3").arg(level).arg(rendered ? "document" : "source").arg(before.isEmpty() ? "first" : "after")))
                    << level << rendered << before;
}

void EditingHeadingsAuditTests::headingSpacesAndChineseInput() {
    QFETCH(int, level);
    QFETCH(bool, rendered);
    QFETCH(QString, before);
    if (rendered && !before.isEmpty()) before += '\n';
    MarkdownDocumentView documentView;
    MarkdownEditor sourceView;
    QTextEdit *view = rendered ? static_cast<QTextEdit *>(&documentView) : &sourceView;
    view->resize(650, 450); view->show(); view->setFocus();
    if (rendered) documentView.setMarkdownSource("heading-audit.md", before.isEmpty() ? QString() : QString("Before"));
    else sourceView.setSource(before);
    view->moveCursor(QTextCursor::End);
    if (rendered && !before.isEmpty()) QTest::keyClick(view, Qt::Key_Return);
    const auto source = [&] { return rendered ? documentView.markdownSource() : sourceView.source(); };
    const QString hashes(level, '#');
    QTest::keyClicks(view, hashes); QTest::keyClick(view, Qt::Key_Space);
    QCOMPARE(source(), before + hashes + ' ');
    if (rendered) QCOMPARE(view->textCursor().blockFormat().headingLevel(), level);
    for (int count = 0; count < 3; ++count) QTest::keyClick(view, Qt::Key_Space);
    QInputMethodEvent commit;
    const QString body = QString::fromUtf8("标题 中文");
    commit.setCommitString(body); QApplication::sendEvent(view, &commit);
    QTest::keyClick(view, Qt::Key_Space);
    const QString expected = before + hashes + "    " + body + ' ';
    QCOMPARE(source(), expected);
    QTest::qWait(550); QCOMPARE(source(), expected);
    if (rendered) {
        QCOMPARE(documentView.inlineCursorSourcePosition(), expected.size());
        QCOMPARE(view->textCursor().blockFormat().headingLevel(), level);
    } else QCOMPARE(view->textCursor().position(), expected.size());
    // Delete only the entered body and its extra spaces, then restore the raw marker.
    for (int count = 0; count < body.size() + 4; ++count) QTest::keyClick(view, Qt::Key_Backspace);
    QCOMPARE(source(), before + hashes + ' ');
    QTest::keyClick(view, Qt::Key_Backspace);
    QCOMPARE(source(), before + hashes);
    if (rendered) {
        QCOMPARE(view->textCursor().blockFormat().headingLevel(), 0);
        QCOMPARE(documentView.inlineCursorSourcePosition(), source().size());
        QVERIFY(view->textCursor().block().text().endsWith(hashes));
    }
    QTest::keyClick(view, Qt::Key_Space);
    commit.setCommitString(body); QApplication::sendEvent(view, &commit);
    QCOMPARE(source(), before + hashes + ' ' + body);
    QTest::qWait(550); QCOMPARE(source(), before + hashes + ' ' + body);
    if (rendered) QCOMPARE(view->textCursor().blockFormat().headingLevel(), level);
}

void EditingHeadingsAuditTests::quoteEnterKeepsSeparateParagraphs_data() {
    QTest::addColumn<QString>("quote");
    QTest::newRow("one-level") << QString("> ");
    QTest::newRow("two-levels") << QString("> > ");
}

void EditingHeadingsAuditTests::quoteEnterKeepsSeparateParagraphs() {
    QFETCH(QString, quote);
    const QString initial = "Before\n\n" + quote + "Title\n\nAfter";
    MarkdownDocumentView view; view.resize(650, 450); view.show(); view.setFocus();
    view.setMarkdownSource("quote-audit.md", initial);
    auto caret = view.document()->find("Title"); caret.setPosition(caret.selectionEnd()); view.setTextCursor(caret);
    QTest::keyClick(&view, Qt::Key_Return); QTest::keyClicks(&view, "next");
    const QString expected = "Before\n\n" + quote + "Title\n" + quote + '\n' + quote + "next\n\nAfter";
    QCOMPARE(view.markdownSource(), expected);
    const auto verifyParagraphs = [&] {
        QCOMPARE(view.toPlainText(), QString("Before\nTitle\nnext\nAfter"));
        const auto title = view.document()->find("Title"), next = view.document()->find("next");
        QVERIFY(title.block() != next.block());
        QCOMPARE(title.blockFormat().intProperty(QTextFormat::BlockQuoteLevel), quote.count('>'));
        QCOMPARE(next.blockFormat().intProperty(QTextFormat::BlockQuoteLevel), quote.count('>'));
    };
    QTest::qWait(550); verifyParagraphs();
    QCOMPARE(view.inlineCursorSourcePosition(), expected.indexOf("next") + 4);
    view.setMarkdownSource("quote-audit.md", expected, true); verifyParagraphs();
    QCOMPARE(view.markdownSource(), expected);
}

void EditingHeadingsAuditTests::emptyQuoteEnterLeavesContainer_data() {
    quoteEnterKeepsSeparateParagraphs_data();
}

void EditingHeadingsAuditTests::emptyQuoteEnterLeavesContainer() {
    QFETCH(QString, quote);
    MarkdownDocumentView view; view.resize(650, 450); view.show(); view.setFocus();
    view.setMarkdownSource("quote-exit-audit.md", "Before\n\n" + quote + "Title\n\nAfter");
    auto caret = view.document()->find("Title"); caret.setPosition(caret.selectionEnd()); view.setTextCursor(caret);
    QTest::keyClick(&view, Qt::Key_Return); QTest::keyClick(&view, Qt::Key_Return);
    const int remainingDepth = quote.count('>') - 1; // N46: leave one layer at a time.
    QCOMPARE(view.textCursor().blockFormat().intProperty(QTextFormat::BlockQuoteLevel), remainingDepth);
    QVERIFY(view.textCursor().block().text().isEmpty());
    QTest::keyClicks(&view, "normal"); QTest::qWait(550);
    const QString source = view.markdownSource();
    const int titleEnd = source.indexOf("Title") + 5, normal = source.indexOf("normal");
    QVERIFY(normal > titleEnd);
    QCOMPARE(source.mid(titleEnd, normal - titleEnd).count('>'), remainingDepth * 2);
    const QString display = "Before\nTitle\nnormal\nAfter";
    QCOMPARE(view.toPlainText(), display);
    QCOMPARE(view.document()->find("normal").blockFormat().intProperty(QTextFormat::BlockQuoteLevel), remainingDepth);
    view.setMarkdownSource("quote-exit-audit.md", source, true);
    QCOMPARE(view.toPlainText(), display);
    QCOMPARE(view.document()->find("normal").blockFormat().intProperty(QTextFormat::BlockQuoteLevel), remainingDepth);
}

void EditingHeadingsAuditTests::headingEnterAndShiftEnterAlwaysStartBody_data() {
    QTest::addColumn<int>("level");
    QTest::addColumn<QString>("quote");
    QTest::addColumn<bool>("rendered");
    for (int level = 1; level <= 6; ++level)
        for (const QString &quote : {QString(), QString("> "), QString("> > ")})
            for (bool rendered : {false, true})
                QTest::newRow(qPrintable(QString("h%1-quote%2-%3").arg(level).arg(quote.count('>')).arg(rendered ? "document" : "source")))
                    << level << quote << rendered;
}

void EditingHeadingsAuditTests::headingEnterAndShiftEnterAlwaysStartBody() {
    QFETCH(int, level);
    QFETCH(QString, quote);
    QFETCH(bool, rendered);
    const QString initial = "Before\n\n" + quote + QString(level, '#') + " Title\n\nAfter";
    QString enteredSource;
    for (const auto modifiers : {Qt::NoModifier, Qt::ShiftModifier}) {
        MarkdownDocumentView documentView;
        MarkdownEditor sourceView;
        QTextEdit *view = rendered ? static_cast<QTextEdit *>(&documentView) : &sourceView;
        view->resize(650, 450); view->show(); view->setFocus();
        if (rendered) documentView.setMarkdownSource("heading-enter-audit.md", initial);
        else sourceView.setSource(initial);
        auto caret = view->document()->find("Title");
        caret.setPosition(caret.selectionEnd()); view->setTextCursor(caret);
        QTest::keyClick(view, Qt::Key_Return, modifiers);
        if (rendered) {
            QCOMPARE(view->textCursor().blockFormat().headingLevel(), 0);
            QCOMPARE(view->textCursor().blockFormat().intProperty(QTextFormat::BlockQuoteLevel), 0);
            QVERIFY(!view->textCursor().block().textList());
        }
        QTest::keyClicks(view, "next"); QTest::qWait(550);
        const QString source = rendered ? documentView.markdownSource() : sourceView.source();
        QVERIFY(!source.contains(QChar::LineSeparator));
        if (rendered) {
            QCOMPARE(view->textCursor().block().text(), QString("next"));
            QCOMPARE(view->textCursor().blockFormat().headingLevel(), 0);
            QCOMPARE(view->textCursor().blockFormat().intProperty(QTextFormat::BlockQuoteLevel), 0);
            QVERIFY(!view->textCursor().block().textList());
            QCOMPARE(documentView.inlineCursorSourcePosition(), source.indexOf("next") + 4);
        } else QCOMPARE(view->textCursor().position(), source.indexOf("next") + 4);
        if (modifiers == Qt::NoModifier) enteredSource = source;
        else QCOMPARE(source, enteredSource);
        // Reload in the document view to verify the saved Markdown agrees with
        // the immediate input state, including source-mode edits.
        documentView.setMarkdownSource("heading-enter-audit.md", source, true);
        const auto title = documentView.document()->find("Title");
        const auto next = documentView.document()->find("next");
        QVERIFY(!title.isNull()); QVERIFY(!next.isNull());
        QVERIFY(title.block() != next.block());
        QCOMPARE(title.blockFormat().headingLevel(), level);
        QCOMPARE(title.blockFormat().intProperty(QTextFormat::BlockQuoteLevel), quote.count('>'));
        QCOMPARE(next.blockFormat().headingLevel(), 0);
        QCOMPARE(next.blockFormat().intProperty(QTextFormat::BlockQuoteLevel), 0);
        QVERIFY(!next.block().textList());
        QCOMPARE(next.block().text(), QString("next"));
        QCOMPARE(documentView.markdownSource(), source);
    }
}

void EditingHeadingsAuditTests::paragraphEnterAndShiftEnterAgree_data() {
    QTest::addColumn<QString>("initial");
    QTest::addColumn<bool>("rendered");
    const QList<QPair<QString, QString>> cases {
        {"plain", "Before\n\nTitle\n\nAfter"},
        {"unordered", "- Title\n- After"},
        {"task", "- [x] Title\n- [ ] After"},
        {"quote", "> Title\n\nAfter"},
        {"nested-quote", "> > Title\n\nAfter"},
        {"code", "```cpp\nTitle\nAfter\n```"},
    };
    for (const auto &test : cases)
        for (bool rendered : {false, true})
            QTest::newRow(qPrintable(test.first + (rendered ? "-document" : "-source")))
                << test.second << rendered;
}

void EditingHeadingsAuditTests::paragraphEnterAndShiftEnterAgree() {
    QFETCH(QString, initial);
    QFETCH(bool, rendered);
    QString enteredSource, enteredDisplay;
    int enteredCaret = -1, enteredBlock = -1, enteredColumn = -1;
    for (const auto modifiers : {Qt::NoModifier, Qt::ShiftModifier}) {
        MarkdownDocumentView documentView;
        MarkdownEditor sourceView;
        QTextEdit *view = rendered ? static_cast<QTextEdit *>(&documentView) : &sourceView;
        view->resize(650, 450); view->show(); view->setFocus();
        if (rendered) documentView.setMarkdownSource("paragraph-enter-audit.md", initial);
        else sourceView.setSource(initial);
        auto caret = view->document()->find("Title");
        QVERIFY(!caret.isNull()); caret.setPosition(caret.selectionEnd()); view->setTextCursor(caret);
        QTest::keyClick(view, Qt::Key_Return, modifiers);
        QTest::keyClicks(view, "next"); QTest::qWait(550);
        const QString source = rendered ? documentView.markdownSource() : sourceView.source();
        const int sourceCaret = rendered ? documentView.inlineCursorSourcePosition() : view->textCursor().position();
        QVERIFY(!source.contains(QChar::LineSeparator));
        QCOMPARE(sourceCaret, source.indexOf("next") + 4);
        if (modifiers == Qt::NoModifier) {
            enteredSource = source; enteredDisplay = view->toPlainText(); enteredCaret = sourceCaret;
            enteredBlock = view->textCursor().blockNumber(); enteredColumn = view->textCursor().positionInBlock();
        } else {
            QCOMPARE(source, enteredSource);
            QCOMPARE(view->toPlainText(), enteredDisplay);
            QCOMPARE(sourceCaret, enteredCaret);
            QCOMPARE(view->textCursor().blockNumber(), enteredBlock);
            QCOMPARE(view->textCursor().positionInBlock(), enteredColumn);
        }
    }
}

void EditingHeadingsAuditTests::quotedHeadingImportKeepsContainer_data() {
    QTest::addColumn<QString>("source");
    QTest::addColumn<int>("depth");
    for (const QString &before : {QString("# Before\n\n"), QString("```cpp\ncode\n```\n\n"), QString("```cpp\n\n")})
        for (bool setext : {false, true}) {
            const int depth = setext ? 2 : 1;
            const QString heading = setext ? "> > Title\n> > -----" : "> ### Title";
            QTest::newRow(qPrintable(QString("%1-%2").arg(before.startsWith('#') ? "heading" : before.contains("code") ? "paired-fence" : "unpaired-fence").arg(setext ? "setext" : "atx")))
                << before + heading + "\n\nAfter" << depth;
        }
}

void EditingHeadingsAuditTests::quotedHeadingImportKeepsContainer() {
    QFETCH(QString, source);
    QFETCH(int, depth);
    MarkdownDocumentView view;
    view.setMarkdownSource("quoted-heading-import.md", source);
    for (int pass = 0; pass < 2; ++pass) {
        const auto title = view.document()->find("Title");
        QVERIFY(!title.isNull());
        QCOMPARE(title.blockFormat().intProperty(QTextFormat::BlockQuoteLevel), depth);
        QVERIFY(title.blockFormat().headingLevel() > 0);
        QCOMPARE(view.markdownSource(), source);
        view.refreshAppearance();
    }
}

QTEST_MAIN(EditingHeadingsAuditTests)
#include "EditingHeadingsAuditTests.moc"
