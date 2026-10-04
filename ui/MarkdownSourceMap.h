#pragma once
#include <QString>
#include <QVector>
#include <QMap>

// Maps displayed characters to their original spelling, including escaped text
// and images. Edits touch those spans rather than reserializing the whole file.
class MarkdownSourceMap {
public:
    struct Span { int start = -1, end = -1; };
    void rebuild(const QString &source, const QString &display);
    int sourcePosition(int displayPosition) const;
    int displayPosition(int sourcePosition) const;
    QString edited(int position, int removed, const QString &inserted, int *cursorPosition) const;
    QString applyEdit(int position, int removed, const QString &sourceInsertion, const QString &displayInsertion, int *cursorPosition);
private:
    struct Container { int start, end, contentStart, contentEnd; };
    QString m_source, m_display;
    QVector<Span> m_spans;
    QVector<Container> m_containers;
    QMap<int, int> m_boundaries;
    struct EditPlan { QVector<Span> ranges; int insertion; QString source; };
    EditPlan planEdit(int position, int removed, const QString &inserted) const;
};
