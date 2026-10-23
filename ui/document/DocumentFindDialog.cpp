#include "ui/document/DocumentFindDialog.h"
#include "ui/document/MarkdownDocumentView.h"
#include "ui/document/SearchHighlightOverlay.h"
#include "core/markdown/editing/TextPatch.h"
#include <QApplication>
#include <QButtonGroup>
#include <QFrame>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QScrollBar>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

DocumentFindDialog::DocumentFindDialog(QWidget *parent) : QDialog(parent, Qt::Tool | Qt::FramelessWindowHint),
    m_query(new QLineEdit(this)), m_replacement(new QLineEdit(this)), m_status(new QLabel(this)),
    m_previous(new QToolButton(this)), m_next(new QToolButton(this)),
    m_findMode(new QToolButton(this)), m_replaceMode(new QToolButton(this)), m_close(new QToolButton(this)),
    m_replaceRow(new QWidget(this)), m_refresh(new QTimer(this)) {
    setObjectName("documentFindDialog"); setWindowTitle(QStringLiteral("查找 / 替换")); setModal(false);
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("QDialog#documentFindDialog { background: transparent; }"
        "QToolButton#documentFindClose { border: 1px solid #48514b; border-radius: 8px; padding: 3px; }"
        "QToolButton#documentFindClose:hover { background: #54413b; border-color: #9e7665; }");
    m_query->setObjectName("documentFindInput"); m_query->setPlaceholderText(QStringLiteral("查找当前文档"));
    m_replacement->setObjectName("documentReplaceInput"); m_replacement->setPlaceholderText(QStringLiteral("替换为（Enter 替换当前项）"));
    for (auto *input : {m_query, m_replacement}) { input->setClearButtonEnabled(true); input->setMinimumWidth(200); }
    m_status->setObjectName("documentFindStatus"); m_status->setMinimumWidth(72); m_status->setAlignment(Qt::AlignCenter);
    m_previous->setObjectName("documentFindPrevious"); m_previous->setText(QStringLiteral("↑"));
    m_previous->setToolTip(QStringLiteral("上一个（↑ / Shift+Enter）"));
    m_next->setObjectName("documentFindNext"); m_next->setText(QStringLiteral("↓"));
    m_next->setToolTip(QStringLiteral("下一个（↓；查找模式 Enter）"));
    auto *modes = new QFrame(this); modes->setObjectName("documentFindModeFrame"); modes->setFrameShape(QFrame::StyledPanel);
    auto *modeLayout = new QHBoxLayout(modes); modeLayout->setContentsMargins(3, 3, 3, 3); modeLayout->setSpacing(2);
    auto *group = new QButtonGroup(this); group->setExclusive(true);
    m_findMode->setObjectName("documentFindMode"); m_findMode->setText(QStringLiteral("查找"));
    m_replaceMode->setObjectName("documentReplaceMode"); m_replaceMode->setText(QStringLiteral("替换"));
    for (auto *button : {m_findMode, m_replaceMode}) {
        button->setCheckable(true); button->setToolTip(QStringLiteral("Tab 切换查找 / 替换"));
        group->addButton(button); modeLayout->addWidget(button);
    }
    m_findMode->setChecked(true); m_close->setObjectName("documentFindClose");
    m_close->setToolTip(QStringLiteral("关闭（Esc）")); m_close->setAccessibleName(QStringLiteral("关闭查找"));
    m_close->setFixedSize(28, 28);
    QPixmap icon(20, 20); icon.fill(Qt::transparent);
    { QPainter painter(&icon); painter.setRenderHint(QPainter::Antialiasing);
      painter.setPen(QPen(QColor("#e7e7e7"), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      painter.drawLine(QPointF(5, 5), QPointF(15, 15)); painter.drawLine(QPointF(15, 5), QPointF(5, 15)); }
    m_close->setIcon(QIcon(icon)); m_close->setIconSize(QSize(20, 20));
    auto *layout = new QVBoxLayout(this); layout->setContentsMargins(10, 10, 10, 10); layout->setSpacing(8);
    auto *header = new QHBoxLayout; header->addWidget(modes); header->addStretch(); header->addWidget(m_close);
    layout->addLayout(header);
    auto *findRow = new QHBoxLayout; findRow->setSpacing(6);
    findRow->addWidget(m_query, 1); findRow->addWidget(m_status); findRow->addWidget(m_previous); findRow->addWidget(m_next);
    layout->addLayout(findRow);
    auto *replaceLayout = new QHBoxLayout(m_replaceRow); replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->addWidget(m_replacement); layout->addWidget(m_replaceRow); m_replaceRow->hide();
    for (auto *widget : QList<QWidget *>{m_query, m_replacement, m_previous, m_next, m_findMode, m_replaceMode, m_close})
        widget->installEventFilter(this);
    resize(460, layout->sizeHint().height()); setFixedHeight(layout->sizeHint().height());
    m_refresh->setSingleShot(true);
    connect(m_refresh, &QTimer::timeout, this, [this] { refresh(false, true); });
    connect(m_query, &QLineEdit::textChanged, this, [this] { releaseHeld(); refresh(true, false); });
    const auto enter = [this] { if (m_replaceEnabled) replaceCurrent(); else navigate(1); };
    connect(m_query, &QLineEdit::returnPressed, this, enter);
    connect(m_replacement, &QLineEdit::returnPressed, this, enter);
    connect(m_previous, &QToolButton::clicked, this, [this] { navigate(-1); });
    connect(m_next, &QToolButton::clicked, this, [this] { navigate(1); });
    connect(m_findMode, &QToolButton::clicked, this, [this] { setReplaceMode(false); });
    connect(m_replaceMode, &QToolButton::clicked, this, [this] { setReplaceMode(true); });
    connect(m_close, &QToolButton::clicked, this, &QDialog::close);
    refresh(false, false);
}
DocumentFindDialog::~DocumentFindDialog() { releaseHeld(); clearHighlights(); }
void DocumentFindDialog::setEditor(QTextEdit *editor) {
    if (m_editor == editor) return;
    releaseHeld(); clearHighlights(); disconnect(m_documentConnection);
    if (m_editor) m_editor->removeEventFilter(this);
    if (m_overlay) delete m_overlay.data();
    m_editor = editor; m_matches.clear(); m_current = -1;
    if (m_editor) {
        m_overlay = new SearchHighlightOverlay(m_editor);
        m_editor->installEventFilter(this);
        m_documentConnection = connect(m_editor->document(), &QTextDocument::contentsChanged, this, [this] {
            if (isVisible() && !m_replacing) m_refresh->start();
        });
    }
    refresh(false, false);
}
void DocumentFindDialog::openSearch() {
    if (!m_editor || !m_editor->isEnabled()) return;
    const QString selected = m_editor->textCursor().selectedText();
    if (!m_hasHeld && !selected.isEmpty() && !selected.contains(QChar::ParagraphSeparator) && !selected.contains(QChar::LineSeparator))
        m_query->setText(selected);
    const bool alreadyVisible = isVisible(); show();
    if (!alreadyVisible) {
        const auto available = m_editor->screen()->availableGeometry();
        const auto point = m_editor->viewport()->mapToGlobal(QPoint(qMax(0, m_editor->viewport()->width() - width()), 8));
        move(qBound(available.left(), point.x(), qMax(available.left(), available.right() - width() + 1)),
            qBound(available.top(), point.y(), qMax(available.top(), available.bottom() - height() + 1)));
    }
    raise(); activateWindow(); m_query->setFocus(); m_query->selectAll(); refresh(true, true);
}
void DocumentFindDialog::setReplaceMode(bool enabled) {
    m_replaceEnabled = enabled; m_findMode->setChecked(!enabled); m_replaceMode->setChecked(enabled);
    m_replaceRow->setVisible(enabled); layout()->invalidate(); layout()->activate();
    setFixedHeight(layout()->sizeHint().height());
    auto *input = enabled ? m_replacement : m_query; input->setFocus(); input->selectAll();
}
bool DocumentFindDialog::eventFilter(QObject *object, QEvent *event) {
    const bool control = object == m_query || object == m_replacement || object == m_previous || object == m_next
        || object == m_findMode || object == m_replaceMode || object == m_close;
    if (control && (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->matches(QKeySequence::Find)) {
            if (event->type() == QEvent::KeyPress) { m_query->setFocus(); m_query->selectAll(); }
            event->accept(); return true;
        }
        if ((key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab)
            && (key->modifiers() == Qt::NoModifier || key->modifiers() == Qt::ShiftModifier)) {
            if (event->type() == QEvent::KeyPress) setReplaceMode(!m_replaceEnabled);
            event->accept(); return true;
        }
        if (event->type() == QEvent::KeyPress && (object == m_query || object == m_replacement)) {
            if ((key->key() == Qt::Key_Up || key->key() == Qt::Key_Down) && key->modifiers() == Qt::NoModifier) {
                navigate(key->key() == Qt::Key_Up ? -1 : 1); return true;
            }
            if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && key->modifiers().testFlag(Qt::ShiftModifier)) {
                navigate(-1); return true;
            }
        }
    }
    if (object == m_editor && event->type() == QEvent::EnabledChange && isVisible()) {
        if (!m_editor->isEnabled()) releaseHeld(); refresh(false, true);
    }
    return QDialog::eventFilter(object, event);
}
void DocumentFindDialog::hideEvent(QHideEvent *event) {
    m_refresh->stop(); releaseHeld(); clearHighlights(); QDialog::hideEvent(event);
    if (m_editor && m_editor->isEnabled()) m_editor->setFocus();
}
void DocumentFindDialog::paintEvent(QPaintEvent *) {
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QApplication::palette().brush(QPalette::Window)); painter.setPen(QPen(QColor("#526357"), 1));
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
}
void DocumentFindDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && event->position().y() < 54) {
        m_dragOffset = event->globalPosition().toPoint() - pos(); event->accept(); return;
    }
    QDialog::mousePressEvent(event);
}
void DocumentFindDialog::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons().testFlag(Qt::LeftButton)) { move(event->globalPosition().toPoint() - m_dragOffset); event->accept(); return; }
    QDialog::mouseMoveEvent(event);
}
void DocumentFindDialog::releaseHeld() {
    if (m_hasHeld && m_editor && m_editor->textCursor() == m_held) {
        if (auto *view = qobject_cast<MarkdownDocumentView *>(m_editor.data())) view->collapseSourceSelection();
        else { auto cursor = m_editor->textCursor(); cursor.clearSelection(); m_editor->setTextCursor(cursor); }
    }
    m_hasHeld = false; m_held = QTextCursor(); m_heldSource.clear(); m_heldSourceRange = {-1, -1};
}
void DocumentFindDialog::resetSearch() {
    releaseHeld(); m_matches.clear(); m_current = -1;
    if (isVisible()) m_refresh->start();
}
void DocumentFindDialog::refresh(bool reveal, bool keepCurrent) {
    if (m_hasHeld && m_heldSourceRange.first >= 0) if (auto *view = qobject_cast<MarkdownDocumentView *>(m_editor.data())) {
        const auto current = view->markdownSource(); const auto patch = tmd::TextPatch::between(m_heldSource, current);
        if (patch.changed()) {
            const int end = patch.start + patch.removed, delta = patch.inserted.size() - patch.removed;
            if (end <= m_heldSourceRange.first) { m_heldSourceRange.first += delta; m_heldSourceRange.second += delta; }
            else if (patch.start < m_heldSourceRange.second) {
                m_heldSourceRange.first = qMin(m_heldSourceRange.first, patch.start);
                m_heldSourceRange.second = qMax(patch.start + int(patch.inserted.size()), m_heldSourceRange.second + delta);
            }
            m_heldSource = current;
        }
        m_held = view->cursorForSourceRange(m_heldSourceRange.first, m_heldSourceRange.second);
    }
    const int position = keepCurrent && m_current >= 0 && m_current < m_matches.size()
        ? m_matches[m_current].selectionStart() : m_editor ? m_editor->textCursor().selectionStart() : 0;
    m_matches.clear(); m_current = -1;
    if (isVisible() && m_editor && m_editor->isEnabled() && !m_query->text().isEmpty()) {
        QTextCursor cursor(m_editor->document());
        for (;;) {
            cursor = m_editor->document()->find(m_query->text(), cursor);
            if (cursor.isNull()) break;
            m_matches.append(cursor);
        }
        if (!m_matches.isEmpty() && !m_hasHeld) {
            const auto found = std::lower_bound(m_matches.cbegin(), m_matches.cend(), position,
                [](const auto &match, int value) { return match.selectionStart() < value; });
            m_current = found == m_matches.cend() ? 0 : int(found - m_matches.cbegin());
        }
    }
    highlight(); if (reveal) revealCurrent();
}
void DocumentFindDialog::navigate(int direction) {
    if (m_refresh->isActive()) { m_refresh->stop(); refresh(false, true); }
    if (m_hasHeld) {
        const int boundary = direction > 0 ? m_held.selectionEnd() : m_held.selectionStart();
        releaseHeld(); refresh(false, false);
        m_current = direction > 0 ? 0 : int(m_matches.size()) - 1;
        if (direction > 0) {
            for (int i = 0; i < m_matches.size(); ++i) if (m_matches[i].selectionStart() >= boundary) { m_current = i; break; }
        } else {
            for (int i = m_matches.size() - 1; i >= 0; --i) if (m_matches[i].selectionEnd() <= boundary) { m_current = i; break; }
        }
    } else if (!m_matches.isEmpty()) m_current = (m_current + direction + m_matches.size()) % m_matches.size();
    highlight(); revealCurrent();
}
void DocumentFindDialog::replaceCurrent() {
    if (!m_editor || !m_editor->isEnabled() || m_editor->isReadOnly()) return;
    if (m_refresh->isActive()) { m_refresh->stop(); refresh(false, true); }
    if (!m_hasHeld && (m_current < 0 || m_current >= m_matches.size())) return;
    QTextCursor target = m_hasHeld ? m_held : m_matches[m_current];
    if (target.isNull() || target.document() != m_editor->document()) return;
    const QString replacement = m_replacement->text(); const int first = target.selectionStart(), length = target.selectionEnd() - first;
    m_replacing = true;
    if (auto *view = qobject_cast<MarkdownDocumentView *>(m_editor.data())) {
        if (!view->replaceDisplayText(first, length, replacement, &m_heldSourceRange)) { m_replacing = false; return; }
        m_heldSource = view->markdownSource();
    } else {
        if (target.selectedText() != replacement) {
            target.beginEditBlock(); target.insertText(replacement);
            target.setPosition(first); target.setPosition(first + replacement.size(), QTextCursor::KeepAnchor); target.endEditBlock();
        }
        m_editor->setTextCursor(target);
    }
    m_held = m_editor->textCursor(); m_hasHeld = true; m_replacing = false; m_refresh->stop();
    refresh(false, true); revealCurrent();
}
void DocumentFindDialog::clearHighlights() {
    if (!m_editor) return;
    auto selections = m_editor->extraSelections();
    selections.erase(std::remove_if(selections.begin(), selections.end(), [](const auto &selection) {
        return selection.format.boolProperty(SearchHighlightOverlay::Tag);
    }), selections.end());
    m_editor->setExtraSelections(selections);
    if (m_overlay) m_overlay->refresh();
}
void DocumentFindDialog::highlight() {
    clearHighlights();
    m_status->setText(m_hasHeld ? QStringLiteral("已替换") : m_query->text().isEmpty() ? QStringLiteral("0/0") : m_matches.isEmpty()
        ? QStringLiteral("未找到") : QStringLiteral("%1/%2").arg(m_current + 1).arg(m_matches.size()));
    m_status->setToolTip(QStringLiteral("查找匹配：%1 项").arg(m_matches.size()));
    m_previous->setEnabled(!m_matches.isEmpty()); m_next->setEnabled(!m_matches.isEmpty());
    if (!m_editor || !isVisible()) return;
    auto selections = m_editor->extraSelections();
    const bool dark = m_editor->palette().color(QPalette::Base).lightness() < 128;
    auto add = [&](const QTextCursor &cursor, bool current, bool held) {
        QTextEdit::ExtraSelection selection; selection.cursor = cursor;
        selection.format.setProperty(SearchHighlightOverlay::Tag, true);
        selection.format.setProperty(SearchHighlightOverlay::Held, held);
        selection.format.setProperty(SearchHighlightOverlay::Background,
            current ? QColor("#b76c20") : dark ? QColor("#65521f") : QColor("#ffe4a3"));
        selection.format.setBackground(QBrush(Qt::NoBrush));
        selection.format.setForeground(current || dark ? QColor("#ffffff") : QColor("#332300"));
        selections.append(selection);
    };
    for (int i = 0; i < m_matches.size(); ++i) {
        if (m_hasHeld && m_matches[i].selectionStart() < m_held.selectionEnd() && m_matches[i].selectionEnd() > m_held.selectionStart()) continue;
        add(m_matches[i], i == m_current, false);
    }
    if (m_hasHeld) add(m_held, true, true);
    m_editor->setExtraSelections(selections);
    if (m_overlay) m_overlay->refresh();
}
void DocumentFindDialog::revealCurrent() {
    if (!m_editor || (!m_hasHeld && (m_current < 0 || m_current >= m_matches.size()))) return;
    const auto rect = m_editor->cursorRect(m_hasHeld ? m_held : m_matches[m_current]);
    if (rect.top() < 12 || rect.bottom() > m_editor->viewport()->height() - 12)
        m_editor->verticalScrollBar()->setValue(m_editor->verticalScrollBar()->value() + rect.center().y() - m_editor->viewport()->height() / 3);
    if (rect.left() < 0 || rect.right() > m_editor->viewport()->width())
        m_editor->horizontalScrollBar()->setValue(m_editor->horizontalScrollBar()->value() + rect.left() - m_editor->viewport()->width() / 2);
}
