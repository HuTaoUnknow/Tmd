#include "ui/document/MarkdownDocumentView.h"
#include "ui/document/MarkdownEditor.h"
#include "core/settings/MarkdownSettings.h"
#include "core/markdown/MarkdownAnalysis.h"
#include "core/markdown/MarkdownTableEditing.h"
#include "core/library/MarkdownManager.h"
#include "ui/MainWindow.h"
#include <QApplication>
#include <QClipboard>
#include <QInputMethodEvent>
#include <QTextBlock>
#include <QTextTable>
#include <QTemporaryDir>
#include <QtTest>

class EditingCodeAuditTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_settings;
private slots:
    void initTestCase() {
        QVERIFY(m_settings.isValid());
        MarkdownSettingsStore::setStoragePath(m_settings.filePath("settings.bin"));
        QVERIFY(MarkdownSettingsStore::save(MarkdownSettings()));
    }
    void codeDownStaysInNextCodeLine_data();
    void codeDownStaysInNextCodeLine();
    void nestedCodeBackspaceKeepsCaretAfterContainer_data();
    void nestedCodeBackspaceKeepsCaretAfterContainer();
    void codeMultilinePastePreservesDelimiters_data();
    void codeMultilinePastePreservesDelimiters();
    void inlineSelectionAndImeReplacement_data();
    void inlineSelectionAndImeReplacement();
    void tableSingleCellPastePreservesStructure();
    void deletingOnlyCodeBodyKeepsFences_data();
    void deletingOnlyCodeBodyKeepsFences();
    void tableNewlineReplacesOnlyOneCell_data();
    void tableNewlineReplacesOnlyOneCell();
    void tableEnterTypingReloadBackspaceUndo_data();
    void tableEnterTypingReloadBackspaceUndo();
    void raggedTableCellsCanBeMaterialized();
    void raggedTableEnterStaysInVirtualCell_data();
    void raggedTableEnterStaysInVirtualCell();
    void tableAndFollowingParagraphKeepSeparateCarets_data();
    void tableAndFollowingParagraphKeepSeparateCarets();
    void typingInMissingTableCellStaysOnRow();
    void blankParagraphAfterHeaderOnlyTableIsEditable_data();
    void blankParagraphAfterHeaderOnlyTableIsEditable();
    void tableGapTypingPreservesParagraphSeparators_data();
    void tableGapTypingPreservesParagraphSeparators();
    void tableGapInputUndoRedoKeepsCaret();
};

void EditingCodeAuditTests::codeDownStaysInNextCodeLine_data() {
    QTest::addColumn<QString>("delimiter"); QTest::addColumn<bool>("rendered");
    QTest::newRow("backticks-document") << QString("```") << true;
    QTest::newRow("tildes-document") << QString("~~~") << true;
    QTest::newRow("backticks-source") << QString("```") << false;
    QTest::newRow("tildes-source") << QString("~~~") << false;
}
void EditingCodeAuditTests::codeDownStaysInNextCodeLine() {
    QFETCH(QString, delimiter); QFETCH(bool, rendered);
    const QString source = "Before\n\n" + delimiter + "cpp\nfirst();\nsecond();\nthird();\n" + delimiter + "\n\nAfter";
    MarkdownDocumentView view; MarkdownEditor editor; QTextEdit *target = rendered ? static_cast<QTextEdit *>(&view) : &editor;
    target->resize(700, 420); target->show(); target->setFocus();
    if (rendered) view.setMarkdownSource("audit.md", source); else editor.setSource(source);
    auto cursor = target->document()->find("first();"); cursor.setPosition(cursor.selectionStart() + 3); target->setTextCursor(cursor);
    QTest::keyClick(target, Qt::Key_Down); QCOMPARE(target->textCursor().block().text(), QString("second();"));
    QTest::keyClick(target, Qt::Key_Down); QCOMPARE(target->textCursor().block().text(), QString("third();"));
    QTest::keyClick(target, Qt::Key_Up); QCOMPARE(target->textCursor().block().text(), QString("second();"));
    QCOMPARE(rendered ? view.markdownSource() : editor.source(), source);
}

