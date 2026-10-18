#include "core/markdown/MarkdownSourceMap.h"
#include "core/markdown/MarkdownImages.h"
#include "core/markdown/MarkdownAnalysis.h"
#include "core/markdown/MarkdownSyntax.h"
#include "core/markdown/MarkdownBlockSyntax.h"
#include "core/markdown/analysis/MarkdownSnapshot.h"
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
const tmd::MarkdownSnapshot &MarkdownSourceMap::snapshot() const {
    if (!m_snapshot || m_snapshot->source() != m_source || m_snapshot->mode() != tmd::MarkdownSnapshot::Mode::Rendering)
        m_snapshot = std::make_shared<tmd::MarkdownSnapshot>(m_source, tmd::MarkdownSnapshot::Mode::Rendering);
    return *m_snapshot;
}
void MarkdownSourceMap::rebuild(const QString &source, const QString &display, const QVector<QPair<int, int>> &blockBoundaries,
                                std::shared_ptr<const tmd::MarkdownSnapshot> provided) {
    m_source = source; m_display = display; m_containers.clear(); m_boundaries.clear();
    if (provided && provided->source() == source && provided->mode() == tmd::MarkdownSnapshot::Mode::Rendering)
        m_snapshot = std::move(provided);
    for (const auto &boundary : blockBoundaries) m_boundaries.insert(boundary.first, boundary.second);
    QString visible; QVector<Span> candidates;
    const auto &analysis = snapshot().analysis();
    const auto &syntax = snapshot().syntax();
    const auto separators = MarkdownBlockSyntax::separatorLines(source, &analysis);
    QMap<int, int> editablePrefixEnds;
    for (const auto &marker : MarkdownBlockSyntax::markers(source, &analysis)) if (marker.prefix) editablePrefixEnds.insert(marker.lineStart, marker.end);
    QVector<char> opaque(source.size(), 0);
    QSet<int> imagePositions; for (const auto &image : snapshot().images()) imagePositions.insert(image.position);
    auto append = [&](QChar character, int start, int end) { visible += character; candidates.append({start, end}); };
    static const QRegularExpression hidden(R"(^ {0,3}(?:\[[^]]+\]:.*|(?:-[ \t]*){3,}|(?:[*][ \t]*){3,}|(?:_[ \t]*){3,}|\|? *:?-{3,}:? *(?:\| *:?-{3,}:? *)+\|?)$)");
    for (const auto &classified : analysis.lines) {
        const int lineStart = classified.start, end = classified.end;
        if (separators.contains(lineStart)) continue;
        const QString line = source.mid(lineStart, end - lineStart);
        const bool revealed = std::any_of(m_literalRanges.cbegin(), m_literalRanges.cend(), [&](const auto &r) { return r.start <= lineStart && r.end >= end; });
        if (classified.fenceMarker || (!revealed && syntax.underlineLines.contains(classified.start))) continue;
        const bool code = classified.code;
        if (!code && !revealed && hidden.match(line).hasMatch()) continue;
        int start = code ? classified.contentStart : lineStart;
        if (!code) {
            // The renderer and the caret map must agree on the exact active
            // prefix. In particular, an unfinished task marker is literal.
            start = editablePrefixEnds.value(lineStart, lineStart);
        }
        const bool table = !code && line.contains('|') && (line.trimmed().startsWith('|') || line.count('|') >= 2);
        for (int i = start; i < end;) {
            if (opaque[i]) { ++i; continue; }
            const QChar character = source[i];
            if (std::any_of(m_literalRanges.cbegin(), m_literalRanges.cend(), [&](const auto &r) { return i >= r.start && i < r.end; })) {
                append(character, i, i + 1); ++i; continue;
            }
            if (code || classified.html || classified.comment) { append(character, i, i + 1); ++i; continue; }
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
                    QString tag = inside.trimmed(); if (tag.endsWith('/')) { tag.chop(1); tag = tag.trimmed(); }
                    if (table && tag.compare("br", Qt::CaseInsensitive) == 0) append(QChar::LineSeparator, i, close + 1);
                    else if (inside.startsWith("https://") || inside.startsWith("http://") || QRegularExpression("^[^ <>@]+@[^ <>@]+$").match(inside).hasMatch())
                        { for (int j = i + 1; j < close; ++j) append(source[j], j, j + 1); }
                    else for (int j = i; j <= close; ++j) append(source[j], j, j + 1);
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
                const QString delimiter(length, character); int close = source.indexOf(delimiter, i + length);
                if (character == '`') {
                    close = -1;
                    const auto span = std::lower_bound(analysis.inlineCodes.cbegin(), analysis.inlineCodes.cend(), i,
                        [](const auto &item, int position) { return item.start < position; });
                    if (span != analysis.inlineCodes.cend() && span->start == i) close = span->contentEnd;
                }
                const bool valid = close > i + length && close < end && (character == '`' || (!source[i + length].isSpace() && !source[close - 1].isSpace() && (character != '_' || i == start || !source[i - 1].isLetterOrNumber()))) && (character != '~' || length == 2);
                if (valid) {
                    m_containers.append({i, close + length, i + length, close, character == '`'});
                    if (character == '`') { for (int j = i + length; j < close; ++j) append(source[j], j, j + 1); }
                    else for (int j = i + length; j < close; ++j) if (source[j] != '*' && source[j] != '_' && source[j] != '~') append(source[j], j, j + 1);
                    i = close + length; continue;
                }
            }
            if (!table || character != '|') append(character, i, i + 1);
            ++i;
        }
        if (end < source.size()) append('\n', end, end + 1);
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
        if (display[i] == ' ') {
            const int newline = source.indexOf('\n', begin);
            if (newline >= begin && newline < end && (found < 0 || found >= end || newline < found)) found = newline;
        }
        if (found >= begin && found < end) m_spans[i] = {found, found + 1};
    }
    // Keep link/image transactions, but take formatting delimiters from the
    // semantic parser so unmatched punctuation never becomes an edit container.
    m_containers.erase(std::remove_if(m_containers.begin(), m_containers.end(), [&](const auto &c) {
        return c.start >= 0 && c.start < source.size() && QString("`*_~").contains(source[c.start]);
    }), m_containers.end());
    for (const auto &format : syntax.formats) m_containers.append({format.start, format.end, format.contentStart, format.contentEnd, format.code});
    std::sort(m_containers.begin(), m_containers.end(), [](const auto &a, const auto &b) { return a.start < b.start; });
    indexInlineCodes();
}
QVector<MarkdownSourceMap::WhitespaceEdit> MarkdownSourceMap::editableWhitespaceEdits() const {
    QVector<WhitespaceEdit> result;
    const auto &analysis = snapshot().analysis();
    const auto &syntax = snapshot().syntax();
    const auto markers = MarkdownBlockSyntax::markers(m_source, &analysis);
    QMap<int, int> markerEnds;
    for (const auto &marker : markers) markerEnds[marker.lineStart] = qMax(markerEnds.value(marker.lineStart), marker.end);
    QMap<int, int> prefixEnds;
    for (const auto &marker : markers) if (marker.prefix) prefixEnds.insert(marker.lineStart, marker.end);
    const auto add = [&](int start, int end) {
        if (start == end) return;
        const int position = displayPosition(start); int removed = 0;
        while (position + removed < m_spans.size()) {
            const auto &span = m_spans[position + removed];
            if (span.start < start || span.end > end || (m_display[position + removed] != ' ' && m_display[position + removed] != '\t')) break;
            ++removed;
        }
        const QString whitespace = m_source.mid(start, end - start);
        if (m_display.mid(position, removed) != whitespace) result.append({position, removed, whitespace});
    };
    static const QRegularExpression hidden(R"(^ {0,3}(?:\[[^]]+\]:.*|(?:-[ \t]*){3,}|(?:[*][ \t]*){3,}|(?:_[ \t]*){3,})$)");
    for (const auto &line : analysis.lines) {
        if (line.code || line.fenceMarker || line.start < syntax.frontMatterEnd
            || syntax.underlineLines.contains(line.start)) continue;
        const int prefixEnd = prefixEnds.value(line.start, -1); int gapEnd = prefixEnd;
        if (prefixEnd < 0) {
            int leading = line.start;
            while (leading < line.end && (m_source[leading] == ' ' || m_source[leading] == '\t')) ++leading;
            if (!hidden.match(m_source.mid(line.start, line.end - line.start)).hasMatch()) add(line.start, leading);
        }
        while (gapEnd >= 0 && gapEnd < line.end && (m_source[gapEnd] == ' ' || m_source[gapEnd] == '\t')) ++gapEnd;
        if (prefixEnd >= 0) add(prefixEnd, gapEnd);
        int start = line.end;
        while (start > line.start && (m_source[start - 1] == ' ' || m_source[start - 1] == '\t')) --start;
        if (start == line.end || start == line.start || (prefixEnd >= 0 && start < gapEnd)) continue;
        const QString text = m_source.mid(line.start, line.end - line.start);
        if (hidden.match(text).hasMatch() || text.trimmed().endsWith('|')) continue;
        if (markerEnds.value(line.start) > start) continue;
        add(start, line.end);
    }
    return result;
}
void MarkdownSourceMap::indexInlineCodes() {
    m_inlineCodes.clear(); m_inlineFormats.clear();
    QVector<QPair<int, int>> mapped;
    for (int i = 0; i < m_spans.size(); ++i) if (m_spans[i].start >= 0) mapped.append({m_spans[i].start, i});
    for (const auto &c : m_containers) {
        if (c.start < 0 || c.end > m_source.size() || !QString("`*_~").contains(m_source[c.start])) continue;
        const auto first = std::lower_bound(mapped.cbegin(), mapped.cend(), c.contentStart,
            [](const auto &p, int at) { return p.first < at; });
        const auto after = std::lower_bound(first, mapped.cend(), c.contentEnd,
            [](const auto &p, int at) { return p.first < at; });
        if (first == after) continue;
        InlineFormat format{first->second, (after - 1)->second + 1, c.start, c.end, c.contentStart, c.contentEnd,
            m_source.mid(c.start, c.contentStart - c.start), m_source.mid(c.contentEnd, c.end - c.contentEnd), c.inlineCode};
        m_inlineFormats.append(format); if (format.code) m_inlineCodes.append(format);
    }
    const auto order = [](const auto &a, const auto &b) { return a.displayStart < b.displayStart; };
    std::stable_sort(m_inlineFormats.begin(), m_inlineFormats.end(), order);
    QVector<InlineFormat> merged;
    for (const auto &format : m_inlineFormats) {
        if (!merged.isEmpty() && merged.back().displayStart == format.displayStart && merged.back().displayEnd == format.displayEnd) {
            auto &whole = merged.back(); whole.sourceStart = qMin(whole.sourceStart, format.sourceStart); whole.sourceEnd = qMax(whole.sourceEnd, format.sourceEnd);
            whole.contentStart = qMax(whole.contentStart, format.contentStart); whole.contentEnd = qMin(whole.contentEnd, format.contentEnd); whole.code |= format.code;
            whole.opening = m_source.mid(whole.sourceStart, whole.contentStart - whole.sourceStart); whole.closing = m_source.mid(whole.contentEnd, whole.sourceEnd - whole.contentEnd);
        } else merged.append(format);
    }
    m_inlineFormats = merged; indexBlockMarkers(mapped); m_formatEndPrefix.clear(); m_markerBoundaries.clear(); m_markerRanges.clear(); int furthest = 0;
    auto boundary = [&](int display, int first, int last) {
        m_markerRanges[display].append({first, last});
        auto &group = m_markerBoundaries[display]; group.displayPosition = display;
        group.sourceStart = group.sourceStart < 0 ? first : qMin(group.sourceStart, first);
        group.sourceEnd = qMax(group.sourceEnd, last);
    };
    for (const auto &format : m_inlineFormats) {
        boundary(format.displayStart, format.sourceStart, format.contentStart);
        boundary(format.displayEnd, format.contentEnd, format.sourceEnd);
    }
    for (const auto &marker : m_blockMarkers) boundary(marker.displayPosition, marker.sourceStart, marker.sourceEnd);
    for (auto &ranges : m_markerRanges)
        std::sort(ranges.begin(), ranges.end(), [](const auto &a, const auto &b) { return a.start < b.start; });
    for (const auto &format : m_inlineFormats) { furthest = qMax(furthest, format.displayEnd); m_formatEndPrefix.append(furthest); }
    std::sort(m_inlineCodes.begin(), m_inlineCodes.end(), order);
}
void MarkdownSourceMap::indexBlockMarkers(const QVector<QPair<int, int>> &mapped) {
    m_blockMarkers.clear(); QMap<int, int> explicitPositions;
    for (auto it = m_boundaries.cbegin(); it != m_boundaries.cend(); ++it) explicitPositions.insert(it.value(), it.key());
    const auto firstAt = [&](int source) { return std::lower_bound(mapped.cbegin(), mapped.cend(), source,
        [](const auto &p, int at) { return p.first < at; }); };
    for (auto marker : MarkdownBlockSyntax::markers(m_source, &snapshot().analysis())) {
        int display = -1;
        const auto visible = firstAt(marker.start);
        // A prefix already visible as literal text must not be painted twice.
        if (visible != mapped.cend() && visible->first < marker.end) {
            if (!marker.prefix) continue;
            marker.end = visible->first;
        }
        if (marker.end <= marker.start) continue;
        if (marker.prefix) {
            const auto body = firstAt(marker.end);
            if (body != mapped.cend() && body->first <= marker.lineEnd) display = body->second;
            if (explicitPositions.contains(marker.end)) display = explicitPositions.value(marker.end);
            if (display < 0 && m_display.isEmpty()) display = 0;
        } else {
            const auto before = firstAt(marker.start);
            if (before != mapped.cbegin()) {
                const int index = (before - 1)->second;
                if (m_spans[index].start >= marker.lineStart && m_spans[index].end <= marker.start) display = index + 1;
            }
        }
        if (display < 0) continue;
        m_blockMarkers.append({display, marker.start, marker.end, marker.lineStart, marker.lineEnd,
            m_source.mid(marker.start, marker.end - marker.start), marker.prefix});
        if (marker.prefix && !m_boundaries.contains(display)) m_boundaries.insert(display, marker.end);
    }
}
bool MarkdownSourceMap::isInsideBlockMarker(int source) const {
    for (const auto &marker : m_blockMarkers) if (source >= marker.sourceStart && source < marker.sourceEnd) return true;
    return false;
}
MarkdownSourceMap::MarkerBoundary MarkdownSourceMap::markerBoundaryAt(int position) const {
    return m_markerBoundaries.value(position);
}
int MarkdownSourceMap::stepAcrossMarker(int display, int source, bool right) const {
    const auto boundary = markerBoundaryAt(display);
    if (!boundary.valid()) return -1;
    if (source < boundary.sourceStart || source > boundary.sourceEnd) return -1;
    const auto found = m_markerRanges.constFind(display); if (found == m_markerRanges.cend()) return -1;
    if (right) {
        for (const auto &range : found.value()) if (source < range.end) return qMax(source, range.start) + 1;
    } else {
        for (auto it = found.value().crbegin(); it != found.value().crend(); ++it) if (source > it->start) return qMin(source, it->end) - 1;
    }
    return -1;
}
MarkdownSourceMap::InlineFormat MarkdownSourceMap::inlineFormatAt(int position) const {
    InlineFormat result;
    const auto after = std::upper_bound(m_inlineFormats.cbegin(), m_inlineFormats.cend(), position,
        [](int at, const auto &format) { return at < format.displayStart; });
    for (int i = after - m_inlineFormats.cbegin() - 1; i >= 0 && m_formatEndPrefix[i] >= position; --i) {
        const auto &format = m_inlineFormats[i];
        if (position <= format.displayEnd && (!result.valid() || format.sourceEnd - format.sourceStart < result.sourceEnd - result.sourceStart)) result = format;
    }
    return result;
}
MarkdownSourceMap::InlineCode MarkdownSourceMap::inlineCodeAt(int position) const {
    const auto found = std::lower_bound(m_inlineCodes.cbegin(), m_inlineCodes.cend(), position,
        [](const auto &code, int at) { return code.displayEnd < at; });
    return found != m_inlineCodes.cend() && position >= found->displayStart ? *found : InlineCode{};
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
    // Every delimiter character has a source caret even though the glyph is
    // painted separately. Preserve intermediate positions during reloads.
    for (const auto &boundary : m_markerBoundaries)
        if (position >= boundary.sourceStart && position <= boundary.sourceEnd) return boundary.displayPosition;
    int result = 0, distance = INT_MAX;
    for (int i = 0; i < m_spans.size(); ++i) if (m_spans[i].start >= 0) {
        const int delta = qAbs(m_spans[i].start - position);
        if (delta < distance) { result = i; distance = delta; }
        const int endDistance = qAbs(m_spans[i].end - position);
        if (m_spans[i].end >= 0 && endDistance < distance) { result = i + 1; distance = endDistance; }
    }
    for (auto it = m_boundaries.cbegin(); it != m_boundaries.cend(); ++it) {
        const int delta = qAbs(it.value() - position);
        if (delta <= distance) { result = it.key(); distance = delta; }
    }
    // A table's final frame separator has no source glyph. An exact body or
    // empty-cell boundary must win over the document-end fallback.
    if (distance > 0 && !m_spans.isEmpty() && position >= m_source.size()) return m_spans.size();
    return result;
}
MarkdownSourceMap::EditPlan MarkdownSourceMap::planEdit(int position, int removed, const QString &inserted) const {
    const bool singlePrefixedBody = !m_display.contains('\n') && std::any_of(m_blockMarkers.cbegin(), m_blockMarkers.cend(), [](const auto &marker) { return marker.prefix; });
    if (position == 0 && removed >= m_display.size() && removed > 0 && !singlePrefixedBody) {
        const auto &analysis = snapshot().analysis();
        const bool singleFencedBody = analysis.fences.size() == 1 && analysis.fences[0].closed()
            && m_source.left(analysis.fences[0].start).trimmed().isEmpty()
            && m_source.mid(analysis.fences[0].end).trimmed().isEmpty();
        if (!singleFencedBody) return {{{0, int(m_source.size())}}, 0, inserted};
    }
    const int end = qMin(position + removed, int(m_spans.size()));
    int insertion = sourcePosition(position); QVector<Span> ranges;
    if (removed > 0) for (int i = position; i < end; ++i) if (m_spans[i].start >= 0) { insertion = m_spans[i].start; break; }
    for (int i = position; i < end; ++i) if (m_spans[i].start >= 0) ranges.append(m_spans[i]);
    if (removed > 0) for (const auto &container : m_containers) {
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
    if (inserted.isEmpty()) ranges = MarkdownSpelling::protectDeletions(m_source, ranges);
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
        if (!deleted) containers.append({translate(container.start, true), translate(container.end, false), translate(container.contentStart, false), translate(container.contentEnd, true), container.inlineCode});
    }
    for (auto &range : m_literalRanges) { range.start = translate(range.start, false); range.end = translate(range.end, true); }
    m_spans = updated; m_containers = containers; m_source = plan.source;
    QMap<int, int> boundaries;
    for (auto it = m_boundaries.cbegin(); it != m_boundaries.cend(); ++it) {
        if (it.key() < position) boundaries.insert(it.key(), translate(it.value(), false));
        else if (it.key() > position + removed) boundaries.insert(it.key() + displayInsertion.size() - removed, translate(it.value(), true));
    }
    boundaries.insert(position + displayInsertion.size(), plan.insertion + sourceInsertion.size()); m_boundaries = boundaries;
    m_display.replace(position, removed, displayInsertion);
    indexInlineCodes();
    if (cursorPosition) *cursorPosition = plan.insertion + sourceInsertion.size();
    return m_source;
}
