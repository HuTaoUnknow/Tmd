#include "MarkdownSourceMap.h"
#include "core/MarkdownImages.h"
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <algorithm>
#include <climits>
#include <QSet>

namespace {
// Myers alignment keeps repeated words anchored in document order. Markdown
// punctuation is removed before alignment, so URLs cannot steal label matches.
QVector<int> align(const QString &a, const QString &b) {
    QVector<int> result(b.size(), -1);
    const int limit = qMin(1600, int(a.size() + b.size()));
    const int offset = limit + 1;
    QVector<int> frontier(2 * limit + 3, 0);
    QVector<QVector<int>> trace;
    for (int d = 0; d <= limit; ++d) {
        trace.append(frontier);
        for (int k = -d; k <= d; k += 2) {
            const int index = offset + k;
            int x = k == -d || (k != d && frontier[index - 1] < frontier[index + 1])
                ? frontier[index + 1] : frontier[index - 1] + 1;
            int y = x - k;
            while (x < a.size() && y < b.size() && a[x] == b[y]) { ++x; ++y; }
            frontier[index] = x;
            if (x < a.size() || y < b.size()) continue;
            for (int step = d; step >= 0; --step) {
                const auto &previous = trace[step]; const int diagonal = x - y;
                const int prior = diagonal == -step || (diagonal != step && previous[offset + diagonal - 1] < previous[offset + diagonal + 1])
                    ? diagonal + 1 : diagonal - 1;
                const int priorX = previous[offset + prior], priorY = priorX - prior;
                while (x > priorX && y > priorY) { --x; --y; result[y] = x; }
                x = priorX; y = priorY;
            }
            return result;
        }
    }
    // Bound memory for unusually large/irregular HTML. Still retain exact
    // forward matches; unmapped layout characters never delete source text.
    int search = 0;
    for (int i = 0; i < b.size(); ++i) {
        const int found = a.indexOf(b[i], search);
        if (found >= 0) { result[i] = found; search = found + 1; }
    }
    return result;
}
}
void MarkdownSourceMap::rebuild(const QString &source, const QString &display) {
    m_source = source; m_display = display; m_containers.clear(); m_boundaries.clear();
    QString visible; QVector<Span> candidates;
    QSet<int> imagePositions; for (const auto &image : MarkdownImages::parse(source)) imagePositions.insert(image.position);
    auto append = [&](QChar character, int start, int end) { visible += character; candidates.append({start, end}); };
    bool fenced = false; QChar fenceCharacter; int fenceLength = 0;
    static const QRegularExpression fence("^ {0,3}(`{3,}|~{3,})(.*)$");
    static const QRegularExpression prefix("^ {0,3}(?:#{1,6} +|(?:> ?)+|(?:[-+*]|[0-9]+[.)]) +(?:\\[[ xX]\\] +)?)");
    static const QRegularExpression hidden("^ {0,3}(?:\\[[^]]+\\]:.*|(?:[-*_] *){3,}|[=-]+ *|\\|? *:?-{3,}:? *(?:\\| *:?-{3,}:? *)+\\|?)$");
    for (int lineStart = 0; lineStart < source.size();) {
        int end = source.indexOf('\n', lineStart); if (end < 0) end = source.size();
        const QString line = source.mid(lineStart, end - lineStart);
        const auto fenceMatch = fence.match(line);
        if (fenceMatch.hasMatch() && (!fenced || (fenceMatch.captured(1)[0] == fenceCharacter && fenceMatch.captured(1).size() >= fenceLength && fenceMatch.captured(2).trimmed().isEmpty()))) {
            if (!fenced) { fenceCharacter = fenceMatch.captured(1)[0]; fenceLength = fenceMatch.captured(1).size(); }
            fenced = !fenced; lineStart = end + 1; continue;
        }
        if (!fenced && hidden.match(line).hasMatch()) { lineStart = end + 1; continue; }
        int start = lineStart;
        bool code = fenced || line.startsWith("    ") || line.startsWith('\t');
        if (!fenced && code) start += line.startsWith('\t') ? 1 : 4;
        else if (!code) {
            auto match = prefix.match(line); if (match.hasMatch()) start += match.capturedLength();
            // Nested quote/list prefixes may appear on the same line.
            while (start < end) { match = prefix.match(source.mid(start, end - start)); if (!match.hasMatch()) break; start += match.capturedLength(); }
        }
        const bool table = !code && line.contains('|') && (line.trimmed().startsWith('|') || line.count('|') >= 2);
        for (int i = start; i < end;) {
            const QChar character = source[i];
            if (code) { append(character, i, i + 1); ++i; continue; }
            if (character == '\\' && i + 1 < end && source[i + 1].isPunct()) { append(source[i + 1], i, i + 2); i += 2; continue; }
            if (character == '&') {
                const int semi = source.indexOf(';', i + 1);
                if (semi >= 0 && semi < qMin(end, i + 18)) {
                    const QString entity = source.mid(i, semi - i + 1);
                    const QString decoded = QTextDocumentFragment::fromHtml(entity).toPlainText();
                    if (decoded != entity) { for (auto c : decoded) append(c, i, semi + 1); i = semi + 1; continue; }
                }
            }
            if (character == '<') {
                const int close = source.indexOf('>', i + 1);
                if (close >= 0 && close < end) {
                    const QString inside = source.mid(i + 1, close - i - 1);
                    if (inside.startsWith("img ", Qt::CaseInsensitive)) append(QChar::ObjectReplacementCharacter, i, close + 1);
                    else if (inside.startsWith("http") || inside.contains('@')) { for (int j = i + 1; j < close; ++j) append(source[j], j, j + 1); }
                    i = close + 1; continue;
                }
            }
            const bool image = character == '!' && i + 1 < end && source[i + 1] == '[';
            if (image || character == '[') {
                const int labelStart = i + (image ? 2 : 1), close = source.indexOf(']', labelStart);
                int finish = close + 1;
                if (close >= 0 && close < end && finish < end && (source[finish] == '(' || source[finish] == '[')) {
                    const QChar opening = source[finish], closing = opening == '(' ? ')' : ']'; int depth = 1;
                    ++finish;
                    while (finish < end && depth) { if (source[finish] == '\\') { finish += 2; continue; } if (source[finish] == opening) ++depth; if (source[finish] == closing) --depth; ++finish; }
                    if (!depth) {
                        m_containers.append({i, finish, labelStart, close});
                        if (image) append(QChar::ObjectReplacementCharacter, i, finish);
                        else for (int j = labelStart; j < close; ++j) if (source[j] != '*' && source[j] != '~') append(source[j], j, j + 1);
                        i = finish; continue;
                    }
                }
                if (image && imagePositions.contains(i) && close >= 0 && close < end) { append(QChar::ObjectReplacementCharacter, i, close + 1); i = close + 1; continue; }
            }
            if (character == '`' || character == '*' || character == '_' || character == '~') {
                int length = 1; while (i + length < end && source[i + length] == character) ++length;
                const QString delimiter(length, character); const int close = source.indexOf(delimiter, i + length);
                const bool valid = close > i + length && close < end && (character == '`' || (!source[i + length].isSpace() && !source[close - 1].isSpace() && (character != '_' || i == start || !source[i - 1].isLetterOrNumber()))) && (character != '~' || length == 2);
                if (valid) {
                    m_containers.append({i, close + length, i + length, close});
                    if (character == '`') { for (int j = i + length; j < close; ++j) append(source[j], j, j + 1); }
                    else for (int j = i + length; j < close; ++j) if (source[j] != '*' && source[j] != '_' && source[j] != '~') append(source[j], j, j + 1);
                    i = close + length; continue;
                }
            }
            if (!table || character != '|') append(character, i, i + 1);
            ++i;
        }
        if (end < source.size()) append('\n', end, end + 1);
        lineStart = end + 1;
    }
    const auto matches = align(visible, display); m_spans.resize(display.size());
    for (int i = 0; i < matches.size(); ++i) m_spans[i] = matches[i] < 0 ? Span{} : candidates[matches[i]];
    // A soft source newline is displayed as a space. Other unmatched literal
    // punctuation can also be recovered between its adjacent exact matches.
    for (int i = 0; i < m_spans.size(); ++i) if (m_spans[i].start < 0 && display[i].unicode() != 0xfdd0 && display[i].unicode() != 0xfdd1) {
        int before = i - 1, after = i + 1;
        while (before >= 0 && m_spans[before].end < 0) --before;
        while (after < m_spans.size() && m_spans[after].start < 0) ++after;
        const int begin = before < 0 ? 0 : m_spans[before].end, end = after == m_spans.size() ? source.size() : m_spans[after].start;
        int found = source.indexOf(display[i], begin);
        if ((found < 0 || found >= end) && display[i] == ' ') found = source.indexOf('\n', begin);
        if (found >= begin && found < end) m_spans[i] = {found, found + 1};
    }
}
int MarkdownSourceMap::sourcePosition(int position) const {
    position = qBound(0, position, int(m_spans.size()));
    if (m_boundaries.contains(position)) return m_boundaries.value(position);
    if (position && m_spans[position - 1].start >= 0 && m_display[position - 1] != '\n') return m_spans[position - 1].end;
    for (int i = position; i < m_spans.size(); ++i) if (m_spans[i].start >= 0) return m_spans[i].start;
    for (int i = position - 1; i >= 0; --i) if (m_spans[i].end >= 0) return m_spans[i].end;
    return m_source.size();
}
int MarkdownSourceMap::displayPosition(int position) const {
    int result = 0, distance = INT_MAX;
    for (int i = 0; i < m_spans.size(); ++i) if (m_spans[i].start >= 0) {
        const int delta = qAbs(m_spans[i].start - position);
        if (delta < distance) { result = i; distance = delta; }
    }
    if (!m_spans.isEmpty() && position >= sourcePosition(m_spans.size())) return m_spans.size();
    return result;
}
MarkdownSourceMap::EditPlan MarkdownSourceMap::planEdit(int position, int removed, const QString &inserted) const {
    if (position == 0 && removed >= m_display.size() && removed > 0) return {{{0, int(m_source.size())}}, 0, inserted};
    const int end = qMin(position + removed, int(m_spans.size()));
    int insertion = sourcePosition(position); QVector<Span> ranges;
    if (removed > 0) for (int i = position; i < end; ++i) if (m_spans[i].start >= 0) { insertion = m_spans[i].start; break; }
    for (int i = position; i < end; ++i) if (m_spans[i].start >= 0) ranges.append(m_spans[i]);
    for (const auto &container : m_containers) {
        bool selected = false, unselected = false;
        for (int i = 0; i < m_spans.size(); ++i) if (m_spans[i].start >= container.contentStart && m_spans[i].end <= container.contentEnd) {
            if (i >= position && i < end) selected = true; else unselected = true;
        }
        if (selected && !unselected && inserted.isEmpty()) ranges.append({container.start, container.end});
    }
    // Merging blocks also removes the next block's structural prefix. Inline
    // formatting around surviving characters remains untouched.
    for (int i = position; i < end; ++i) if (m_display[i] == '\n' && m_spans[i].start >= 0) {
        int next = i + 1; while (next < m_spans.size() && m_spans[next].start < 0) ++next;
        if (next < m_spans.size()) ranges.append({m_spans[i].start, m_spans[next].start});
    }
    std::sort(ranges.begin(), ranges.end(), [](Span a, Span b) { return a.start < b.start; });
    QVector<Span> merged;
    for (const auto &range : ranges) {
        if (!merged.isEmpty() && range.start <= merged.last().end) merged.last().end = qMax(merged.last().end, range.end);
        else merged.append(range);
    }
    QString result = m_source;
    for (int i = merged.size() - 1; i >= 0; --i) {
        const auto range = merged[i]; result.remove(range.start, range.end - range.start);
        if (insertion > range.start) insertion -= qMin(insertion, range.end) - range.start;
    }
    insertion = qBound(0, insertion, int(result.size())); result.insert(insertion, inserted);
    return {merged, insertion, result};
}
QString MarkdownSourceMap::edited(int position, int removed, const QString &inserted, int *cursorPosition) const {
    const auto plan = planEdit(position, removed, inserted);
    if (cursorPosition) *cursorPosition = plan.insertion + inserted.size();
    return plan.source;
}
QString MarkdownSourceMap::applyEdit(int position, int removed, const QString &sourceInsertion, const QString &displayInsertion, int *cursorPosition) {
    const auto plan = planEdit(position, removed, sourceInsertion);
    auto translate = [&](int index, bool right) {
        if (index < 0) return index;
        int changed = index;
        for (const auto &range : plan.ranges) if (index > range.start) changed -= qMin(index, range.end) - range.start;
        if (changed > plan.insertion || (right && changed == plan.insertion)) changed += sourceInsertion.size();
        return changed;
    };
    QVector<Span> updated; updated.reserve(m_spans.size() - removed + displayInsertion.size());
    for (int i = 0; i < position; ++i) updated.append({translate(m_spans[i].start, true), translate(m_spans[i].end, false)});
    int search = 0;
    for (const auto character : displayInsertion) {
        if (character == QChar::LineSeparator) {
            const int found = sourceInsertion.indexOf('\n', search);
            if (found >= 0) { updated.append({plan.insertion + qMax(search, found - 2), plan.insertion + found + 1}); search = found + 1; continue; }
        }
        const int found = sourceInsertion.indexOf(character, search);
        if (found >= 0) { updated.append({plan.insertion + found, plan.insertion + found + 1}); search = found + 1; }
        else updated.append(Span{});
    }
    for (int i = position + removed; i < m_spans.size(); ++i) updated.append({translate(m_spans[i].start, true), translate(m_spans[i].end, false)});
    QVector<Container> containers;
    for (const auto &container : m_containers) {
        bool deleted = false;
        for (const auto &range : plan.ranges) if (range.start <= container.start && range.end >= container.end) deleted = true;
        if (!deleted) containers.append({translate(container.start, true), translate(container.end, false), translate(container.contentStart, false), translate(container.contentEnd, true)});
    }
    m_spans = updated; m_containers = containers; m_source = plan.source;
    QMap<int, int> boundaries;
    for (auto it = m_boundaries.cbegin(); it != m_boundaries.cend(); ++it) {
        if (it.key() < position) boundaries.insert(it.key(), translate(it.value(), false));
        else if (it.key() > position + removed) boundaries.insert(it.key() + displayInsertion.size() - removed, translate(it.value(), true));
    }
    boundaries.insert(position + displayInsertion.size(), plan.insertion + sourceInsertion.size()); m_boundaries = boundaries;
    m_display.replace(position, removed, displayInsertion);
    if (cursorPosition) *cursorPosition = plan.insertion + sourceInsertion.size();
    return m_source;
}
