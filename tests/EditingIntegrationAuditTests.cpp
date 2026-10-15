#include "ui/MainWindow.h"
#include "ui/document/MarkdownDocumentView.h"
#include "ui/document/MarkdownEditor.h"
#include "core/library/MarkdownManager.h"
#include "core/settings/MarkdownSettings.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QSettings>
#include <QtTest>

class EditingIntegrationAuditTests : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void dynamicListIndentBackspace_data();
    void dynamicListIndentBackspace();
    void listBackspaceUndoRedoCaret_data();
    void listBackspaceUndoRedoCaret();
    void selectionReplacementUndoRedo();
    void sourceModeUndoReturnsToDocumentTransaction();
    void unavailableRedoPreservesMovedCaret();
    void codeDownUndoRestoresOriginalBodyCaret();
private:
    QTemporaryDir m_settings;
};

void EditingIntegrationAuditTests::initTestCase() {
    QVERIFY(m_settings.isValid());
    QCoreApplication::setOrganizationName("TmdEditingAudit"); QCoreApplication::setApplicationName("IntegrationRegression");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
    MarkdownSettingsStore::setStoragePath(m_settings.filePath("settings.json"));
    QVERIFY(MarkdownSettingsStore::save(MarkdownSettings()));
}

void EditingIntegrationAuditTests::dynamicListIndentBackspace_data() {
    QTest::addColumn<int>("pause");
    QTest::newRow("fast") << 0;
    QTest::newRow("parse-timer") << 220;
    QTest::newRow("render-timer") << 500;
}

void EditingIntegrationAuditTests::dynamicListIndentBackspace() {
    QFETCH(int, pause);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    MarkdownManager seed(directory.path());
    QVERIFY(seed.createDocument("doc.md", QString()));
    MainWindow window(directory.path()); window.resize(1100, 800); window.show();
    QVERIFY(window.openDocument("doc.md")); window.setSourceMode(false);
    auto *view=window.findChild<MarkdownDocumentView *>();
    auto *editor=window.findChild<MarkdownEditor *>();
    QVERIFY(view); QVERIFY(editor); view->setFocus();
    auto wait=[&] { if (pause) QTest::qWait(pause); };
    QTest::keyClicks(view, "-"); QTest::keyClick(view, Qt::Key_Space); wait();
    QTest::keyClicks(view, "first"); wait();
    QTest::keyClick(view, Qt::Key_Return); wait();
    QCOMPARE(editor->source(), QString("- first\n- "));
    QTest::keyClick(view, Qt::Key_Tab); wait();
    QCOMPARE(editor->source(), QString("- first\n    - "));
    QTest::keyClick(view, Qt::Key_Backspace); wait();
    QCOMPARE(editor->source(), QString("- first\n- "));
    QCOMPARE(view->markdownSource(), editor->source());
    QCOMPARE(view->inlineCursorSourcePosition(), editor->source().size());
    QCOMPARE(view->textCursor().blockNumber(), 1);
    QTest::keyClicks(view, "child"); QTest::qWait(550);
    QCOMPARE(editor->source(), QString("- first\n- child"));
    QCOMPARE(view->markdownSource(), editor->source());
    QCOMPARE(view->textCursor().block().text(), QString("child"));
    QCOMPARE(view->inlineCursorSourcePosition(), editor->source().size());
}

void EditingIntegrationAuditTests::listBackspaceUndoRedoCaret_data() {
    QTest::addColumn<QString>("source");
    QTest::addColumn<int>("caret");
    QTest::newRow("middle-list") << QString("- first\n- \n- after") << 10;
    QTest::newRow("following-heading") << QString("- first\n- \n\n## After") << 10;
    QTest::newRow("following-paragraph") << QString("- first\n- \n\nAfter") << 10;
    QTest::newRow("task-list") << QString("- [ ] first\n- [ ] \n- [ ] after") << 18;
    QTest::newRow("ordered-list") << QString("1. first\n2. \n3. after") << 12;
}