void EditingCodeAuditTests::nestedCodeBackspaceKeepsCaretAfterContainer_data() {
    QTest::addColumn<QString>("source"); QTest::addColumn<QString>("expected"); QTest::addColumn<bool>("rendered");
    for (bool rendered : {true, false}) {
        const QString mode = rendered ? "document" : "source";
        QTest::newRow(qPrintable("quote-" + mode)) << QString("> ```cpp\n> \n> next\n> ```\n\nAfter") << QString("> ```cpp\n> next\n> ```\n\nAfter") << rendered;
        QTest::newRow(qPrintable("list-" + mode)) << QString("- item\n\n  ```cpp\n  \n  next\n  ```\n\nAfter") << QString("- item\n\n  ```cpp\n  next\n  ```\n\nAfter") << rendered;
        QTest::newRow(qPrintable("uncontained-" + mode)) << QString("Before\n\n```cpp\n \nnext\n```\n\nAfter") << QString("Before\n\n```cpp\nnext\n```\n\nAfter") << rendered;
    }
}
void EditingCodeAuditTests::nestedCodeBackspaceKeepsCaretAfterContainer() {
    QFETCH(QString, source); QFETCH(QString, expected); QFETCH(bool, rendered);
    MarkdownDocumentView view; MarkdownEditor editor; QTextEdit *target = rendered ? static_cast<QTextEdit *>(&view) : &editor;
    target->resize(700, 420); target->show(); target->setFocus();
    if (rendered) view.setMarkdownSource("audit.md", source); else editor.setSource(source);
    QTextBlock first; for (auto b = view.document()->begin(); b.isValid(); b = b.next())
        if (b.blockFormat().hasProperty(QTextFormat::BlockCodeFence)) { first = b; break; }
    if (rendered) { QVERIFY(first.isValid()); view.setTextCursor(QTextCursor(first)); }
    else { auto cursor = editor.textCursor(); const int headerEnd = source.indexOf('\n', source.indexOf("```cpp"));
        const auto analysis = MarkdownAnalysis::scan(source); int position = headerEnd + 1;
        for (const auto &line : analysis.lines) if (line.start == position) { position = line.contentStart; break; }
        cursor.setPosition(position); editor.setTextCursor(cursor); }
    QTest::keyClick(target, Qt::Key_Backspace);
    QCOMPARE(rendered ? view.markdownSource() : editor.source(), expected);
    QCOMPARE(rendered ? view.inlineCursorSourcePosition() : editor.textCursor().position(), expected.indexOf("next"));
    QTest::keyClicks(target, "X"); expected.replace("next", "Xnext"); QCOMPARE(rendered ? view.markdownSource() : editor.source(), expected);
    QTest::qWait(500); QCOMPARE(rendered ? view.markdownSource() : editor.source(), expected);
    if (rendered) QCOMPARE(view.textCursor().block().text(), QString("Xnext"));
}

void EditingCodeAuditTests::codeMultilinePastePreservesDelimiters_data() {
    QTest::addColumn<QString>("delimiter"); QTest::newRow("backticks") << QString("```"); QTest::newRow("tildes") << QString("~~~");
}
void EditingCodeAuditTests::codeMultilinePastePreservesDelimiters() {
    QFETCH(QString, delimiter);
    const QString source = "Before\n\n" + delimiter + "cpp\nfirst();\nsecond();\nthird();\n" + delimiter + "\n\nAfter";
    MarkdownDocumentView view; view.resize(700, 420); view.show(); view.setFocus(); view.setMarkdownSource("audit.md", source);
    auto cursor = view.document()->find("second();"); cursor.setPosition(cursor.selectionStart() + 3); view.setTextCursor(cursor);
    QApplication::clipboard()->setText("X\nY"); QTest::keyClick(&view, Qt::Key_V, Qt::ControlModifier);
    QString expected = source; expected.replace("second();", "secX\nYond();"); QCOMPARE(view.markdownSource(), expected);
    QTest::qWait(500); QCOMPARE(view.markdownSource(), expected);
    cursor = view.document()->find("Yond();"); cursor.setPosition(cursor.selectionStart()); view.setTextCursor(cursor);
    QTest::keyClick(&view, Qt::Key_Backspace); expected.replace("secX\nYond();", "secXYond();"); QCOMPARE(view.markdownSource(), expected);
}

