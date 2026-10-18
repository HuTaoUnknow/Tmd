#include "core/markdown/MarkdownParagraphEditing.h"
#include "core/markdown/MarkdownSyntax.h"
#include "core/markdown/MarkdownBlockSyntax.h"
#include "core/markdown/MarkdownAnalysis.h"
#include "core/markdown/MarkdownParser.h"
#include "core/markdown/MarkdownSpelling.h"
#include "core/markdown/MarkdownTableEditing.h"
#include <QRegularExpression>
#include <algorithm>
namespace {
int startOf(const QString &s, int p) { return p ? s.lastIndexOf('\n', p - 1) + 1 : 0; }
int endOf(const QString &s, int p) { const int end = s.indexOf('\n', p); return end < 0 ? int(s.size()) : end; }
QString quotes(const QString &line) {
    static const QRegularExpression prefix("^(?: {0,3}> )+"); return prefix.match(line).captured(0);
}
bool panelBoundary(const QString &source, int before, int after) {
    const auto analysis = MarkdownAnalysis::forRendering(source);
    for (const auto &line : analysis.lines)
        if ((line.start == after || line.end == before) && (line.code || line.fenceMarker)) return true;
    for (const auto &table : MarkdownTableEditing::tables(source))
        if (table.end == before || table.start == after) return true;
    return false;
}
MarkdownEdit backspaceAtHeadingMarker(const QString &source, int caret, bool separateElements) {
    MarkdownEdit edit{source, caret, caret};
    if (caret < 0 || caret >= source.size() || source[caret] != '#') return edit;
    const auto analysis = MarkdownAnalysis::forRendering(source);
    const auto markers = MarkdownParser::headingMarkers(source, analysis);
    if (std::none_of(markers.cbegin(), markers.cend(), [&](const auto &marker) {
        return marker.separated && marker.markerStart == caret;
    })) return edit;
    edit.handled = true;
    const int start = startOf(source, caret);
    // A nested heading belongs to its quote/list. Never consume its container
    // prefix or join it to another element through Qt's default Backspace.
    if (!start || !source.mid(start, caret - start).trimmed().isEmpty()) return edit;
    int previous = startOf(source, start - 1);
    if (separateElements && MarkdownBlockSyntax::separatorLines(source, &analysis).contains(previous))
        previous = startOf(source, previous - 1);
    const int previousEnd = endOf(source, previous);
    const QString previousText = source.mid(previous, previousEnd - previous);
    if (previousText.isEmpty() || previousText == "\r") {
        // One visible empty row moves the whole heading up by one row.
        const int from = start - (start >= 2 && source[start - 2] == '\r' ? 2 : 1);
        edit.source.remove(from, start - from); edit.anchor = edit.caret = caret - (start - from);
    } else {
        const auto syntax = MarkdownSyntax::scan(source, &analysis);
        for (const auto &line : analysis.lines) if (line.start == previous
            && (line.code || line.fenceMarker || line.html || line.comment || line.listMarkerStart >= 0
                || !source.mid(line.start, line.contentStart - line.start).trimmed().isEmpty())) return edit;
        if (syntax.containerParagraphLines.contains(previous) || syntax.underlineLines.contains(previous)) return edit;
        for (const auto &heading : syntax.headings) if (heading.position == previous) return edit;
        for (const auto &table : MarkdownTableEditing::tables(source))
            if (previous >= table.start && previous <= table.end) return edit;
        for (const auto &region : MarkdownSpelling::regions(source))
            if (region.start >= previous && region.start < previousEnd
                && (region.kind == MarkdownSpelling::Kind::Rule || region.kind == MarkdownSpelling::Kind::Underline
                    || region.kind == MarkdownSpelling::Kind::Definition || region.kind == MarkdownSpelling::Kind::Image)) return edit;
        // Joining to a plain paragraph naturally makes the hashes literal.
        // Preserve its trailing spaces and the heading's indentation/content.
        const int from = previousEnd - (previousEnd > previous && source[previousEnd - 1] == '\r' ? 1 : 0);
        edit.source.remove(from, start - from); edit.anchor = edit.caret = caret - (start - from);
    }
    edit.changed = true; return edit;
}
}
MarkdownEdit MarkdownParagraphEditing::newline(const QString &source, int anchor, int caret, bool separateElements) {
    MarkdownEdit edit{source, anchor, caret};
    const int begin = qMin(anchor, caret), finish = qMax(anchor, caret), start = startOf(source, begin), end = endOf(source, finish);
    const auto syntax = MarkdownSyntax::scan(source);
    const auto separators = separateElements ? MarkdownBlockSyntax::separatorLines(source) : QSet<int>();
    const QString line = source.mid(start, end - start), quote = quotes(line);
    const bool heading = std::any_of(syntax.headings.cbegin(), syntax.headings.cend(), [&](const auto &h) { return h.position == start; });
    if (heading && syntax.underlineLines.contains(end + 1)) {
        const int underlineEnd = endOf(source, end + 1);
        const QString underline = source.mid(end + 1, underlineEnd - end - 1);
        const QString left = source.mid(start, begin - start), right = source.mid(finish, end - finish);
        const QString separator = separateElements ? "\n\n" : "\n";
        const int reuse = separateElements && right.isEmpty() && separators.contains(underlineEnd + 1) ? 1 : 0;
        edit.source.replace(start, underlineEnd - start + reuse, left + '\n' + underline + separator + right);
        edit.anchor = edit.caret = start + left.size() + underline.size() + 1 + separator.size();
    } else if (!quote.isEmpty() && line.mid(quote.size()).isEmpty() && !heading && anchor == caret) {
        const int last = quote.lastIndexOf('>');
        int from = start;
        if (start > 0) {
            const int previous = startOf(source, start - 1);
            if (source.mid(previous, start - previous - 1) == quote) from = previous;
        }
        const auto remaining = quote.left(last);
        edit.source.replace(from, end - from, remaining); edit.anchor = edit.caret = from + remaining.size();
        if (from > 0 && quotes(source.mid(startOf(source, from - 1), from - startOf(source, from - 1) - 1)).count('>') > remaining.count('>')) {
            // On a document exit, share the existing outside separator. Its
            // replacement body row still starts after the hidden first blank.
            const bool share = separateElements && remaining.isEmpty() && source.mid(end, 2) == "\n\n";
            if (!share) edit.source.insert(from, remaining + '\n');
            edit.anchor += remaining.size() + 1; edit.caret = edit.anchor;
        }
    } else {
        QString tail = source.mid(finish, end - finish), suffix;
        if (heading) for (const auto &marker : MarkdownBlockSyntax::markers(source)) {
            if (!marker.prefix && marker.lineStart == start && marker.start >= finish) {
                suffix = source.mid(marker.start, marker.end - marker.start); tail.chop(suffix.size()); break;
            }
        }
        const QString prefix = heading ? QString() : quote;
        const bool leftHasContent = heading || !source.mid(start, begin - start).trimmed().isEmpty();
        const QString separator = prefix.isEmpty() ? QString(separateElements && leftHasContent ? "\n\n" : "\n") : '\n' + prefix + '\n';
        // Share the old separator when splitting at a paragraph's end. Adding
        // another whole separator there would turn one Enter into two blanks.
        int reuse = 0;
        if (separateElements && tail.isEmpty() && separators.contains(end + 1)) {
            if (prefix.isEmpty()) reuse = 1;
            else if (source.mid(end + 1, prefix.size()) == prefix) reuse = 1 + prefix.size();
        }
        edit.source.replace(begin, end - begin + reuse, suffix + separator + prefix + tail);
        edit.anchor = edit.caret = begin + suffix.size() + separator.size() + prefix.size();
    }
    edit.handled = true; edit.changed = edit.source != source; return edit;
}
MarkdownEdit MarkdownParagraphEditing::backspace(const QString &source, int caret, bool separateElements) {
    const auto headingStart = backspaceAtHeadingMarker(source, caret, separateElements);
    if (headingStart.handled) return headingStart;
    MarkdownEdit edit{source, caret, caret}; const int start = startOf(source, caret), end = endOf(source, caret);
    const auto syntax = MarkdownSyntax::scan(source);
    const QString quote = quotes(source.mid(start, end - start));
    if (syntax.underlineLines.contains(start) && caret > start) {
        edit.source.remove(caret - 1, 1); edit.anchor = edit.caret = caret - 1;
    } else if (caret == start + quote.size() && syntax.underlineLines.contains(end + 1)) {
        const int last = endOf(source, end + 1); edit.source.remove(end, last - end);
    } else if (!quote.isEmpty() && caret == start + quote.size()) {
        const int last = quote.lastIndexOf('>'); edit.source.remove(start + last, quote.size() - last);
        edit.anchor = edit.caret = start + last;
        const auto remaining = quote.left(last);
        if (start > 0 && quotes(source.mid(startOf(source, start - 1), start - startOf(source, start - 1) - 1)).count('>') > remaining.count('>')) {
            edit.source.insert(start, remaining + '\n'); edit.anchor += remaining.size() + 1; edit.caret = edit.anchor;
        }
    } else if (caret == start && start > 0) {
        const int previous = startOf(source, start - 1);
        if (separateElements && MarkdownBlockSyntax::separatorLines(source).contains(previous)) {
            const int prior = startOf(source, previous - 1), priorEnd = previous - 1;
            if (panelBoundary(source, priorEnd, start)) { edit.handled = true; return edit; }
            static const QRegularExpression rule("^ {0,3}(?:(?:- *){3,}|(?:\\* *){3,}|(?:_ *){3,})\\r?$");
            const bool removeRule = end == start && rule.match(source.mid(prior, priorEnd - prior)).hasMatch();
            if (syntax.underlineLines.contains(prior)) {
                const int titleEnd = prior - 1;
                const QString underline = source.mid(prior, priorEnd - prior);
                edit.source.replace(titleEnd, end - titleEnd, source.mid(start, end - start) + '\n' + underline);
                edit.anchor = edit.caret = titleEnd; edit.handled = edit.changed = true; return edit;
            }
            const int from = removeRule ? prior : priorEnd;
            edit.source.remove(from, start - from); edit.anchor = edit.caret = from;
            edit.handled = edit.changed = true; return edit;
        }
        if (source.mid(previous, start - previous - 1).isEmpty() && previous > 0) {
            const int prior = startOf(source, previous - 1);
            for (const auto &line : MarkdownAnalysis::scan(source).lines) if (line.start == prior && line.listMarkerStart >= 0) {
                edit.source.remove(line.end, start - line.end); edit.anchor = edit.caret = line.end; edit.handled = edit.changed = true; return edit;
            }
        }
        static const QRegularExpression rule("^ {0,3}(?:(?:- *){3,}|(?:\\* *){3,}|(?:_ *){3,})\\r?$");
        if (end != start || !rule.match(source.mid(previous, start - previous - 1)).hasMatch()) return edit;
        edit.source.remove(previous, start - previous); edit.anchor = edit.caret = previous;
    } else return edit;
    edit.handled = edit.changed = true; return edit;
}
MarkdownEdit MarkdownParagraphEditing::deleteForward(const QString &source, int caret) {
    MarkdownEdit edit{source, caret, caret};
    if (caret != endOf(source, caret) || caret >= source.size()) return edit;
    const auto syntax = MarkdownSyntax::scan(source);
    const bool setext = syntax.underlineLines.contains(caret + 1);
    const int before = setext ? endOf(source, caret + 1) : caret;
    if (!MarkdownBlockSyntax::separatorLines(source).contains(before + 1)) return edit;
    const int after = endOf(source, before + 1) + 1;
    edit.handled = true;
    if (panelBoundary(source, before, after)) return edit;
    if (setext) {
        const int tailEnd = endOf(source, after);
        const QString underline = source.mid(caret + 1, before - caret - 1);
        edit.source.replace(caret, tailEnd - caret, source.mid(after, tailEnd - after) + '\n' + underline);
    } else edit.source.remove(caret, after - caret);
    edit.changed = true; return edit;
}
MarkdownEdit MarkdownParagraphEditing::insertParagraph(const QString &source, int caret, const QString &text) {
    MarkdownEdit edit{source, caret, caret};
    if (text.isEmpty() || text.contains('\n') || caret >= source.size() || caret != startOf(source, caret)
        || source[caret] != '\n') return edit;
    const auto analysis = MarkdownAnalysis::forRendering(source);
    for (const auto &line : analysis.lines) if (line.start == caret && (line.code || line.fenceMarker || line.html || line.comment)) return edit;
    edit.source.insert(caret, text + '\n');
    edit.anchor = edit.caret = caret + text.size(); edit.handled = edit.changed = true; return edit;
}
MarkdownEdit MarkdownParagraphEditing::completeUnderline(const QString &source, int caret) {
    MarkdownEdit edit{source, caret, caret};
    const int start = startOf(source, caret), end = endOf(source, caret);
    static const QRegularExpression underline("^ {0,3}(?:={3,}|-{3,})[ \\t]*$");
    if (!start || caret != end || !underline.match(source.mid(start, end - start)).hasMatch()) return edit;
    const int previous = startOf(source, start - 1);
    if (!MarkdownBlockSyntax::separatorLines(source).contains(previous)) return edit;
    const int prior = startOf(source, previous - 1);
    const auto analysis = MarkdownAnalysis::forRendering(source);
    for (const auto &line : analysis.lines) if (line.start == prior
        && (line.code || line.fenceMarker || line.listMarkerStart >= 0 || line.contentStart != line.start)) return edit;
    for (const auto &heading : MarkdownSyntax::scan(source, &analysis).headings) if (heading.position == prior) return edit;
    for (const auto &table : MarkdownTableEditing::tables(source)) if (table.end == previous - 1) return edit;
    edit.source.remove(previous, start - previous); edit.anchor = edit.caret = caret - (start - previous);
    edit.handled = edit.changed = true; return edit;
}
