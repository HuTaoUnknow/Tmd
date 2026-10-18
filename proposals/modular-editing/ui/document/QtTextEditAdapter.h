#pragma once
#include "core/markdown/MarkdownEdit.h"
#include <QTextCursor>

namespace tmd::proposal {
class QtTextEditAdapter final {
public:
    // The caller owns the cursor and restores it to its QTextEdit afterwards.
    // Reject stale source rather than overwrite a newer edit.
    static bool apply(QTextCursor &cursor, const QString &expectedSource,
                      const MarkdownEdit &edit, QString *error = nullptr);
};
}