void EditingCodeAuditTests::inlineSelectionAndImeReplacement_data() {
    QTest::addColumn<QString>("source");
    QTest::newRow("single-backtick") << QString("Before `body` After");
    QTest::newRow("double-backtick") << QString("Before ``body`` After");
    QTest::newRow("bold") << QString("Before **body** After");
}
void EditingCodeAuditTests::inlineSelectionAndImeReplacement() {
    QFETCH(QString, source);
    MarkdownDocumentView view; view.resize(700, 420); view.show(); view.setFocus(); view.setMarkdownSource("audit.md", source);
    auto selected = view.document()->find("body"); view.setTextCursor(selected); QTest::keyClicks(&view, "new");
    QString expected = source; expected.replace("body", "new"); QCOMPARE(view.markdownSource(), expected);
    view.setMarkdownSource("audit.md", source); selected = view.document()->find("body");
    selected.setPosition(selected.selectionStart() + 2); view.setTextCursor(selected);
    QInputMethodEvent input; input.setCommitString(QString::fromUtf8("中"), -1, 1); QApplication::sendEvent(&view, &input);
    expected = source; expected.replace("body", QString::fromUtf8("b中dy")); QCOMPARE(view.markdownSource(), expected);
    QTest::qWait(500); QCOMPARE(view.markdownSource(), expected);
}
void EditingCodeAuditTests::tableSingleCellPastePreservesStructure() {
    const QString source = "Before\n\n| A | B |\n|---|---|\n| cell | value |\n\nAfter";
    MarkdownDocumentView view; view.resize(700, 420); view.show(); view.setFocus(); view.setMarkdownSource("audit.md", source);
    auto cursor = view.document()->find("cell"); view.setTextCursor(cursor); QApplication::clipboard()->setText("new");
    QTest::keyClick(&view, Qt::Key_V, Qt::ControlModifier);
    QString expected = source; expected.replace("cell", "new"); QCOMPARE(view.markdownSource(), expected);
}
void EditingCodeAuditTests::deletingOnlyCodeBodyKeepsFences_data() {
    QTest::addColumn<QString>("body"); QTest::addColumn<bool>("selected");
    QTest::newRow("selected-body") << QString("body") << true;
    QTest::newRow("last-character") << QString("x") << false;
}
void EditingCodeAuditTests::deletingOnlyCodeBodyKeepsFences() {
    QFETCH(QString, body); QFETCH(bool, selected);
    MarkdownDocumentView view; view.resize(700, 420); view.show(); view.setFocus();
    view.setMarkdownSource("audit.md", "```cpp\n" + body + "\n```");
    auto cursor = view.document()->find(body); QVERIFY(!cursor.isNull());
    if (!selected) cursor.setPosition(cursor.selectionEnd()); view.setTextCursor(cursor);
    QTest::keyClick(&view, Qt::Key_Backspace); QCOMPARE(view.markdownSource(), QString("```cpp\n\n```"));
    QTest::qWait(500); QCOMPARE(view.markdownSource(), QString("```cpp\n\n```"));
    QTest::keyClicks(&view, "new"); QCOMPARE(view.markdownSource(), QString("```cpp\nnew\n```"));
}
void EditingCodeAuditTests::tableNewlineReplacesOnlyOneCell_data() {
    QTest::addColumn<QString>("source"); QTest::addColumn<int>("anchor"); QTest::addColumn<int>("caret");
    QTest::addColumn<bool>("handled"); QTest::addColumn<QString>("expected");
    const QString ordinary = "| A | B |\n|:---|---:|\n| cell | value |";
    auto append = [&](const char *name, const QString &source, int anchor, int caret, bool handled) {
        QString expected = source;
        if (handled) expected.replace(qMin(anchor, caret), qAbs(anchor - caret), "<br>");
        QTest::newRow(name) << source << anchor << caret << handled << expected;
    };
    append("header", ordinary, ordinary.indexOf('A') + 1, ordinary.indexOf('A') + 1, true);
    append("body", ordinary, ordinary.indexOf("cell") + 2, ordinary.indexOf("cell") + 2, true);
    append("backward-cell-selection", ordinary, ordinary.indexOf("cell") + 4, ordinary.indexOf("cell"), true);
    append("cross-cell-selection", ordinary, ordinary.indexOf("cell"), ordinary.indexOf("value") + 5, false);
    append("divider", ordinary, ordinary.indexOf(":---") + 2, ordinary.indexOf(":---") + 2, false);
    append("cross-line-selection", ordinary, ordinary.indexOf('A'), ordinary.indexOf("cell"), false);
    const QString existing = "| 中 | B |\n|---|---|\n| cell<br>old | value |";
    append("unicode-and-existing-break", existing, existing.indexOf("old") + 3, existing.indexOf("old") + 3, true);
    const QString escaped = "| A | B |\n|---|---|\n| a\\|cell | value |";
    append("escaped-pipe", escaped, escaped.indexOf("cell") + 2, escaped.indexOf("cell") + 2, true);
    const QString empty = "| | |\n|---|---|\n| | |";
    append("empty-header", empty, 2, 2, true);
    append("empty-second-body-cell", empty, empty.size() - 1, empty.size() - 1, true);
    for (const auto &entry : QVector<QPair<const char *, QString>>{
        {"fenced-code", "```\n" + ordinary + "\n```"},
        {"html", "<div>\n" + ordinary + "\n</div>"},
        {"comment", "<!--\n" + ordinary + "\n-->"},
        {"paragraph", "Intro\n" + ordinary}})
        append(entry.first, entry.second, entry.second.indexOf("cell"), entry.second.indexOf("cell"), false);
    const QString unpaired = "```\n\n" + ordinary;
    append("unpaired-fence-before-table", unpaired, unpaired.indexOf("cell"), unpaired.indexOf("cell"), true);
}
void EditingCodeAuditTests::tableNewlineReplacesOnlyOneCell() {
    QFETCH(QString, source); QFETCH(int, anchor); QFETCH(int, caret); QFETCH(bool, handled); QFETCH(QString, expected);
    const auto edit = MarkdownTableEditing::newline(source, anchor, caret);
    QCOMPARE(edit.handled, handled); QCOMPARE(edit.changed, handled); QCOMPARE(edit.source, expected);
    if (handled) { QCOMPARE(edit.anchor, qMin(anchor, caret) + 4); QCOMPARE(edit.caret, edit.anchor); }
}
void EditingCodeAuditTests::tableEnterTypingReloadBackspaceUndo_data() {
    QTest::addColumn<QString>("source"); QTest::addColumn<QString>("token"); QTest::addColumn<bool>("rendered");
    for (bool rendered : {true, false}) {
        const QString mode = rendered ? "document" : "source";
        QTest::newRow(qPrintable("header-" + mode)) << QString("| Header | B |\n|:---|---:|\n| cell | value |") << QString("Header") << rendered;
        QTest::newRow(qPrintable("body-" + mode)) << QString("| A | B |\n|---|---|\n| cell | value |") << QString("cell") << rendered;
        QTest::newRow(qPrintable("existing-break-" + mode)) << QString("| 中 | B |\n|---|---|\n| cell<br>old | value |") << QString("old") << rendered;
        QTest::newRow(qPrintable("escaped-pipe-" + mode)) << QString("| A | B |\n|---|---|\n| a\\|cell | value |") << QString("cell") << rendered;
    }
}
void EditingCodeAuditTests::tableEnterTypingReloadBackspaceUndo() {
    QFETCH(QString, source); QFETCH(QString, token); QFETCH(bool, rendered);
    QTemporaryDir dir; QVERIFY(dir.isValid()); MarkdownManager manager(dir.path()); QVERIFY(manager.createDocument("doc.md", source));
    MainWindow window(dir.path()); window.show(); window.activateWindow(); QVERIFY(QTest::qWaitForWindowActive(&window));
    window.setSourceMode(!rendered);
    auto *view = window.findChild<MarkdownDocumentView *>(); auto *editor = window.findChild<MarkdownEditor *>(); QVERIFY(view); QVERIFY(editor);
    QTextEdit *target = rendered ? static_cast<QTextEdit *>(view) : editor;
    auto cursor = target->document()->find(token); QVERIFY(!cursor.isNull()); cursor.setPosition(cursor.selectionEnd()); target->setTextCursor(cursor); target->setFocus();
    const int insertion = source.indexOf(token) + token.size(); QString expected = source; expected.insert(insertion, "<br>");
    QTest::keyClick(target, Qt::Key_Return); QCOMPARE(editor->source(), expected);
    QTest::keyClicks(target, "tail"); expected.insert(insertion + 4, "tail"); QCOMPARE(editor->source(), expected);
    QTest::qWait(550); QCOMPARE(editor->source(), expected);
    if (rendered) {
        view->setMarkdownSource(dir.filePath("doc.md"), expected, true);
        QCOMPARE(view->inlineCursorSourcePosition(), insertion + 8);
        bool table = false; for (auto *frame : view->document()->rootFrame()->childFrames()) if (dynamic_cast<QTextTable *>(frame)) table = true;
        QVERIFY(table);
    }
    QTest::keyClick(target, Qt::Key_Backspace); QString shortened = expected; shortened.remove(insertion + 7, 1); QCOMPARE(editor->source(), shortened);
    QTest::keyClick(target, Qt::Key_Z, Qt::ControlModifier); QTRY_COMPARE(editor->source(), expected);
    window.hide();
}
void EditingCodeAuditTests::raggedTableCellsCanBeMaterialized() {
    const QString source = "| A | B |\n|---|---|\n| x |";
    const auto tables = MarkdownTableEditing::tables(source); QCOMPARE(tables.size(), 1); QCOMPARE(tables[0].rows.size(), 2);
    const auto &cell = tables[0].rows[1][1]; QVERIFY(cell.virtualCell); QCOMPARE(cell.start, source.size()); QCOMPARE(cell.end, source.size());
    QVERIFY(!MarkdownTableEditing::newline(source, source.size(), source.size()).handled);
    const auto edit = MarkdownTableEditing::newline(source, source.size(), source.size(), 0, 1, 1);
    QVERIFY(edit.handled); QCOMPARE(edit.source, source + " <br> |"); QCOMPARE(edit.caret, source.size() + 5);
    const auto expanded = MarkdownTableEditing::tables(edit.source); QCOMPARE(expanded.size(), 1); QVERIFY(!expanded[0].rows[1][1].virtualCell);
    const QString wider = "A | B | C\n---|---|---\nx";
    const auto last = MarkdownTableEditing::newline(wider, wider.size(), wider.size(), 0, 1, 2);
    QVERIFY(last.handled); QCOMPARE(last.source, wider + " | | <br> |");
}
void EditingCodeAuditTests::raggedTableEnterStaysInVirtualCell_data() {
    QTest::addColumn<QString>("source");
    QTest::newRow("last-row") << QString("| A | B |\n|---|---|\n| x |");
    QTest::newRow("middle-row") << QString("| A | B |\n|---|---|\n| x |\n| other | value |");
}
void EditingCodeAuditTests::raggedTableEnterStaysInVirtualCell() {
    QFETCH(QString, source);
    QTemporaryDir dir; QVERIFY(dir.isValid()); MarkdownManager manager(dir.path()); QVERIFY(manager.createDocument("doc.md", source));
    MainWindow window(dir.path()); window.show(); window.activateWindow(); QVERIFY(QTest::qWaitForWindowActive(&window));
    auto *view = window.findChild<MarkdownDocumentView *>(); auto *editor = window.findChild<MarkdownEditor *>(); QVERIFY(view); QVERIFY(editor);
    QTextTable *table = nullptr; for (auto *frame : view->document()->rootFrame()->childFrames()) if ((table = dynamic_cast<QTextTable *>(frame))) break;
    QVERIFY(table); QVERIFY(table->cellAt(1, 1).isValid()); view->setTextCursor(table->cellAt(1, 1).firstCursorPosition()); view->setFocus();
    // N53: loading fills omitted cells in both source and document before editing.
    const QString normalized = QString(source).replace("| x |", "| x |   |");
    QCOMPARE(editor->source(), normalized);
    const int insertion = normalized.indexOf("| x |") + 8;
    QString expected = normalized; expected.insert(insertion, "<br>");
    QTest::keyClick(view, Qt::Key_Return); QCOMPARE(editor->source(), expected);
    QTest::keyClicks(view, "tail"); expected.insert(insertion + 4, "tail"); QCOMPARE(editor->source(), expected);
    QTest::qWait(550); view->setMarkdownSource(dir.filePath("doc.md"), expected, true);
    QCOMPARE(view->inlineCursorSourcePosition(), insertion + 8);
    table = view->textCursor().currentTable(); QVERIFY(table); QCOMPARE(table->cellAt(view->textCursor()).row(), 1); QCOMPARE(table->cellAt(view->textCursor()).column(), 1);
    QTest::keyClick(view, Qt::Key_Backspace); QString shortened = expected; shortened.remove(insertion + 7, 1); QCOMPARE(editor->source(), shortened);
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); QTRY_COMPARE(editor->source(), expected); window.hide();
}

