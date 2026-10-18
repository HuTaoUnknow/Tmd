#include "core/markdown/MarkdownBlockSyntax.h"
#include "core/markdown/MarkdownAnalysis.h"
#include "core/markdown/MarkdownListEditing.h"
#include "core/markdown/MarkdownSyntax.h"
#include <QRegularExpression>
#include <QSet>
#include <QMap>
#include <algorithm>

namespace {
int skipSpaces(const QString &s, int p, int end) { while (p < end && s[p] == ' ') ++p; return p; }
bool taskAt(const QString &s, int p, int end) {
    return p + 3 < end && s[p] == '[' && QString(" xX").contains(s[p + 1]) && s[p + 2] == ']' && s[p + 3] == ' ';
}
int markerEnd(const QString &s, int p, int end) {
    if (p >= end) return p;
    if (QString("-+*>").contains(s[p])) return p + 1;
    if (s[p] == '#') { int q = p; while (q < end && s[q] == '#') ++q; return q - p <= 6 ? q : p; }
    int q = p; while (q < end && s[q] >= '0' && s[q] <= '9') ++q;
    return q > p && q - p <= 9 && q < end && QString(".)").contains(s[q]) ? q + 1 : p;
}
bool inactiveTask(const QString &s, int start, int afterMarker, int end) {
    if (!QString("-+*").contains(s[start]) && !s[start].isDigit()) return false;
    const int checkbox = skipSpaces(s, afterMarker, end);
    return checkbox > afterMarker && checkbox + 3 == end && s[checkbox] == '['
        && QString(" xX").contains(s[checkbox + 1]) && s[checkbox + 2] == ']';
}
}
MarkdownBlockSyntax::Prepared MarkdownBlockSyntax::prepare(const QString &source, const MarkdownAnalysis *analysis) {
    const auto scanned = analysis ? MarkdownAnalysis() : MarkdownAnalysis::forRendering(source);
    const auto &lines = analysis ? analysis->lines : scanned.lines;
    const auto literal = (analysis ? *analysis : scanned).literalMask(source.size());
    QSet<int> escapes;
    QMap<int, QString> separators;
    int parentIndent = -1, blankLines = 0;
    for (const auto &range : (analysis ? *analysis : scanned).literalFences)
        for (int i = range.start; i < range.end; ++i) escapes.insert(i);
    for (const auto &line : lines) {
        if (line.listMarkerStart >= 0) {
            const int indent = line.listMarkerStart - line.start;
            if (parentIndent >= 0 && blankLines == 0 && indent > parentIndent && source[line.listMarkerStart].isDigit()) {
                static const QRegularExpression quotes("^(?: {0,3}> +)*");
                separators.insert(line.start, quotes.match(source.mid(line.start, indent)).captured(0) + '\n');
            }
            parentIndent = indent; blankLines = 0;
        }
        else if (source.mid(line.start, line.end - line.start).trimmed().isEmpty()) {
            if (++blankLines > 1) parentIndent = -1;
        } else {
            int p = skipSpaces(source, line.start, line.end);
            const int end = markerEnd(source, p, line.end);
            const bool fenced = std::any_of((analysis ? *analysis : scanned).fences.cbegin(), (analysis ? *analysis : scanned).fences.cend(),
                [&](const auto &fence) { return line.start >= fence.start && line.start < fence.end; });
            if (parentIndent >= 0 && p - line.start >= parentIndent + 2 && end > p && end == line.end && !fenced && !line.html && !line.comment)
                escapes.insert(source[p].isDigit() ? end - 1 : p);
            if (line.fenceMarker || p - line.start <= parentIndent) parentIndent = -1;
            blankLines = 0;
        }
        if (line.code || line.html || line.comment) continue;
        int p = line.start, indentColumns = 0;
        while (p < line.end && (source[p] == ' ' || source[p] == '\t')) {
            indentColumns += source[p] == '\t' ? 4 - indentColumns % 4 : 1; ++p;
        }
        if (indentColumns > 3 && parentIndent < 0 && line.listMarkerStart != p) {
            const int end = markerEnd(source, p, line.end);
            if (end > p && (end == line.end || source[end] == ' ')) escapes.insert(source[p].isDigit() ? end - 1 : p);
            static const QRegularExpression rule("^(?:(?:- *){3,}|(?:\\* *){3,}|(?:_ *){3,})\\r?$");
            if (rule.match(source.mid(p, line.end - p)).hasMatch()) escapes.insert(p);
            if (p < line.end && QString("`~").contains(source[p])) {
                int after = p; while (after < line.end && source[after] == source[p]) ++after;
                if (after - p >= 3) for (int marker = p; marker < after; ++marker) escapes.insert(marker);
            }
            continue;
        }
        while (p < line.end && !literal[p]) {
            const int end = markerEnd(source, p, line.end); if (end == p) break;
            if (end < line.end && source[end] == ' ') {
                if (inactiveTask(source, p, end, line.end)) {
                    escapes.insert(source[p].isDigit() ? end - 1 : p); break;
                }
                const QChar marker = source[p]; p = skipSpaces(source, end, line.end);
                if (marker == '#' || p == line.end) break;
                // Checkbox spelling is content unless followed by a space.
                if (taskAt(source, p, line.end))
                    p = skipSpaces(source, p + 3, line.end);
                continue;
            }
            const QChar marker = source[p];
            const bool empty = end == line.end || (end + 1 == line.end && source[end] == '\r');
            if (marker == '#' || marker == '>' || empty || (end < line.end && source[end] == '\t'))
                escapes.insert(marker.isDigit() ? end - 1 : p);
            break;
        }
    }
    // Physical body lines are editing paragraphs. Only grammar-owned rows
    // (fences, tables, Setext and container continuations) stay joined.
    QSet<int> tableLines;
    static const QRegularExpression divider("^\\s*\\|?\\s*:?-+:?\\s*(?:\\|\\s*:?-+:?\\s*)*\\|?\\s*$");
    static const QRegularExpression underline("^ {0,3}(?:={3,}|-{3,}) *\\r?$");
    static const QRegularExpression shortUnderline("^ {0,3}(?:={1,2}|-{1,2}) *\\r?$");
    static const QRegularExpression quote("^(?: {0,3}> )+");
    static const QRegularExpression tableBoundary("^ {0,3}(?:#{1,6} |>(?: |$)|(?:(?:- *){3,}|(?:\\* *){3,}|(?:_ *){3,})$)");
    for (int i = 1; i < lines.size(); ++i) {
        const auto &line = lines[i]; const auto text = source.mid(line.contentStart, line.end - line.contentStart);
        if (!line.code && text.contains('|') && divider.match(text).hasMatch() && source.mid(lines[i - 1].start, lines[i - 1].end - lines[i - 1].start).contains('|')) {
            tableLines.insert(i - 1); tableLines.insert(i);
            for (int row = i + 1; row < lines.size() && !lines[row].code; ++row) {
                const auto text = source.mid(lines[row].start, lines[row].end - lines[row].start);
                if (text.trimmed().isEmpty() || lines[row].listMarkerStart >= 0 || lines[row].html || lines[row].comment || tableBoundary.match(text).hasMatch()) break;
                tableLines.insert(row);
            }
        }
    }
    const int metadata = MarkdownSyntax::frontMatterEnd(source);
    for (int i = 0; i < lines.size(); ++i) {
        const auto &line = lines[i]; if (line.code || line.start < metadata) continue;
        const auto text = source.mid(line.start, line.end - line.start);
        int base = quote.match(text).capturedLength();
        int blanks = 0;
        const auto candidate = text.mid(base).trimmed();
        const bool possibleUnderline = !candidate.isEmpty() && (candidate == QString(candidate.size(), '-') || candidate == QString(candidate.size(), '='));
        for (int previous = possibleUnderline ? i - 1 : -1; previous >= 0; --previous) {
            const auto &parent = lines[previous];
            const auto row = source.mid(parent.start, parent.end - parent.start);
            if (row.trimmed().isEmpty()) { if (++blanks >= 2) break; continue; }
            if (parent.listMarkerStart >= 0) { base = qMax(base, markerEnd(source, parent.listMarkerStart, parent.end) + 1 - parent.start); break; }
            if (parent.code || row.mid(quote.match(row).capturedLength()).isEmpty()) break;
            int indent = quote.match(row).capturedLength(); while (indent < row.size() && row[indent] == ' ') ++indent;
            if (indent <= quote.match(row).capturedLength()) break;
        }
        const auto body = text.mid(qMin(base, int(text.size())));
        const bool setext = !tableLines.contains(i - 1) && line.listMarkerStart < 0 && underline.match(body).hasMatch();
        if (line.html || line.comment) for (int p = line.contentStart; p < line.end; ++p)
            if (source[p].isPunct() || source[p] == '&') escapes.insert(p);
        if (line.listMarkerStart < 0 && shortUnderline.match(body).hasMatch() && !body.trimmed().isEmpty()) escapes.insert(line.start + base + body.indexOf(body.trimmed()[0]));
        if (setext && i >= 2 && !source.mid(lines[i - 2].start, lines[i - 2].end - lines[i - 2].start).trimmed().isEmpty()) {
            const auto &heading = lines[i - 1];
            if (!heading.code) separators.insert(heading.start, quote.match(source.mid(heading.start, heading.end - heading.start)).captured(0) + '\n');
        }
        if (!i || tableLines.contains(i) || setext || text.trimmed().isEmpty()) continue;
        const auto &previous = lines[i - 1];
        if (previous.code || source.mid(previous.start, previous.end - previous.start).trimmed().isEmpty()) continue;
        const auto previousText = source.mid(previous.start, previous.end - previous.start);
        const auto quotes = quote.match(text).captured(0), previousQuotes = quote.match(previousText).captured(0);
        if (line.listMarkerStart >= 0) {
            if (previous.listMarkerStart < 0 && !separators.contains(line.start)) separators.insert(line.start, quotes + '\n');
            continue;
        }
        if (previous.listMarkerStart >= 0) continue;
        if (!quotes.isEmpty() || !previousQuotes.isEmpty()) {
            if (quotes == previousQuotes && !text.mid(quotes.size()).isEmpty() && !previousText.mid(previousQuotes.size()).isEmpty()
                && !separators.contains(line.start)) separators.insert(line.start, quotes + '\n');
        } else if (!separators.contains(line.start)) separators.insert(line.start, "\n");
    }
    Prepared result; result.text.reserve(source.size() + escapes.size()); result.positions.reserve(source.size() + escapes.size() + 1);
    for (int p = 0; p < source.size(); ++p) {
        if (separators.contains(p)) for (const auto character : separators.value(p)) { result.positions.append(p); result.synthetic.append(1); result.text += character; }
        if (escapes.contains(p)) { result.positions.append(p); result.synthetic.append(1); result.text += '\\'; }
        result.positions.append(p); result.synthetic.append(0); result.text += source[p];
    }
    result.positions.append(source.size()); return result;
}
QSet<int> MarkdownBlockSyntax::separatorLines(const QString &source, const MarkdownAnalysis *analysis) {
    const auto scanned = analysis ? MarkdownAnalysis() : MarkdownAnalysis::forRendering(source);
    const auto &classified = analysis ? *analysis : scanned;
    const auto rows = source.split('\n');
    QVector<int> starts; int start = 0;
    for (const auto &row : rows) { starts.append(start); start += row.size() + 1; }
    QSet<int> closedCodeEnds;
    for (const auto &fence : classified.fences) if (fence.closed()) closedCodeEnds.insert(fence.end);
    const int metadataEnd = MarkdownSyntax::frontMatterEnd(source);
    static const QRegularExpression quoteOnly("^(?: {0,3}> )+$");
    QSet<int> result;
    for (int i = 1; i < rows.size(); ++i) {
        const auto &previous = classified.lines[i - 1];
        // Literal interiors never own paragraph separators. The closing fence
        // (and metadata closer) can own one immediately outside the panel.
        const bool metadataCloser = metadataEnd > 0 && previous.fenceMarker && previous.end + 1 == metadataEnd;
        const bool literalFence = std::any_of(classified.literalFences.cbegin(), classified.literalFences.cend(),
            [&](const auto &range) { return range.start >= previous.start && range.end <= previous.end; });
        if ((previous.code || previous.fenceMarker) && !closedCodeEnds.contains(previous.end) && !metadataCloser && !literalFence) continue;
        if (i < classified.lines.size() && (classified.lines[i].code || classified.lines[i].html || classified.lines[i].comment)) continue;
        // Ordinary spaces are content, even on otherwise blank rows.
        if (rows[i].isEmpty() || rows[i] == "\r") {
            // A final split() record is an editable body position, not a
            // physical blank row. Consume exactly the first terminated blank
            // after every element, independently of the following element.
            if (i + 1 < rows.size() && !rows[i - 1].trimmed().isEmpty()) result.insert(starts[i]);
        } else if (quoteOnly.match(rows[i]).hasMatch() && !quoteOnly.match(rows[i - 1]).hasMatch()) {
            // The next quote paragraph may itself still be empty while typing.
            // Its first separator must not turn into another empty container.
            const int next = i + 1;
            if (next < rows.size() && rows[i - 1].startsWith(rows[i]) && rows[next].startsWith(rows[i])) result.insert(starts[i]);
        }
    }
    return result;
}
QVector<MarkdownBlockSyntax::Marker> MarkdownBlockSyntax::markers(const QString &source, const MarkdownAnalysis *analysis) {
    const auto scanned = analysis ? MarkdownAnalysis() : MarkdownAnalysis::forRendering(source);
    const auto &lines = analysis ? analysis->lines : scanned.lines;
    const auto literal = (analysis ? *analysis : scanned).literalMask(source.size());
    QVector<Marker> result;
    for (const auto &line : lines) {
        if (line.code || line.html || line.comment) continue;
        int p = skipSpaces(source, line.start, line.end), first = line.start;
        if ((p - first > 3 && line.listMarkerStart != p) || (p < line.end && literal[p])) continue;
        bool any = false, heading = false; int body = p;
        while (p < line.end) {
            const int end = markerEnd(source, p, line.end);
            if (end == p || end >= line.end || source[end] != ' ') break;
            if (inactiveTask(source, p, end, line.end)) break;
            heading = source[p] == '#'; any = true; body = end + 1; p = skipSpaces(source, end, line.end);
            if (!heading && taskAt(source, p, line.end)) {
                body = p + 4; p = skipSpaces(source, p + 3, line.end);
            }
            if (heading) break;
        }
        if (any) result.append({first, body, first, line.end, source.mid(first, body - first), true, heading});
        if (heading) {
            static const QRegularExpression closing(" +#+ *\\r?$");
            const auto match = closing.match(source.mid(p, line.end - p));
            if (match.hasMatch()) {
                const int start = p + match.capturedStart();
                result.append({start, line.end, first, line.end, source.mid(start, line.end - start), false, true});
            }
        }
    }
    return result;
}
MarkdownEdit MarkdownBlockSyntax::backspaceEmptyPrefix(const QString &source, int caret) {
    MarkdownEdit edit{source, caret, caret};
    const int start = caret ? source.lastIndexOf('\n', caret - 1) + 1 : 0;
    int end = source.indexOf('\n', caret); if (end < 0) end = source.size();
    static const QRegularExpression raw("^((?: {0,3}> +)*)( *)(?:[-+*]|[0-9]{1,9}[.)])(?: +\\[[ xX]\\])?$");
    const auto match = raw.match(source.mid(start, end - start));
    if (caret == end && match.hasMatch() && match.captured(2).isEmpty()) {
        const int from = start + match.capturedLength(1);
        edit.source.remove(from, end - from); edit.anchor = edit.caret = from;
        edit.handled = edit.changed = true; return edit;
    }
    for (const auto &marker : markers(source)) {
        if (!marker.prefix || marker.end != caret || marker.end != marker.lineEnd) continue;
        // Ordinary empty lists exit in one key, just like a populated item's
        // body start. Tasks retain their explicit restore-then-clear rule.
        static const QRegularExpression plainList("^((?: {0,3}> +)*)( *)(?:[-+*]|[0-9]{1,9}[.)]) $");
        if (plainList.match(marker.text).hasMatch())
            return MarkdownListEditing::backspace(source, caret);
        edit.source.remove(caret - 1, 1); edit.anchor = edit.caret = caret - 1;
        // A literal prefix immediately following a list is a lazy continuation
        // to Markdown importers. Separate the restored row without moving its
        // indentation or leaving the caret at the old pre-render position.
        const auto analysis = MarkdownAnalysis::forRendering(source);
        const bool afterContainer = std::any_of(analysis.lines.cbegin(), analysis.lines.cend(), [&](const auto &line) {
            return line.end == marker.lineStart - 1 && (line.listMarkerStart >= 0 || source.mid(line.start, line.end - line.start).trimmed().startsWith("> "));
        });
        if (afterContainer && marker.lineStart > 0 && source[marker.lineStart - 1] == '\n'
            && (marker.lineStart < 2 || source[marker.lineStart - 2] != '\n')) {
            edit.source.insert(marker.lineStart, '\n'); ++edit.anchor; ++edit.caret;
        }
        // Keep the restored literal row independent of what follows too.
        // Otherwise a non-1 ordered item or an indented child can become its
        // lazy continuation and swallow the next row into the same paragraph.
        const int end = edit.source.indexOf('\n', edit.caret);
        if (end >= 0 && end + 1 < edit.source.size() && edit.source[end + 1] != '\n') edit.source.insert(end + 1, '\n');
        edit.handled = edit.changed = true; break;
    }
    return edit;
}
