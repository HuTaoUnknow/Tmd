#pragma once
#include "core/markdown/MarkdownEdit.h"
#include <QTextCursor>

namespace tmd {
class QtTextEditAdapter final {
public:
    // The caller owns the cursor and restores it to its QTextEdit afterwards.
    // Reject stale source rather than overwrite a newer edit.
    static bool apply(QTextCursor &cursor, const QString &expectedSource,
                      const MarkdownEdit &edit, QString *error = nullptr);
    // The rendered editor supplies its pre-edit source caret when available.
    // Otherwise infer it from the same patch used for the undo transaction.
    static bool applyWithHistory(QTextCursor &cursor, const QString &expectedSource,
                                 const MarkdownEdit &edit, int requestedBeforeCaret,
                                 int *actualBeforeCaret, QString *error = nullptr);
};
}
