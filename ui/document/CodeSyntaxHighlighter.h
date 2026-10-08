#pragma once
#include <QSyntaxHighlighter>
class CodeSyntaxHighlighter : public QSyntaxHighlighter {
public:
    explicit CodeSyntaxHighlighter(QTextDocument *document) : QSyntaxHighlighter(document) {}
protected:
    void highlightBlock(const QString &text) override;
};
