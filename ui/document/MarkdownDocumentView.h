#pragma once
#include <QTextBrowser>
#include <QImage>
#include <QMap>
#include <memory>
#include "core/markdown/MarkdownImages.h"
#include "core/markdown/MarkdownSourceMap.h"
class ImageLoader;
struct MarkdownEdit;
namespace tmd { class MarkdownSnapshot; }
class MarkdownDocumentView : public QTextBrowser {
    Q_OBJECT
public:
    explicit MarkdownDocumentView(QWidget *parent = nullptr);
    void setMarkdownSource(const QString &documentPath, const QString &source, bool force = false);
    void setHostedLinkInput(bool enabled) { m_hostedLinks = enabled; }
    void goToSourcePosition(int position);
    int viewportSourcePosition() const;
    void alignSourcePosition(int position);
    QStringList visibleInlineMarkers() const;
    QStringList visibleBlockMarkers() const;
    int inlineCursorSourcePosition() const { return m_sourceMap.sourcePosition(textCursor().position()); }
    void restoreSourceCursor(int anchor, int caret);
    QRect cursorRect() const;
    QRect cursorRect(const QTextCursor &cursor) const;
    QString markdownSource() const { return m_source; }
    void setUndoAvailability(bool undo, bool redo) { m_canUndo = undo; m_canRedo = redo; }
    void setZoomPercent(int percent);
    int zoomPercent() const { return m_zoomPercent; }
    void refreshAppearance();
    int loadedImageCount() const { return m_loaded.size(); }
    QStringList imageErrors() const { return m_errors.values(); }
    QVariant loadResource(int type, const QUrl &name) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
public slots:
    void pasteImages();
signals:
    void sourceEditStarted(int sourceCaretPosition);
    void sourceEditFinished();
    void imageInputPositionRequested(int sourcePosition);
    void imageFilesInserted(const QList<QUrl> &files);
    void clipboardImageInserted(const QImage &image);
    void imagesSettled();
    void sourceEdited(const QString &source, int sourceCursorPosition);
    void undoRequested();
    void redoRequested();
    void zoomRequested(int percent);
protected:
    bool canInsertFromMimeData(const QMimeData *data) const override;
    void insertFromMimeData(const QMimeData *data) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    QMimeData *createMimeDataFromSelection() const override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
private:
    void applyMarkdownStyles();
    void applySourceInsertion(const QString &text);
    bool hasSourceSelection() const;
    void replaceSourceSelection(const QString &text);
    void copySourceSelection();
    void clearSourceSelection();
    int sourcePositionAt(const QPoint &point) const;
    void applySourceEdit(const MarkdownEdit &edit);
    void refreshInlineCursorFormat();
    void revealSyntax(const MarkdownSpelling::Region &region, int caret);
    void setInlineBoundary(const MarkdownSourceMap::InlineCode &code, bool atEnd, bool outside);
    void documentEdited(int position, int removed, int added);
    QString displayedText() const;
    void insertImages(const QMimeData *data);
    void loadImage(const QUrl &url, int generation);
    void fitImages();
    QImage placeholder(const QString &message) const;
    QString m_path, m_source;
    std::shared_ptr<const tmd::MarkdownSnapshot> m_snapshot;
    QVector<MarkdownImage> m_references;
    QMap<QUrl, QString> m_sources;
    QMap<QUrl, QImage> m_images, m_loaded;
    QMap<QUrl, QString> m_errors;
    QMap<QUrl, ImageLoader *> m_pending;
    QMap<int, QSizeF> m_requestedSizes;
    int m_generation = 0;
    int m_zoomPercent = 100, m_wheelRemainder = 0;
    bool m_hostedLinks = false, m_preparing = false, m_markupRefresh = false;
    bool m_headingExit = false;
    bool m_rebuilding = false, m_revealScheduled = false;
    MarkdownSpelling::Region m_revealed;
    bool m_canUndo = false, m_canRedo = false;
    bool m_selecting = false;
    int m_selectionAnchor = -1, m_selectionCaret = -1;
    int m_selectionDisplayAnchor = -1, m_selectionDisplayCaret = -1;
    int m_paragraphInsertion = -1;
    QString m_display;
    MarkdownSourceMap m_sourceMap;
    class QTimer *m_markupTimer;
    class MarkdownFenceCompleter *m_fenceCompleter;
    class InlineMarkupDecoration *m_inlineDecorations;
    class MarkdownViewport *m_viewport;
    class CodeBlockDecoration *m_codeDecoration;
    class CodeSyntaxHighlighter *m_highlighter;
};
