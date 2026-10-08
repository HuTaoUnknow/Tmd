#include "core/markdown/MarkdownParser.h"
#include <QRegularExpression>

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
            int(match.captured(1).size()), !match.captured(2).isEmpty()});
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
    static const QRegularExpression atx("^(#{1,6})[ \\t]+(.*?)[ \\t]*$");
    static const QRegularExpression closingHashes("[ \\t]+#+[ \\t]*$");
    static const QRegularExpression setext("^(=+|-+)[ \\t]*$");
    MarkdownOutline result;
    QString previousText;
    int previousPosition = 0, previousLine = 0;
    bool previousParagraph = false;
    for (const auto &line : analysis.lines) {
        const QString content = markdown.mid(line.contentStart, line.end - line.contentStart);
        if (line.code || line.html || line.comment) previousParagraph = false;
        else if (const auto match = atx.match(content); match.hasMatch()) {
            QString text = match.captured(2); text.remove(closingHashes);
            result.append({int(match.captured(1).size()), line.number, line.start, plainInlineText(text)});
            previousParagraph = false;
        } else if (const auto match = setext.match(content); match.hasMatch() && previousParagraph) {
            result.append({match.captured(1).front() == '=' ? 1 : 2, previousLine, previousPosition, plainInlineText(previousText)});
            previousParagraph = false;
        } else previousParagraph = !content.trimmed().isEmpty() && !content.startsWith('#')
            && !content.startsWith('-') && !content.startsWith('*');
        previousText = content; previousPosition = line.start; previousLine = line.number;
    }
    return result;
}
