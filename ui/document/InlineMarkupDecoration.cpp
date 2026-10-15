#include "ui/document/InlineMarkupDecoration.h"
#include <QTextEdit>
#include <QFontMetricsF>
#include <QPainter>
#include <QTimer>
#include <QApplication>
#include <QEvent>
#include <QTextLayout>
#include <QPainterPath>
#include "ui/document/MarkdownTypography.h"
#include <algorithm>

InlineMarkupDecoration::InlineMarkupDecoration(QTextEdit *view)
    : QObject(view), m_view(view), m_caretTimer(new QTimer(this)), m_cursorWidth(qMax(1, view->cursorWidth())) {
    view->installEventFilter(this);
    connect(m_caretTimer, &QTimer::timeout, this, [this] { m_caretVisible = !m_caretVisible; m_view->viewport()->update(); });
}
bool InlineMarkupDecoration::eventFilter(QObject *object, QEvent *event) {
    if (object == m_view && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)) {
        m_caretVisible = event->type() == QEvent::FocusIn;
        if (m_caretVisible && m_customCaret && QApplication::cursorFlashTime() > 0) m_caretTimer->start(qMax(1, QApplication::cursorFlashTime() / 2));
        else m_caretTimer->stop();
        m_view->viewport()->update();
    }
    return QObject::eventFilter(object, event);
}
void InlineMarkupDecoration::clear() {
    for (auto it = m_originalFormats.cbegin(); it != m_originalFormats.cend(); ++it) {
        if (it.key() < 0 || it.key() >= m_view->document()->characterCount() - 1) continue;
        QTextCursor cursor(m_view->document()); cursor.setPosition(it.key()); cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor); cursor.setCharFormat(it.value());
    }
    for (const auto &entry : m_originalMargins) if (entry.first.isValid()) {
        QTextCursor cursor(entry.first); auto format = cursor.blockFormat(); format.setLeftMargin(entry.second); cursor.setBlockFormat(format);
    }
    m_originalFormats.clear(); m_originalMargins.clear(); m_slots.clear(); m_signature.clear();
    m_caretTimer->stop(); m_customCaret = false; m_caretSource = m_caretPosition = -1; m_view->setCursorWidth(m_cursorWidth);
}
void InlineMarkupDecoration::translateEdit(int position, int removed, int added) {
    QMap<int, QTextCharFormat> translated;
    for (auto it = m_originalFormats.cbegin(); it != m_originalFormats.cend(); ++it) {
        if (it.key() >= position && it.key() < position + removed) continue;
        translated.insert(it.key() < position ? it.key() : it.key() + added - removed, it.value());
    }
    m_originalFormats = translated; m_signature.clear();
}
void InlineMarkupDecoration::addSpacing(int position, qreal extra) {
    QTextCursor cursor(m_view->document()); cursor.setPosition(position); cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    auto format = cursor.charFormat(); if (!m_originalFormats.contains(position)) m_originalFormats.insert(position, format);
    const qreal previous = format.fontLetterSpacingType() == QFont::AbsoluteSpacing ? format.fontLetterSpacing() : 0;
    format.setFontLetterSpacingType(QFont::AbsoluteSpacing); format.setFontLetterSpacing(previous + extra); cursor.setCharFormat(format);
}
void InlineMarkupDecoration::refresh(const MarkdownSourceMap &mapping) {
    if (m_frozen) { updateCaret(mapping); m_view->viewport()->update(); return; }
    const auto selection = m_view->textCursor(); const int caret = mapping.sourcePosition(selection.position());
    const qreal halfSpace = QFontMetricsF(m_view->font()).horizontalAdvance(' ') / 2;
    QMap<int, Slot> groups;
    QString signature = selection.hasSelection() ? QString("%1:%2;").arg(selection.selectionStart()).arg(selection.selectionEnd()) : QString();
    for (const auto &format : mapping.inlineFormats()) {
        const bool selected = selection.hasSelection() && selection.selectionStart() < format.displayEnd && selection.selectionEnd() > format.displayStart;
        const bool active = selected || (!selection.hasSelection() && ((caret >= format.sourceStart && caret <= format.sourceEnd)
            || selection.position() == format.displayStart || selection.position() == format.displayEnd));
        signature += QString("%1,%2,%3;").arg(format.displayStart).arg(format.displayEnd).arg(active);
        QTextCursor sample(m_view->document()); sample.setPosition(format.displayStart); sample.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        QFont font = sample.charFormat().font(); if (font.pointSizeF() <= 0) font.setPointSizeF(m_view->font().pointSizeF());
        font.setBold(false); font.setItalic(false); font.setUnderline(false); font.setStrikeOut(false); font.setLetterSpacing(QFont::AbsoluteSpacing, 0);
        const QBrush background = format.code ? QBrush(MarkdownTypography::codeBackground(true)) : QBrush();
        auto add = [&](int position, bool opening, int first, int last, const QString &marker) {
            auto &slot = groups[position]; slot.position = position; slot.opening |= opening;
            // Shared/nested delimiters occupy one boundary, in source order.
            if (slot.gap == 0) slot.gap = halfSpace;
            if (active) { slot.pieces.append({first, last, marker, font, background, selected}); slot.gap += QFontMetricsF(font).horizontalAdvance(marker); }
        };
        add(format.displayStart, true, format.sourceStart, format.contentStart, format.opening);
        add(format.displayEnd, false, format.contentEnd, format.sourceEnd, format.closing);
    }
    for (const auto &marker : mapping.blockMarkers()) {
        const bool active = marker.prefix ? caret >= marker.sourceStart && caret < marker.sourceEnd
                                          : caret >= marker.lineStart && caret <= marker.lineEnd;
        signature += QString("b%1,%2,%3,%4;").arg(marker.displayPosition).arg(marker.sourceStart).arg(marker.sourceEnd).arg(active);
        if (!active) continue;
        auto &slot = groups[marker.displayPosition]; slot.position = marker.displayPosition;
        slot.opening |= marker.prefix; slot.blockPrefix |= marker.prefix;
        const QFont font = m_view->font();
        if (slot.gap == 0) slot.gap = halfSpace;
        slot.pieces.append({marker.sourceStart, marker.sourceEnd, marker.text, font, QBrush(), false, true});
        slot.gap += QFontMetricsF(font).horizontalAdvance(marker.text);
    }
    signature += QString::number(m_view->font().pointSizeF());
    if (signature == m_signature) { updateCaret(mapping); m_view->viewport()->update(); return; }
    clear(); m_signature = signature;
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        auto &slot = it.value();
        std::sort(slot.pieces.begin(), slot.pieces.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
        const auto block = m_view->document()->findBlock(slot.position);
        if (slot.position == block.position()) {
            m_originalMargins.append({block, block.blockFormat().leftMargin()});
            QTextCursor cursor(block); auto format = cursor.blockFormat(); format.setLeftMargin(format.leftMargin() + slot.gap); cursor.setBlockFormat(format);
        } else if (slot.position > 0) addSpacing(slot.position - 1, slot.gap);
        m_slots.append(slot);
    }
    updateCaret(mapping); m_view->viewport()->update();
}
void InlineMarkupDecoration::setSourceSelection(int anchor, int caret, bool frozen) {
    m_selectionAnchor = anchor; m_selectionCaret = caret; m_frozen = frozen;
    m_view->viewport()->update();
}
QVector<QRectF> InlineMarkupDecoration::codePanels(const MarkdownSourceMap &mapping) const {
    QVector<QRectF> panels;
    for (const auto &format : mapping.inlineFormats()) {
        if (!format.code) continue;
        for (auto block = m_view->document()->findBlock(format.displayStart); block.isValid() && block.position() < format.displayEnd; block = block.next()) {
            const auto *layout = block.layout();
            for (int i = 0; i < layout->lineCount(); ++i) {
                const auto line = layout->lineAt(i);
                const int first = qMax(format.displayStart, block.position() + line.textStart());
                const int last = qMin(format.displayEnd, block.position() + line.textStart() + line.textLength());
                if (first >= last) continue;
                QTextCursor cursor(m_view->document()); cursor.setPosition(first);
                const auto native = m_view->cursorRect(cursor);
                const qreal origin = native.left() - line.cursorToX(first - block.position());
                qreal left = native.left(), right = origin + line.cursorToX(last - block.position());
                for (const auto &slot : m_slots) {
                    if (first == format.displayStart && slot.position == first && !slot.pieces.isEmpty()) left = markerLeft(slot, native);
                    if (last == format.displayEnd && slot.position == last && slot.pieces.isEmpty()) right -= slot.gap;
                }
                panels.append(QRectF(left, native.top(), qMax(qreal(1), right - left), native.height()));
            }
        }
    }
    return panels;
}
void InlineMarkupDecoration::paintCodeBackground(QPainter &painter, const MarkdownSourceMap &mapping) const {
    painter.save(); painter.setRenderHint(QPainter::Antialiasing); painter.setPen(Qt::NoPen);
    painter.setBrush(MarkdownTypography::codeBackground(true));
    for (const auto &panel : codePanels(mapping)) painter.drawRoundedRect(panel, 4, 4);
    painter.restore();
}
const InlineMarkupDecoration::Slot *InlineMarkupDecoration::caretSlot() const {
    for (const auto &slot : m_slots) if (slot.position == m_caretPosition && !slot.pieces.isEmpty()
        && m_caretSource >= slot.pieces.first().first && m_caretSource <= slot.pieces.last().last) return &slot;
    return nullptr;
}
void InlineMarkupDecoration::updateCaret(const MarkdownSourceMap &mapping) {
    const int source = mapping.sourcePosition(m_view->textCursor().position());
    const bool changed = source != m_caretSource || m_view->textCursor().position() != m_caretPosition;
    m_caretSource = source; m_caretPosition = m_view->textCursor().position();
    m_customCaret = !m_view->textCursor().hasSelection() && m_selectionAnchor == m_selectionCaret && caretSlot();
    m_view->setCursorWidth(m_customCaret ? 0 : m_cursorWidth);
    if (changed) m_caretVisible = true;
    if (m_customCaret && m_view->hasFocus() && QApplication::cursorFlashTime() > 0) {
        if (changed || !m_caretTimer->isActive()) m_caretTimer->start(qMax(1, QApplication::cursorFlashTime() / 2));
    } else { m_caretTimer->stop(); m_caretVisible = true; }
}
qreal InlineMarkupDecoration::markerLeft(const Slot &slot, const QRect &native) const {
    return native.left() - slot.gap + QFontMetricsF(m_view->font()).horizontalAdvance(' ') / 4;
}
QRect InlineMarkupDecoration::caretRect(const QRect &native) const {
    const auto *slot = m_customCaret ? caretSlot() : nullptr; if (!slot) return native;
    qreal x = markerLeft(*slot, native);
    for (const auto &piece : slot->pieces) {
        if (m_caretSource >= piece.last) x += QFontMetricsF(piece.font).horizontalAdvance(piece.marker);
        else { x += QFontMetricsF(piece.font).horizontalAdvance(piece.marker.left(qMax(0, m_caretSource - piece.first))); break; }
    }
    if (slot->opening && m_caretSource == slot->pieces.last().last) x = native.left();
    return QRect(qRound(x), native.top(), m_cursorWidth, native.height());
}
InlineMarkupDecoration::Hit InlineMarkupDecoration::hitTest(const QPoint &point) const {
    for (const auto &slot : m_slots) {
        if (slot.pieces.isEmpty()) continue;
        QTextCursor cursor(m_view->document()); cursor.setPosition(slot.position); const auto rect = m_view->cursorRect(cursor);
        qreal x = markerLeft(slot, rect);
        if (point.y() < rect.top() || point.y() > rect.bottom() || point.x() < x - 1 || point.x() > rect.left() + 2) continue;
        for (const auto &piece : slot.pieces) {
            const QFontMetricsF metrics(piece.font);
            for (int i = 0; i < piece.marker.size(); ++i) {
                const qreal next = x + metrics.horizontalAdvance(piece.marker.left(i + 1));
                const qreal previous = x + metrics.horizontalAdvance(piece.marker.left(i));
                if (point.x() < (previous + next) / 2) return {slot.position, piece.first + i};
            }
            x += metrics.horizontalAdvance(piece.marker);
        }
        return {slot.position, slot.pieces.last().last};
    }
    return {};
}
void InlineMarkupDecoration::paint(QPainter &painter) const {
    painter.save();
    for (const auto &slot : m_slots) {
        if (slot.pieces.isEmpty()) continue;
        QTextCursor cursor(m_view->document()); cursor.setPosition(slot.position); const auto rect = m_view->cursorRect(cursor);
        if (rect.bottom() < 0 || rect.top() > m_view->viewport()->height()) continue;
        if (slot.blockPrefix && slot.position == cursor.block().position()) painter.fillRect(QRectF(0, rect.top(), rect.left(), rect.height()), m_view->palette().brush(QPalette::Base));
        qreal x = markerLeft(slot, rect);
        // Qt assigns this space to the preceding glyph. Clear its selection
        // color here so delimiters have their own source character boundaries.
        painter.fillRect(QRectF(x, rect.top(), rect.left() - x, rect.height()), m_view->palette().brush(QPalette::Base));
        for (int i = 0; i < slot.pieces.size(); ++i) {
            const auto &piece = slot.pieces[i]; const qreal width = QFontMetricsF(piece.font).horizontalAdvance(piece.marker);
            const qreal end = i == slot.pieces.size() - 1 ? rect.left() : x + width;
            const QRectF boundary(x, rect.top(), end - x, rect.height());
            if (piece.background.style() != Qt::NoBrush) {
                painter.save(); QPainterPath shape;
                // Round only the outside edge; the content joins the inside edge.
                const auto panel = slot.opening ? boundary.adjusted(0, 0, 4, 0) : boundary.adjusted(-4, 0, 0, 0);
                shape.addRoundedRect(panel, 4, 4); painter.setClipRect(boundary); painter.setRenderHint(QPainter::Antialiasing);
                painter.fillPath(shape, piece.background); painter.restore();
            }
            const QFontMetricsF metrics(piece.font);
            for (int character = 0; character < piece.marker.size(); ++character) {
                const qreal first = x + metrics.horizontalAdvance(piece.marker.left(character));
                const qreal next = character + 1 == piece.marker.size() ? end : x + metrics.horizontalAdvance(piece.marker.left(character + 1));
                const bool selected = m_selectionAnchor >= 0
                    ? piece.first + character >= qMin(m_selectionAnchor, m_selectionCaret) && piece.first + character < qMax(m_selectionAnchor, m_selectionCaret)
                    : piece.selected;
                if (selected) painter.fillRect(QRectF(first, rect.top(), next - first, rect.height()), m_view->palette().brush(QPalette::Highlight));
                painter.setFont(piece.font); painter.setPen(m_view->palette().color(selected ? QPalette::HighlightedText : QPalette::Text));
                painter.drawText(QRectF(first, rect.top(), next - first + 1, rect.height()), Qt::AlignVCenter | Qt::AlignLeft, piece.marker.mid(character, 1));
            }
            x += width;
        }
    }
    if (m_customCaret && m_caretVisible && m_view->hasFocus()) {
        const auto rect = caretRect(m_view->cursorRect()); painter.fillRect(rect, m_view->palette().brush(QPalette::Text));
    }
    painter.restore();
}
QStringList InlineMarkupDecoration::visibleMarkers() const {
    QStringList result;
    for (const auto &slot : m_slots) {
        QString marker; for (const auto &piece : slot.pieces) if (!piece.block) marker += piece.marker;
        if (!marker.isEmpty()) result.append(marker);
    }
    return result;
}

QStringList InlineMarkupDecoration::visibleBlockMarkers() const {
    QStringList result;
    for (const auto &slot : m_slots) for (const auto &piece : slot.pieces) if (piece.block) result.append(piece.marker);
    return result;
}
bool InlineMarkupDecoration::hasBlockPrefix(int position) const {
    for (const auto &slot : m_slots) if (slot.position == position && slot.blockPrefix) return true;
    return false;
}