void EditingCodeAuditTests::tableAndFollowingParagraphKeepSeparateCarets_data() {
    QTest::addColumn<QString>("source");
    for (int newlines : {2, 4}) {
        const QString separator(newlines, '\n');
        QTest::newRow(qPrintable("literal-" + QString::number(newlines))) << "| unfinished |" + separator + "next";
        QTest::newRow(qPrintable("divider-" + QString::number(newlines))) << "|---|---|" + separator + "next";
        QTest::newRow(qPrintable("complete-" + QString::number(newlines))) << "| A | B |\n|---|---|\n| x | y |" + separator + "next";
        QTest::newRow(qPrintable("unwrapped-" + QString::number(newlines))) << "A | B\n---|---\nx | y" + separator + "next";
    }
}

void EditingCodeAuditTests::tableAndFollowingParagraphKeepSeparateCarets() {
    QFETCH(QString, source);
    MarkdownDocumentView view;
    view.resize(700, 420); view.show(); view.setFocus();
    view.setMarkdownSource("table-boundaries.md", source);
    auto next = view.document()->find("next");
    QVERIFY(!next.isNull()); next.setPosition(next.selectionStart()); view.setTextCursor(next);
    QCOMPARE(view.inlineCursorSourcePosition(), source.indexOf("next"));
    QTest::keyClicks(&view, "tail");
    QString expected = source; expected.insert(source.indexOf("next"), "tail");
    QCOMPARE(view.markdownSource(), expected);
    QTest::qWait(550);
    QCOMPARE(view.inlineCursorSourcePosition(), expected.indexOf("next"));
    QCOMPARE(view.textCursor().block().text(), QString("tailnext"));
    QVERIFY(!view.textCursor().currentTable());
    QCOMPARE(view.markdownSource(), expected);
}

