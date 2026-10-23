#pragma once
#include <QString>
#include <QVector>
#include <QMap>
#include <memory>
#include "core/markdown/MarkdownSpelling.h"
namespace tmd { class MarkdownSnapshot; }

// Maps displayed characters to their original spelling, including escaped text
// and images. Edits touch those spans rather than reserializing the whole file.
class MarkdownSourceMap {
public:
    using Span = MarkdownSpelling::Range;
    void setLiteralRanges(const QVector<Span> &ranges) { m_literalRanges = ranges; }
    const QVector<Span> &literalRanges() const { return m_literalRanges; }
    struct InlineFormat {
        int displayStart = -1, displayEnd = -1, sourceStart = -1, sourceEnd = -1, contentStart = -1, contentEnd = -1;
        QString opening, closing;
        bool code = true;
        bool valid() const { return displayStart >= 0; }
    };
    struct BlockMarker {
        int displayPosition, sourceStart, sourceEnd, lineStart, lineEnd;
        QString text;
        bool prefix;
    };
    const QVector<BlockMarker> &blockMarkers() const { return m_blockMarkers; }
    bool isInsideBlockMarker(int sourcePosition) const;
    struct MarkerBoundary {
        int displayPosition = -1, sourceStart = -1, sourceEnd = -1;
        bool valid() const { return displayPosition >= 0; }
    };
    MarkerBoundary markerBoundaryAt(int displayPosition) const;
    int stepAcrossMarker(int displayPosition, int sourcePosition, bool right) const;
    using InlineCode = InlineFormat;
    const QVector<InlineFormat> &inlineFormats() const { return m_inlineFormats; }
    InlineFormat inlineFormatAt(int displayPosition) const;
    void rebuild(const QString &source, const QString &display, const QVector<QPair<int, int>> &blockBoundaries = {},
                 std::shared_ptr<const tmd::MarkdownSnapshot> snapshot = {});
    struct WhitespaceEdit { int position, removed; QString text; };
    QVector<WhitespaceEdit> editableWhitespaceEdits() const;
    int sourcePosition(int displayPosition) const;
    int displayPosition(int sourcePosition) const;
    InlineCode inlineCodeAt(int displayPosition) const;
    void setBoundary(int displayPosition, int sourcePosition) { m_boundaries.insert(displayPosition, sourcePosition); }
    QString edited(int position, int removed, const QString &inserted, int *cursorPosition) const;
    // Text search replaces visible characters, even when they cover the whole
    // view; hidden Markdown containers are not an implicit Select All.
    QString replacedText(int position, int removed, const QString &inserted, int *cursorPosition) const;
    QString applyEdit(int position, int removed, const QString &sourceInsertion, const QString &displayInsertion, int *cursorPosition);
private:
    struct Container { int start, end, contentStart, contentEnd; bool inlineCode = false; };
    QString m_source, m_display;
    mutable std::shared_ptr<const tmd::MarkdownSnapshot> m_snapshot;
    const tmd::MarkdownSnapshot &snapshot() const;
    QVector<Span> m_spans;
    QVector<Span> m_literalRanges;
    QVector<Container> m_containers;
    QVector<InlineCode> m_inlineCodes;
    QVector<InlineFormat> m_inlineFormats;
    QVector<int> m_formatEndPrefix;
    QVector<BlockMarker> m_blockMarkers;
    void indexBlockMarkers(const QVector<QPair<int, int>> &mapped);
    QMap<int, MarkerBoundary> m_markerBoundaries;
    QMap<int, QVector<Span>> m_markerRanges;
    void indexInlineCodes();
    QMap<int, int> m_boundaries;
    struct EditPlan { QVector<Span> ranges; int insertion; QString source; };
    EditPlan planEdit(int position, int removed, const QString &inserted, bool wholeDocumentSelection = true) const;
};
