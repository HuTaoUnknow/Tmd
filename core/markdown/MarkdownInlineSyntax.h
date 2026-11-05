#pragma once
#include <QSet>
#include <QString>
struct MarkdownAnalysis;
namespace MarkdownInlineSyntax {
// Chinese punctuation at a strong delimiter is a text boundary even when
// the following Chinese sentence has no separating ASCII space.
QSet<int> strongBoundaryPositions(const QString &source, const MarkdownAnalysis &analysis);
}
