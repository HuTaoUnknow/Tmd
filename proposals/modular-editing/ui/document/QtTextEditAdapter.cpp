#include "ui/document/QtTextEditAdapter.h"
#include "core/markdown/editing/TextPatch.h"
#include <QTextDocument>

namespace tmd::proposal {
bool QtTextEditAdapter::apply(QTextCursor &cursor, const QString &expected, const MarkdownEdit &edit, QString *error) {
    const auto fail = [error](const QString &message) { if (error) *error = message; return false; };
    if (!cursor.document() || !edit.handled) return fail(QStringLiteral("没有可应用的编辑交易。"));
    const QString current = cursor.document()->toRawText().replace(QChar::ParagraphSeparator, '\n');
    if (current != expected) return fail(QStringLiteral("文档已改变，拒绝应用过期交易。"));
    if (edit.anchor < 0 || edit.caret < 0 || edit.anchor > edit.source.size() || edit.caret > edit.source.size())
        return fail(QStringLiteral("交易的源码选区无效。"));
    if (edit.changed != (current != edit.source)) return fail(QStringLiteral("交易内容与 changed 标记不一致。"));
    const auto patch = TextPatch::between(current, edit.source);
    if (patch.changed()) {
        cursor.beginEditBlock();
        cursor.setPosition(patch.start);
        cursor.setPosition(patch.start + patch.removed, QTextCursor::KeepAnchor);
        cursor.insertText(patch.inserted);
    }
    cursor.setPosition(edit.anchor);
    cursor.setPosition(edit.caret, QTextCursor::KeepAnchor);
    if (patch.changed()) cursor.endEditBlock();
    if (error) error->clear();
    return true;
}
}
