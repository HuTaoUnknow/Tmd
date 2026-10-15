#pragma once
#include <QString>
#include <QVector>
class MarkdownSpelling {
public:
    struct Range { int start = -1, end = -1; };
    enum class Kind { Link, Image, Entity, Definition, Rule, Underline };
    struct Region {
        int start = -1, end = -1, contentStart = -1, contentEnd = -1;
        Kind kind = Kind::Link;
        bool valid() const { return start >= 0 && end >= start; }
    };
    static QVector<Region> regions(const QString &source);
    static QString target(const QString &source, const Region &region);
    static QVector<Range> protectDeletions(const QString &source, const QVector<Range> &ranges);
};
