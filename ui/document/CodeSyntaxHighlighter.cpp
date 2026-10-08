#include "ui/document/CodeSyntaxHighlighter.h"
#include "core/settings/MarkdownSettings.h"
#include <QRegularExpression>
#include <QTextBlock>
void CodeSyntaxHighlighter::highlightBlock(const QString &text) {
    if (!MarkdownSettingsStore::current().enabled(MarkdownSettings::SyntaxColors)) return;
    if (!currentBlock().blockFormat().nonBreakableLines() && !currentBlock().blockFormat().hasProperty(QTextFormat::BlockCodeFence)) return;
    static const QList<QPair<QRegularExpression, QColor>> rules = {
        {QRegularExpression("\\b(?:class|struct|public|private|protected|return|if|else|for|while|void|int|bool|const|auto|static|include|def|import|from|as|function|let|var|true|false|null|None|True|False|SELECT|FROM|WHERE|JOIN|INSERT|UPDATE|DELETE|CREATE|TABLE|AND|OR|ORDER|BY|GROUP|LIMIT)\\b"), QColor("#b8a6e0")},
        {QRegularExpression("\\b[0-9]+(?:\\.[0-9]+)?\\b"), QColor("#e5bb87")},
        {QRegularExpression("(?:\"(?:\\\\.|[^\"\\\\])*\"|'(?:\\\\.|[^'\\\\])*')"), QColor("#a7cca1")},
        {QRegularExpression("(?://.*$|^\\s*#.*$|-- .*$)"), QColor("#899d96")}
    };
    for (const auto &rule : rules) {
        auto matches = rule.first.globalMatch(text);
        while (matches.hasNext()) { const auto match = matches.next(); setFormat(match.capturedStart(), match.capturedLength(), rule.second); }
    }
}
