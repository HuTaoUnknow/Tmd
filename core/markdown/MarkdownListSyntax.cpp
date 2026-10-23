#include "core/markdown/MarkdownListSyntax.h"
#include "core/markdown/MarkdownAnalysis.h"
#include <QRegularExpression>

namespace {
MarkdownListSyntax::Item parse(const QString &source, const MarkdownAnalysis::Line &line) {
    if (line.listMarkerStart < 0 || line.code || line.html || line.comment) return {};
    static const QRegularExpression prefix("^((?: {0,3}> )*)( *)([-+*]|[0-9]{1,9}[.)])( +)(\\[[ xX]\\] +)?");
    const auto match = prefix.match(source.mid(line.start, line.end - line.start));
    if (!match.hasMatch()) return {};
    return {line.start, line.end, line.start + int(match.captured(1).size()), int(match.captured(2).size()),
        line.start + int(match.capturedLength()), match.captured(1), match.captured(3), match.captured(4), match.captured(5),
        qMax(1, line.listDepth)};
}
}
int MarkdownListSyntax::Item::number() const {
    return ordered() ? marker.left(marker.size() - 1).toInt() : 0;
}
MarkdownListSyntax::Style MarkdownListSyntax::Item::style() const {
    if (depth >= 3) return Style::SmallDisc;
    if (ordered()) return depth == 2 && number() > 0 ? Style::LowerAlpha : Style::Decimal;
    return depth == 2 ? Style::Circle : Style::Disc;
}
MarkdownListSyntax::Item MarkdownListSyntax::itemAt(const QString &source, const MarkdownAnalysis &analysis, int caret) {
    for (const auto &line : analysis.lines)
        if (caret >= line.start && caret <= line.end) return parse(source, line);
    return {};
}
QVector<MarkdownListSyntax::Item> MarkdownListSyntax::items(const QString &source, const MarkdownAnalysis &analysis) {
    QVector<Item> result;
    for (const auto &line : analysis.lines) {
        const auto item = parse(source, line);
        if (item.valid()) result.append(item);
    }
    return result;
}
