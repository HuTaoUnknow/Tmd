#include "MarkdownEditor.h"
#include "ImageInput.h"
#include "MarkdownTypography.h"
#include "MarkdownFenceCompleter.h"
#include "core/MarkdownSettings.h"
#include <QFontDatabase>
#include <QMimeData>
#include <QDropEvent>
#include <QScrollBar>
#include <QWheelEvent>

MarkdownEditor::MarkdownEditor(QWidget *parent) : QTextEdit(parent) {
    setObjectName("markdownEditor"); setAcceptRichText(false);
    setFont(MarkdownTypography::codeFont());
    document()->setDocumentMargin(28); setTabStopDistance(32);
    setPlaceholderText(QStringLiteral("Markdown 原文。可直接编辑、粘贴或拖入图片。"));
    m_fenceCompleter = new MarkdownFenceCompleter(this,
        [this] { return MarkdownFenceCompleter::Context(source(), textCursor().position()); },
        [this](int start, int length, const QString &language) {
            auto cursor = textCursor(); cursor.beginEditBlock(); cursor.setPosition(start);
            cursor.setPosition(start + length, QTextCursor::KeepAnchor); cursor.insertText(language); cursor.endEditBlock();
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
