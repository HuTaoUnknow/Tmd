#pragma once
#include <QString>
#include <QVector>
#include <QMap>

// Maps displayed characters to their original spelling, including escaped text
// and images. Edits touch those spans rather than reserializing the whole file.
class MarkdownSourceMap {
public:
    struct Span { int start = -1, end = -1; };
    struct InlineCode {
        int displayStart = -1, displayEnd = -1, sourceStart = -1, sourceEnd = -1, contentStart = -1, contentEnd = -1;
        bool valid() const { return displayStart >= 0; }
    };
    void rebuild(const QString &source, const QString &display);
    int sourcePosition(int displayPosition) const;
    int displayPosition(int sourcePosition) const;
    InlineCode inlineCodeAt(int displayPosition) const;
    void setBoundary(int displayPosition, int sourcePosition) { m_boundaries.insert(displayPosition, sourcePosition); }
    QString edited(int position, int removed, const QString &inserted, int *cursorPosition) const;
    QString applyEdit(int position, int removed, const QString &sourceInsertion, const QString &displayInsertion, int *cursorPosition);
private:
    struct Container { int start, end, contentStart, contentEnd; bool inlineCode = false; };
    QString m_source, m_display;
    QVector<Span> m_spans;
    QVector<Container> m_containers;
    QVector<InlineCode> m_inlineCodes;
    void indexInlineCodes();
    QMap<int, int> m_boundaries;
    struct EditPlan { QVector<Span> ranges; int insertion; QString source; };
    EditPlan planEdit(int position, int removed, const QString &inserted) const;
};
