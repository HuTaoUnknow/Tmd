#pragma once
#include <QSyntaxHighlighter>
#include <QMap>
class CodeSyntaxHighlighter : public QSyntaxHighlighter {
public:
    explicit CodeSyntaxHighlighter(QTextDocument *document, bool sourceMode = false);
    void refresh();
protected:
    void highlightBlock(const QString &text) override;
private:
    void updateSource();
    struct Context {
        QString language; int id = -1, offset = 0;
        bool operator==(const Context &other) const { return language == other.language && id == other.id && offset == other.offset; }
    };
    bool m_sourceMode = false;
    QString m_source;
    QMap<int, Context> m_contexts;
};
