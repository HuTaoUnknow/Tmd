#pragma once
#include "core/markdown/MarkdownEdit.h"

// Source-only edits, shared by the rendered document and Markdown source editor.
class MarkdownCodeEditing {
public:
    using Edit = MarkdownEdit;
    static QString continuationPrefix(const QString &fenceIndent);
    static Edit indent(const QString &source, int anchor, int caret, bool outdent, int width = 4);
    static Edit newline(const QString &source, int anchor, int caret);
    static Edit leave(const QString &source, int caret);
    static Edit backspace(const QString &source, int caret);
    static Edit removeEmptyMetadata(const QString &before, const QString &after, int caret);
};
