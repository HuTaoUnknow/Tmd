#include "core/markdown/MarkdownSelectionEditing.h"
#include "core/markdown/MarkdownAnalysis.h"
#include <algorithm>

bool MarkdownSelectionEditing::isMarkerInput(const QString &text) {
    return !text.isEmpty() && std::all_of(text.cbegin(), text.cend(), [](QChar c) { return QStringLiteral("*_~`").contains(c); });
}
QString MarkdownSelectionEditing::closingMarker(const QString &opening) {
    if (!isMarkerInput(opening)) return {};
    if (opening.contains('`')) return opening == QString(opening.size(), '`') ? opening : QString();
    for (int i = 0; i < opening.size();) {
        const QChar marker = opening[i]; int end = i + 1;
        while (end < opening.size() && opening[end] == marker) ++end;
        if (marker == '~' && end - i != 2) return {};
        i = end;
    }
    QString closing = opening; std::reverse(closing.begin(), closing.end()); return closing;
}
MarkdownEdit MarkdownSelectionEditing::wrap(const QString &source, int anchor, int caret, const QString &opening) {
    MarkdownEdit edit{source, anchor, caret}; const auto closing = closingMarker(opening);
    const int first = qMin(anchor, caret), last = qMax(anchor, caret);
    if (first < 0 || last > source.size() || first == last || closing.isEmpty()) return edit;
    edit.source.insert(last, closing); edit.source.insert(first, opening);
    // Keep the original content selected, including a backwards mouse selection.
    edit.anchor = anchor + opening.size(); edit.caret = caret + opening.size();
    edit.handled = edit.changed = true; return edit;
}
MarkdownEdit MarkdownSelectionEditing::selectCode(const QString &source, int anchor, int caret, int stage) {
    MarkdownEdit edit{source, anchor, caret};
    const int first = qMin(anchor, caret), last = qMax(anchor, caret);
    if (first < 0 || last > source.size()) return edit;
    const auto analysis = MarkdownAnalysis::forRendering(source);
    for (const auto &fence : analysis.fences) {
        if (!fence.closed() || first < fence.bodyStart || last > fence.closeStart
            || (first == fence.closeStart && fence.bodyStart != fence.closeStart)) continue;
        int begin = fence.bodyStart, end = fence.bodyStart;
        bool found = false;
        for (const auto &line : analysis.lines) if (line.start >= fence.bodyStart && line.start < fence.closeStart && !line.fenceMarker) {
            if (!found) { begin = line.contentStart; found = true; }
            end = line.end;
            if (stage == 0 && caret >= line.start && caret <= line.end) { begin = line.contentStart; end = line.end; break; }
        }
        edit.anchor = stage >= 2 ? 0 : begin; edit.caret = stage >= 2 ? source.size() : end;
        edit.handled = true; return edit;
    }
    return edit;
}