void EditingCodeAuditTests::typingInMissingTableCellStaysOnRow() {
    const QString source = "| A | B |\n|---|---|\n| x |\n\n\n\nnext";
    MarkdownDocumentView view;
    view.resize(700, 420); view.show(); view.setFocus();
    view.setMarkdownSource("table-missing-cell.md", source);
    QTextTable *table = nullptr;
    for (auto *frame : view.document()->rootFrame()->childFrames()) if ((table = dynamic_cast<QTextTable *>(frame))) break;
    QVERIFY(table);
    view.setTextCursor(table->cellAt(1, 1).firstCursorPosition());
    QTest::keyClicks(&view, "tail");
    // N53: the normalized cell keeps its padding and final pipe while typing.
    const QString expected = "| A | B |\n|---|---|\n| x |   tail|\n\n\n\nnext";
    QCOMPARE(view.markdownSource(), expected);
    QTest::qWait(550);
    QCOMPARE(view.markdownSource(), expected);
    QCOMPARE(view.textCursor().block().text(), QString("tail"));
    table = view.textCursor().currentTable(); QVERIFY(table);
    QCOMPARE(table->cellAt(view.textCursor()).row(), 1);
    QCOMPARE(table->cellAt(view.textCursor()).column(), 1);
    QCOMPARE(view.inlineCursorSourcePosition(), expected.indexOf("tail") + 4);
}

