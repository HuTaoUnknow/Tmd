#include "core/markdown/MarkdownParagraphEditing.h"
#include "core/markdown/MarkdownSyntax.h"
#include "core/markdown/MarkdownBlockSyntax.h"
#include "core/markdown/MarkdownAnalysis.h"
#include <QRegularExpression>
#include <algorithm>
namespace {
int startOf(const QString &s, int p) { return p ? s.lastIndexOf('\n', p - 1) + 1 : 0; }
int endOf(const QString &s, int p) { const int end = s.indexOf('\n', p); return end < 0 ? int(s.size()) : end; }
QString quotes(const QString &line) {
    static const QRegularExpression prefix("^(?: {0,3}> )+"); return prefix.match(line).captured(0);
}
}
MarkdownEdit MarkdownParagraphEditing::newline(const QString &source, int anchor, int caret) {
    MarkdownEdit edit{source, anchor, caret};
    const int begin = qMin(anchor, caret), finish = qMax(anchor, caret), start = startOf(source, begin), end = endOf(source, finish);
    const auto syntax = MarkdownSyntax::scan(source);
    const QString line = source.mid(start, end - start), quote = quotes(line);
    const bool heading = std::any_of(syntax.headings.cbegin(), syntax.headings.cend(), [&](const auto &h) { return h.position == start; });
    if (heading && syntax.underlineLines.contains(end + 1)) {
        const int underlineEnd = endOf(source, end + 1);
        const QString underline = source.mid(end + 1, underlineEnd - end - 1);
        const QString left = source.mid(start, begin - start), right = source.mid(finish, end - finish);
        edit.source.replace(start, underlineEnd - start, left + '\n' + underline + '\n' + right);
        edit.anchor = edit.caret = start + left.size() + underline.size() + 2;
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
            edit.source.insert(from, remaining + '\n'); edit.anchor += remaining.size() + 1; edit.caret = edit.anchor;
        }
    } else {
        QString tail = source.mid(finish, end - finish), suffix;
        if (heading) for (const auto &marker : MarkdownBlockSyntax::markers(source)) {
            if (!marker.prefix && marker.lineStart == start && marker.start >= finish) {
                suffix = source.mid(marker.start, marker.end - marker.start); tail.chop(suffix.size()); break;
            }
        }
        const QString prefix = heading ? QString() : quote;
        const QString separator = prefix.isEmpty() ? QString("\n") : '\n' + prefix + '\n';
        edit.source.replace(begin, end - begin, suffix + separator + prefix + tail);
        edit.anchor = edit.caret = begin + suffix.size() + separator.size() + prefix.size();
    }
    edit.handled = true; edit.changed = edit.source != source; return edit;
}
MarkdownEdit MarkdownParagraphEditing::backspace(const QString &source, int caret) {
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
