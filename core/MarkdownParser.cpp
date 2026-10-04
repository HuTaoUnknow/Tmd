#include "MarkdownParser.h"
#include <QRegularExpression>

QString MarkdownParser::plainInlineText(QString text) {
    static const QRegularExpression link(QStringLiteral("!?\\[([^\\]]*)\\]\\([^)]*\\)"));
    text.replace(link, QStringLiteral("\\1"));
    text.remove(QRegularExpression(QStringLiteral("[*_`~]")));
    return text.trimmed();
}
MarkdownOutline MarkdownParser::outline(const QString &markdown) {
    static const QRegularExpression fence(QStringLiteral("^ {0,3}(`{3,}|~{3,})(.*)$"));
    static const QRegularExpression atx(QStringLiteral("^ {0,3}(#{1,6})(?:[ \\t]+(.*?)|)[ \\t]*$"));
    static const QRegularExpression closingHashes(QStringLiteral("[ \\t]+#+[ \\t]*$"));
    static const QRegularExpression setext(QStringLiteral("^ {0,3}(=+|-+)[ \\t]*$"));
    const QStringList lines = markdown.split('\n');
    MarkdownOutline result;
    QChar fenceChar;
    int fenceLength = 0, position = 0, previousPosition = 0;
    bool previousParagraph = false;
    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];
        const auto fm = fence.match(line);
        if (!fenceChar.isNull()) {
            if (fm.hasMatch() && fm.captured(1).front() == fenceChar
                && fm.captured(1).size() >= fenceLength && fm.captured(2).trimmed().isEmpty()) fenceChar = {};
            previousParagraph = false;
        } else if (fm.hasMatch() && !(fm.captured(1).front() == '`' && fm.captured(2).contains('`'))) {
            fenceChar = fm.captured(1).front();
            fenceLength = fm.captured(1).size();
            previousParagraph = false;
        } else if (const auto hm = atx.match(line); hm.hasMatch()) {
            QString text = hm.captured(2);
            text.remove(closingHashes);
            result.append({static_cast<int>(hm.captured(1).size()), i, position, plainInlineText(text)});
            previousParagraph = false;
        } else if (const auto sm = setext.match(line); sm.hasMatch() && previousParagraph) {
            result.append({sm.captured(1).front() == '=' ? 1 : 2, i - 1, previousPosition, plainInlineText(lines[i - 1])});
            previousParagraph = false;
        } else {
            previousParagraph = !line.trimmed().isEmpty() && !line.startsWith("    ") && !line.startsWith('\t')
                && !line.trimmed().startsWith('>') && !line.trimmed().startsWith('-') && !line.trimmed().startsWith('*');
        }
        previousPosition = position;
        position += static_cast<int>(line.size()) + 1;
    }
    return result;
}