void EditingCodeAuditTests::blankParagraphAfterHeaderOnlyTableIsEditable_data() {
    QTest::addColumn<QString>("source");
    QTest::newRow("reported") << QString::fromUtf8("## 7.解析漏洞\n| 文件类型 | 十六进制头文件 | ASCII表现 |\n| --- | --- | --- |\n\n\n \n## 8.条件竞争");
    for (int blankRows : {1, 2, 3, 4}) {
        for (const QString &table : {QString("| A | B |\n|---|---|"), QString("| A | B |\n|---|---|\n| x | y |"), QString("A | B\n---|---")}) {
            QTest::newRow(qPrintable(QString("%1-%2").arg(blankRows).arg(table.startsWith('A') ? "unwrapped" : table.contains('x') ? "body-rows" : "header-only")))
                << QString::fromUtf8("## 7.解析漏洞\n") + table + QString(blankRows + 1, '\n') + QString::fromUtf8("## 8.条件竞争");
        }
    }
}

void EditingCodeAuditTests::blankParagraphAfterHeaderOnlyTableIsEditable() {
    QFETCH(QString, source);
    MarkdownDocumentView view;
    view.resize(700, 420); view.show(); view.setFocus();
    view.setMarkdownSource("table-empty-paragraph.md", source);
    const auto heading = view.document()->find(QString::fromUtf8("8.条件竞争"));
    QVERIFY(!heading.isNull());
    QTextTable *table = nullptr;
    for (auto *frame : view.document()->rootFrame()->childFrames()) if ((table = dynamic_cast<QTextTable *>(frame))) break;
    QVERIFY(table);
    const int afterTable = table->lastCursorPosition().position() + 1;
    QTextBlock body;
    for (auto block = view.document()->findBlock(afterTable); block.isValid() && block.position() < heading.block().position(); block = block.next()) {
        if (block.isVisible() && block.text().trimmed().isEmpty() && !QTextCursor(block).currentTable()) { body = block; break; }
    }
    const auto sourceTables = MarkdownTableEditing::tables(source); QCOMPARE(sourceTables.size(), 1);
    const int headingStart = source.indexOf("## 8.");
    if (source.mid(sourceTables[0].end, headingStart - sourceTables[0].end).count('\n') == 2) {
        // N55: the only physical blank before a heading is a hidden separator.
        QVERIFY(!body.isValid());
        auto next = view.document()->findBlock(afterTable);
        while (next.isValid() && !next.isVisible()) next = next.next();
        QCOMPARE(next, heading.block()); return;
    }
    QVERIFY2(body.isValid(), "The source blank paragraph after the table must remain editable");
    view.setTextCursor(QTextCursor(body));
    const int sourceCaret = view.inlineCursorSourcePosition();
    QVERIFY(sourceCaret > sourceTables[0].end);
    QVERIFY(sourceCaret < source.indexOf("## 8."));
    QTest::keyClicks(&view, "body");
    const QString edited = view.markdownSource();
    QVERIFY(edited.contains("body"));
    QVERIFY(edited.endsWith(QString::fromUtf8("## 8.条件竞争")));
    QCOMPARE(edited.count('\n'), source.count('\n') + 1); // New body's hidden separator; retain later user blanks.
    QTest::qWait(550);
    QCOMPARE(view.textCursor().block().text().trimmed(), QString("body"));
    QCOMPARE(view.textCursor().blockFormat().headingLevel(), 0);
    QVERIFY(!view.textCursor().currentTable());
    QCOMPARE(view.markdownSource(), edited);
}

