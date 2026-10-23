#pragma once
#include <QString>
#include <QVector>
struct MarkdownAnalysis;

// List identity and presentation are derived from the source, never written
// back when the view changes a marker's appearance.
class MarkdownListSyntax {
public:
    enum class Style { Disc, Circle, SmallDisc, Decimal, LowerAlpha };
    struct Item {
        int start = -1, end = 0, indentStart = 0, indent = 0, body = 0;
        QString quote, marker, spaces, task;
        int depth = 1;
        bool valid() const { return start >= 0; }
        bool ordered() const { return marker.size() > 1; }
        int markerStart() const { return indentStart + indent; }
        int number() const;
        Style style() const;
    };
    static Item itemAt(const QString &source, const MarkdownAnalysis &analysis, int caret);
    static QVector<Item> items(const QString &source, const MarkdownAnalysis &analysis);
};
