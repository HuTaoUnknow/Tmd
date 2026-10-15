#include "ui/document/MarkdownViewport.h"
#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QScrollBar>
#include <QTextEdit>
#include <QTimer>
#include <QtMath>
#include <QtCore/qscopeguard.h>

MarkdownViewport::MarkdownViewport(QTextEdit *view) : QObject(view), m_view(view) {
    view->viewport()->installEventFilter(this);
    connect(view->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this] { updateScrollRange(); });
    connect(view->document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
        this, [this] { updateScrollRange(); });
    updateScrollRange();
}
void MarkdownViewport::updateScrollRange() {
    if (m_updating) return;
    m_updating = true;
    const auto reset = qScopeGuard([this] { m_updating = false; });
    const int height = m_view->viewport()->height();
    const int contentHeight = qCeil(m_view->document()->documentLayout()->documentSize().height());
    m_view->verticalScrollBar()->setMaximum(qMax(0, contentHeight + height / 2 - height));
}
bool MarkdownViewport::eventFilter(QObject *object, QEvent *event) {
    if (object == m_view->viewport() && event->type() == QEvent::Resize)
        QTimer::singleShot(0, this, [this] { updateScrollRange(); });
    return QObject::eventFilter(object, event);
}
int MarkdownViewport::anchorPosition() const {
    return m_view->cursorForPosition(QPoint(qRound(m_view->document()->documentMargin()) + 2,
        m_view->viewport()->height() / 3)).position();
}
void MarkdownViewport::alignPosition(int position) {
    updateScrollRange();
    QTextCursor cursor(m_view->document());
    cursor.setPosition(qBound(0, position, m_view->document()->characterCount() - 1));
    auto *scroll = m_view->verticalScrollBar();
    scroll->setValue(scroll->value() + m_view->cursorRect(cursor).center().y() - m_view->viewport()->height() / 3);
}
