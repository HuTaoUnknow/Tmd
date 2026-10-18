#include <QtTest>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QRandomGenerator>
#include "application/editing/EditCommandRouter.h"
#include "core/markdown/analysis/MarkdownSnapshot.h"
#include "core/markdown/editing/TextPatch.h"
#include "core/markdown/MarkdownTableEditing.h"
#include "core/markdown/MarkdownSourceMap.h"
#include "core/settings/MarkdownSettings.h"
#include "ui/document/MarkdownEditor.h"
#include "ui/document/QtTextEditAdapter.h"

using namespace tmd;
class ModularEditingTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_settings;
private slots:
    void initTestCase() {
        QVERIFY(m_settings.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
        QCoreApplication::setOrganizationName("TmdModularEditingTests");
        QCoreApplication::setApplicationName("IsolatedTests");
        MarkdownSettingsStore::setStoragePath(m_settings.filePath("settings.hex"));
    }
    void patchRoundtripUnicode() {
        const QStringList corpus{"", "abc", "ab", "中文正文", "a\nb\n", "a\r\nb", "**粗体**",
            QString::fromUtf8("甲😀乙"), QString::fromUtf8("甲😁乙"), QString::fromUtf8("😀😀"),
            QString::fromUtf8("𐀀𠀀"), QString::fromUtf8("éé"), "\t    - item"};
        auto boundary = [](const QString &s, int p) {
            return p == 0 || p == s.size() || !s[p - 1].isHighSurrogate() || !s[p].isLowSurrogate();
        };
        for (const auto &before : corpus) for (const auto &after : corpus) {
            const auto patch = TextPatch::between(before, after);
            QString actual = before; actual.replace(patch.start, patch.removed, patch.inserted);
            QCOMPARE(actual, after);
            QCOMPARE(patch.changed(), before != after);
            QVERIFY(boundary(before, patch.start));
            QVERIFY(boundary(before, patch.start + patch.removed));
        }
    }
    void patchRandomSequences() {
        QRandomGenerator random(20261018);
        const QStringList tokens{"a", "\n", "\t", "中", " ", "*", QString::fromUtf8("😀"), QString::fromUtf8("𠀀")};
        for (int sample = 0; sample < 500; ++sample) {
            QString before, after;
            for (int i = 0, length = int(random.bounded(30)); i < length; ++i) before += tokens[random.bounded(tokens.size())];
            for (int i = 0, length = int(random.bounded(30)); i < length; ++i) after += tokens[random.bounded(tokens.size())];
            const auto patch = TextPatch::between(before, after);
            before.replace(patch.start, patch.removed, patch.inserted);
            QCOMPARE(before, after);
        }
    }
    void qtAdapterUndoAndStaleSource() {
        const QString before = QString::fromUtf8("甲😀乙\n尾行");
        const QString after = QString::fromUtf8("甲😁新行\n尾行");
        QTextDocument document; document.setPlainText(before); document.clearUndoRedoStacks();
        QTextCursor cursor(&document); cursor.setPosition(1);
        MarkdownEdit edit{after, 1, 3, true, true}; QString error;
        QVERIFY2(QtTextEditAdapter::apply(cursor, before, edit, &error), qPrintable(error));
        QCOMPARE(document.toPlainText(), after);
        QCOMPARE(cursor.anchor(), 1); QCOMPARE(cursor.position(), 3);
        document.undo(); QCOMPARE(document.toPlainText(), before); QVERIFY(!document.isUndoAvailable());
        document.redo(); QCOMPARE(document.toPlainText(), after);
        const int undoSteps = document.availableUndoSteps();
        QVERIFY(!QtTextEditAdapter::apply(cursor, before, edit, &error));
        QCOMPARE(document.toPlainText(), after); QCOMPARE(document.availableUndoSteps(), undoSteps);
    }
    void cursorOnlyTransactionDoesNotAddUndo() {
        QTextDocument document; document.setPlainText("text"); document.clearUndoRedoStacks();
        QTextCursor cursor(&document);
        QVERIFY(QtTextEditAdapter::apply(cursor, "text", MarkdownEdit{"text", 2, 2, true, false}));
        QCOMPARE(cursor.position(), 2); QVERIFY(!document.isUndoAvailable());
        QVERIFY(!QtTextEditAdapter::apply(cursor, "text", MarkdownEdit{"x", 0, 0, true, false}));
        QCOMPARE(document.toPlainText(), QString("text"));
    }
    void renderedHistoryUsesOnePatchAndOneUndoStep() {
        const QString before = QString::fromUtf8("甲😀乙");
        const QString after = QString::fromUtf8("甲😁乙");
        QTextDocument document; document.setPlainText(before); document.clearUndoRedoStacks();
        QTextCursor cursor(&document); int previousCaret = -1;
        MarkdownEdit edit{after, 3, 3, true, true};
        QVERIFY(QtTextEditAdapter::applyWithHistory(cursor, before, edit, -1, &previousCaret));
        QCOMPARE(previousCaret, 3); QCOMPARE(cursor.position(), 3);
        QCOMPARE(document.toPlainText(), after);
        document.undo(); QCOMPARE(document.toPlainText(), before); QVERIFY(!document.isUndoAvailable());
        QVERIFY(QtTextEditAdapter::applyWithHistory(cursor, before, edit, 1, &previousCaret));
        QCOMPARE(previousCaret, 1); QCOMPARE(cursor.position(), 3);
    }
    void sourceMapSharesAnalysisAcrossRebuildAndWhitespaceRecovery() {
        const QString source = "# 标题\n\n甲  \n";
        auto snapshot = std::make_shared<MarkdownSnapshot>(source, MarkdownSnapshot::Mode::Rendering);
        MarkdownSourceMap map;
        map.rebuild(source, "标题\n甲\n", {}, snapshot);
        map.editableWhitespaceEdits();
        map.rebuild(source, "标题\n甲  \n", {}, snapshot);
        map.editableWhitespaceEdits();
        const auto counts = snapshot->buildCounts();
        QCOMPARE(counts.analysis, 1); QCOMPARE(counts.syntax, 1); QCOMPARE(counts.images, 1);
        QCOMPARE(map.sourcePosition(3), source.indexOf("甲"));
    }
    void sourceMapRejectsStaleAndWrongModeSnapshots() {
        auto stale = std::make_shared<MarkdownSnapshot>("# 旧", MarkdownSnapshot::Mode::Rendering);
        MarkdownSourceMap map; map.rebuild("# 新", "新", {}, stale);
        QCOMPARE(stale->buildCounts().analysis, 0);
        QCOMPARE(map.sourcePosition(0), 2);
        int caret = -1;
        QCOMPARE(map.applyEdit(0, 1, "乙", "乙", &caret), QString("# 乙"));
        map.editableWhitespaceEdits();
        QCOMPARE(map.sourcePosition(0), 2);
        auto raw = std::make_shared<MarkdownSnapshot>("# 丙", MarkdownSnapshot::Mode::Raw);
        map.rebuild("# 丙", "丙", {}, raw);
        QCOMPARE(raw->buildCounts().analysis, 0);
        QCOMPARE(map.sourcePosition(0), 2);
    }
    void snapshotReusesAnalysisAndKeepsSource() {
        const QString source = "# Title\n\n![real](image.png)\n\n```cpp\n![literal](other.png)\n```\n";
        MarkdownSnapshot snapshot(source, MarkdownSnapshot::Mode::Rendering);
        QCOMPARE(snapshot.buildCounts().analysis, 0);
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(snapshot.syntax().headings.size(), 1);
            QCOMPARE(snapshot.images().size(), 1);
            QCOMPARE(snapshot.images().front().source, QString("image.png"));
            QCOMPARE(snapshot.codeBlocks().size(), 1);
            QCOMPARE(snapshot.source(), source);
        }
        const auto counts = snapshot.buildCounts();
        QCOMPARE(counts.analysis, 1); QCOMPARE(counts.syntax, 1);
        QCOMPARE(counts.images, 1); QCOMPARE(counts.codeBlocks, 1);
        const auto baseline = MarkdownAnalysis::forRendering(source);
        const auto syntax = MarkdownSyntax::scan(source, &baseline);
        QCOMPARE(snapshot.syntax().headings.front().position, syntax.headings.front().position);
        QCOMPARE(snapshot.codeBlocks().front().code, MarkdownCodeBlocks::scan(source, &baseline).front().code);
    }
    void rawAndRenderingSnapshotsHaveDifferentFenceSemantics() {
        const QString source = "```cpp\n## Heading\n";
        MarkdownSnapshot raw(source, MarkdownSnapshot::Mode::Raw);
        MarkdownSnapshot rendered(source, MarkdownSnapshot::Mode::Rendering);
        QVERIFY(raw.analysis().literalFences.isEmpty());
        QVERIFY(!rendered.analysis().literalFences.isEmpty());
        QCOMPARE(raw.source(), rendered.source());
        QCOMPARE(raw.analysis().lines.size(), MarkdownAnalysis::scan(source).lines.size());
        QCOMPARE(rendered.syntax().headings.size(), MarkdownSyntax::scan(source, &rendered.analysis()).headings.size());
    }
    void headingAndParagraphPoliciesStaySeparate() {
        EditRequest request{"## 甲乙", {4, 4}, EditCommand::Newline, ViewMode::Rendered};
        auto result = EditCommandRouter::dispatch(request);
        QVERIFY(result.edit.handled); QCOMPARE(result.edit.source, QString("## 甲\n\n乙"));
        QCOMPARE(result.edit.caret, 6);
        request.mode = ViewMode::Source;
        result = EditCommandRouter::dispatch(request);
        QCOMPARE(result.edit.source, QString("## 甲\n乙"));
        QCOMPARE(result.edit.caret, 5);
    }
    void taskContinuationAndNestedBackspace() {
        EditRequest request{"- [x] 完成", {8, 8}, EditCommand::Newline, ViewMode::Rendered};
        auto result = EditCommandRouter::dispatch(request);
        QVERIFY(result.edit.handled); QVERIFY(result.edit.source.endsWith("\n- [ ] "));
        QCOMPARE(result.handler, EditHandler::List);
        request = {"- 父项\n    - ", {11, 11}, EditCommand::Backspace, ViewMode::Rendered};
        result = EditCommandRouter::dispatch(request);
        QVERIFY(result.edit.handled); QCOMPARE(result.edit.source, QString("- 父项\n- "));
        QCOMPARE(result.edit.caret, 7);
    }
    void tableCommandsOperateOnWholeGrid() {
        const QString source = "| A | B |\n| --- | --- |\n| 甲 | 乙 |";
        const int caret = source.indexOf("甲");
        EditRequest request{source, {caret, caret}, EditCommand::AddColumn, ViewMode::Rendered};
        request.tableCell = TableCellAddress{0, 1, 0};
        auto result = EditCommandRouter::dispatch(request);
        QVERIFY(result.edit.handled);
        auto tables = MarkdownTableEditing::tables(result.edit.source);
        QCOMPARE(tables.size(), 1); QCOMPARE(tables.front().rows.size(), 2);
        QCOMPARE(tables.front().rows[0].size(), 3); QCOMPARE(tables.front().rows[1].size(), 3);
        request.command = EditCommand::AddRow;
        result = EditCommandRouter::dispatch(request);
        QCOMPARE(MarkdownTableEditing::tables(result.edit.source).front().rows.size(), 3);
        request.command = EditCommand::Newline;
        result = EditCommandRouter::dispatch(request);
        QVERIFY(result.edit.source.contains("<br>"));
        QCOMPARE(MarkdownTableEditing::tables(result.edit.source).front().rows.size(), 2);
    }
    void invalidRequestsCannotMutateSource() {
        EditRequest request{"abc", {-1, 0}, EditCommand::Newline, ViewMode::Rendered};
        QVERIFY(!EditCommandRouter::dispatch(request).edit.handled);
        request.selection = {1, 1}; request.command = EditCommand::AddColumn;
        request.tableCell = TableCellAddress{99, 0, 0};
        QVERIFY(!EditCommandRouter::dispatch(request).edit.handled);
        request.command = EditCommand::Backspace; request.selection = {0, 2};
        QVERIFY(!EditCommandRouter::dispatch(request).edit.handled);
    }
    void sourceEditorParity_data() {
        QTest::addColumn<QString>("markedSource");
        QTest::addColumn<int>("key");
        QTest::addColumn<int>("command");
        QTest::addColumn<QString>("expectedSource");
        QTest::addColumn<int>("expectedCaret");
        const auto add = [](const char *name, const QString &source, int key, EditCommand command, const QString &expected, int caret) {
            QTest::newRow(name) << source << key << int(command) << expected << caret;
        };
        add("heading-split", "## 甲▏乙", Qt::Key_Return, EditCommand::Newline, "## 甲\n乙", 5);
        add("task-continue", "- [x] 完成▏", Qt::Key_Return, EditCommand::Newline, "- [x] 完成\n- [ ] ", 15);
        add("nested-list-backspace", "- 父项\n    - ▏", Qt::Key_Backspace, EditCommand::Backspace, "- 父项\n- ", 7);
        add("list-indent", "- 前项\n- ▏父项\n    - 子项", Qt::Key_Tab, EditCommand::Indent, "- 前项\n    - 父项\n        - 子项", 11);
        add("code-newline", "```cpp\n  x▏\n```", Qt::Key_Return, EditCommand::Newline, "```cpp\n  x\n  \n```", 13);
        add("code-indent", "```cpp\nx▏\n```", Qt::Key_Tab, EditCommand::Indent, "```cpp\nx   \n```", 11);
        add("empty-list-exits", "- ▏", Qt::Key_Backspace, EditCommand::Backspace, "", 0);
        add("empty-ordered-list-exits", "1. ▏", Qt::Key_Backspace, EditCommand::Backspace, "", 0);
        add("ordinary-newline", "甲▏乙", Qt::Key_Return, EditCommand::Newline, "甲\n乙", 2);
    }
    void sourceEditorParity() {
        QFETCH(QString, markedSource); QFETCH(int, key); QFETCH(int, command);
        QFETCH(QString, expectedSource); QFETCH(int, expectedCaret);
        const int caret = markedSource.indexOf(QChar(0x258f));
        QVERIFY(caret >= 0); markedSource.remove(caret, 1);
        EditRequest request{markedSource, {caret, caret}, EditCommand(command), ViewMode::Source};
        const auto routed = EditCommandRouter::dispatch(request);
        QVERIFY(routed.edit.handled);
        QCOMPARE(routed.edit.source, expectedSource); QCOMPARE(routed.edit.caret, expectedCaret);
        MarkdownEditor editor; editor.setSource(markedSource);
        QSignalSpy rejections(&editor, &MarkdownEditor::editRejected);
        auto cursor = editor.textCursor(); cursor.setPosition(caret); editor.setTextCursor(cursor);
        QTest::keyClick(&editor, Qt::Key(key));
        QCOMPARE(editor.source(), routed.edit.source);
        QCOMPARE(editor.textCursor().anchor(), routed.edit.anchor);
        QCOMPARE(editor.textCursor().position(), routed.edit.caret);
        QCOMPARE(rejections.size(), 0);
    }
};
QTEST_MAIN(ModularEditingTests)
#include "ModularEditingTests.moc"
