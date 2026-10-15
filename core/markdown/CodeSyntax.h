#pragma once
#include <QColor>
#include <QString>
#include <QVector>

// Language lexing and semantic colors are independent of text widgets. Offsets
// are UTF-16, matching QTextDocument; strings/comments never receive keywords.
namespace CodeSyntax {
enum class Kind { Keyword, Type, String, Comment, Number, Literal, Function, Property,
    Tag, Attribute, Operator, Punctuation, Preprocessor, Variable, Entity, Selector, Inserted, Deleted };
struct Token { int start, length; Kind kind; };
struct State {
    enum Mode { Normal, Comment, String, Tag, CData } mode = Normal;
    QString end, tag, embedded;
    Kind stringKind = Kind::String;
    bool multiline = false, doubled = false, closingTag = false, escapes = true;
    int braces = 0;
    bool operator==(const State &other) const {
        return mode == other.mode && end == other.end && tag == other.tag && embedded == other.embedded
            && stringKind == other.stringKind && multiline == other.multiline && doubled == other.doubled
            && closingTag == other.closingTag && escapes == other.escapes && braces == other.braces;
    }
};
struct Line { QVector<Token> tokens; State state; };
Line highlightLine(const QString &text, const QString &language, const State &previous = {});
QColor color(Kind kind, bool darkBackground = true);
int stateKey(const State &state);
}
