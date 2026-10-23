#pragma once
#include <QString>
#include <QVector>

// Destination offsets in unchanged Markdown; literal regions and code are excluded.
struct MarkdownReference {
    int start = 0, length = 0;
    QString source;
    bool image = false;
};
class MarkdownReferences {
public:
    static QVector<MarkdownReference> parse(const QString &source);
};
