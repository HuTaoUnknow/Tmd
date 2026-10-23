#pragma once
#include <QDialog>
#include <QPointer>
#include <QTextCursor>
#include <QVector>
class QTextEdit;
class QLineEdit;
class QLabel;
class QToolButton;
class QTimer;
class QFrame;
class SearchHighlightOverlay;

// Search highlights are temporary; replacement uses the active view's normal
// editing transaction and keeps the inserted range selected until a search.
class DocumentFindDialog : public QDialog {
    Q_OBJECT
public:
    explicit DocumentFindDialog(QWidget *parent);
    ~DocumentFindDialog() override;
    void setEditor(QTextEdit *editor);
    void openSearch();
    void resetSearch();
private:
    bool eventFilter(QObject *object, QEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void refresh(bool reveal, bool keepCurrent);
    void navigate(int direction);
    void setReplaceMode(bool enabled);
    void replaceCurrent();
    void releaseHeld();
    void highlight();
    void clearHighlights();
    void revealCurrent();
    QPointer<QTextEdit> m_editor;
    QLineEdit *m_query, *m_replacement;
    QLabel *m_status;
    QToolButton *m_previous, *m_next, *m_findMode, *m_replaceMode, *m_close;
    QWidget *m_replaceRow;
    QPointer<SearchHighlightOverlay> m_overlay;
    QTimer *m_refresh;
    QMetaObject::Connection m_documentConnection;
    QVector<QTextCursor> m_matches;
    int m_current = -1;
    bool m_replacing = false, m_hasHeld = false, m_replaceEnabled = false;
    QTextCursor m_held;
    QString m_heldSource;
    QPair<int, int> m_heldSourceRange{-1, -1};
    QPoint m_dragOffset;
};
