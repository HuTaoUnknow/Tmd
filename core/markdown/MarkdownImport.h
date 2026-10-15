#pragma once
#include <QString>
class MarkdownImport {
public:
    static QString convertIndentedCode(const QString &source);
    static QString paste(const QString &document, int caret, const QString &text);
};
