#include "core/markdown/MarkdownParser.h"
#include <QRegularExpression>
#include "core/markdown/MarkdownSyntax.h"

QList<MarkdownParser::CodeFence> MarkdownParser::codeFences(const QString &markdown) {
    return MarkdownAnalysis::scan(markdown).fences;
}
QList<MarkdownParser::HeadingMarker> MarkdownParser::headingMarkers(const QString &markdown) {
    return headingMarkers(markdown, MarkdownAnalysis::scan(markdown));
}
QList<MarkdownParser::HeadingMarker> MarkdownParser::headingMarkers(const QString &markdown, const MarkdownAnalysis &analysis) {
    static const QRegularExpression marker("^(#{1,6})(?:([ \\t]+)(.*)|)$");
    QList<HeadingMarker> result;
    for (const auto &line : analysis.lines) {
        if (line.code || line.html || line.comment) continue;
        const auto match = marker.match(markdown.mid(line.contentStart, line.end - line.contentStart));
        if (match.hasMatch()) result.append({line.contentStart,
            line.contentStart + int(match.captured(1).size() + match.captured(2).size()), line.end,
            int(match.captured(1).size()), match.captured(2).startsWith(' ')});
    }
    return result;
}
QString MarkdownParser::plainInlineText(QString text) {
    static const QRegularExpression link(QStringLiteral("!?\\[([^\\]]*)\\]\\([^)]*\\)"));
    text.replace(link, QStringLiteral("\\1"));
    text.remove(QRegularExpression(QStringLiteral("[*_`~]")));
    return text.trimmed();
}
MarkdownOutline MarkdownParser::outline(const QString &markdown) {
    return outline(markdown, MarkdownAnalysis::scan(markdown));
}
MarkdownOutline MarkdownParser::outline(const QString &markdown, const MarkdownAnalysis &analysis) {
    return MarkdownSyntax::scan(markdown, &analysis).headings;
}
