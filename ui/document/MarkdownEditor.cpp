#include "ui/document/MarkdownEditor.h"
#include "core/markdown/MarkdownImport.h"
#include "ui/document/ImageInput.h"
#include "ui/document/MarkdownTypography.h"
#include "ui/document/MarkdownFenceCompleter.h"
#include "ui/document/MarkdownViewport.h"
#include "ui/document/CodeSyntaxHighlighter.h"
#include "core/settings/MarkdownSettings.h"
#include "core/markdown/MarkdownParser.h"
#include "application/editing/EditCommandRouter.h"
#include "ui/document/QtTextEditAdapter.h"
#include <QFontDatabase>
#include <QMimeData>
#include <QDropEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QTextBlock>
#include <QTextLayout>

MarkdownEditor::MarkdownEditor(QWidget *parent) : QTextEdit(parent) {
    setObjectName("markdownEditor"); setAcceptRichText(false);
    setFont(MarkdownTypography::codeFont());
    document()->setDocumentMargin(28); setTabStopDistance(32);
    setPlaceholderText(QStringLiteral("Markdown 原文。可直接编辑、粘贴或拖入图片。"));
    m_viewport = new MarkdownViewport(this);
    m_highlighter = new CodeSyntaxHighlighter(document(), true);
    m_fenceCompleter = new MarkdownFenceCompleter(this,
        [this] { return MarkdownFenceCompleter::Context(source(), textCursor().position()); },
        [this](int start, int length, const QString &replacement, int caret) {
            auto cursor = textCursor(); cursor.beginEditBlock(); cursor.setPosition(start);
            cursor.setPosition(start + length, QTextCursor::KeepAnchor); cursor.insertText(replacement); cursor.endEditBlock();
            cursor.setPosition(caret);
            setTextCursor(cursor);
        });
}
void MarkdownEditor::setSource(const QString &source) { m_fenceCompleter->reset(); setPlainText(source); document()->setModified(false); }
int MarkdownEditor::viewportSourcePosition() const { return m_viewport->anchorPosition(); }
void MarkdownEditor::alignSourcePosition(int position) { m_viewport->alignPosition(position); }
void MarkdownEditor::refreshAppearance() {
    const auto &settings = MarkdownSettingsStore::current(); const bool modified = document()->isModified();
    setFont(MarkdownTypography::codeFont(m_zoomPercent)); setTabStopDistance(settings.number(MarkdownSettings::TabWidth));
    setStyleSheet(QString("QTextEdit#markdownEditor { background: %1; color: %2; }")
        .arg(settings.color(MarkdownSettings::PageBackground).name(), settings.color(MarkdownSettings::TextColor).name()));
    document()->setModified(modified); m_fenceCompleter->reset();
    m_highlighter->refresh();
}
void MarkdownEditor::setZoomPercent(int percent) {
    percent = qBound(50, percent, 200); if (percent == m_zoomPercent) return;
    const QTextCursor anchor = cursorForPosition(QPoint(30, viewport()->height() / 3)); const int y = cursorRect(anchor).top();
    const bool modified = document()->isModified(); m_zoomPercent = percent; setFont(MarkdownTypography::codeFont(percent));
    document()->setModified(modified); verticalScrollBar()->setValue(verticalScrollBar()->value() + cursorRect(anchor).top() - y);
}
void MarkdownEditor::wheelEvent(QWheelEvent *event) {
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        const int steps = MarkdownTypography::wheelSteps(event, m_wheelRemainder);
        if (steps) emit zoomRequested(qBound(50, m_zoomPercent + steps * 10, 200));
        event->accept(); return;
    }
    QTextEdit::wheelEvent(event);
}
void MarkdownEditor::goToPosition(int position) {
    QTextCursor cursor(document()); cursor.setPosition(qBound(0, position, document()->characterCount() - 1));
    setTextCursor(cursor); ensureCursorVisible();
    verticalScrollBar()->setValue(verticalScrollBar()->value() + cursorRect(cursor).top() - viewport()->height() * 0.28); setFocus();
}
void MarkdownEditor::keyPressEvent(QKeyEvent *event) {
    const auto apply = [this](const MarkdownEdit &edit) {
        auto cursor = textCursor(); QString error;
        if (!tmd::QtTextEditAdapter::apply(cursor, source(), edit, &error)) { emit editRejected(error); return; }
        setTextCursor(cursor); ensureCursorVisible();
    };
    if (event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::ShiftModifier) {
        const bool shift = event->modifiers() == Qt::ShiftModifier;
        const bool tab = event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab;
        const bool row = shift && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter);
        if (tab || row) {
            const QString text = source(); const int caret = textCursor().position();
            if (const auto cell = tmd::EditCommandRouter::tableCellAt(text, caret)) {
                using Command = tmd::EditCommand;
                tmd::EditRequest request{text, {textCursor().anchor(), caret}, row ? Command::AddRow
                    : shift || event->key() == Qt::Key_Backtab ? Command::AddColumn : Command::NextCell, tmd::ViewMode::Source};
                request.tableCell = cell;
                apply(tmd::EditCommandRouter::dispatch(request).edit);
                event->accept(); return;
            }
        }
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && event->modifiers() == Qt::ShiftModifier) {
        QKeyEvent normal(QEvent::KeyPress, event->key(), Qt::NoModifier, event->text(), event->isAutoRepeat(), event->count());
        keyPressEvent(&normal); event->accept(); return;
    }
    const bool indent = (event->key() == Qt::Key_Tab && (event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::ShiftModifier))
        || (event->key() == Qt::Key_Backtab && (event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::ShiftModifier));
    const bool newline = (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && event->modifiers() == Qt::NoModifier;
    const bool backspace = event->key() == Qt::Key_Backspace && event->modifiers() == Qt::NoModifier && !textCursor().hasSelection();
    if (indent || newline || backspace) {
        const QString text = source();
        using Command = tmd::EditCommand;
        const bool outdent = event->key() == Qt::Key_Backtab || event->modifiers() == Qt::ShiftModifier;
        const auto edit = tmd::EditCommandRouter::dispatch({text, {textCursor().anchor(), textCursor().position()},
            backspace ? Command::Backspace : indent ? (outdent ? Command::Outdent : Command::Indent) : Command::Newline,
            tmd::ViewMode::Source}).edit;
        if (edit.handled) {
            apply(edit);
            event->accept(); return;
        }
    }
    if (event->key() == Qt::Key_Down && event->modifiers() == Qt::NoModifier && !textCursor().hasSelection()) {
        const auto *layout = textCursor().block().layout();
        if (layout->lineForTextPosition(textCursor().positionInBlock()).lineNumber() + 1 < layout->lineCount()) {
            QTextEdit::keyPressEvent(event); return;
        }
        const QString text = source(); const int caret = textCursor().position();
        for (const auto &fence : MarkdownParser::codeFences(text)) {
            if (!fence.closed() || caret < fence.bodyStart || caret >= fence.closeStart) continue;
            const int rowEnd = text.indexOf('\n', caret);
            if (rowEnd >= 0 && rowEnd + 1 < fence.closeStart) break;
            int next = fence.end;
            if (next < text.size() && text[next] == '\n') ++next;
            auto cursor = textCursor();
            if (next == fence.end) { cursor.movePosition(QTextCursor::End); cursor.insertText("\n"); ++next; }
            cursor.setPosition(next); setTextCursor(cursor); ensureCursorVisible(); event->accept(); return;
        }
    }
    QTextEdit::keyPressEvent(event);
}
bool MarkdownEditor::canInsertFromMimeData(const QMimeData *data) const {
    return ImageInput::canInsert(data, m_hostedLinks) || QTextEdit::canInsertFromMimeData(data);
}
void MarkdownEditor::insertFromMimeData(const QMimeData *data) {
    const auto urls = ImageInput::urls(data, m_hostedLinks);
    if (!urls.isEmpty()) { emit imageFilesInserted(urls); return; }
    const QImage image = ImageInput::image(data);
    if (!image.isNull()) { emit clipboardImageInserted(image); return; }
    if (data->hasText()) { textCursor().insertText(MarkdownImport::paste(source(), textCursor().position(), data->text())); return; }
    QTextEdit::insertFromMimeData(data);
}
void MarkdownEditor::dropEvent(QDropEvent *event) {
    if (!ImageInput::canInsert(event->mimeData(), m_hostedLinks)) { QTextEdit::dropEvent(event); return; }
    setTextCursor(cursorForPosition(event->position().toPoint())); insertFromMimeData(event->mimeData());
    event->setDropAction(Qt::CopyAction); event->accept();
}
