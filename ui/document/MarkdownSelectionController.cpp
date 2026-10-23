#include "ui/document/MarkdownSelectionController.h"
#include "core/markdown/MarkdownSelectionEditing.h"
#include <QKeyEvent>
#include <QInputMethodEvent>
#include <QLabel>
#include <QTextEdit>
#include <QTimer>
#include <utility>

MarkdownSelectionController::MarkdownSelectionController(QTextEdit *editor, Read read, Apply apply)
    : QObject(editor), m_editor(editor), m_preview(new QLabel(editor->viewport())), m_read(std::move(read)), m_apply(std::move(apply)) {
    m_preview->setObjectName("selectionWrapPreview"); m_preview->setTextFormat(Qt::PlainText);
    m_preview->setAttribute(Qt::WA_TransparentForMouseEvents); m_preview->setWordWrap(true);
    m_preview->setStyleSheet("QLabel { color: #e7e7e7; background: #26392e; border: 1px solid #749781; border-radius: 7px; padding: 7px; }");
    m_preview->hide(); editor->installEventFilter(this); editor->viewport()->installEventFilter(this);
    connect(editor, &QTextEdit::textChanged, this, [this] {
        if (!m_markers.isEmpty() && m_read().source != m_pending.source) clearPending();
        // Rendered text changes can be emitted before its source map is ready.
        QTimer::singleShot(0, this, [this] {
            if (!m_markers.isEmpty() && m_read().source != m_pending.source) clearPending();
        });
    });
}
bool MarkdownSelectionController::unchanged(const Context &a, const Context &b) const {
    return a.source == b.source && a.anchor == b.anchor && a.caret == b.caret;
}
void MarkdownSelectionController::clearPending() { m_markers.clear(); m_preview->hide(); }
void MarkdownSelectionController::reset() { clearPending(); m_stage = 0; }
void MarkdownSelectionController::showPreview() {
    const int first = qMin(m_pending.anchor, m_pending.caret), length = qAbs(m_pending.caret - m_pending.anchor);
    QString content = m_pending.source.mid(first, length);
    if (content.size() > 80) content = content.left(77) + QStringLiteral("…");
    auto closing = MarkdownSelectionEditing::closingMarker(m_markers);
    if (closing.isEmpty()) closing = m_markers;
    m_preview->setText(m_markers + content + closing + QStringLiteral("\nEnter 确认 · Esc 取消"));
    m_preview->setMaximumWidth(qMax(100, m_editor->viewport()->width() - 12)); m_preview->adjustSize();
    const auto caret = m_editor->cursorRect(); const auto area = m_editor->viewport()->rect();
    m_preview->move(qBound(0, caret.left(), qMax(0, area.width() - m_preview->width())),
        qBound(0, caret.bottom() + 5, qMax(0, area.height() - m_preview->height())));
    m_preview->show(); m_preview->raise();
}
bool MarkdownSelectionController::eventFilter(QObject *object, QEvent *event) {
    if (!m_editor) return false;
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusOut || event->type() == QEvent::Hide) reset();
    if (event->type() == QEvent::InputMethod) {
        const auto *input = static_cast<QInputMethodEvent *>(event);
        if (!input->commitString().isEmpty() || !input->preeditString().isEmpty() || input->replacementLength()) reset();
    }
    if (object != m_editor || (event->type() != QEvent::KeyPress && event->type() != QEvent::ShortcutOverride)) return false;
    auto *key = static_cast<QKeyEvent *>(event);
    if (key->key() == Qt::Key_Control || key->key() == Qt::Key_Shift || key->key() == Qt::Key_Alt
        || key->key() == Qt::Key_Meta || key->key() == Qt::Key_AltGr) return false;
    const auto context = m_read();
    if (!m_markers.isEmpty() && !unchanged(context, m_pending)) clearPending();
    if (key->matches(QKeySequence::SelectAll)) {
        if (m_stage && !unchanged(context, m_lastSelection)) m_stage = 0;
        const auto origin = m_stage ? m_origin : context;
        const auto edit = MarkdownSelectionEditing::selectCode(origin.source, origin.anchor, origin.caret, qMin(m_stage, 2));
        if (edit.handled) {
            if (event->type() == QEvent::KeyPress) {
                clearPending(); m_origin = origin; m_apply(edit); m_lastSelection = m_read(); ++m_stage;
            }
            event->accept(); return true;
        }
    }
    if (event->type() == QEvent::ShortcutOverride) {
        if (!m_markers.isEmpty()) { event->accept(); return true; }
        return false;
    }
    m_stage = 0;
    const bool typing = (key->modifiers() == Qt::NoModifier || key->modifiers() == Qt::ShiftModifier) && !m_editor->isReadOnly();
    if (typing && context.anchor != context.caret && MarkdownSelectionEditing::isMarkerInput(key->text())) {
        if (m_markers.isEmpty()) m_pending = context;
        m_markers += key->text(); showPreview(); event->accept(); return true;
    }
    if (m_markers.isEmpty()) return false;
    if (key->key() == Qt::Key_Escape) { clearPending(); event->accept(); return true; }
    if (typing && key->key() == Qt::Key_Backspace) {
        m_markers.chop(1); if (m_markers.isEmpty()) clearPending(); else showPreview(); event->accept(); return true;
    }
    if (typing && (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) {
        const auto edit = MarkdownSelectionEditing::wrap(context.source, context.anchor, context.caret, m_markers);
        if (edit.handled) { clearPending(); m_apply(edit); }
        else { m_preview->setText(m_preview->text() + QStringLiteral("\n请补齐成对的语法符号")); m_preview->adjustSize(); }
        event->accept(); return true;
    }
    clearPending(); return false;
}
