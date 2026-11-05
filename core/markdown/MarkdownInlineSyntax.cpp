#include "core/markdown/MarkdownInlineSyntax.h"
#include "core/markdown/MarkdownAnalysis.h"
#include "core/markdown/MarkdownReferences.h"
namespace {
bool escaped(const QString &source, int position) {
    int count = 0; while (position > 0 && source[--position] == '\\') ++count;
    return count % 2;
}
bool cjkPunctuation(QChar c) { return c.unicode() >= 0x2000 && c.isPunct(); }
}
QSet<int> MarkdownInlineSyntax::strongBoundaryPositions(const QString &source, const MarkdownAnalysis &analysis) {
    QSet<int> result;
    const auto literal = analysis.literalMask(source.size());
    for (const auto &line : analysis.lines) {
        if (line.code || line.html || line.comment) continue;
        int opening = -1; bool urlToken = false;
        for (int i = line.contentStart; i + 1 < line.end; ++i) {
            if (source[i].isSpace()) urlToken = false;
            else if (source.mid(i, 3) == "://" || source.mid(i, 4) == "www.") urlToken = true;
            if (source[i] != '*' || source[i + 1] != '*' || literal[i] || escaped(source, i)) continue;
            int end = i + 2; while (end < line.end && source[end] == '*') ++end;
            if (end != i + 2 || (i > line.start && source[i - 1] == '*')) { i = end - 1; continue; }
            if (opening < 0 && urlToken) { i = end - 1; continue; }
            if (opening >= 0 && i > opening + 2 && !source[i - 1].isSpace()) {
                if (end < line.end && cjkPunctuation(source[i - 1]) && source[end].isLetterOrNumber()) result.insert(end);
                if (opening > line.start && source[opening - 1].isLetterOrNumber() && cjkPunctuation(source[opening + 2])) result.insert(opening);
                opening = -1;
            } else if (end < line.end && !source[end].isSpace()) opening = i;
            i = end - 1;
        }
    }
    if (!result.isEmpty()) {
        for (const auto &reference : MarkdownReferences::parse(source))
            for (auto it = result.begin(); it != result.end();)
                if (*it >= reference.start && *it <= reference.start + reference.length) it = result.erase(it);
                else ++it;
    }
    return result;
}
