#include "core/markdown/MarkdownAnalysis.h"
#include "core/markdown/CodeSyntax.h"
#include "core/markdown/LanguageCatalog.h"
#include "core/markdown/MarkdownCodeBlocks.h"
#include "core/markdown/MarkdownSyntax.h"
#include "core/markdown/MarkdownBlockSyntax.h"
#include "core/markdown/MarkdownCodeEditing.h"
#include "core/markdown/MarkdownListEditing.h"
#include "core/markdown/MarkdownParser.h"
#include "core/markdown/MarkdownImages.h"
#include "core/markdown/MarkdownSourceMap.h"
#include "core/images/ImageStorage.h"
#include "core/images/ImageInsertJob.h"
#include "core/library/MarkdownManager.h"
#include "core/library/MarkdownFileIO.h"
#include <QTemporaryDir>
#include <QDir>
#include <QtTest>
#include <QUrl>

class MarkdownCoreTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_settingsDirectory;
private slots:
    void saveAllModifiedPreservesConflicts() {
        QTemporaryDir directory; QVERIFY(directory.isValid()); MarkdownManager manager(directory.path());
        QVERIFY(manager.createDocument("A.md", "# A")); QVERIFY(manager.createDocument("B.md", "# B")); QVERIFY(manager.createDocument("C.md", "# C"));
        manager.updateContent("A.md", "# A edit"); manager.updateContent("B.md", "# B edit");
        QCOMPARE(manager.modifiedDocuments(), QStringList({"A.md", "B.md"}));
        QVERIFY(MarkdownFileIO::writeFile(directory.filePath("B.md"), "# B external"));
        const auto result = manager.saveAllModified(); QVERIFY(!result.success()); QCOMPARE(result.saved, QStringList({"A.md"}));
        QVERIFY(result.failures.contains("B.md")); QCOMPARE(manager.modifiedDocuments(), QStringList({"B.md"}));
        QString disk; QVERIFY(MarkdownFileIO::readFile(directory.filePath("A.md"), disk)); QCOMPARE(disk, QString("# A edit"));
        QVERIFY(MarkdownFileIO::readFile(directory.filePath("B.md"), disk)); QCOMPARE(disk, QString("# B external"));
        QCOMPARE(manager.loadNode("B.md")->content(), QString("# B edit"));
    }
    void trailingWhitespacePreservesSemantics() {
        for (const QString source : {QString("--   "), QString("## Title   "), QString("- item   "), QString("> quote   "),
                                     QString("- [ ] task   "), QString("Title  \n---")}) {
            QTextDocument document; document.setDefaultFont(QFont("Segoe UI", 12)); document.setMarkdown(MarkdownBlockSyntax::prepare(source).text, MarkdownSyntax::renderFeatures());
            QString display = document.toRawText(); display.replace(QChar::ParagraphSeparator, '\n');
            MarkdownSourceMap map; map.rebuild(source, display);
            const auto edits = map.editableWhitespaceEdits(); QCOMPARE(edits.size(), 1);
            const int firstEnd = source.contains('\n') ? source.indexOf('\n') : source.size();
            QCOMPARE(map.sourcePosition(edits[0].position), firstEnd - edits[0].text.size());
            QCOMPARE(edits[0].text, source.mid(firstEnd - edits[0].text.size(), edits[0].text.size()));
        }
        for (const QString source : {QString("---  "), QString("***  "), QString("## Title ##  "), QString("- "),
                                     QString("> "), QString("[target]: https://example.com  "), QString("```text\nx  \n```")}) {
            QTextDocument document; document.setDefaultFont(QFont("Segoe UI", 12)); document.setMarkdown(MarkdownBlockSyntax::prepare(source).text, MarkdownSyntax::renderFeatures());
            QString display = document.toRawText(); display.replace(QChar::ParagraphSeparator, '\n');
            MarkdownSourceMap map; map.rebuild(source, display); QVERIFY(map.editableWhitespaceEdits().isEmpty());
        }
        MarkdownSourceMap alreadyVisible; alreadyVisible.rebuild("## Title   ", "Title   ");
        QVERIFY(alreadyVisible.editableWhitespaceEdits().isEmpty());
    }
    void prefixSeparatesActivationFromEditableSpaces() {
        for (const QString prefix : {QString("-"), QString("###"), QString(">"), QString("12."), QString("+"), QString("*")}) {
            const QString source = prefix + "   "; const auto markers = MarkdownBlockSyntax::markers(source);
            QCOMPARE(markers.size(), 1); QCOMPARE(markers[0].end, prefix.size() + 1);
            MarkdownSourceMap map; map.rebuild(source, ""); const auto edits = map.editableWhitespaceEdits();
            QCOMPARE(edits.size(), 1); QCOMPARE(edits[0].text, QString("  "));
            const auto edit = MarkdownBlockSyntax::backspaceEmptyPrefix(prefix + ' ', prefix.size() + 1);
            const bool list = QStringList{"-", "+", "*", "12."}.contains(prefix);
            QVERIFY(edit.handled); QCOMPARE(edit.source, list ? QString() : prefix); QCOMPARE(edit.caret, list ? 0 : prefix.size());
            QVERIFY(!MarkdownBlockSyntax::backspaceEmptyPrefix(source, source.size()).handled);
            map.rebuild(prefix + " x", "x");
            QCOMPARE(map.edited(0, 1, "", nullptr), prefix + ' ');
        }
    }
    void listPrefixExitKeepsParagraphAndCaret() {
        for (const QString source : {QString("- first\n- "), QString("- \n- "), QString("- first\n    - "),
                                     QString("- first\n    - child\n        - ")}) {
            const auto edit = MarkdownBlockSyntax::backspaceEmptyPrefix(source, source.size());
            QVERIFY(edit.handled); QVERIFY(edit.changed);
            const int row = source.lastIndexOf('\n') + 1;
            const int indent = source.size() - row - 2;
            QString expected = source; int caret;
            if (indent) { expected.remove(row, qMin(indent, 4)); caret = source.size() - qMin(indent, 4); }
            else { expected.remove(row, 2); expected.insert(row, '\n'); caret = row + 1; }
            QCOMPARE(edit.source, expected); QCOMPARE(edit.caret, caret); QCOMPARE(edit.anchor, edit.caret);
        }
    }
    void codeBlockLanguagesAndExactCopyText() {
        QCOMPARE(LanguageCatalog::canonicalName("C++ extra"), QString("cpp"));
        QCOMPARE(LanguageCatalog::canonicalName("language-XML"), QString("xml"));
        QCOMPARE(LanguageCatalog::canonicalName("{.py}"), QString("python"));
        QCOMPARE(LanguageCatalog::displayName("cxx"), QString("C++"));
        QCOMPARE(LanguageCatalog::displayName("unknown flags"), QString("unknown"));
        const QString source = "Before\n\n> ```html\n> <p>中文</p>\n> \n> \tvalue\n> ```\nAfter\n\n```\n\n```\n```xml\nunfinished";
        const auto blocks = MarkdownCodeBlocks::scan(source); QCOMPARE(blocks.size(), 2);
        QCOMPARE(blocks[0].language, QString("html")); QCOMPARE(blocks[0].label, QString("HTML"));
        QCOMPARE(blocks[0].code, QString("<p>中文</p>\n\n\tvalue\n"));
        QCOMPARE(blocks[1].code, QString("\n")); QCOMPARE(blocks[1].language, QString("plaintext"));
        QVERIFY(!blocks[0].code.contains("After")); QVERIFY(MarkdownCodeBlocks::scan("```html\n<p>").isEmpty());
    }
    void codeSyntaxUsesLanguageAndLexicalPriority() {
        using namespace CodeSyntax;
        const auto kindAt = [](const Line &line, int position) {
            for (const auto &part : line.tokens) if (position >= part.start && position < part.start + part.length) return int(part.kind);
            return -1;
        };
        QString text = "#include <vector>";
        QCOMPARE(kindAt(highlightLine(text, "c++"), 0), int(Kind::Preprocessor));
        QCOMPARE(kindAt(highlightLine(text, "python"), 0), int(Kind::Comment));
        text = "const char *url = \"https://host/return\"; // if 42";
        auto line = highlightLine(text, "cpp");
        QCOMPARE(kindAt(line, text.indexOf("const")), int(Kind::Keyword));
        QCOMPARE(kindAt(line, text.indexOf("char")), int(Kind::Type));
        QCOMPARE(kindAt(line, text.indexOf("return")), int(Kind::String));
        QCOMPARE(kindAt(line, text.indexOf("42")), int(Kind::Comment));
        text = "<div class=\"if // 42\">return</div>"; line = highlightLine(text, "html");
        QCOMPARE(kindAt(line, text.indexOf("div")), int(Kind::Tag));
        QCOMPARE(kindAt(line, text.indexOf("class")), int(Kind::Attribute));
        QCOMPARE(kindAt(line, text.indexOf("if")), int(Kind::String));
        QCOMPARE(kindAt(line, text.indexOf("return")), -1);
        text = "select 'it''s -- text', 0x10 -- FROM"; line = highlightLine(text, "sql");
        QCOMPARE(kindAt(line, 0), int(Kind::Keyword));
        QCOMPARE(kindAt(line, text.indexOf("-- text")), int(Kind::String));
        QCOMPARE(kindAt(line, text.indexOf("FROM")), int(Kind::Comment));
        text = "{\"return\": \"true //\", \"active\": true}"; line = highlightLine(text, "json");
        QCOMPARE(kindAt(line, text.indexOf("return")), int(Kind::Property));
        QCOMPARE(kindAt(line, text.indexOf("true")), int(Kind::String));
        QCOMPARE(kindAt(line, text.lastIndexOf("true")), int(Kind::Literal));
        text = "#app { color: #ffcc00; }"; line = highlightLine(text, "css");
        QCOMPARE(kindAt(line, 0), int(Kind::Selector)); QCOMPARE(kindAt(line, text.indexOf("color")), int(Kind::Property));
        QCOMPARE(kindAt(line, text.indexOf("#ff")), int(Kind::Number));
        text = "echo $HOME # comment"; line = highlightLine(text, "bash");
        QCOMPARE(kindAt(line, text.indexOf("$HOME")), int(Kind::Variable));
        QCOMPARE(kindAt(line, text.indexOf("comment")), int(Kind::Comment));
        QVERIFY(highlightLine("return 'if' // 42", "plaintext").tokens.isEmpty());
        QVERIFY(highlightLine("return 'if' // 42", "unrecognized").tokens.isEmpty());
    }
    void codeSyntaxMultilineAndEmbeddedLanguages() {
        using namespace CodeSyntax;
        const auto kindAt = [](const Line &line, int position) {
            for (const auto &part : line.tokens) if (position >= part.start && position < part.start + part.length) return int(part.kind);
            return -1;
        };
        auto first = highlightLine("/* class", "cpp"); QCOMPARE(first.state.mode, State::Comment);
        auto next = highlightLine("return */ int n = 1;", "cpp", first.state);
        QCOMPARE(kindAt(next, 0), int(Kind::Comment)); QCOMPARE(kindAt(next, 10), int(Kind::Type));
        first = highlightLine("auto s = R\"tag(if //", "cpp"); QCOMPARE(first.state.mode, State::String);
        const QString raw = "class )tag\"; return 1;"; next = highlightLine(raw, "cpp", first.state);
        QCOMPARE(kindAt(next, 0), int(Kind::String)); QCOMPARE(kindAt(next, raw.indexOf("return")), int(Kind::Keyword));
        first = highlightLine("s = '''class", "python"); next = highlightLine("def # not a comment'''; return", "python", first.state);
        QCOMPARE(kindAt(next, 0), int(Kind::String)); QCOMPARE(next.state.mode, State::Normal);
        first = highlightLine("<!-- if", "xml"); next = highlightLine("class --> <node id='42'/>", "xml", first.state);
        QCOMPARE(kindAt(next, 0), int(Kind::Comment)); QCOMPARE(kindAt(next, 11), int(Kind::Tag));
        first = highlightLine("<script>const value = \"return //\";", "html"); QCOMPARE(first.state.embedded, QString("javascript"));
        QCOMPARE(kindAt(first, 8), int(Kind::Keyword));
        const QString embedded = "if (value) call();</script><style>p {color:#fff;}</style>";
        next = highlightLine(embedded, "html", first.state);
        QCOMPARE(kindAt(next, 0), int(Kind::Keyword)); QCOMPARE(kindAt(next, embedded.indexOf("color")), int(Kind::Property));
        QVERIFY(next.state.embedded.isEmpty());
    }
    void emptyCodeLineBackspaceTransactions() {
        const QString source = "```cpp\n\nnext\n```";
        auto edit = MarkdownCodeEditing::backspace(source, 7); QVERIFY(edit.handled && edit.changed); QCOMPARE(edit.source, QString("```cpp\nnext\n```"));
        edit = MarkdownCodeEditing::backspace(edit.source, edit.caret); QVERIFY(edit.handled); QVERIFY(!edit.changed);
        edit = MarkdownCodeEditing::backspace("```cpp\n\n```", 7); QVERIFY(edit.handled && edit.changed); QVERIFY(edit.source.isEmpty());
        const auto rendered = MarkdownAnalysis::forRendering("```py\n**body**"); QVERIFY(rendered.fences.isEmpty()); QCOMPARE(rendered.literalFences.size(), 1);
        const auto prepared = MarkdownBlockSyntax::prepare("```py\n**body**"); QCOMPARE(prepared.text, QString("\\`\\`\\`py\n**body**"));
    }
    void listEditingTransactions() {
        QString source = "- parent\n- child";
        auto edit = MarkdownListEditing::newline(source, source.size(), source.size());
        QVERIFY(edit.handled); QCOMPARE(edit.source, QString("- parent\n- child\n- "));
        edit = MarkdownListEditing::indent(edit.source, edit.caret, edit.caret, false);
        QCOMPARE(edit.source, QString("- parent\n- child\n    - "));
        const auto nested = MarkdownAnalysis::scan(edit.source); QVERIFY(!nested.lines.back().code);
        QVERIFY(nested.lines.back().listMarkerStart >= 0); QCOMPARE(MarkdownBlockSyntax::markers(edit.source).back().text, QString("    - "));
        edit = MarkdownListEditing::backspace(edit.source, edit.caret);
        QCOMPARE(edit.source, QString("- parent\n- child\n- ")); QCOMPARE(edit.caret, edit.source.size());
        source = "- parent\n    - child\n    - peer";
        edit = MarkdownListEditing::indent(source, source.size(), source.size(), false);
        QCOMPARE(edit.source, QString("- parent\n    - child\n        - peer")); QVERIFY(!MarkdownAnalysis::scan(edit.source).lines.back().code);
        edit = MarkdownListEditing::indent(edit.source, edit.caret, edit.caret, true); QCOMPARE(edit.source, source);
        source = "3. numbered"; edit = MarkdownListEditing::newline(source, source.size(), source.size()); QCOMPARE(edit.source, QString("3. numbered\n4. "));
        source = "- [x] done"; edit = MarkdownListEditing::newline(source, source.size(), source.size()); QCOMPARE(edit.source, QString("- [x] done\n- [ ] "));
        QVERIFY(!MarkdownListEditing::newline("-NoSpace", 8, 8).handled);
        QVERIFY(!MarkdownListEditing::indent("    - literal", 13, 13, false).handled);
        QVERIFY(!MarkdownAnalysis::scan("    - literal").lines.front().code); // N43: local indentation is ordinary text.
        QVERIFY(MarkdownAnalysis::scan("    - literal", true).lines.front().code);
        source = "```md\n- literal\n```"; QVERIFY(!MarkdownListEditing::backspace(source, source.indexOf("literal")).handled);
        source = "- text"; QVERIFY(!MarkdownListEditing::indent(source, source.size(), source.size(), false).changed);
        edit = MarkdownListEditing::backspace(source, 2); QCOMPARE(edit.source, QString("text")); QCOMPARE(edit.caret, 0);
    }
    void initTestCase() {
        QVERIFY(m_settingsDirectory.isValid());
        MarkdownSettingsStore::setStoragePath(m_settingsDirectory.filePath("settings.txt"));
    }
    void quotedFencesAndComments_data() {
        QTest::addColumn<QString>("source");
        QTest::newRow("longer-close") << QString("> ```md\n> # literal\n> ![example](missing.png)\n> ````\n\n# Real\n![real](real.png)");
        QTest::newRow("container-ends") << QString("> ```md\n> # literal\n> ![example](missing.png)\n\n# Real\n![real](real.png)");
        QTest::newRow("comment") << QString("<!--\n# literal\n![example](missing.png)\n-->\n# Real\n![real](real.png)");
        QTest::newRow("raw-html") << QString("<pre>\n# literal\n![example](missing.png)\n</pre>\n# Real\n![real](real.png)");
        QTest::newRow("list-code") << QString("- ```md\n  # literal\n  ![example](missing.png)\n  ````\n\n# Real\n![real](real.png)");
    }
    void quotedFencesAndComments() {
        QFETCH(QString, source);
        const auto analysis = MarkdownAnalysis::scan(source);
        const auto outline = MarkdownParser::outline(source, analysis);
        QCOMPARE(outline.size(), 1); QCOMPARE(outline.front().text, QString("Real"));
        const auto markers = MarkdownParser::headingMarkers(source, analysis);
        QCOMPARE(markers.size(), 1);
        const auto images = MarkdownImages::parse(source, analysis);
        QCOMPARE(images.size(), 1); QCOMPARE(images.front().source, QString("real.png"));
    }

    void standardHeadingBoundaries_data() {
        QTest::addColumn<QString>("source"); QTest::addColumn<QStringList>("expected");
        const auto row = [](const char *name, const QString &source, const QStringList &expected = {}) { QTest::newRow(name) << source << expected; };
        row("setext", "Text\n---", {"2:Text"}); row("single-dash-literal", "Text\n-");
        row("blank-break", "Text\n\n---"); row("bullet-break", "- item\n---"); row("plus-break", "+ item\n---"); row("ordered-break", "1. item\n---");
        row("quote-ends", "> quote\n---"); row("quote-begins", "Text\n> ---"); row("quoted-setext", "> quote\n> ---", {"2:quote"});
        row("list-setext", "- item\n  ---", {"2:item"}); row("new-item-break", "Text\n- ---");
        row("multiline", "First\nSecond\n---", {"2:Second"}); row("list-multiline", "- item\n  text\n  ---", {"2:text"}); // N42
        row("dash-word", "-Word\n---", {"2:-Word"}); row("hash-word", "#word\n---", {"2:#word"});
        row("table", "| A | B |\n| --- | --- |\n---"); row("spaced-break", "Text\n- - -"); row("mixed-break", "Text\n--*");
        row("spaced-equals", "Text\n= ="); row("indented", "Text\n    ---"); row("fenced", "```md\nText\n---\n```");
        row("html", "<pre>\nText\n---\n</pre>"); row("comment", "<!--\nText\n---\n-->");
        row("front-matter", "---\n# hidden\n---\n# Visible", {"1:Visible"});
        row("front-matter-empty-heading", "---\n# hidden\n---\n# ", {"1:"});
        row("crlf", "First\r\nSecond\r\n---", {"2:Second"});
        row("atx-break", "# Title\n---", {"1:Title"}); row("atx-lone-dash", "# Title\n- ", {"1:Title"}); row("empty", "# \n##\t", {"1:"}); row("bare", "#\n##\n#word");
        row("utf16", QString::fromUtf8("前文😀\n后文\n---"), {QString::fromUtf8("2:后文")});
    }
    void standardHeadingBoundaries() {
        QFETCH(QString, source); QFETCH(QStringList, expected); QStringList actual;
        const auto headings = MarkdownParser::outline(source);
        for (const auto &h : headings) actual += QString::number(h.level) + ':' + h.text;
        QCOMPARE(actual, expected);
        if (!headings.isEmpty() && (source.startsWith("First") || source.startsWith("- item\n  text") || source.startsWith(QString::fromUtf8("前文")))) QCOMPARE(headings.first().position, source.indexOf('\n') + 1);
    }
    void strictLinePrefixesKeepOriginalOffsets() {
        for (const QString source : {QString("Text\n-"), QString("Text\n- "), QString("Text\n*"), QString("#\tTitle"), QString("##"), QString("-\titem"), QString(">quote")}) {
            QVERIFY(MarkdownParser::outline(source).isEmpty());
            const auto prepared = MarkdownBlockSyntax::prepare(source);
            QCOMPARE(prepared.positions.size(), prepared.text.size() + 1);
            QCOMPARE(prepared.synthetic.size(), prepared.text.size());
            for (int i = 0; i < prepared.text.size(); ++i)
                if (!prepared.synthetic[i]) QCOMPARE(prepared.text[i], source[prepared.positions[i]]);
        }
        const QString source = QString::fromUtf8("前😀\n-\n\n## 正文 `code`\n\n- item");
        const auto parsed = MarkdownSyntax::scan(source); QCOMPARE(parsed.headings.size(), 1);
        QCOMPARE(parsed.headings.first().position, source.indexOf("##")); QCOMPARE(parsed.formats.size(), 1);
        QCOMPARE(parsed.formats.first().start, source.indexOf("`code`"));
        const QString literal = "```md\n-\n#\tTitle\n>no-space\n```";
        QCOMPARE(MarkdownBlockSyntax::prepare(literal).text, literal); QVERIFY(MarkdownBlockSyntax::markers(literal).isEmpty());
        MarkdownSourceMap mapping; mapping.rebuild("## Title", "Title");
        QCOMPARE(mapping.blockMarkers().size(), 1); QCOMPARE(mapping.sourcePosition(0), 3);
        QCOMPARE(mapping.stepAcrossMarker(0, 3, false), 2); QCOMPARE(mapping.displayPosition(1), 0);
        mapping.setBoundary(0, 1); QCOMPARE(mapping.edited(0, 0, "#", nullptr), QString("### Title"));
        mapping.rebuild("> one\n> two", "one two"); QCOMPARE(mapping.blockMarkers().size(), 2);
        QCOMPARE(mapping.blockMarkers().last().text, QString("> ")); QCOMPARE(mapping.sourcePosition(3), 5);
    }
    void literalPunctuationRemainsEditable() {
        const QString source = "Text --*\n\n=\n\n==";
        MarkdownSourceMap mapping; mapping.rebuild(source, "Text --*\n=\n==");
        QCOMPARE(mapping.edited(5, 3, "literal", nullptr), QString("Text literal\n\n=\n\n=="));
        QCOMPARE(mapping.edited(9, 1, "+", nullptr), QString("Text --*\n\n+\n\n=="));
    }
    void semanticInlineDelimiters() {
        const auto syntax = MarkdownSyntax::scan("x_y_z **a** *b* ~~c~~ `d` and \\*literal\\*");
        QCOMPARE(syntax.formats.size(), 4);
        const QString source = "***bold*** **outer `code`**";
        const auto nested = MarkdownSyntax::scan(source); QCOMPARE(nested.formats.size(), 4);
        for (const auto &f : nested.formats) {
            QCOMPARE(source.mid(f.start, f.contentStart - f.start), f.opening);
            QCOMPARE(source.mid(f.contentEnd, f.end - f.contentEnd), f.closing);
            QVERIFY(f.contentStart < f.contentEnd);
        }
    }

    void markerCharactersHaveDistinctSourcePositions() {
        MarkdownSourceMap mapping; mapping.rebuild("`123` tail", "123 tail");
        QCOMPARE(mapping.markerBoundaryAt(0).sourceStart, 0); QCOMPARE(mapping.markerBoundaryAt(0).sourceEnd, 1);
        QCOMPARE(mapping.stepAcrossMarker(0, 0, true), 1); QCOMPARE(mapping.stepAcrossMarker(0, 1, true), -1);
        QCOMPARE(mapping.stepAcrossMarker(3, 4, true), 5); QCOMPARE(mapping.stepAcrossMarker(3, 5, false), 4);
        QCOMPARE(mapping.displayPosition(0), 0); QCOMPARE(mapping.displayPosition(1), 0); QCOMPARE(mapping.displayPosition(5), 3);
        mapping.rebuild("**bold** tail", "bold tail");
        QCOMPARE(mapping.stepAcrossMarker(0, 0, true), 1); QCOMPARE(mapping.stepAcrossMarker(0, 1, true), 2);
        QCOMPARE(mapping.displayPosition(1), 0); QCOMPARE(mapping.displayPosition(7), 4);
        mapping.rebuild("**one**<!-- gap -->*two*", "onetwo");
        const int opening = QString("**one**<!-- gap -->").size();
        QCOMPARE(mapping.stepAcrossMarker(3, 5, true), 6); QCOMPARE(mapping.stepAcrossMarker(3, 6, true), 7);
        QCOMPARE(mapping.stepAcrossMarker(3, 7, true), opening + 1);
        QCOMPARE(mapping.stepAcrossMarker(3, opening, false), 6);
    }
    void inlineSelectionEdgesRemainStable() {
        MarkdownSourceMap mapping; mapping.rebuild("before `code` after", "before code after");
        const auto format = mapping.inlineCodeAt(9); QVERIFY(format.valid());
        QCOMPARE(mapping.displayPosition(format.sourceStart), format.displayStart);
        QCOMPARE(mapping.displayPosition(format.contentStart), format.displayStart);
        QCOMPARE(mapping.displayPosition(format.contentEnd), format.displayEnd);
        QCOMPARE(mapping.displayPosition(format.sourceEnd), format.displayEnd);
        QCOMPARE(mapping.displayPosition(mapping.sourcePosition(7)), 7);
        QCOMPARE(mapping.displayPosition(mapping.sourcePosition(11)), 11);
        mapping.rebuild("```\n    code\n```", "    code");
        QCOMPARE(mapping.displayPosition(12), 8);
    }
    void fencedIndentSourceTransactions() {
        const QString source = "```cpp\none\n  two\n```\nTail";
        const int first = source.indexOf("one"), last = source.indexOf("two") + 3;
        auto edit = MarkdownCodeEditing::indent(source, last, first, false);
        QVERIFY(edit.handled); QCOMPARE(edit.source, QString("```cpp\n    one\n      two\n```\nTail"));
        QVERIFY(edit.anchor > edit.caret); edit = MarkdownCodeEditing::indent(edit.source, edit.anchor, edit.caret, true); QCOMPARE(edit.source, source);
        edit = MarkdownCodeEditing::indent(source, first + 2, first + 2, false); QCOMPARE(edit.source, QString("```cpp\non  e\n  two\n```\nTail"));
        const QString quote = "> ```cpp\n> one\n> two\n> ```";
        edit = MarkdownCodeEditing::indent(quote, quote.indexOf("one"), quote.indexOf("two") + 3, false);
        QCOMPARE(edit.source, QString("> ```cpp\n>     one\n>     two\n> ```"));
        const QString list = "- ```cpp\n  one\n  two\n  ```";
        edit = MarkdownCodeEditing::indent(list, list.indexOf("one"), list.indexOf("two") + 3, false);
        QCOMPARE(edit.source, QString("- ```cpp\n      one\n      two\n  ```"));
        const QString indented = "> ```cpp\n>     one\n> ```"; const int caret = indented.indexOf("one") + 3;
        edit = MarkdownCodeEditing::newline(indented, caret, caret); QCOMPARE(edit.source, QString("> ```cpp\n>     one\n>     \n> ```"));
        const QString emptyQuote = "> ```cpp\n> ```"; const int emptyStart = emptyQuote.indexOf('\n') + 1;
        edit = MarkdownCodeEditing::indent(emptyQuote, emptyStart, emptyStart, false); QCOMPARE(edit.source, QString("> ```cpp\n>     \n> ```"));
        edit = MarkdownCodeEditing::newline(emptyQuote, emptyStart, emptyStart); QCOMPARE(edit.source, QString("> ```cpp\n> \n> \n> ```"));
        QVERIFY(!MarkdownCodeEditing::indent(source, 0, first, false).handled);
        QVERIFY(!MarkdownCodeEditing::indent(source, first, source.size(), false).handled);
        QVERIFY(!MarkdownCodeEditing::indent(source, source.lastIndexOf("```"), source.lastIndexOf("```"), false).handled);
        edit = MarkdownCodeEditing::indent("```\n\tvalue\n```", 6, 6, true); QCOMPARE(edit.source, QString("```\nvalue\n```"));
    }
    void metadataImagesAreNotLoaded() {
        const QString source = "---\n# hidden\n![private](missing.png)\n---\n# Real\n![real](real.png)";
        const auto images = MarkdownImages::parse(source); QCOMPARE(images.size(), 1); QCOMPARE(images.first().source, QString("real.png"));
    }
    void emptyContainerEnter() {
        for (const QString source : {QString("- item\n- "), QString("1. item\n2. "), QString("- [x] item\n- [ ] "), QString("> quote\n>\n> ")}) {
            const auto edit = MarkdownSyntax::leaveEmptyContainer(source, source.size()); QVERIFY(edit.changed);
            QVERIFY(edit.source.endsWith("\n\n")); QCOMPARE(edit.caret, edit.source.size());
        }
        QVERIFY(!MarkdownSyntax::leaveEmptyContainer("```\n- \n```", 6).changed);
        QVERIFY(!MarkdownSyntax::leaveEmptyContainer("plain", 5).changed);
    }

    void importDoesNotReadLiteralImages() {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        const QString source = "> ```md\n> ![example](missing.png)\n> ````\n<!-- ![hidden](missing2.png) -->\n# Real\n";
        ImageStorage storage(QDir(temp.path()).filePath("md_data"));
        QString rewritten, error; QStringList created;
        QVERIFY2(storage.prepareImport(QDir(temp.path()).filePath("source.md"), "guide.md", source, rewritten, created, &error), qPrintable(error));
        QCOMPARE(rewritten, source); QVERIFY(created.isEmpty());
    }
    void inlineCommentDelimitersAreLiteral() {
        const QString source = "`<!--` ![real](real.png) `-->`\n<!-- ![hidden](missing.png) -->";
        const auto analysis = MarkdownAnalysis::scan(source);
        QCOMPARE(analysis.inlineCodes.size(), 2);
        const auto images = MarkdownImages::parse(source, analysis);
        QCOMPARE(images.size(), 1); QCOMPARE(images.front().source, QString("real.png"));
        QCOMPARE(analysis.opaque.size(), 1);
    }
    void exactInlineDelimiterRuns() {
        const QString source = "``a ` b`` and `c`";
        MarkdownSourceMap mapping; mapping.rebuild(source, "a ` b and c");
        const auto first = mapping.inlineCodeAt(2), second = mapping.inlineCodeAt(10);
        QVERIFY(first.valid()); QCOMPARE(first.sourceEnd, 9); QVERIFY(second.valid());
        QCOMPARE(mapping.edited(2, 0, "x", nullptr), QString("``a x` b`` and `c`"));
    }
    void inlineRangesFollowIncrementalEdits() {
        MarkdownSourceMap mapping; mapping.rebuild("`one` and `two`", "one and two");
        QCOMPARE(mapping.inlineCodeAt(9).sourceStart, 10);
        QCOMPARE(mapping.applyEdit(0, 0, "x", "x", nullptr), QString("`xone` and `two`"));
        QCOMPARE(mapping.inlineCodeAt(10).sourceStart, 11);
        QCOMPARE(mapping.applyEdit(1, 3, "", "", nullptr), QString("`x` and `two`"));
        QVERIFY(mapping.inlineCodeAt(0).valid()); QCOMPARE(mapping.inlineCodeAt(7).sourceStart, 8);
    }
    void batchImportScansOnceAndRetainsPartialSuccess() {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        const QString source = QDir(temp.path()).filePath("source"); QVERIFY(QDir().mkpath(QDir(source).filePath("group")));
        for (int i = 0; i < 30; ++i) QVERIFY(MarkdownFileIO::writeFile(QDir(source).filePath(QString("group/%1.md").arg(i)), "# Document\n"));
        MarkdownManager manager(QDir(temp.path()).filePath("md_data"));
        QSignalSpy changed(&manager, &MarkdownManager::documentsChanged);
        const auto result = manager.importDirectory(source);
        QVERIFY2(result.success(), qPrintable(result.error)); QCOMPARE(result.imported.size(), 30);
        QCOMPARE(manager.allNodes().size(), 30); QCOMPARE(changed.size(), 2);
        QVERIFY(MarkdownFileIO::writeFile(QDir(source).filePath("new.md"), "# New\n"));
        const auto again = manager.importDirectory(source);
        QVERIFY(!again.success()); QCOMPARE(again.failures.size(), 30); QCOMPARE(again.imported, QStringList{"new.md"});
        QVERIFY(manager.loadNode("new.md"));
    }
    void imageInsertionCapturesConfiguration() {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        MarkdownSettings settings; settings.absolutePhotoPath = temp.filePath("photos-one");
        const QString root = QDir(temp.path()).filePath("md_data"); QVERIFY(QDir().mkpath(root));
        ImageInsertJob job(root, "group/test.md", settings);
        QSignalSpy ready(&job, &ImageInsertJob::ready), failed(&job, &ImageInsertJob::failed);
        settings.absolutePhotoPath = temp.filePath("photos-two");
        QImage image(8, 8, QImage::Format_ARGB32); image.fill(Qt::blue); job.start(image);
        QCOMPARE(failed.size(), 0); QCOMPARE(ready.size(), 1);
        const QString file = MarkdownImages::localPath(ready[0][0].toString(), QDir(root).filePath("group/test.md"));
        QVERIFY(QFileInfo::exists(file)); QCOMPARE(QFileInfo(file).absolutePath(), temp.filePath("photos-one"));
        QVERIFY(QDir::isAbsolutePath(QUrl::fromPercentEncoding(ready[0][0].toString().toUtf8())));
    }
    void copyRelocatesImagesAndPreservesUnsavedContent() {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        const QString root = temp.filePath("md_data");
        MarkdownManager manager(root); QString error;
        QVERIFY(manager.scan(&error)); QVERIFY(manager.createDocument("source/a.md", "# Saved\n", &error));
        QImage image(8, 8, QImage::Format_ARGB32); image.fill(Qt::red);
        ImageStorage storage(root); QString reference;
        QVERIFY2(storage.storeImage("source/a.md", image, reference, &error), qPrintable(error));
        const QString content = "# Unsaved\n\n" + MarkdownImages::markup(reference);
        manager.updateContent("source/a.md", content);
        QVERIFY2(manager.copyDocument("source/a.md", "target/b.md", content, &error), qPrintable(error));
        auto *copy = manager.loadNode("target/b.md"); QVERIFY(copy); QCOMPARE(copy->title(), QString("Unsaved"));
        const auto images = MarkdownImages::parse(copy->content()); QCOMPARE(images.size(), 1);
        const QString file = MarkdownImages::localPath(images.front().source, copy->filePath());
        QVERIFY(QFileInfo::exists(file)); QCOMPARE(QFileInfo(file).absolutePath(), storage.photoRoot());
        QCOMPARE(images.front().source, reference);
        QVERIFY(manager.loadNode("source/a.md")->isModified()); QCOMPARE(manager.loadNode("source/a.md")->content(), content);
        const QString before = copy->content();
        QVERIFY(!manager.copyDocument("source/a.md", "target/b.md", "replacement", &error));
        QCOMPARE(manager.loadNode("target/b.md")->content(), before);
        QVERIFY(!manager.copyDocument("source/a.md", "../escape.md", content, &error));
        QVERIFY(!QFileInfo::exists(temp.filePath("escape.md")));
    }
};
QTEST_GUILESS_MAIN(MarkdownCoreTests)
#include "MarkdownCoreTests.moc"
