#pragma once
#include <QTextEdit>
#include <QTextDocument>
#include <QImage>
#include <QUrl>
// The editable source is always plain Markdown. Rendering uses a separate document.
class MarkdownEditor : public QTextEdit {
    Q_OBJECT
public:
    explicit MarkdownEditor(QWidget *parent = nullptr);
    void setSource(const QString &source);
    QString source() const { return document()->toRawText().replace(QChar::ParagraphSeparator, '\n'); }
    void goToPosition(int position);
    void setHostedLinkInput(bool enabled) { m_hostedLinks = enabled; }
    void setZoomPercent(int percent);
    int zoomPercent() const { return m_zoomPercent; }
signals:
    void zoomRequested(int percent);
    void imageFilesInserted(const QList<QUrl> &files);
    void clipboardImageInserted(const QImage &image);
protected:
    bool canInsertFromMimeData(const QMimeData *source) const override;
    void insertFromMimeData(const QMimeData *source) override;
    void dropEvent(QDropEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
private:
    bool m_hostedLinks = false;
    int m_zoomPercent = 100, m_wheelRemainder = 0;
};