void EditingCodeAuditTests::tableGapTypingPreservesParagraphSeparators_data() {
    QTest::addColumn<int>("blankRows");
    for (int blankRows : {1, 2, 3}) QTest::newRow(qPrintable(QString::number(blankRows))) << blankRows;
}

void EditingCodeAuditTests::tableGapTypingPreservesParagraphSeparators() {
    QFETCH(int, blankRows);
    const QString tableSource = "| A | B |\n|---|---|";
    const QString source = tableSource + QString(blankRows + 1, '\n') + "next";
    MarkdownDocumentView view;
    view.resize(700, 420); view.show(); view.setFocus();
    view.setMarkdownSource("table-gap-text.md", source);
    QTextTable *table = nullptr;
    for (auto *frame : view.document()->rootFrame()->childFrames()) if ((table = dynamic_cast<QTextTable *>(frame))) break;
    QVERIFY(table);
    auto body = view.document()->findBlock(table->lastCursorPosition().position() + 1);
    while (body.isValid() && !body.isVisible()) body = body.next();
    if (blankRows == 1) {
        // The only existing empty row belongs to the table/body grammar. Add
        // a real user row before typing rather than editing an invisible gap.
        QCOMPARE(body.text(), QString("next"));
        view.restoreSourceCursor(source.indexOf("next"), source.indexOf("next"));
        QTest::keyClick(&view, Qt::Key_Return);
        body = view.document()->find("next").block().previous();
        while (body.isValid() && !body.isVisible()) body = body.previous();
    }
    QVERIFY(body.isValid()); QVERIFY(body.text().isEmpty());
    view.setTextCursor(QTextCursor(body));
    QTest::keyClicks(&view, "body");
    const QString expected = tableSource + "\n\nbody" + QString(qMax(2, blankRows), '\n') + "next";
    QCOMPARE(view.markdownSource(), expected);
    QTest::qWait(550);
    QCOMPARE(view.textCursor().block().text(), QString("body"));
    QVERIFY(!view.textCursor().currentTable());
    QCOMPARE(view.inlineCursorSourcePosition(), expected.indexOf("body") + 4);
    QCOMPARE(view.markdownSource(), expected);
}

