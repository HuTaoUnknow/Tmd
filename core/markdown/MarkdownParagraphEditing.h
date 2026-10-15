#pragma once
#include "core/markdown/MarkdownEdit.h"
class MarkdownParagraphEditing {
public:
    static MarkdownEdit newline(const QString &source, int anchor, int caret);
    static MarkdownEdit backspace(const QString &source, int caret);
};
