#include "core/markdown/MarkdownAnalysis.h"
#include <QRegularExpression>
#include <algorithm>

namespace {
bool escaped(const QString &source, int position) {
    int backslashes = 0;
    while (position > 0 && source[--position] == '\\') ++backslashes;
    return backslashes % 2;
}
}

MarkdownAnalysis MarkdownAnalysis::scan(const QString &source) {
    static const QRegularExpression prefix("^((?: {0,3}(?:>[ \\t]?|(?:[-+*]|[0-9]+[.)])[ \\t]+))* {0,3})");
    static const QRegularExpression quotes("^(?: {0,3}>[ \\t]?)*");
    static const QRegularExpression list("[-+*]|[0-9]+[.)]");
    static const QRegularExpression fence("^(`{3,}|~{3,})(.*)$");
    static const QRegularExpression rawHtml("^<(pre|script|style|textarea)(?=[ \\t>])", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression htmlTag("^</?(?:address|article|aside|base|basefont|blockquote|body|caption|center|col|colgroup|dd|details|dialog|dir|div|dl|dt|fieldset|figcaption|figure|footer|form|frame|frameset|h[1-6]|head|header|hr|html|iframe|legend|li|link|main|menu|menuitem|nav|noframes|ol|optgroup|option|p|param|search|section|summary|table|tbody|td|tfoot|th|thead|title|tr|track|ul)(?=[ \\t>/])", QRegularExpression::CaseInsensitiveOption);
    MarkdownAnalysis result;
    int active = -1, fenceQuotes = 0, listIndent = 0, fenceIndent = 0;
    QString htmlEnd;
    bool htmlUntilBlank = false, htmlOpaque = false;
    int number = 0;
    for (int start = 0; start < source.size();) {
        int end = source.indexOf('\n', start);
        if (end < 0) end = source.size();
        const QString line = source.mid(start, end - start);
        const QString container = prefix.match(line).captured(1);
        const int quoteEnd = quotes.match(line).capturedLength();
        const int quoteDepth = container.count('>');
        int indent = 0;
        while (quoteEnd + indent < line.size() && line[quoteEnd + indent] == ' ') ++indent;
        const QString listPrefix = container.mid(quoteEnd);
        const bool listMarker = list.match(listPrefix).hasMatch();
        const QString content = line.mid(container.size());
        Line classified{start, end, start + int(container.size()), number++};

        if (active >= 0 && (quoteDepth < fenceQuotes || (listIndent && indent < listIndent && !content.trimmed().isEmpty()))) {
            result.fences[active].end = start;
            active = -1;
        }
        const auto fm = fence.match(content);
        if (active >= 0) {
            classified.code = true;
            const auto &opening = result.fences[active];
            if (!listMarker && quoteDepth == fenceQuotes && fm.hasMatch()
                && fm.captured(1)[0] == opening.delimiter[0]
                && fm.captured(1).size() >= opening.delimiter.size() && fm.captured(2).trimmed().isEmpty()) {
                classified.fenceMarker = true;
                result.fences[active].closeStart = start;
                result.fences[active].end = end;
                active = -1;
            } else {
                classified.contentStart = start + quoteEnd + qMin(indent, listIndent + fenceIndent);
            }
        } else if (!htmlEnd.isEmpty()) {
            classified.html = true;
            if (htmlOpaque) result.opaque.append({start, end});
            if (line.contains(htmlEnd, Qt::CaseInsensitive)) htmlEnd.clear();
        } else if (htmlUntilBlank && !content.trimmed().isEmpty()) {
            classified.html = true;
        } else if (fm.hasMatch() && !(fm.captured(1)[0] == '`' && fm.captured(2).contains('`'))
            && indent < 4) {
            htmlUntilBlank = false;
            classified.code = classified.fenceMarker = true;
            result.fences.append({start, end, qMin(end + 1, int(source.size())), -1, int(source.size()), fm.captured(1), container});
            active = result.fences.size() - 1;
            fenceQuotes = quoteDepth;
            listIndent = listMarker ? listPrefix.size() : 0;
            fenceIndent = listMarker ? 0 : indent;
        } else {
            htmlUntilBlank = false;
            if (const auto tag = rawHtml.match(content); tag.hasMatch()) {
                htmlEnd = "</" + tag.captured(1) + ">";
                classified.html = htmlOpaque = true;
                result.opaque.append({start, end});
            } else if (content.startsWith("<!--")) {
                htmlEnd = "-->";
                classified.html = true;
                htmlOpaque = false; // Comment ranges are collected precisely below.
            } else if (htmlTag.match(content).hasMatch()) {
                classified.html = htmlUntilBlank = true;
            } else if (!listMarker && indent >= 4) {
                classified.code = true;
                classified.contentStart = start + quoteEnd + 4;
            } else if (line.startsWith('\t')) {
                classified.code = true;
                classified.contentStart = start + 1;
            }
            if (!htmlEnd.isEmpty() && content.contains(htmlEnd, Qt::CaseInsensitive)) htmlEnd.clear();
        }
        result.lines.append(classified);
        start = end + 1;
    }
    // Scan comments and inline code in source order: comment-looking text inside
    // a code span is literal, while backticks inside a comment have no meaning.
    auto mask = result.literalMask(source.size());
    for (int i = 0; i < source.size(); ++i) {
        if (mask[i]) continue;
        if (source.mid(i, 4) == "<!--") {
            const int close = source.indexOf("-->", i + 4);
            const int end = close < 0 ? source.size() : close + 3;
            result.opaque.append({i, end});
            result.comments.append({i, end});
            std::fill(mask.begin() + i, mask.begin() + end, 1);
            i = end - 1;
            continue;
        }
        if (source[i] != '`' || escaped(source, i)) continue;
        int count = 1;
        while (i + count < source.size() && source[i + count] == '`') ++count;
        for (int end = i + count; end < source.size();) {
            if (mask[end]) break;
            if (source[end] != '`') { ++end; continue; }
            int run = 1;
            while (end + run < source.size() && source[end + run] == '`') ++run;
            if (run == count) {
                result.inlineCodes.append({i, end + run, i + count, end});
                i = end + run - 1;
                break;
            }
            end += run;
        }
    }
    for (auto &line : result.lines)
        for (const auto &range : result.comments)
            if (line.contentStart >= range.start && line.contentStart < range.end) { line.comment = true; break; }
    return result;
}

QVector<char> MarkdownAnalysis::literalMask(int sourceLength) const {
    QVector<char> mask(sourceLength, 0);
    auto fill = [&](int start, int end) {
        std::fill(mask.begin() + qBound(0, start, sourceLength), mask.begin() + qBound(0, end, sourceLength), 1);
    };
    for (const auto &line : lines) if (line.code) fill(line.start, line.end);
    for (const auto &range : opaque) fill(range.start, range.end);
    for (const auto &range : inlineCodes) fill(range.start, range.end);
    return mask;
}
