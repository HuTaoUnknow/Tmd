#pragma once
#include <QObject>
#include <QPointer>
#include <functional>
#include "core/markdown/MarkdownEdit.h"
class QTextEdit;
class QLabel;

// UI state only: pending delimiter input and successive Ctrl+A stages.
class MarkdownSelectionController : public QObject {
public:
    struct Context { QString source; int anchor = 0, caret = 0; };
    using Read = std::function<Context()>;
    using Apply = std::function<void(const MarkdownEdit &)>;
    MarkdownSelectionController(QTextEdit *editor, Read read, Apply apply);
    void reset();
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    void clearPending();
    void showPreview();
    bool unchanged(const Context &a, const Context &b) const;
    QPointer<QTextEdit> m_editor;
    QLabel *m_preview;
    Read m_read;
    Apply m_apply;
    QString m_markers;
    Context m_pending, m_origin, m_lastSelection;
    int m_stage = 0;
};
