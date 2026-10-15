#pragma once
#include <QObject>

class QTextEdit;

// View-only spacing and scroll positioning, shared by source and rendered text.
class MarkdownViewport : public QObject {
public:
    explicit MarkdownViewport(QTextEdit *view);
    int anchorPosition() const;
    void alignPosition(int position);
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    void updateScrollRange();
    QTextEdit *m_view;
    bool m_updating = false;
};
