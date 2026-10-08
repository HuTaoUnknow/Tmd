#include "ui/document/MarkdownEditor.h"
#include "ui/document/ImageInput.h"
#include "ui/document/MarkdownTypography.h"
#include "ui/document/MarkdownFenceCompleter.h"
#include "core/settings/MarkdownSettings.h"
#include "core/markdown/MarkdownParser.h"
#include <QFontDatabase>
#include <QMimeData>
#include <QDropEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <QKeyEvent>

MarkdownEditor::MarkdownEditor(QWidget *parent) : QTextEdit(parent) {
    setObjectName("markdownEditor"); setAcceptRichText(false);
    setFont(MarkdownTypography::codeFont());
    document()->setDocumentMargin(28); setTabStopDistance(32);
    setPlaceholderText(QStringLiteral("Markdown 原文。可直接编辑、粘贴或拖入图片。"));
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
void MarkdownEditor::refreshAppearance() {
    const auto &settings = MarkdownSettingsStore::current(); const bool modified = document()->isModified();
    setFont(MarkdownTypography::codeFont(m_zoomPercent)); setTabStopDistance(settings.number(MarkdownSettings::TabWidth));
    setStyleSheet(QString("QTextEdit#markdownEditor { background: %1; color: %2; }")
        .arg(settings.color(MarkdownSettings::PageBackground).name(), settings.color(MarkdownSettings::TextColor).name()));
    document()->setModified(modified); m_fenceCompleter->reset();
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
    if (event->key() == Qt::Key_Down && event->modifiers() == Qt::NoModifier && !textCursor().hasSelection()) {
        const QString text = source(); const int caret = textCursor().position();
        for (const auto &fence : MarkdownParser::codeFences(text)) {
            if (!fence.closed() || caret < fence.bodyStart || caret >= fence.closeStart) continue;
            int next = fence.end;
            if (next < text.size() && text[next] == '\n') ++next;
            if (next < text.size() && text[next] == '\n') ++next;
            auto cursor = textCursor();
            if (next == fence.end) { cursor.movePosition(QTextCursor::End); cursor.insertText("\n\n"); next += 2; }
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
    QTextEdit::insertFromMimeData(data);
}
void MarkdownEditor::dropEvent(QDropEvent *event) {
    if (!ImageInput::canInsert(event->mimeData(), m_hostedLinks)) { QTextEdit::dropEvent(event); return; }
    setTextCursor(cursorForPosition(event->position().toPoint())); insertFromMimeData(event->mimeData());
    event->setDropAction(Qt::CopyAction); event->accept();
}
