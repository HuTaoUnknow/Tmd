#include "core/markdown/MarkdownCodeEditing.h"
#include "core/markdown/MarkdownAnalysis.h"
#include "core/markdown/MarkdownSyntax.h"
#include <QRegularExpression>

namespace {
struct Context {
    MarkdownAnalysis analysis;
    int bodyEnd = -1;
    bool virtualLine = false;
    QString continuation;
};
Context context(const QString &source, int anchor, int caret) {
    Context result{MarkdownAnalysis::scan(source)};
    const int begin = qMin(anchor, caret), end = qMax(anchor, caret);
    if (begin < 0 || end > source.size()) return result;
    const int metadata = MarkdownSyntax::frontMatterEnd(source), body = source.indexOf('\n') + 1;
    if (metadata > 0) for (const auto &line : result.analysis.lines) if (line.fenceMarker && line.start > 0 && line.start < metadata
        && begin >= body && end <= line.start) {
        result.bodyEnd = line.start; result.virtualLine = body == line.start; return result;
    }
    for (const auto &fence : result.analysis.fences) {
        const int limit = fence.closed() ? fence.closeStart : fence.end;
        const bool empty = fence.bodyStart == limit && begin == limit && end == limit;
        if (begin >= fence.bodyStart && end <= limit && (begin < limit || empty || !fence.closed())) {
            result.bodyEnd = limit; result.virtualLine = empty && fence.closed(); result.continuation = MarkdownCodeEditing::continuationPrefix(fence.indent); break;
        }
    }
    return result;
}
struct Change { int position, removed; QString inserted; };
void apply(MarkdownCodeEditing::Edit &edit, const QList<Change> &changes) {
    auto translate = [&](int position) {
        int result = position;
        for (const auto &change : changes) if (position >= change.position)
            result += change.inserted.size() - qMin(change.removed, position - change.position);
        return result;
    };
    edit.anchor = translate(edit.anchor); edit.caret = translate(edit.caret);
    for (auto it = changes.crbegin(); it != changes.crend(); ++it) edit.source.replace(it->position, it->removed, it->inserted);
    edit.changed = !changes.isEmpty();
}
}
QString MarkdownCodeEditing::continuationPrefix(const QString &fenceIndent) {
    QString result = fenceIndent;
    static const QRegularExpression marker("(?:[-+*]|[0-9]{1,9}[.)])[ \t]+");
    auto match = marker.match(result);
    while (match.hasMatch()) { result.replace(match.capturedStart(), match.capturedLength(), QString(match.capturedLength(), ' ')); match = marker.match(result); }
    return result;
}
MarkdownCodeEditing::Edit MarkdownCodeEditing::backspace(const QString &source, int caret) {
    Edit edit{source, caret, caret}; const auto analysis = MarkdownAnalysis::scan(source);
    auto fences = analysis.fences; const int metadata = MarkdownSyntax::frontMatterEnd(source);
    if (metadata > 0) for (const auto &line : analysis.lines) if (line.fenceMarker && line.start > 0 && line.start < metadata)
        fences.prepend({0, int(source.indexOf('\n')), int(source.indexOf('\n') + 1), line.start, metadata, "---", {}});
    for (const auto &fence : fences) {
        if (!fence.closed()) continue;
        const MarkdownAnalysis::Line *first = nullptr;
        for (const auto &line : analysis.lines) if (line.start == fence.bodyStart && !line.fenceMarker) { first = &line; break; }
        const bool virtualLine = fence.bodyStart == fence.closeStart && caret == fence.bodyStart;
        if (!virtualLine && (!first || caret != first->contentStart)) continue;
        edit.handled = true;
        if (!virtualLine && !source.mid(first->contentStart, first->end - first->contentStart).trimmed().isEmpty()) return edit;
        if (!virtualLine && first->end + 1 < fence.closeStart) {
            edit.source.remove(first->start, first->end + 1 - first->start);
            int prefix = 0;
            for (const auto &line : analysis.lines) if (line.start == first->end + 1) { prefix = line.contentStart - line.start; break; }
            edit.anchor = edit.caret = fence.bodyStart + prefix; edit.changed = true; return edit;
        }
        // The only empty code line is removed together with its header/closer.
        edit.source.remove(fence.start, fence.end - fence.start);
        edit.anchor = edit.caret = fence.start; edit.changed = true; return edit;
    }
    return edit;
}
MarkdownCodeEditing::Edit MarkdownCodeEditing::removeEmptyMetadata(const QString &before, const QString &after, int caret) {
    Edit edit{after, caret, caret}; const int metadata = MarkdownSyntax::frontMatterEnd(after);
    if (after.size() >= before.size() || !metadata || !MarkdownSyntax::frontMatterEnd(before)) return edit;
    const int body = after.indexOf('\n') + 1;
    for (const auto &line : MarkdownAnalysis::scan(after).lines) if (line.fenceMarker && line.start > 0 && line.start < metadata) {
        const auto content = after.mid(body, line.start - body);
        if (!content.isEmpty() && content != "\n" && content != "\r\n") return edit;
        edit.source.remove(0, metadata); edit.anchor = edit.caret = qMax(0, caret - metadata); edit.handled = edit.changed = true; return edit;
    }
    return edit;
}
MarkdownCodeEditing::Edit MarkdownCodeEditing::indent(const QString &source, int anchor, int caret, bool outdent, int width) {
    Edit result{source, anchor, caret}; const auto code = context(source, anchor, caret);
    if (code.bodyEnd < 0) return result;
    result.handled = true; width = qBound(1, width, 16);
    if (code.virtualLine) {
        if (!outdent) { result.source.insert(caret, code.continuation + QString(width, ' ') + '\n'); result.anchor = result.caret = caret + code.continuation.size() + width; result.changed = true; }
        return result;
    }
    const int begin = qMin(anchor, caret), end = qMax(anchor, caret);
    if (anchor == caret && !outdent) {
        int start = source.lastIndexOf('\n', caret - 1) + 1;
        for (const auto &line : code.analysis.lines) if (line.start == start) { start = line.contentStart; break; }
        int column = 0; for (int p = start; p < caret; ++p) column += source[p] == '\t' ? width - column % width : 1;
        apply(result, {{caret, 0, QString(width - column % width, ' ')}}); return result;
    }
    QList<Change> changes;
    for (const auto &line : code.analysis.lines) {
        if (!line.code || line.fenceMarker || line.start >= code.bodyEnd || line.end < begin || (anchor != caret && line.start >= end) || (anchor == caret && line.start > caret)) continue;
        const int start = line.contentStart;
        if (!outdent) changes.append({start, 0, QString(width, ' ')});
        else {
            int count = 0;
            if (start < line.end && source[start] == '\t') count = 1;
            else while (count < width && start + count < line.end && source[start + count] == ' ') ++count;
            if (count) changes.append({start, count, {}});
        }
    }
    apply(result, changes); return result;
}
MarkdownCodeEditing::Edit MarkdownCodeEditing::newline(const QString &source, int anchor, int caret) {
    Edit result{source, anchor, caret}; const auto code = context(source, anchor, caret);
    if (code.bodyEnd < 0) return result;
    result.handled = true;
    const int begin = qMin(anchor, caret), end = qMax(anchor, caret);
    QString prefix = code.virtualLine ? code.continuation : QString();
    for (const auto &line : code.analysis.lines) if (line.start <= begin && line.end >= begin && !line.fenceMarker) {
        int first = line.contentStart;
        while (first < begin && (source[first] == ' ' || source[first] == '\t')) ++first;
        prefix = source.mid(line.start, first - line.start); break;
    }
    const QString inserted = (code.virtualLine ? prefix : QString()) + '\n' + prefix;
    result.source.replace(begin, end - begin, inserted);
    if (code.virtualLine) result.source.insert(begin + inserted.size(), '\n');
    result.anchor = result.caret = begin + inserted.size(); result.changed = true; return result;
}
