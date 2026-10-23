#include "ui/document/SearchHighlightOverlay.h"
#include "ui/document/MarkdownDocumentView.h"
#include "ui/document/MarkdownTypography.h"
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QMoveEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QTextEdit>
#include <QTextBlock>
#include <QTextLayout>
#include <QTimer>

SearchHighlightOverlay::SearchHighlightOverlay(QTextEdit *editor)
    : QWidget(editor->viewport()), m_editor(editor), m_caretTimer(new QTimer(this)) {
    setObjectName("documentSearchHighlights"); setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground); setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::NoFocus); editor->viewport()->installEventFilter(this); editor->installEventFilter(this);
    connect(editor->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { update(); });
    connect(editor->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this] { update(); });
    connect(editor, &QTextEdit::cursorPositionChanged, this, [this] { m_caretVisible = true; update(); });
    connect(m_caretTimer, &QTimer::timeout, this, [this] { m_caretVisible = !m_caretVisible; update(); });
    setGeometry(editor->viewport()->rect()); hide();
}
void SearchHighlightOverlay::refresh() {
    if (!m_editor) return;
    bool active = false;
    for (const auto &selection : m_editor->extraSelections()) if (selection.format.boolProperty(Tag)) { active = true; break; }
    setGeometry(m_editor->viewport()->rect()); setVisible(active);
    if (active) { raise(); update(); }
    if (active && m_editor->hasFocus() && QApplication::cursorFlashTime() > 0)
        m_caretTimer->start(qMax(1, QApplication::cursorFlashTime() / 2));
    else { m_caretTimer->stop(); m_caretVisible = true; }
}
bool SearchHighlightOverlay::eventFilter(QObject *object, QEvent *event) {
    if (m_editor && object == m_editor->viewport() && event->type() == QEvent::Resize) setGeometry(m_editor->viewport()->rect());
    if (m_editor && object == m_editor && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut
        || event->type() == QEvent::KeyPress || event->type() == QEvent::MouseButtonPress)) {
        m_caretVisible = true; QTimer::singleShot(0, this, [this] { refresh(); });
    }
    return QWidget::eventFilter(object, event);
}
void SearchHighlightOverlay::moveEvent(QMoveEvent *event) {
    QWidget::moveEvent(event);
    // QTextEdit scrolls viewport children; this layer stays fixed and computes
    // each visual line from the editor's current native cursor geometry.
    if (m_editor && pos() != QPoint()) setGeometry(m_editor->viewport()->rect());
}
void SearchHighlightOverlay::paintEvent(QPaintEvent *) {
    if (!m_editor) return;
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
    const auto selections = m_editor->extraSelections(); const auto nativeSelection = m_editor->textCursor();
    const auto *rendered = qobject_cast<MarkdownDocumentView *>(m_editor.data());
    const auto caret = rendered ? rendered->cursorRect() : m_editor->cursorRect();
    bool coveredCaret = false;
    for (const auto &selection : selections) {
        if (!selection.format.boolProperty(Tag) || selection.cursor.isNull()) continue;
        const int first = selection.cursor.selectionStart(), last = selection.cursor.selectionEnd();
        const bool empty = first == last && selection.format.boolProperty(Held);
        for (auto block = m_editor->document()->findBlock(first); block.isValid() && (block.position() < last || empty); block = block.next()) {
            const auto *layout = block.layout(); if (!layout) break;
            for (int i = 0; i < layout->lineCount(); ++i) {
                const auto line = layout->lineAt(i);
                const int begin = qMax(first, block.position() + line.textStart());
                const int end = qMin(last, block.position() + line.textStart() + line.textLength());
                if (begin >= end && !(empty && first >= block.position() + line.textStart()
                    && first <= block.position() + line.textStart() + line.textLength())) continue;
                QTextCursor start(block); start.setPosition(begin); const auto native = m_editor->cursorRect(start);
                const QPointF origin(native.left() - line.cursorToX(begin - block.position()), native.top() - line.y());
                const qreal x1 = origin.x() + line.cursorToX(begin - block.position());
                const qreal x2 = empty ? x1 + 3 : origin.x() + line.cursorToX(end - block.position());
                const QRectF panel(qMin(x1, x2) - 2, native.top(), qMax(3.0, qAbs(x2 - x1)) + 4, native.height());
                if (!panel.intersects(rect())) continue;
                QList<QTextLayout::FormatRange> formats;
                for (const auto &extra : selections) {
                    const int a = qMax(block.position(), extra.cursor.selectionStart());
                    const int b = qMin(block.position() + block.length() - 1, extra.cursor.selectionEnd());
                    if (a < b) formats.append({a - block.position(), b - a, extra.format});
                }
                // A held replacement has a real selection. Restore its native
                // square selection background before drawing the rounded panel.
                if (nativeSelection.hasSelection() && nativeSelection.selectionStart() < end && nativeSelection.selectionEnd() > begin) {
                    auto background = m_editor->palette().brush(QPalette::Base);
                    if (qobject_cast<MarkdownDocumentView *>(m_editor.data()) && start.charFormat().fontFixedPitch()
                        && !block.blockFormat().hasProperty(QTextFormat::BlockCodeFence)) background = MarkdownTypography::codeBackground(true);
                    painter.save(); painter.setClipRect(panel); painter.fillRect(panel, background);
                    const auto bounds = m_editor->document()->documentLayout()->blockBoundingRect(block);
                    const QPointF offset = origin - bounds.topLeft();
                    painter.translate(offset);
                    QAbstractTextDocumentLayout::PaintContext context; context.palette = m_editor->palette();
                    context.clip = panel.translated(-offset); context.cursorPosition = -1;
                    for (const auto &extra : selections) context.selections.append({extra.cursor, extra.format});
                    m_editor->document()->documentLayout()->draw(&painter, context); painter.restore();
                }
                QPainterPath shape; shape.addRoundedRect(panel, 4, 4);
                painter.fillPath(shape, selection.format.property(Background).value<QColor>());
                painter.save(); painter.setClipPath(shape, Qt::IntersectClip);
                // QTextLayout::draw adds its document position internally.
                // Native cursor geometry already includes that translation.
                layout->draw(&painter, origin - layout->position(), formats, panel); painter.restore();
                coveredCaret |= panel.intersects(caret);
            }
            if (empty) break;
        }
    }
    if (coveredCaret && m_editor->hasFocus() && !nativeSelection.hasSelection() && caret.width() > 0 && m_caretVisible)
        painter.fillRect(caret, m_editor->palette().brush(QPalette::Text));
}