void EditingIntegrationAuditTests::listBackspaceUndoRedoCaret() {
    QFETCH(QString, source); QFETCH(int, caret);
    QTemporaryDir directory; QVERIFY(directory.isValid());
    MarkdownManager seed(directory.path()); QVERIFY(seed.createDocument("doc.md", source));
    MainWindow window(directory.path()); window.resize(1100, 800); window.show();
    QVERIFY(window.openDocument("doc.md")); window.setSourceMode(false);
    auto *view=window.findChild<MarkdownDocumentView *>();
    auto *editor=window.findChild<MarkdownEditor *>();
    QVERIFY(view); QVERIFY(editor); view->setFocus(); view->goToSourcePosition(caret);
    QTest::keyClick(view, Qt::Key_Tab); QTest::qWait(550);
    QCOMPARE(view->inlineCursorSourcePosition(), caret+4);
    const QString indented=editor->source();
    QTest::keyClick(view, Qt::Key_Backspace); QTest::qWait(550);
    QCOMPARE(editor->source(), source); QCOMPARE(view->inlineCursorSourcePosition(), caret);
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); QTest::qWait(550);
    QCOMPARE(editor->source(), indented); QCOMPARE(view->inlineCursorSourcePosition(), caret+4);
    QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier); QTest::qWait(550);
    QCOMPARE(editor->source(), source); QCOMPARE(view->inlineCursorSourcePosition(), caret);
    QTest::keyClick(view, Qt::Key_Backspace); QTest::qWait(550);
    QString raw=source; raw.remove(caret-1, 1);
    raw.insert(source.lastIndexOf('\n', caret-1)+1, '\n');
    const int following = raw.indexOf('\n', caret);
    if (following >= 0 && following + 1 < raw.size() && raw[following + 1] != '\n') raw.insert(following + 1, '\n');
    QCOMPARE(editor->source(), raw); QCOMPARE(view->inlineCursorSourcePosition(), caret);
    QCOMPARE(view->textCursor().positionInBlock(), view->textCursor().block().text().size());
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); QTest::qWait(550);
    QCOMPARE(editor->source(), source); QCOMPARE(view->inlineCursorSourcePosition(), caret);
    QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier); QTest::qWait(550);
    QCOMPARE(editor->source(), raw); QCOMPARE(view->inlineCursorSourcePosition(), caret);
    QTest::keyClicks(view, "X"); QTest::qWait(550);
    QString typed=raw; typed.insert(caret, 'X');
    QCOMPARE(editor->source(), typed); QCOMPARE(view->inlineCursorSourcePosition(), caret+1);
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); QTest::qWait(550);
    QCOMPARE(editor->source(), raw); QCOMPARE(view->markdownSource(), raw);
    QCOMPARE(view->inlineCursorSourcePosition(), caret);
    QCOMPARE(view->textCursor().positionInBlock(), view->textCursor().block().text().size());
    QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier); QTest::qWait(550);
    QCOMPARE(editor->source(), typed); QCOMPARE(view->markdownSource(), typed);
    QCOMPARE(view->inlineCursorSourcePosition(), caret+1);
}

void EditingIntegrationAuditTests::selectionReplacementUndoRedo() {
    const QString source="Before **bold** After\n\nTail";
    QTemporaryDir directory; QVERIFY(directory.isValid());
    MarkdownManager seed(directory.path()); QVERIFY(seed.createDocument("doc.md", source));
    MainWindow window(directory.path()); window.resize(1100, 800); window.show();
    QVERIFY(window.openDocument("doc.md")); window.setSourceMode(false);
    auto *view=window.findChild<MarkdownDocumentView *>();
    auto *editor=window.findChild<MarkdownEditor *>(); QVERIFY(view); QVERIFY(editor);
    view->setFocus(); auto selected=view->document()->find("bold"); QVERIFY(!selected.isNull()); view->setTextCursor(selected);
    QTest::keyClicks(view, "X"); QTest::qWait(550);
    const QString replaced=editor->source(); QVERIFY(replaced.contains('X')); QVERIFY(!replaced.contains("bold"));
    const int newCaret=view->inlineCursorSourcePosition();
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); QTest::qWait(550);
    QCOMPARE(editor->source(), source); QCOMPARE(view->markdownSource(), source);
    QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier); QTest::qWait(550);
    QCOMPARE(editor->source(), replaced); QCOMPARE(view->markdownSource(), replaced);
    QCOMPARE(view->inlineCursorSourcePosition(), newCaret);
}

