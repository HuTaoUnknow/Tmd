#include "core/markdown/MarkdownSyntax.h"
#include "core/settings/MarkdownSettings.h"
#include "ui/document/MarkdownDocumentView.h"
#include "ui/common/Theme.h"
#include <QSettings>
#include <QTemporaryDir>
#include <QTextTable>
#include <QTextBlock>
#include <QtTest>
class DocumentPresentationTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_temp;
private slots:
    void initTestCase() {
        QVERIFY(m_temp.isValid()); QCoreApplication::setOrganizationName("TmdPresentationTests"); QCoreApplication::setApplicationName("Isolated");
        QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,m_temp.path());
        MarkdownSettingsStore::setStoragePath(m_temp.filePath("style.txt")); QVERIFY(MarkdownSettingsStore::save(MarkdownSettings())); applyTreeMdTheme(*qApp);
    }
    void chinesePunctuationStrongKeepsSourceAndEditing() {
        for(const QString source : {QStringLiteral("**PHP文件被直接包含时，其中的PHP代码通常会被执行，而不是原样显示源码。**源码泄露需要其他条件。"),
                                    QStringLiteral("前文**“中文引号。”**后文"),QStringLiteral("[**中文。**后文](https://example.com/page)"),
                                    QStringLiteral("| 说明 |\n| --- |\n| **中文。**后文 |")}) {
            const auto syntax=MarkdownSyntax::scan(source); QVERIFY(!syntax.formats.isEmpty());
            MarkdownDocumentView view; view.resize(1100,420); view.show(); view.setMarkdownSource(m_temp.filePath("sample.md"),source);
            const QString needle=source.contains("PHP") ? QStringLiteral("PHP文件") : source.contains("引号") ? QStringLiteral("中文引号") : QStringLiteral("中文");
            auto cursor=view.document()->find(needle); QVERIFY(!cursor.isNull()); QVERIFY(cursor.charFormat().fontWeight()>=QFont::Bold);
            QVERIFY(!view.toPlainText().contains("**")); QVERIFY(!view.toPlainText().contains(QChar(0xe800))); QCOMPARE(view.markdownSource(),source);
            cursor.setPosition(cursor.selectionEnd()); view.setTextCursor(cursor); QTest::keyClicks(&view,"X");
            QTRY_VERIFY(view.markdownSource().contains(needle+"X")); QVERIFY(view.markdownSource().contains("**"));
            view.setMarkdownSource(m_temp.filePath("sample.md"),source,true); QCOMPARE(view.markdownSource(),source);
            const int after=syntax.formats.front().end;
            view.restoreSourceCursor(after,after); QTest::keyClicks(&view,"Z");
            QTRY_COMPARE(view.markdownSource(),source.left(after)+"Z"+source.mid(after));
        }
        const auto heading=MarkdownSyntax::scan(QStringLiteral("## **标题。**后文"));
        QCOMPARE(heading.headings.front().text,QStringLiteral("标题。后文"));
    }
    void escapedUnpairedCodeAndWhitespaceStayLiteral() {
        for(const QString source : {QStringLiteral("\\*\\*中文。**后文"),QStringLiteral("`**中文。**后文`"),QStringLiteral("```text\n**中文。**后文\n```"),
                                    QStringLiteral("[路径](file:///D:/a/**中文。**后文.md)"),QStringLiteral("[ref]: file:///D:/a/**中文。**后文.md"),
                                    QStringLiteral("https://example.com/**中文。**后文"),
                                    QStringLiteral("**未完成。后文"),QStringLiteral("**末尾空格。 **后文")}) {
            const auto syntax=MarkdownSyntax::scan(source);
            for(const auto &format:syntax.formats) QVERIFY(format.code || format.opening!="**");
            MarkdownDocumentView view; view.setMarkdownSource(m_temp.filePath("literal.md"),source); QCOMPARE(view.markdownSource(),source);
        }
    }
    void tablesUseLightHorizontalRulesAndKeepKeyboardEditing() {
        const QString source=QStringLiteral("### 包含、普通读取与路径穿越的区别\n\n| 名称 | 含义 | 常见结果 |\n| --- | --- | --- |\n| 文件包含 | 使用 `include` | **执行。**后文 |\n| 普通读取 | 读取字节 | 作为数据 |\n\n**PHP文件被直接包含时，其中的PHP代码通常会被执行，而不是原样显示源码。**源码泄露需要其他读取、转换或错误输出条件。\n");
        MarkdownDocumentView view; view.resize(1300,420); view.show(); view.setMarkdownSource(m_temp.filePath("table.md"),source);
        auto *table=qobject_cast<QTextTable *>(view.document()->rootFrame()->childFrames().value(0)); QVERIFY(table);
        QCOMPARE(table->rows(),3); QCOMPARE(table->columns(),3); QCOMPARE(table->format().border(),0.0);
        const auto header=table->cellAt(0,0).format().toTableCellFormat(),body=table->cellAt(1,0).format().toTableCellFormat();
        QCOMPARE(body.leftBorder(),0.0); QCOMPARE(body.rightBorder(),0.0); QVERIFY(header.bottomBorder()>body.bottomBorder());
        QCOMPARE(body.background().color(),MarkdownSettingsStore::current().color(MarkdownSettings::PageBackground));
        QCOMPARE(header.background().color(),body.background().color());
        auto cursor=table->cellAt(0,0).firstCursorPosition(); cursor.movePosition(QTextCursor::NextCharacter,QTextCursor::KeepAnchor); QVERIFY(cursor.charFormat().fontWeight()>=QFont::Bold);
        const QString folder=qEnvironmentVariable("TMD_PRESENTATION_QA_DIR");
        if(!folder.isEmpty()) { QDir().mkpath(folder); QVERIFY(view.grab().save(QDir(folder).filePath("table-and-bold.png"))); }
        view.setTextCursor(table->cellAt(2,0).firstCursorPosition()); QTest::keyClick(&view,Qt::Key_Return,Qt::ShiftModifier);
        QTRY_VERIFY(view.markdownSource().split('\n').size()>source.split('\n').size());
        QCOMPARE(view.markdownSource().count("普通读取"),source.count("普通读取"));
        QCOMPARE(view.markdownSource().count("文件包含"),source.count("文件包含"));
    }
};
QTEST_MAIN(DocumentPresentationTests)
#include "DocumentPresentationTests.moc"
