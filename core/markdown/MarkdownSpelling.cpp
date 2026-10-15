#include "core/markdown/MarkdownSpelling.h"
#include "core/markdown/MarkdownAnalysis.h"
#include "core/markdown/MarkdownSyntax.h"
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <QSet>
#include <algorithm>
namespace {
int matching(const QString &source, int at, QChar open, QChar close) {
    int depth = 1;
    for (int i = at + 1; i < source.size() && source[i] != '\n'; ++i) {
        if (source[i] == '\\') { ++i; continue; }
        if (open == '(' && (source[i] == '<' || ((source[i] == '\'' || source[i] == '"') && i > at + 1 && source[i - 1].isSpace()))) {
            const QChar terminator = source[i] == '<' ? QChar('>') : source[i];
            int end = i + 1;
            while (end < source.size() && source[end] != '\n' && source[end] != terminator) { if (source[end] == '\\') ++end; ++end; }
            if (end < source.size() && source[end] == terminator) { i = end; continue; }
        }
        if (open == '[' && source[i] == '`') {
            int end = i; while (end < source.size() && source[end] == '`') ++end;
            const auto delimiter = source.mid(i, end - i); const int closeCode = source.indexOf(delimiter, end);
            if (closeCode >= 0 && !source.mid(end, closeCode - end).contains('\n')) { i = closeCode + delimiter.size() - 1; continue; }
        }
        if (source[i] == open) ++depth;
        if (source[i] == close && !--depth) return i;
    }
    return -1;
}
}
QVector<MarkdownSpelling::Region> MarkdownSpelling::regions(const QString &source) {
    QVector<Region> result; const auto analysis = MarkdownAnalysis::forRendering(source);
    auto literal = analysis.literalMask(source.size()); const auto syntax = MarkdownSyntax::scan(source, &analysis);
    static const QRegularExpression definition("^ {0,3}\\[([^]\\n]+)\\]: +\\S.*$");
    static const QRegularExpression rule("^ {0,3}(?:(?:- *){3,}|(?:\\* *){3,}|(?:_ *){3,})\\r?$");
    QSet<QString> definitions;
    for (const auto &line : analysis.lines) {
        if (line.code || line.html || line.comment) continue;
        const auto text = source.mid(line.start, line.end - line.start);
        const auto match = definition.match(text);
        if (match.hasMatch()) {
            result.append({line.start, line.end, line.start, line.end, Kind::Definition}); definitions.insert(match.captured(1).trimmed().toCaseFolded());
            std::fill(literal.begin() + line.start, literal.begin() + line.end, 1);
        } else if (syntax.underlineLines.contains(line.start)) result.append({line.start, line.end, line.start, line.end, Kind::Underline});
        else if (rule.match(text).hasMatch()) result.append({line.start, line.end, line.start, line.end, Kind::Rule});
    }
    for (int i = 0; i < source.size(); ++i) {
        if (literal[i]) continue;
        if (source[i] == '\\' && i + 1 < source.size() && source[i + 1].isPunct()) {
            result.append({i, i + 2, i, i + 2, Kind::Entity}); ++i; continue;
        }
        if (source[i] == '&') {
            const int semi = source.indexOf(';', i + 1);
            if (semi > i && semi < i + 18) {
                const auto spelling = source.mid(i, semi - i + 1);
                if (QTextDocumentFragment::fromHtml(spelling).toPlainText() != spelling) {
                    result.append({i, semi + 1, i, semi + 1, Kind::Entity}); i = semi; continue;
                }
            }
        }
        if (source[i] != '[') continue;
        const int close = matching(source, i, '[', ']'); if (close < 0) continue;
        int slashes = 0; for (int p = i - 2; p >= 0 && source[p] == '\\'; --p) ++slashes;
        const bool image = i > 0 && source[i - 1] == '!' && !(slashes % 2); int end = close + 1;
        if (end < source.size() && source[end] == '(') {
            const int target = matching(source, end, '(', ')'); if (target < 0) continue; end = target + 1;
        } else if (end < source.size() && source[end] == '[') {
            const int target = matching(source, end, '[', ']'); if (target < 0) continue;
            const auto name = source.mid(end + 1, target - end - 1).trimmed();
            if (!definitions.contains((name.isEmpty() ? source.mid(i + 1, close - i - 1) : name).toCaseFolded())) continue;
            end = target + 1;
        } else if (!definitions.contains(source.mid(i + 1, close - i - 1).trimmed().toCaseFolded())) continue;
        result.append({image ? i - 1 : i, end, i + 1, close, image ? Kind::Image : Kind::Link}); i = end - 1;
    }
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return a.start < b.start; }); return result;
}
QString MarkdownSpelling::target(const QString &source, const Region &region) {
    if (region.kind != Kind::Link || !region.valid()) return {};
    QString destination;
    if (region.contentEnd + 1 < source.size() && source[region.contentEnd + 1] == '(')
        destination = source.mid(region.contentEnd + 2, region.end - region.contentEnd - 3).trimmed();
    else {
        QString name = source.mid(region.contentStart, region.contentEnd - region.contentStart);
        if (region.contentEnd + 1 < source.size() && source[region.contentEnd + 1] == '[') {
            const auto reference = source.mid(region.contentEnd + 2, region.end - region.contentEnd - 3);
            if (!reference.isEmpty()) name = reference;
        }
        const auto match = QRegularExpression("^ {0,3}\\[" + QRegularExpression::escape(name) + "\\]: +(.*)$",
            QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption).match(source);
        if (match.hasMatch()) destination = match.captured(1).trimmed();
    }
    const auto match = QRegularExpression("^(?:<([^>]+)>|(\\S+))").match(destination);
    if (!match.hasMatch()) return {};
    QString value = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1), result;
    for (int i = 0; i < value.size(); ++i) { if (value[i] == '\\' && i + 1 < value.size() && value[i + 1].isPunct()) ++i; result += value[i]; }
    static const QRegularExpression entity("&(?:#[0-9]+|#[xX][0-9A-Fa-f]+|[A-Za-z][A-Za-z0-9]+);");
    auto matches = entity.globalMatch(result); QVector<QRegularExpressionMatch> entities;
    while (matches.hasNext()) entities.append(matches.next());
    for (auto it = entities.crbegin(); it != entities.crend(); ++it)
        result.replace(it->capturedStart(), it->capturedLength(), QTextDocumentFragment::fromHtml(it->captured()).toPlainText());
    return result;
}
QVector<MarkdownSpelling::Range> MarkdownSpelling::protectDeletions(const QString &source, const QVector<Range> &ranges) {
    if (ranges.isEmpty()) return ranges;
    auto deletions = ranges;
    int first = source.size(), last = 0;
    for (const auto &range : ranges) { first = qMin(first, range.start); last = qMax(last, range.end); }
    QVector<Range> protectedRanges;
    const auto analysis = MarkdownAnalysis::forRendering(source);
    auto fences = analysis.fences;
    const int metadataEnd = MarkdownSyntax::frontMatterEnd(source);
    if (metadataEnd > 0) for (const auto &line : analysis.lines) if (line.start > 0 && line.start < metadataEnd && line.fenceMarker) {
        fences.append({0, analysis.lines.front().end, analysis.lines.front().end + 1, line.start, line.end, "---", {}}); break;
    }
    for (const auto &fence : fences) {
        if (!fence.closed()) continue;
        // If a cross-block selection consumes the entire body, remove both
        // hidden delimiters. Deleting only the body keeps an empty code panel.
        if ((first < fence.bodyStart && last >= fence.closeStart - 1)
            || (first <= fence.bodyStart && last >= fence.closeStart && last > fence.bodyStart)) {
            deletions.append({fence.start, fence.end}); continue;
        }
        const bool tailRemains = first < fence.bodyStart && last > fence.bodyStart && last < fence.closeStart - 1;
        const bool headRemains = first > fence.bodyStart && first < fence.closeStart && last >= fence.closeStart;
        if (!tailRemains && !headRemains) continue;
        protectedRanges.append({qMax(0, fence.start - 1), fence.bodyStart});
        protectedRanges.append({qMax(fence.bodyStart, fence.closeStart - 1), qMin(int(source.size()), fence.end + 1)});
    }
    QVector<Range> result;
    for (const auto &range : deletions) {
        QVector<Range> pieces{range};
        for (const auto &boundary : protectedRanges) {
            QVector<Range> next;
            for (const auto &piece : pieces) {
                if (boundary.end <= piece.start || boundary.start >= piece.end) next.append(piece);
                else {
                    if (piece.start < boundary.start) next.append({piece.start, boundary.start});
                    if (piece.end > boundary.end) next.append({boundary.end, piece.end});
                }
            }
            pieces = next;
        }
        result += pieces;
    }
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return a.start < b.start; });
    QVector<Range> merged;
    for (const auto &range : result) {
        if (!merged.isEmpty() && range.start <= merged.back().end) merged.back().end = qMax(merged.back().end, range.end);
        else merged.append(range);
    }
    return merged;
}
