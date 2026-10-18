#pragma once
#include <QString>
#include <QVector>
#include <QSet>
#include "core/markdown/MarkdownEdit.h"
struct MarkdownAnalysis;

// Tmd's editing rules for line prefixes; source offsets never include escapes.
class MarkdownBlockSyntax {
public:
    struct Prepared { QString text; QVector<int> positions; QVector<char> synthetic; };
    struct Marker {
        int start, end, lineStart, lineEnd;
        QString text;
        bool prefix = true, heading = false;
    };
    static Prepared prepare(const QString &source, const MarkdownAnalysis *analysis = nullptr);
    // The first empty physical row after each element is its source separator.
    // Literal interiors, whitespace content and additional rows stay visible.
    static QSet<int> separatorLines(const QString &source, const MarkdownAnalysis *analysis = nullptr);
    static QVector<Marker> markers(const QString &source, const MarkdownAnalysis *analysis = nullptr);
    static MarkdownEdit backspaceEmptyPrefix(const QString &source, int caret);
};
