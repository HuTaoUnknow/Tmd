#pragma once
#include "MarkdownOutline.h"

class MarkdownParser {
public:
    static MarkdownOutline outline(const QString &markdown);
    static QString plainInlineText(QString text);
};
