#pragma once
#include "core/markdown/MarkdownEdit.h"

class MarkdownListEditing {
public:
    static MarkdownEdit indent(const QString &source, int anchor, int caret, bool outdent);
    static MarkdownEdit newline(const QString &source, int anchor, int caret);
    static MarkdownEdit backspace(const QString &source, int caret);
    static MarkdownEdit backspaceIndent(const QString &source, int caret);
};
