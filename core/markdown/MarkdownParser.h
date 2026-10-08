#pragma once
#include "core/markdown/MarkdownOutline.h"
#include "core/markdown/MarkdownAnalysis.h"
#include <QList>

class MarkdownParser {
public:
    using CodeFence = MarkdownAnalysis::Fence;
    struct HeadingMarker {
        int markerStart = 0, contentStart = 0, end = 0, level = 0;
        bool separated = false;
    };
    static QList<CodeFence> codeFences(const QString &markdown);
    static QList<HeadingMarker> headingMarkers(const QString &markdown);
    static QList<HeadingMarker> headingMarkers(const QString &markdown, const MarkdownAnalysis &analysis);
    static MarkdownOutline outline(const QString &markdown);
    static MarkdownOutline outline(const QString &markdown, const MarkdownAnalysis &analysis);
    static QString plainInlineText(QString text);
};
