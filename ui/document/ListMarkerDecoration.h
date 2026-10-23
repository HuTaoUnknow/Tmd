#pragma once
#include "core/markdown/MarkdownListSyntax.h"
#include <QTextBlock>
#include <QRectF>
#include <QObject>
class QTextEdit;
class QPainter;
class InlineMarkupDecoration;
class MarkdownSourceMap;
struct MarkdownAnalysis;

// Qt owns the list layout. Only the small third-level dots need custom paint;
// marker clicks enter the existing source-prefix editor.
class ListMarkerDecoration : public QObject {
public:
    explicit ListMarkerDecoration(QTextEdit *view);
    struct Hit {
        int displayPosition = -1;
        bool valid() const { return displayPosition >= 0; }
    };
    void rebuild(const QString &source, const MarkdownAnalysis &analysis, const MarkdownSourceMap &mapping);
    void paint(QPainter &painter, const InlineMarkupDecoration &prefixes) const;
    Hit hitTest(const QPoint &point, const InlineMarkupDecoration &prefixes) const;
private:
    struct Marker { QTextBlock block; MarkdownListSyntax::Item item; };
    QRectF markerRect(const Marker &marker) const;
    QTextEdit *m_view;
    QVector<Marker> m_markers;
};
