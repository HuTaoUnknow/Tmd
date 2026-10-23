#pragma once
#include "core/markdown/MarkdownEdit.h"

// Source-only selection operations shared by both editing views.
class MarkdownSelectionEditing {
public:
    static bool isMarkerInput(const QString &text);
    static QString closingMarker(const QString &opening);
    static MarkdownEdit wrap(const QString &source, int anchor, int caret, const QString &opening);
    // stage 0: logical code line; 1: code body; 2: entire document.
    static MarkdownEdit selectCode(const QString &source, int anchor, int caret, int stage);
};
