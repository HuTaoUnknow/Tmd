#include "core/markdown/MarkdownListEditing.h"
#include "core/markdown/MarkdownListSyntax.h"
#include "core/markdown/MarkdownAnalysis.h"
#include "core/markdown/MarkdownSyntax.h"
#include <QMap>
#include <QSet>

namespace {
using Item = MarkdownListSyntax::Item;
Item itemAt(const QString &source, const MarkdownAnalysis &analysis, int caret) {
    return MarkdownListSyntax::itemAt(source, analysis, caret);
}
void replace(MarkdownEdit &edit, int start, int count, const QString &text) {
    auto translate = [&](int p) { return p < start ? p : p + text.size() - qMin(count, p - start); };
    edit.anchor = translate(edit.anchor); edit.caret = translate(edit.caret);
    edit.source.replace(start, count, text); edit.changed = true;
}
QVector<QPair<int, int>> descendants(const QString &source, const MarkdownAnalysis &analysis, const Item &parent) {
    QVector<QPair<int, int>> children;
    int blanks = 0;
    for (const auto &line : analysis.lines) {
        if (line.start <= parent.start) continue;
        const auto text = source.mid(line.start, line.end - line.start);
        if (text.trimmed().isEmpty()) { if (++blanks >= 2) break; continue; }
        blanks = 0;
        if (!text.startsWith(parent.quote)) break;
        int indent = 0; while (parent.quote.size() + indent < text.size() && text[parent.quote.size() + indent] == ' ') ++indent;
        if (indent <= parent.indent) break;
        children.append({line.start + int(parent.quote.size()), indent});
    }
    return children;
}
void moveChildren(MarkdownEdit &edit, const QString &source, const MarkdownAnalysis &analysis, const Item &parent, int width) {
    const auto children = descendants(source, analysis, parent);
    for (auto it = children.crbegin(); it != children.crend(); ++it)
        replace(edit, it->first, width < 0 ? qMin(-width, it->second) : 0, width > 0 ? QString(width, ' ') : QString());
}
}
MarkdownEdit MarkdownListEditing::indent(const QString &source, int anchor, int caret, bool outdent) {
    MarkdownEdit edit{source, anchor, caret}; const auto analysis = MarkdownAnalysis::scan(source);
    const auto first = itemAt(source, analysis, qMin(anchor, caret));
    if (first.valid() && qMax(anchor, caret) > first.end) {
        // A selection changes complete subtrees once; QTextEdit's default Tab
        // would otherwise replace the selected Markdown with a single tab.
        edit.handled = true;
        QMap<int, QPair<int, int>> moves; QSet<int> covered;
        for (const auto &line : analysis.lines) {
            if (line.start < first.start || line.start >= qMax(anchor, caret) || covered.contains(line.start)) continue;
            if (source.mid(line.start, line.end - line.start).trimmed().isEmpty()) continue;
            const auto root = itemAt(source, analysis, line.end);
            if (!root.valid()) return edit;
            const auto change = indent(source, root.body, root.body, outdent);
            if (!change.changed) return edit;
            const int width = change.caret - root.body;
            moves.insert(root.indentStart, {root.indent, width});
            for (const auto &child : descendants(source, analysis, root)) {
                moves.insert(child.first, {child.second, width});
                covered.insert(child.first - root.quote.size());
            }
        }
        for (auto it = moves.constEnd(); it != moves.constBegin();) {
            --it; const int width = it.value().second;
            replace(edit, it.key(), width < 0 ? qMin(-width, it.value().first) : 0, width > 0 ? QString(width, ' ') : QString());
        }
        return edit;
    }
    const auto current = itemAt(source, analysis, caret);
    if (!current.valid() || anchor < current.start || anchor > current.end) return edit;
    edit.handled = true;
    if (outdent) {
        const int removed = qMin(4, current.indent);
        if (removed) { moveChildren(edit, source, analysis, current, -removed); replace(edit, current.indentStart, removed, {}); }
        return edit;
    }
    // A preceding peer becomes the parent; the first item cannot nest under itself.
    Item previous;
    for (const auto &line : analysis.lines) {
        if (line.start >= current.start) break;
        if (source.mid(line.start, line.end - line.start).trimmed().isEmpty()) continue;
        const auto candidate = itemAt(source, analysis, line.end);
        if (candidate.valid() && candidate.quote == current.quote && candidate.indent <= current.indent) previous = candidate;
        else if (!line.code && line.start == line.contentStart) previous = {};
    }
    if (!previous.valid() || previous.indent < current.indent) return edit;
    const int width = qMax(4, previous.indent + int(previous.marker.size() + previous.spaces.size()) - current.indent);
    moveChildren(edit, source, analysis, current, width);
    replace(edit, current.indentStart, 0, QString(width, ' ')); return edit;
}
MarkdownEdit MarkdownListEditing::newline(const QString &source, int anchor, int caret) {
    MarkdownEdit edit{source, anchor, caret}; const auto analysis = MarkdownAnalysis::scan(source);
    const auto item = itemAt(source, analysis, qMin(anchor, caret));
    if (!item.valid() || qMin(anchor, caret) < item.body || qMax(anchor, caret) > item.end) return edit;
    edit.handled = true;
    QString marker = item.marker;
    if (marker.size() > 1) marker = QString::number(marker.left(marker.size() - 1).toLongLong() + 1) + marker.back();
    QString task = item.task; if (!task.isEmpty()) task[1] = ' ';
    const QString inserted = '\n' + item.quote + QString(item.indent, ' ') + marker + item.spaces + task;
    const int begin = qMin(anchor, caret);
    edit.source.replace(begin, qAbs(anchor - caret), inserted); edit.anchor = edit.caret = begin + inserted.size(); edit.changed = true;
    return edit;
}
MarkdownEdit MarkdownListEditing::backspaceIndent(const QString &source, int caret) {
    MarkdownEdit edit{source, caret, caret};
    const auto item = itemAt(source, MarkdownAnalysis::scan(source), caret);
    if (!item.valid() || caret != item.body || item.indent == 0) return edit;
    edit = indent(source, caret, caret, true);
    if (!source.mid(item.body, item.end - item.body).trimmed().isEmpty()) return edit;
    const auto analysis = MarkdownAnalysis::scan(source);
    int end = item.end + 1;
    for (const auto &line : analysis.lines) {
        if (line.start < end) continue;
        const auto next = itemAt(source, analysis, line.end);
        if (!next.valid() || next.indent != item.indent || next.quote != item.quote
            || !source.mid(next.body, next.end - next.body).trimmed().isEmpty()) break;
        end = qMin(next.end + 1, int(source.size()));
    }
    if (end > item.end + 1) {
        const int from = item.end + (end == source.size() ? 0 : 1);
        edit.source.remove(from + edit.source.size() - source.size(), end - from);
    }
    return edit;
}
MarkdownEdit MarkdownListEditing::backspace(const QString &source, int caret) {
    auto indented = backspaceIndent(source, caret);
    if (indented.handled) return indented;
    MarkdownEdit edit{source, caret, caret}; const auto item = itemAt(source, MarkdownAnalysis::scan(source), caret);
    if (!item.valid() || caret != item.body) return edit;
    edit.handled = true;
    replace(edit, item.indentStart, item.body - item.indentStart, {});
    // A paragraph after a list needs a blank source separator, otherwise it is a lazy continuation.
    if (item.start > 0 && source[item.start - 1] == '\n' && (item.start < 2 || source[item.start - 2] != '\n'))
        replace(edit, item.start, 0, item.quote + "\n");
    return edit;
}