void EditingCodeAuditTests::tableGapInputUndoRedoKeepsCaret() {
    QTemporaryDir directory; QVERIFY(directory.isValid());
    const QString tableSource = "| A | B |\n|---|---|";
    const QString source = tableSource + "\n\n\n\n## Next";
    const QString expected = tableSource + "\n\nbody\n\n\n## Next";
    MarkdownManager manager(directory.path()); QVERIFY(manager.createDocument("doc.md", source));
    MainWindow window(directory.path()); window.show();
    QVERIFY(window.openDocument("doc.md")); window.setSourceMode(false);
    auto *view = window.findChild<MarkdownDocumentView *>();
    auto *editor = window.findChild<MarkdownEditor *>(); QVERIFY(view); QVERIFY(editor);
    QTextTable *table = nullptr;
    for (auto *frame : view->document()->rootFrame()->childFrames()) if ((table = dynamic_cast<QTextTable *>(frame))) break;
    QVERIFY(table);
    auto body = view->document()->findBlock(table->lastCursorPosition().position() + 1);
    while (body.isValid() && !body.isVisible()) body = body.next();
    QVERIFY(body.isValid()); view->setTextCursor(QTextCursor(body)); view->setFocus();
    const int originalCaret = view->inlineCursorSourcePosition();
    QInputMethodEvent commit; commit.setCommitString("body"); QApplication::sendEvent(view, &commit);
    QCOMPARE(editor->source(), expected); QTest::qWait(550);
    QCOMPARE(view->textCursor().block().text(), QString("body"));
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier);
    QCOMPARE(editor->source(), source); QCOMPARE(view->inlineCursorSourcePosition(), originalCaret);
    QVERIFY(view->textCursor().block().text().isEmpty());
    QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier); QTest::qWait(550);
    QCOMPARE(editor->source(), expected);
    QCOMPARE(view->textCursor().block().text(), QString("body"));
    QCOMPARE(view->inlineCursorSourcePosition(), expected.indexOf("body") + 4);
    QVERIFY(!view->textCursor().currentTable()); window.hide();
}

QTEST_MAIN(EditingCodeAuditTests)
#include "EditingCodeAuditTests.moc"
