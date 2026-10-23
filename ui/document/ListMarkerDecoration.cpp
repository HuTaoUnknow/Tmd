#include "ui/document/ListMarkerDecoration.h"
#include "ui/document/InlineMarkupDecoration.h"
#include "core/markdown/MarkdownSourceMap.h"
#include <QFontMetricsF>
#include <QPainter>
#include <QTextEdit>
#include <QTextList>

ListMarkerDecoration::ListMarkerDecoration(QTextEdit *view) : QObject(view), m_view(view) {}
void ListMarkerDecoration::rebuild(const QString &source, const MarkdownAnalysis &analysis, const MarkdownSourceMap &mapping) {
    m_markers.clear();
    for (const auto &item : MarkdownListSyntax::items(source, analysis)) {
        const auto block = m_view->document()->findBlock(mapping.displayPosition(item.body));
        if (!block.textList()) continue;
        auto format = block.textList()->format();
        using Style = MarkdownListSyntax::Style;
        switch (item.style()) {
        case Style::Disc: format.setStyle(QTextListFormat::ListDisc); break;
        case Style::Circle: format.setStyle(QTextListFormat::ListCircle); break;
        case Style::SmallDisc:
            // Task checkboxes keep their native marker and hit target.
            format.setStyle(item.task.isEmpty() ? QTextListFormat::ListStyleUndefined : QTextListFormat::ListDisc);
            break;
        case Style::Decimal: format.setStyle(QTextListFormat::ListDecimal); break;
        case Style::LowerAlpha: format.setStyle(QTextListFormat::ListLowerAlpha); break;
        }
        if (item.ordered()) { format.setStart(item.number()); format.setNumberSuffix(item.marker.right(1)); }
        // One list object per item preserves explicit source numbers, including
        // nonconsecutive values and manual edits, without renumbering siblings.
        QTextCursor(block).createList(format);
        m_markers.append({block, item});
    }
}
QRectF ListMarkerDecoration::markerRect(const Marker &marker) const {
    const auto cursor = QTextCursor(marker.block);
    const auto native = m_view->cursorRect(cursor);
    const QFontMetricsF metrics(m_view->font());
    const qreal gap = metrics.horizontalAdvance(' ');
    const bool small = marker.item.style() == MarkdownListSyntax::Style::SmallDisc;
    const qreal width = small ? qMax(2.0, metrics.lineSpacing() / 6)
        : marker.item.ordered() ? metrics.horizontalAdvance(marker.block.textList()->itemText(marker.block)) : metrics.lineSpacing() / 3;
    return QRectF(native.left() - gap - width, native.top(), width, native.height());
}
void ListMarkerDecoration::paint(QPainter &painter, const InlineMarkupDecoration &prefixes) const {
    painter.save(); painter.setRenderHint(QPainter::Antialiasing);
    for (const auto &marker : m_markers) {
        if (!marker.block.isValid() || !marker.block.textList() || marker.item.style() != MarkdownListSyntax::Style::SmallDisc
            || !marker.item.task.isEmpty() || prefixes.hasBlockPrefix(marker.block.position())) continue;
        const auto rect = markerRect(marker);
        if (rect.bottom() < 0 || rect.top() > m_view->viewport()->height()) continue;
        const auto cursor = m_view->textCursor();
        const bool selected = cursor.hasSelection() && cursor.selectionStart() <= marker.block.position()
            && cursor.selectionEnd() > marker.block.position();
        if (selected) painter.fillRect(rect, m_view->palette().brush(QPalette::Highlight));
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_view->palette().brush(selected ? QPalette::HighlightedText : QPalette::Text));
        const qreal diameter = rect.width();
        painter.drawEllipse(QRectF(rect.left(), rect.center().y() - diameter / 2, diameter, diameter));
    }
    painter.restore();
}
ListMarkerDecoration::Hit ListMarkerDecoration::hitTest(const QPoint &point, const InlineMarkupDecoration &prefixes) const {
    for (const auto &marker : m_markers) {
        if (!marker.block.isValid() || !marker.block.textList() || !marker.item.ordered() || !marker.item.task.isEmpty()
            || prefixes.hasBlockPrefix(marker.block.position())) continue;
        if (markerRect(marker).adjusted(-3, 0, 3, 0).contains(point))
            return {marker.block.position()};
    }
    return {};
}
