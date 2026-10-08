#include "ui/document/MarkdownFenceCompleter.h"
#include "ui/document/MarkdownTypography.h"
#include "core/settings/MarkdownSettings.h"
#include "core/markdown/MarkdownParser.h"
#include "core/markdown/LanguageCatalog.h"
#include <QAbstractItemView>
#include <QCompleter>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QTextEdit>
#include <QTimer>

namespace {
struct Fence {
    int start = -1, length = 0;
    QString prefix, signature;
    int lineEnd = 0;
    QString delimiter, indent;
};
Fence openingFence(const QString &source, int cursor) {
    if (cursor < 0 || cursor > source.size()) return {};
    const int lineStart = cursor ? source.lastIndexOf('\n', cursor - 1) + 1 : 0;
    const int newline = source.indexOf('\n', lineStart);
    const int lineEnd = newline < 0 ? source.size() : newline;
    for (const auto &fence : MarkdownParser::codeFences(source))
        if (fence.start < lineStart && fence.end >= lineStart) return {};
    static const QRegularExpression header("^( {0,3})(`{3,}|~{3,})[ \\t]*([A-Za-z0-9_+.#-]*)[ \\t]*$");
    const auto match = header.match(source.mid(lineStart, lineEnd - lineStart));
    if (!match.hasMatch()) return {};
    const int start = lineStart + match.capturedStart(3), length = match.capturedLength(3);
    if (cursor < start || cursor > start + length) return {};
    return {start, length, source.mid(start, cursor - start), source.left(lineEnd), lineEnd, match.captured(2), match.captured(1)};
}
}

