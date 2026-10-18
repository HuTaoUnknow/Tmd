#pragma once
#include "core/markdown/MarkdownOutline.h"
#include <QSet>
#include <QTextDocument>
struct MarkdownAnalysis;

// Semantic Markdown recognition uses the same MD4C version as Qt's renderer.
// Positions always refer to the original QString, never to rendered text.
class MarkdownSyntax {
public:
    struct Format {
        int start, end, contentStart, contentEnd;
        QString opening, closing;
        bool code = false;
    };
    struct Result {
        MarkdownOutline headings;
        QVector<Format> formats;
        QSet<int> underlineLines;
        QSet<int> containerParagraphLines;
        int frontMatterEnd = 0;
        bool parsed = false;
    };
    struct Edit { QString source; int caret; bool changed = false; };
    static Result scan(const QString &source, const MarkdownAnalysis *analysis = nullptr);
    static int frontMatterEnd(const QString &source);
    static QTextDocument::MarkdownFeatures renderFeatures();
    static Edit leaveEmptyContainer(const QString &source, int caret);
};
