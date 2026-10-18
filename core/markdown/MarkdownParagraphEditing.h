#pragma once
#include "core/markdown/MarkdownEdit.h"
class MarkdownParagraphEditing {
public:
    static MarkdownEdit newline(const QString &source, int anchor, int caret, bool separateElements = false);
    static MarkdownEdit backspace(const QString &source, int caret, bool separateElements = false);
    static MarkdownEdit deleteForward(const QString &source, int caret);
    static MarkdownEdit insertParagraph(const QString &source, int caret, const QString &text);
    static MarkdownEdit completeUnderline(const QString &source, int caret);
};
