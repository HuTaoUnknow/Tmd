#pragma once
#include <QString>
#include <QVector>

// Source-only block classification shared by navigation, assets and editing.
// All offsets refer to the unchanged UTF-16 Markdown source.
struct MarkdownAnalysis {
    struct Range { int start = 0, end = 0; };
    struct Line {
        int start = 0, end = 0, contentStart = 0, number = 0;
        bool code = false, fenceMarker = false, html = false, comment = false;
    };
    struct Fence {
        int start = 0, headerEnd = 0, bodyStart = 0, closeStart = -1, end = 0;
        QString delimiter, indent;
        bool closed() const { return closeStart >= 0; }
    };
    struct InlineCode { int start, end, contentStart, contentEnd; };
    QVector<Line> lines;
    QVector<Fence> fences;
    QVector<Range> opaque;
    QVector<Range> comments;
    QVector<InlineCode> inlineCodes;
    static MarkdownAnalysis scan(const QString &source);
    QVector<char> literalMask(int sourceLength) const;
};