MarkdownFenceCompleter::MarkdownFenceCompleter(QTextEdit *editor, ReadContext read, ReplaceFence replace)
    : QObject(editor), m_editor(editor), m_completer(new QCompleter(this)), m_read(std::move(read)), m_replace(std::move(replace)) {
    m_completer->setObjectName("fenceLanguageCompleter");
    auto *model = new QStandardItemModel(m_completer);
    for (const auto &language : LanguageCatalog::entries()) {
        auto *item = new QStandardItem(language.first + QStringLiteral("  —  ") + language.second);
        item->setData(language.first, Qt::UserRole);
        item->setToolTip(QStringLiteral("代码块语言：%1\n↑↓ 选择 · Tab / 回车补全 · Esc 关闭").arg(language.second));
        model->appendRow(item);
    }
    m_completer->setModel(model); m_completer->setCompletionRole(Qt::UserRole);
    m_completer->setCompletionMode(QCompleter::PopupCompletion); m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setMaxVisibleItems(8); m_completer->setWidget(editor);
    m_completer->popup()->setObjectName("fenceLanguagePopup");
    m_completer->popup()->setAccessibleName(QStringLiteral("代码块语言：↑↓ 选择，Tab 或回车补全，Esc 关闭"));
    m_completer->popup()->setFont(MarkdownTypography::bodyFont());
    m_completer->popup()->setStyleSheet(QStringLiteral(
        "QAbstractItemView { background: #000000; color: #e7e7e7; border: 1px solid #333333; border-radius: 8px; padding: 4px; outline: 0; }"
        "QAbstractItemView::item { padding: 5px 10px; border-radius: 4px; }"
        "QAbstractItemView::item:selected { background: #242424; color: #ffffff; }"));
    connect(m_completer, qOverload<const QString &>(&QCompleter::activated), this, &MarkdownFenceCompleter::complete);
    editor->installEventFilter(this);
    // QCompleter forwards popup keys directly to QWidget::event(), bypassing
    // the editor's filters. Handle selection on the popup before that happens.
    m_completer->popup()->installEventFilter(this);
    m_completer->popup()->viewport()->installEventFilter(this);
    connect(editor, &QTextEdit::textChanged, this, &MarkdownFenceCompleter::scheduleUpdate);
    connect(editor, &QTextEdit::cursorPositionChanged, this, &MarkdownFenceCompleter::scheduleUpdate);
}
bool MarkdownFenceCompleter::isOpeningFence(const QString &source, int cursor) { return openingFence(source, cursor).start >= 0; }
bool MarkdownFenceCompleter::isEditingFence() const { const auto context = m_read(); return isOpeningFence(context.first, context.second); }
void MarkdownFenceCompleter::reset() { m_dismissed.clear(); m_completer->popup()->hide(); }
void MarkdownFenceCompleter::scheduleUpdate() {
    if (m_scheduled) return;
    m_scheduled = true;
    QTimer::singleShot(0, this, [this] { m_scheduled = false; update(); });
}
void MarkdownFenceCompleter::update(bool force) {
    const auto context = m_read(); const auto fence = openingFence(context.first, context.second);
    if (!MarkdownSettingsStore::current().enabled(MarkdownSettings::LanguageCompletion)
        || (!m_editor->hasFocus() && !m_completer->popup()->isVisible()) || !m_editor->isVisible() || m_editor->isReadOnly() || m_editor->textCursor().hasSelection()
        || fence.start < 0 || (!force && fence.signature == m_dismissed)) { m_completer->popup()->hide(); return; }
    auto *popup = m_completer->popup();
    const QString selected = popup->isVisible() && m_completer->completionPrefix() == fence.prefix
        ? popup->currentIndex().data(Qt::UserRole).toString() : QString();
    m_completer->setCompletionPrefix(fence.prefix);
    if (!m_completer->completionCount()) { m_completer->popup()->hide(); return; }
    auto rect = m_editor->cursorRect(); rect.setWidth(330);
    m_completer->complete(rect);
    int row = 0;
    for (int candidate = 0; !selected.isEmpty() && candidate < m_completer->completionModel()->rowCount(); ++candidate)
        if (m_completer->completionModel()->index(candidate, 0).data(Qt::UserRole).toString() == selected) { row = candidate; break; }
    popup->setCurrentIndex(m_completer->completionModel()->index(row, 0)); popup->scrollTo(popup->currentIndex());
}
void MarkdownFenceCompleter::complete(const QString &language) {
    const auto context = m_read(); const auto fence = openingFence(context.first, context.second);
    m_completer->popup()->hide(); if (fence.start < 0) return;
    // Reuse a matching closing fence and preserve its code. A new block gets
    // an editable code line and a paragraph after the closing delimiter.
    int closeStart = -1, closeEnd = context.first.size();
    for (const auto &parsed : MarkdownParser::codeFences(context.first))
        if (parsed.headerEnd == fence.lineEnd) { closeStart = parsed.closeStart; closeEnd = parsed.end; break; }
    const int bodyStart = qMin(fence.lineEnd + 1, int(context.first.size()));
    QString body = context.first.mid(bodyStart, (closeStart < 0 ? context.first.size() : closeStart) - bodyStart);
    if (body.isEmpty() || !body.endsWith('\n')) body += '\n';
    const QString closer = closeStart < 0 ? fence.indent + fence.delimiter : context.first.mid(closeStart, closeEnd - closeStart);
    QString replacement = language + context.first.mid(fence.start + fence.length, fence.lineEnd - fence.start - fence.length) + '\n' + body + closer;
    if (closeEnd == context.first.size()) replacement += "\n\n";
    else if (context.first.mid(closeEnd, 2) != "\n\n") replacement += '\n';
    const int bodyCaret = fence.start + language.size() + fence.lineEnd - fence.start - fence.length + 1;
    m_replace(fence.start, closeEnd - fence.start, replacement, bodyCaret);
    const auto changed = m_read(); m_dismissed = openingFence(changed.first, changed.second).signature;
    m_editor->setFocus(Qt::PopupFocusReason);
}
bool MarkdownFenceCompleter::eventFilter(QObject *object, QEvent *event) {
    auto *popup = m_completer->popup();
    if (object != m_editor && object != popup && object != popup->viewport()) return QObject::eventFilter(object, event);
    if (object == m_editor && (event->type() == QEvent::Hide
        || (event->type() == QEvent::FocusOut && static_cast<QFocusEvent *>(event)->reason() != Qt::PopupFocusReason))) popup->hide();
    if (event->type() == QEvent::ShortcutOverride && popup->isVisible()) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down || key->key() == Qt::Key_Return
            || key->key() == Qt::Key_Enter || key->key() == Qt::Key_Tab || key->key() == Qt::Key_Escape) {
            key->accept(); return true;
        }
    }
    if (event->type() != QEvent::KeyPress) return QObject::eventFilter(object, event);
    auto *key = static_cast<QKeyEvent *>(event);
    if (key->key() == Qt::Key_Space && key->modifiers() == Qt::ControlModifier) { update(true); return true; }
    if (!m_completer->popup()->isVisible()) return false;
    if (key->key() == Qt::Key_Escape) {
        const auto context = m_read(); m_dismissed = openingFence(context.first, context.second).signature;
        m_completer->popup()->hide(); m_editor->setFocus(Qt::PopupFocusReason); return true;
    }
    if (key->key() == Qt::Key_Tab || key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
        const QString language = m_completer->popup()->currentIndex().data(Qt::UserRole).toString();
        if (!language.isEmpty()) complete(language);
        return true;
    }
    if (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down) {
        const int rows = m_completer->completionModel()->rowCount();
        if (!rows) return true;
        const int row = m_completer->popup()->currentIndex().row();
        const bool down = key->key() == Qt::Key_Down;
        const int next = row < 0 ? (down ? 0 : rows - 1) : (row + (down ? 1 : rows - 1)) % rows;
        popup->setCurrentIndex(m_completer->completionModel()->index(next, 0)); popup->scrollTo(popup->currentIndex()); return true;
    }
    return false;
}
