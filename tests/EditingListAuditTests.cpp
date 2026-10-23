#include "ui/document/MarkdownDocumentView.h"
#include "ui/document/MarkdownEditor.h"
#include "core/settings/MarkdownSettings.h"
#include "core/markdown/MarkdownAnalysis.h"
#include "core/markdown/MarkdownListSyntax.h"
#include "ui/document/MarkdownTypography.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextList>
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
    void nestedMarkerStyles_data() {
        QTest::addColumn<QString>("marker"); QTest::addColumn<QString>("quote");
        for (const QString marker : {QString("- "), QString("+ "), QString("* "), QString("1. "), QString("3) ")})
            for (const QString quote : {QString(), QString("> ")})
                QTest::newRow(qPrintable(quote + marker)) << marker << quote;
    }
    void nestedMarkerStyles() {
        QFETCH(QString, marker); QFETCH(QString, quote);
        QString source;
        for (int depth = 0; depth < 5; ++depth) source += quote + QString(depth * 4, ' ') + marker + QString("level%1").arg(depth + 1) + '\n';
        MarkdownDocumentView view; view.resize(850, 600); view.show();
        view.setMarkdownSource("levels.md", source);
        const auto items = MarkdownListSyntax::items(source, MarkdownAnalysis::scan(source));
        QCOMPARE(items.size(), 5);
        for (int i = 0; i < items.size(); ++i) {
            QCOMPARE(items[i].depth, i + 1);
            view.restoreSourceCursor(items[i].body, items[i].body);
            const auto block = view.textCursor().block(); QVERIFY(block.textList());
            const bool ordered = marker.size() > 2;
            const auto expected = i >= 2 ? QTextListFormat::ListStyleUndefined
                : ordered ? i == 1 ? QTextListFormat::ListLowerAlpha : QTextListFormat::ListDecimal
                : i == 1 ? QTextListFormat::ListCircle : QTextListFormat::ListDisc;
            QCOMPARE(block.textList()->format().style(), expected);
            // Reducing the dot must never reduce the item's text size.
            QTextCursor sample(block); sample.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            QCOMPARE(sample.charFormat().fontPointSize(), MarkdownTypography::bodyFont().pointSizeF());
        }
        QCOMPARE(view.markdownSource(), source);
        QVERIFY(!view.document()->isModified());
        view.setMarkdownSource("levels.md", source, true);
        QCOMPARE(view.markdownSource(), source);
        QVERIFY(!view.document()->isModified());
    }
    void secondLevelUsesSourceNumbers_data() {
        QTest::addColumn<int>("number"); QTest::addColumn<QString>("label");
        QTest::newRow("a") << 1 << "a."; QTest::newRow("c") << 3 << "c.";
        QTest::newRow("z") << 26 << "z."; QTest::newRow("aa") << 27 << "aa.";
        QTest::newRow("zero") << 0 << "0.";
    }
    void secondLevelUsesSourceNumbers() {
        QFETCH(int, number); QFETCH(QString, label);
        const QString source = QString("1. parent\n    %1. child\n    9. sibling").arg(number);
        MarkdownDocumentView view; view.setMarkdownSource("numbers.md", source);
        const int caret = source.indexOf("child"); view.restoreSourceCursor(caret, caret);
        const auto block = view.textCursor().block(); QVERIFY(block.textList());
        QCOMPARE(block.textList()->itemText(block), label);
        view.restoreSourceCursor(source.indexOf("sibling"), source.indexOf("sibling"));
        QCOMPARE(view.textCursor().block().textList()->itemText(view.textCursor().block()), QString("i."));
        QCOMPARE(view.markdownSource(), source);
        view.restoreSourceCursor(caret + 5, caret + 5); QTest::keyClick(&view, Qt::Key_Return);
        QVERIFY(view.markdownSource().contains(QString("child\n    %1. \n    9. sibling").arg(number + 1)));
        QVERIFY(view.textCursor().block().textList());
        QCOMPARE(view.textCursor().block().textList()->format().style(), QTextListFormat::ListLowerAlpha);
        QCOMPARE(view.textCursor().block().textList()->format().start(), number + 1);
        // Literal letters never introduce a new source list syntax.
        view.setMarkdownSource("letters.md", "a. literal\nb. literal");
        QVERIFY(!view.document()->begin().textList());
        QCOMPARE(view.toPlainText(), QString("a. literal\nb. literal"));
    }
    void indentationUpdatesMarkers() {
        for (const QString marker : {QString("- "), QString("1. ")}) {
            MarkdownDocumentView view; view.resize(800, 600); view.show(); view.setFocus();
            const QString before = marker + "parent\n    " + marker + "child\n    " + marker + "target";
            view.setMarkdownSource("indent-markers.md", before);
            view.restoreSourceCursor(before.indexOf("target"), before.indexOf("target"));
            QCOMPARE(view.textCursor().block().textList()->format().style(), marker == "- " ? QTextListFormat::ListCircle : QTextListFormat::ListLowerAlpha);
            QTest::keyClick(&view, Qt::Key_Tab);
            QCOMPARE(view.textCursor().block().textList()->format().style(), QTextListFormat::ListStyleUndefined);
            QTest::keyClick(&view, Qt::Key_Backspace);
            QCOMPARE(view.markdownSource(), before);
            QCOMPARE(view.textCursor().block().textList()->format().style(), marker == "- " ? QTextListFormat::ListCircle : QTextListFormat::ListLowerAlpha);
            QTest::keyClick(&view, Qt::Key_Tab, Qt::ShiftModifier);
            QCOMPARE(view.textCursor().block().textList()->format().style(), marker == "- " ? QTextListFormat::ListDisc : QTextListFormat::ListDecimal);
        }
    }
    void mixedListDepthAndQuotedContinuation() {
        for (const QString quote : {QString(), QString("> "), QString("> > ")}) {
            const QString source = quote + "- root\n" + quote + "  1. middle\n" + quote
                + "     continuation\n" + quote + "      - child\n" + quote + "          2. deep\n"
                + quote + "- next root";
            const auto items = MarkdownListSyntax::items(source, MarkdownAnalysis::scan(source));
            QCOMPARE(items.size(), 5);
            QCOMPARE(items[0].depth, 1); QCOMPARE(items[1].depth, 2); QCOMPARE(items[2].depth, 3);
            QCOMPARE(items[3].depth, 4); QCOMPARE(items[4].depth, 1);
        }
        MarkdownDocumentView view; view.resize(800, 600); view.show(); view.setFocus();
        const QString before = "> - parent\n>     - child"; view.setMarkdownSource("quote-indent.md", before);
        view.restoreSourceCursor(before.size() - 5, before.size() - 5);
        QTest::keyClick(&view, Qt::Key_Backspace);
        QCOMPARE(view.markdownSource(), QString("> - parent\n> - child"));
        QCOMPARE(view.textCursor().block().textList()->format().style(), QTextListFormat::ListDisc);
    }
    void clickNumberEditsSource_data() {
        QTest::addColumn<int>("depth");
        QTest::newRow("decimal") << 1; QTest::newRow("alpha") << 2; QTest::newRow("small-dot") << 3;
    }
    void clickNumberEditsSource() {
        QFETCH(int, depth);
        QString lead;
        for (int i = 1; i < depth; ++i) lead += QString((i - 1) * 4, ' ') + "1. parent\n";
        const QString source = lead + QString((depth - 1) * 4, ' ') + "1. target\n1. after";
        MarkdownDocumentView view; view.resize(800, 600); view.show(); view.setFocus();
        view.setMarkdownSource("edit-number.md", source);
        view.restoreSourceCursor(source.indexOf("target") + 3, source.indexOf("target") + 3);
        const auto block = view.textCursor().block(); const QRect body = view.QTextEdit::cursorRect(QTextCursor(block));
        const QFontMetricsF metrics(view.font());
        const qreal width = depth >= 3 ? qMax(2.0, metrics.lineSpacing() / 6)
            : metrics.horizontalAdvance(block.textList()->itemText(block));
        const QPoint marker(qRound(body.left() - metrics.horizontalAdvance(' ') - width / 2), body.center().y());
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, marker);
        const int ordinalEnd = source.indexOf("1. target") + 1;
        QCOMPARE(view.inlineCursorSourcePosition(), ordinalEnd);
        QVERIFY(!view.visibleBlockMarkers().isEmpty());
        QTest::keyClick(&view, Qt::Key_Backspace); QTest::keyClicks(&view, "3");
        QString expected = source; expected.replace(source.indexOf("1. target"), 1, "3");
        QCOMPARE(view.markdownSource(), expected);
        QTest::keyClick(&view, Qt::Key_End); QTest::qWait(500);
        QCOMPARE(view.markdownSource(), expected);
        const auto updated = view.document()->findBlock(view.toPlainText().indexOf("target"));
        QVERIFY(updated.textList());
        if (depth < 3) QCOMPARE(updated.textList()->itemText(updated), depth == 2 ? QString("c.") : QString("3."));
    }
    void nestedTasksKeepCheckboxes() {
        const QString source = "- parent\n    - child\n        - [ ] task";
        MarkdownDocumentView view; view.resize(800, 600); view.show(); view.setFocus();
        view.setMarkdownSource("task-marker.md", source);
        view.restoreSourceCursor(source.size(), source.size());
        const auto block = view.textCursor().block(); QVERIFY(block.textList());
        QCOMPARE(block.blockFormat().marker(), QTextBlockFormat::MarkerType::Unchecked);
        QVERIFY(block.textList()->format().style() != QTextListFormat::ListStyleUndefined);
        const auto rect = view.QTextEdit::cursorRect(QTextCursor(block));
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(rect.left() - 12, rect.center().y()));
        QCOMPARE(view.markdownSource(), QString("- parent\n    - child\n        - [x] task"));
    }
    void markerSizesInRenderedImage() {
        const QString source = "- Unordered level 1\n    - Unordered level 2\n        - Unordered level 3\n            - Unordered level 4\n"
            "1. Ordered level 1\n    1. Ordered level 2 - a\n    2. Ordered level 2 - b\n        1. Ordered level 3\n            1. Ordered level 4\n\nBody";
        MarkdownDocumentView view; view.resize(880, 720); view.show();
        view.refreshAppearance();
        view.setMarkdownSource("marker-preview.md", source); view.restoreSourceCursor(source.size(), source.size());
        QCoreApplication::processEvents();
        const auto image = view.viewport()->grab().toImage(); const auto background = view.palette().color(QPalette::Base);
        QVector<int> ink;
        for (auto block = view.document()->begin(); block.isValid() && ink.size() < 4; block = block.next()) {
            QVERIFY(block.textList()); const auto rect = view.QTextEdit::cursorRect(QTextCursor(block));
            int pixels = 0;
            for (int y = rect.top(); y <= rect.bottom(); ++y) for (int x = rect.left() - 24; x < rect.left() - 3; ++x) {
                if (!image.rect().contains(x, y)) continue;
                const auto color = image.pixelColor(x, y);
                if (qAbs(color.red() - background.red()) + qAbs(color.green() - background.green())
                    + qAbs(color.blue() - background.blue()) > 90) ++pixels;
            }
            ink.append(pixels);
        }
        QCOMPARE(ink.size(), 4); QVERIFY(ink[2] > 0);
        QVERIFY2(ink[0] > ink[2] * 1.5, qPrintable(QString("top=%1 third=%2").arg(ink[0]).arg(ink[2])));
        QVERIFY(qAbs(ink[2] - ink[3]) <= 2);
        QVERIFY(view.grab().save(QCoreApplication::applicationDirPath() + "/list-marker-preview.png"));
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
