#include "MarkdownFenceCompleter.h"
#include "MarkdownTypography.h"
#include "core/MarkdownSettings.h"
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
};
Fence openingFence(const QString &source, int cursor) {
    if (cursor < 0 || cursor > source.size()) return {};
    const int lineStart = cursor ? source.lastIndexOf('\n', cursor - 1) + 1 : 0;
    const int newline = source.indexOf('\n', lineStart);
    const int lineEnd = newline < 0 ? source.size() : newline;
    static const QRegularExpression marker("^ {0,3}(`{3,}|~{3,})(.*)$");
    QChar active;
    int fenceLength = 0;
    for (int start = 0; start < lineStart;) {
        int end = source.indexOf('\n', start); if (end < 0) end = source.size();
        const auto match = marker.match(source.mid(start, end - start));
        if (match.hasMatch()) {
            const QString delimiter = match.captured(1), info = match.captured(2);
            if (active.isNull()) {
                if (delimiter[0] != '`' || !info.contains('`')) { active = delimiter[0]; fenceLength = delimiter.size(); }
            } else if (delimiter[0] == active && delimiter.size() >= fenceLength && info.trimmed().isEmpty()) active = {};
        }
        start = end + 1;
    }
    if (!active.isNull()) return {};
    static const QRegularExpression header("^ {0,3}(?:`{3,}|~{3,})[ \\t]*([A-Za-z0-9_+.#-]*)[ \\t]*$");
    const auto match = header.match(source.mid(lineStart, lineEnd - lineStart));
    if (!match.hasMatch()) return {};
    const int start = lineStart + match.capturedStart(1), length = match.capturedLength(1);
    if (cursor < start || cursor > start + length) return {};
    return {start, length, source.mid(start, cursor - start), source.left(lineEnd)};
}
}

MarkdownFenceCompleter::MarkdownFenceCompleter(QTextEdit *editor, ReadContext read, ReplaceLanguage replace)
    : QObject(editor), m_editor(editor), m_completer(new QCompleter(this)), m_read(std::move(read)), m_replace(std::move(replace)) {
    m_completer->setObjectName("fenceLanguageCompleter");
    auto *model = new QStandardItemModel(m_completer);
    const QList<QPair<QString, QString>> languages = {
        {"plaintext", "纯文本"}, {"python", "Python"}, {"cpp", "C++"}, {"c", "C"}, {"csharp", "C#"},
        {"java", "Java"}, {"javascript", "JavaScript"}, {"typescript", "TypeScript"}, {"js", "JavaScript"}, {"ts", "TypeScript"},
        {"sql", "SQL"}, {"bash", "Bash"}, {"shell", "Shell"}, {"powershell", "PowerShell"}, {"html", "HTML"},
        {"css", "CSS"}, {"scss", "SCSS"}, {"json", "JSON"}, {"yaml", "YAML"}, {"xml", "XML"},
        {"markdown", "Markdown"}, {"go", "Go"}, {"rust", "Rust"}, {"kotlin", "Kotlin"}, {"php", "PHP"},
        {"swift", "Swift"}, {"ruby", "Ruby"}, {"vue", "Vue"}, {"dockerfile", "Dockerfile"}, {"ini", "INI"},
        {"toml", "TOML"}, {"diff", "Diff"}, {"mermaid", "Mermaid"}, {"latex", "LaTeX"}
    };
    for (const auto &language : languages) {
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
        "QAbstractItemView { background: #303030; color: #e7e7e7; border: 1px solid #555b57; padding: 4px; }"
        "QAbstractItemView::item { padding: 5px 10px; }"
        "QAbstractItemView::item:selected { background: #40564a; color: #ffffff; }"));
    connect(m_completer, qOverload<const QString &>(&QCompleter::activated), this, &MarkdownFenceCompleter::complete);
    editor->installEventFilter(this);
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
    m_completer->setCompletionPrefix(fence.prefix);
    if (!m_completer->completionCount()) { m_completer->popup()->hide(); return; }
    auto rect = m_editor->cursorRect(); rect.setWidth(330);
    m_completer->complete(rect); m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0, 0));
}
void MarkdownFenceCompleter::complete(const QString &language) {
    const auto context = m_read(); const auto fence = openingFence(context.first, context.second);
    m_completer->popup()->hide(); if (fence.start < 0) return;
    m_replace(fence.start, fence.length, language);
    const auto changed = m_read(); m_dismissed = openingFence(changed.first, changed.second).signature;
}
bool MarkdownFenceCompleter::eventFilter(QObject *object, QEvent *event) {
    if (object != m_editor) return QObject::eventFilter(object, event);
    if (event->type() == QEvent::Hide || (event->type() == QEvent::FocusOut && static_cast<QFocusEvent *>(event)->reason() != Qt::PopupFocusReason)) m_completer->popup()->hide();
    if (event->type() != QEvent::KeyPress) return QObject::eventFilter(object, event);
    auto *key = static_cast<QKeyEvent *>(event);
    if (key->key() == Qt::Key_Space && key->modifiers() == Qt::ControlModifier) { update(true); return true; }
    if (!m_completer->popup()->isVisible()) return false;
    if (key->key() == Qt::Key_Escape) {
        const auto context = m_read(); m_dismissed = openingFence(context.first, context.second).signature;
        m_completer->popup()->hide(); return true;
    }
    if (key->key() == Qt::Key_Tab || key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
        const QString language = m_completer->popup()->currentIndex().data(Qt::UserRole).toString();
        if (!language.isEmpty()) complete(language);
        return true;
    }
    if (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down) {
        const int rows = m_completer->completionModel()->rowCount();
        const int row = m_completer->popup()->currentIndex().row();
        const int next = (qMax(0, row) + (key->key() == Qt::Key_Down ? 1 : rows - 1)) % rows;
        m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(next, 0)); return true;
    }
    return false;
}