void EditingIntegrationAuditTests::sourceModeUndoReturnsToDocumentTransaction() {
    const QString source="- first\n- \n- after";
    QTemporaryDir directory; QVERIFY(directory.isValid());
    MarkdownManager seed(directory.path()); QVERIFY(seed.createDocument("doc.md", source));
    MainWindow window(directory.path()); window.resize(1100, 800); window.show();
    QVERIFY(window.openDocument("doc.md")); window.setSourceMode(false);
    auto *view=window.findChild<MarkdownDocumentView *>();
    auto *editor=window.findChild<MarkdownEditor *>(); QVERIFY(view); QVERIFY(editor);
    view->setFocus(); view->goToSourcePosition(10);
    QTest::keyClick(view, Qt::Key_Tab); QTest::qWait(550);
    const QString indented="- first\n    - \n- after";
    QCOMPARE(editor->source(), indented); QCOMPARE(view->inlineCursorSourcePosition(), 14);
    QTest::keyClick(view, Qt::Key_Backspace); QTest::qWait(550);
    QCOMPARE(editor->source(), source); QCOMPARE(view->inlineCursorSourcePosition(), 10);
    window.setSourceMode(true); editor->setFocus();
    QCOMPARE(editor->textCursor().position(), 10);
    QTest::keyClicks(editor, "child"); QTest::qWait(550);
    QCOMPARE(editor->source(), QString("- first\n- child\n- after"));
    QTest::keyClick(editor, Qt::Key_Z, Qt::ControlModifier); QTest::qWait(550);
    QCOMPARE(editor->source(), source);
    window.setSourceMode(false); view->setFocus();
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); QTest::qWait(550);
    QCOMPARE(editor->source(), indented); QCOMPARE(view->markdownSource(), indented);
    QCOMPARE(view->inlineCursorSourcePosition(), 14);
    QCOMPARE(view->textCursor().blockNumber(), 1);
    QCOMPARE(view->textCursor().positionInBlock(), 0);
    QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier); QTest::qWait(550);
    QCOMPARE(editor->source(), source); QCOMPARE(view->inlineCursorSourcePosition(), 10);
}

void EditingIntegrationAuditTests::unavailableRedoPreservesMovedCaret() {
    const QString source="Before **bold** After\n\nTail";
    QTemporaryDir directory; QVERIFY(directory.isValid());
    MarkdownManager seed(directory.path()); QVERIFY(seed.createDocument("doc.md", source));
    MainWindow window(directory.path()); window.resize(1100, 800); window.show();
    QVERIFY(window.openDocument("doc.md")); window.setSourceMode(false);
    auto *view=window.findChild<MarkdownDocumentView *>();
    auto *editor=window.findChild<MarkdownEditor *>(); QVERIFY(view); QVERIFY(editor);
    view->setFocus(); view->moveCursor(QTextCursor::End);
    QTest::keyClicks(view, "X"); QTest::qWait(550);
    QVERIFY(!editor->document()->isRedoAvailable());
    auto moved=view->document()->find("After"); QVERIFY(!moved.isNull());
    moved.setPosition(moved.selectionStart()+2); view->setTextCursor(moved);
    const QString text=editor->source(); const int caret=view->inlineCursorSourcePosition();
    const int native=view->textCursor().position();
    QTest::keyClick(view, Qt::Key_Y, Qt::ControlModifier); QTest::qWait(550);
    QCOMPARE(editor->source(), text); QCOMPARE(view->markdownSource(), text);
    QCOMPARE(view->inlineCursorSourcePosition(), caret);
    QCOMPARE(view->textCursor().position(), native);
}

void EditingIntegrationAuditTests::codeDownUndoRestoresOriginalBodyCaret() {
    const QString source="```cpp\nx\n```";
    QTemporaryDir directory; QVERIFY(directory.isValid());
    MarkdownManager seed(directory.path()); QVERIFY(seed.createDocument("doc.md", source));
    MainWindow window(directory.path()); window.resize(1100, 800); window.show();
    QVERIFY(window.openDocument("doc.md")); window.setSourceMode(false);
    auto *view=window.findChild<MarkdownDocumentView *>();
    auto *editor=window.findChild<MarkdownEditor *>(); QVERIFY(view); QVERIFY(editor);
    view->setFocus(); view->goToSourcePosition(8);
    QCOMPARE(view->inlineCursorSourcePosition(), 8);
    QCOMPARE(view->textCursor().block().text(), QString("x"));
    QTest::keyClick(view, Qt::Key_Down); QTest::qWait(550);
    QCOMPARE(editor->source(), source+"\n"); // N02: one editable body row after the closing fence.
    QCOMPARE(view->inlineCursorSourcePosition(), editor->source().size());
    QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); QTest::qWait(550);
    QCOMPARE(editor->source(), source); QCOMPARE(view->markdownSource(), source);
    QCOMPARE(view->inlineCursorSourcePosition(), 8);
    QCOMPARE(view->textCursor().block().text(), QString("x"));
    QCOMPARE(view->textCursor().positionInBlock(), 1);
    QTest::keyClicks(view, "Y"); QTest::qWait(550);
    QCOMPARE(editor->source(), QString("```cpp\nxY\n```"));
    QCOMPARE(view->markdownSource(), editor->source());
    QCOMPARE(view->inlineCursorSourcePosition(), 9);
}

QTEST_MAIN(EditingIntegrationAuditTests)
#include "EditingIntegrationAuditTests.moc"
