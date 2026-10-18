#include "ui/document/MarkdownDocumentView.h"
#include "ui/document/MarkdownEditor.h"
#include "core/settings/MarkdownSettings.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QtTest>

class EditingListAuditTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_settings;
private slots:
    void initTestCase() {
        QVERIFY(m_settings.isValid());
        MarkdownSettingsStore::setStoragePath(m_settings.filePath("settings.bin"));
        QVERIFY(MarkdownSettingsStore::save(MarkdownSettings()));
    }
    void backspaceFirstOutdents_data() {
        QTest::addColumn<QString>("marker"); QTest::addColumn<int>("depth");
        QTest::addColumn<bool>("rendered"); QTest::addColumn<QString>("body");
        for (auto marker : {QString("- "), QString("+ "), QString("* "), QString("1. "), QString("1) "), QString("- [ ] "), QString("- [x] ")})
            for (int depth : {1, 2}) for (bool rendered : {false, true}) for (auto body : {QString(), QString("content")})
                QTest::newRow(qPrintable(marker + QString::number(depth) + (rendered ? "doc" : "source") + body)) << marker << depth << rendered << body;
    }
    void backspaceFirstOutdents() {
        QFETCH(QString, marker); QFETCH(int, depth); QFETCH(bool, rendered); QFETCH(QString, body);
        const QString indent(depth * 4, ' ');
        const QString lead = marker + "parent\n" + (depth == 2 ? "    " + marker + "child\n" : "");
        QString expected = lead + indent + marker + body + "\n" + marker + "after";
        MarkdownDocumentView document; MarkdownEditor source;
        QTextEdit *view = rendered ? static_cast<QTextEdit *>(&document) : &source;
        view->resize(740, 420); view->show(); view->setFocus();
        if (rendered) document.setMarkdownSource("list-audit.md", expected); else source.setSource(expected);
        const auto text = [&] { return rendered ? document.markdownSource() : source.source(); };
        const int initialCaret = lead.size() + indent.size() + marker.size();
        if (rendered) document.restoreSourceCursor(initialCaret, initialCaret);
        else { auto c = view->textCursor(); c.setPosition(initialCaret); view->setTextCursor(c); }
        for (int level = depth; level > 0; --level) {
            QTest::keyClick(view, Qt::Key_Backspace); expected.remove(lead.size(), 4); QCOMPARE(text(), expected);
            const int caret = lead.size() + (level - 1) * 4 + marker.size();
            QCOMPARE(rendered ? document.inlineCursorSourcePosition() : view->textCursor().position(), caret);
            if (rendered) { QVERIFY(view->textCursor().block().textList()); QCOMPARE(view->textCursor().positionInBlock(), 0); }
        }
        const int insertion = lead.size() + marker.size();
        QTest::keyClicks(view, "X"); expected.insert(insertion, 'X'); QCOMPARE(text(), expected);
        QTest::qWait(500); QCOMPARE(text(), expected);
        if (rendered) { QCOMPARE(document.inlineCursorSourcePosition(), insertion + 1); QCOMPARE(view->textCursor().block().text(), "X" + body); }
    }
    void ordinaryListBackspaceExitsDirectly_data() {
        QTest::addColumn<QString>("marker"); QTest::addColumn<QString>("body");
        QTest::addColumn<QString>("lead"); QTest::addColumn<bool>("rendered");
        for (const QString marker : {QString("- "), QString("+ "), QString("* "), QString("1. "), QString("2) ")})
            for (const QString body : {QString(), QString("content")})
                for (const QString lead : {QString(), QString("- before\n")})
                    for (bool rendered : {false, true})
                        QTest::newRow(qPrintable(marker + body + lead + (rendered ? "doc" : "source")))
                            << marker << body << lead << rendered;
    }
    void ordinaryListBackspaceExitsDirectly() {
        QFETCH(QString, marker); QFETCH(QString, body); QFETCH(QString, lead); QFETCH(bool, rendered);
        MarkdownDocumentView document; MarkdownEditor source;
        QTextEdit *view = rendered ? static_cast<QTextEdit *>(&document) : &source;
        view->resize(740, 420); view->show(); view->setFocus();
        const QString before = lead + marker + body + "\n- after";
        if (rendered) document.setMarkdownSource("exit-list.md", before); else source.setSource(before);
        const auto text = [&] { return rendered ? document.markdownSource() : source.source(); };
        const int caret = lead.size() + marker.size();
        if (rendered) document.restoreSourceCursor(caret, caret);
        else { auto cursor = view->textCursor(); cursor.setPosition(caret); view->setTextCursor(cursor); }
        QTest::keyClick(view, Qt::Key_Backspace);
        const QString prefix = lead + (lead.isEmpty() ? QString() : QString("\n"));
        const QString expected = prefix + body + "\n- after";
        QCOMPARE(text(), expected);
        QCOMPARE(rendered ? document.inlineCursorSourcePosition() : view->textCursor().position(), prefix.size());
        if (rendered) { QVERIFY(!view->textCursor().block().textList()); QCOMPARE(view->textCursor().block().text(), body); }
        // Typing and delayed preview must keep this as a body row, with the
        // next list item independent and the original content still intact.
        QTest::keyClicks(view, "X");
        // A newly populated body row gets the document view's normal source
        // separator; it stays hidden in the preview.
        const QString typed = prefix + "X" + body + (rendered && body.isEmpty() ? "\n\n" : "\n") + "- after";
        QCOMPARE(text(), typed);
        QTest::qWait(500); QCOMPARE(text(), typed);
        if (rendered) QVERIFY(!view->textCursor().block().textList());
        // Rendered undo is supplied by MainWindow and covered by the
        // integration suite; this standalone view has no source history.
        if (!rendered) {
            QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); QCOMPARE(text(), expected);
            QTest::keyClick(view, Qt::Key_Z, Qt::ControlModifier); QCOMPARE(text(), before);
        }
    }
    void emptyQuotedListKeepsOuterQuote() {
        for (const QString marker : {QString("- "), QString("+ "), QString("* "), QString("1. ")}) {
            MarkdownDocumentView view; view.resize(740, 420); view.show(); view.setFocus();
            const QString before = "> " + marker; view.setMarkdownSource("quoted-list.md", before);
            view.restoreSourceCursor(before.size(), before.size()); QTest::keyClick(&view, Qt::Key_Backspace);
            QCOMPARE(view.markdownSource(), QString("> "));
            QVERIFY(!view.textCursor().block().textList());
            QVERIFY(view.textCursor().blockFormat().intProperty(QTextFormat::BlockQuoteLevel) > 0);
            QTest::keyClicks(&view, "body"); QCOMPARE(view.markdownSource(), QString("> body"));
        }
    }
    void emptyTaskRestoresAllSymbols_data() {
        QTest::addColumn<QString>("marker"); QTest::addColumn<QString>("tail");
        for (auto marker : {QString("- [ ] "), QString("- [x] "), QString("1. [ ] ")})
            for (auto tail : {QString(), QString("\n- after"), QString("\n\nAfter")})
                QTest::newRow(qPrintable(marker + tail)) << marker << tail;
    }
    void emptyTaskRestoresAllSymbols() {
        QFETCH(QString, marker); QFETCH(QString, tail);
        MarkdownDocumentView view; view.resize(740, 420); view.show(); view.setFocus();
        const QString before = "- parent\n" + marker + tail; view.setMarkdownSource("task-audit.md", before);
        const int caret = 9 + marker.size(); view.restoreSourceCursor(caret, caret);
        QTest::keyClick(&view, Qt::Key_Backspace);
        const auto verify = [&] {
            QCOMPARE(view.textCursor().block().text(), marker.trimmed());
            QCOMPARE(view.textCursor().positionInBlock(), marker.trimmed().size());
            QCOMPARE(view.inlineCursorSourcePosition(), caret);
            QVERIFY(!view.textCursor().block().textList());
        };
        verify(); QTest::qWait(500); verify();
        view.setMarkdownSource("task-audit.md", view.markdownSource(), true); verify();
        QTest::keyClick(&view, Qt::Key_Space); QVERIFY(view.textCursor().block().textList());
        QTest::keyClicks(&view, "new"); QVERIFY(view.markdownSource().contains(marker + "new"));
    }
};
QTEST_MAIN(EditingListAuditTests)
#include "EditingListAuditTests.moc"
