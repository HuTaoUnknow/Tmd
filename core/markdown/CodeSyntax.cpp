#include "core/markdown/CodeSyntax.h"
#include "core/markdown/LanguageCatalog.h"
#include <QHash>
#include <QRegularExpression>
#include <QSet>

namespace {
using namespace CodeSyntax;
struct Profile {
    enum Family { CLike, Python, Shell, Sql, Markup, Css, Json, Config, Diff, Markdown, Latex, Diagram } family = CLike;
    QSet<QString> keywords, types, literals, builtins;
    bool slash = false, hash = false, block = false, insensitive = false, backtick = false;
};
QSet<QString> words(const char *text) {
    const auto list = QString::fromLatin1(text).split(' ', Qt::SkipEmptyParts);
    return QSet<QString>(list.begin(), list.end());
}
const QHash<QString, Profile> &profiles() {
    static const auto all = [] {
        QHash<QString, Profile> result;
        const auto add = [&](const QString &name, Profile::Family family, const char *keywords, const char *types = "", const char *builtins = "") {
            Profile p; p.family = family; p.keywords = words(keywords); p.types = words(types); p.builtins = words(builtins);
            p.literals = family == Profile::Config ? words("true false null yes no on off") : words("true false null");
            p.slash = family == Profile::CLike;
            p.hash = family == Profile::Python || family == Profile::Shell || family == Profile::Config;
            p.block = family == Profile::CLike || family == Profile::Css || family == Profile::Sql;
            p.insensitive = family == Profile::Sql || family == Profile::Config;
            result.insert(name, p);
        };
        add("cpp", Profile::CLike, "alignas alignof asm auto break case catch class concept const constexpr consteval constinit const_cast continue co_await co_return co_yield decltype default delete do dynamic_cast else enum explicit export extern final for friend goto if inline mutable namespace new noexcept operator override private protected public register reinterpret_cast requires return sizeof static static_assert static_cast struct switch template this thread_local throw try typedef typeid typename union using virtual volatile while", "bool char char8_t char16_t char32_t double float int long short signed unsigned void wchar_t size_t string vector map optional unique_ptr shared_ptr", "std printf scanf cout cin move forward make_unique make_shared");
        add("c", Profile::CLike, "auto break case const continue default do else enum extern for goto if inline register restrict return sizeof static struct switch typedef union volatile while _Alignas _Alignof _Atomic _Generic _Noreturn _Static_assert _Thread_local", "bool char double float int long short signed unsigned void size_t", "printf scanf malloc calloc realloc free memcpy strlen");
        add("javascript", Profile::CLike, "async await break case catch class const continue debugger default delete do else export extends finally for from function get if import in instanceof let new of return set static super switch this throw try typeof var void while with yield", "Array Boolean Date Error Map Number Object Promise RegExp Set String Symbol", "console JSON Math parseInt parseFloat require setTimeout fetch");
        add("typescript", Profile::CLike, "abstract as async await break case catch class const constructor continue declare default delete do else enum export extends finally for from function get if implements import in infer instanceof interface is keyof let namespace new of private protected public readonly return set static super switch this throw try type typeof var void while yield satisfies", "any bigint boolean never number object string symbol unknown void Array Promise Record Partial");
        add("python", Profile::Python, "and as assert async await break case class continue def del elif else except finally for from global if import in is lambda match nonlocal not or pass raise return try while with yield", "bool bytes complex dict float int list object set str tuple type", "abs all any enumerate filter input len map max min open print range repr reversed round sorted sum zip isinstance");
        add("sql", Profile::Sql, "add all alter and any as asc begin between by case check column commit constraint create cross database default delete desc distinct drop else end except exists explain foreign from full group having in index inner insert intersect into is join key left like limit not null offset on or order outer over primary references right rollback select set table then top transaction truncate union unique update using values view when where with", "bigint binary bit boolean char date datetime decimal double float int integer numeric real smallint text time timestamp varchar", "avg coalesce count max min now sum");
        add("java", Profile::CLike, "abstract assert break case catch class const continue default do else enum extends final finally for if implements import instanceof interface native new package private protected public record return sealed static strictfp super switch synchronized this throw throws transient try var volatile while yield", "boolean byte char double float int long short void String Integer Object List Map Set");
        add("csharp", Profile::CLike, "abstract as async await base break case catch checked class const continue default delegate do else enum event explicit extern finally fixed for foreach get goto if implicit in interface internal is lock namespace new operator out override params partial private protected public readonly ref return sealed set sizeof stackalloc static struct switch this throw try typeof unchecked unsafe using value var virtual volatile while yield", "bool byte char decimal double dynamic float int long object sbyte short string uint ulong ushort void");
        add("go", Profile::CLike, "break case chan const continue default defer else fallthrough for func go goto if import interface map package range return select struct switch type var", "bool byte complex64 complex128 error float32 float64 int int8 int16 int32 int64 rune string uint uint8 uint16 uint32 uint64 uintptr", "append cap close copy delete len make new panic recover print println");
        add("rust", Profile::CLike, "as async await break const continue crate dyn else enum extern fn for if impl in let loop match mod move mut pub ref return self Self static struct super trait type unsafe use where while", "bool char f32 f64 i8 i16 i32 i64 i128 isize str String u8 u16 u32 u64 u128 usize Vec Option Result", "Some None Ok Err println print format vec");
        add("kotlin", Profile::CLike, "abstract actual annotation as break by catch class companion const constructor continue crossinline data delegate do dynamic else enum expect external field final finally for fun get if import in infix init inline inner interface internal is lateinit noinline object open operator out override package private protected public reified return sealed set super suspend tailrec this throw try typealias val var when where while", "Any Boolean Byte Char Double Float Int Long Nothing Short String Unit");
        add("swift", Profile::CLike, "as associatedtype async await break case catch class continue convenience default defer deinit didSet do dynamic else enum extension fallthrough fileprivate final for func get guard if import in indirect init inout internal is lazy let mutating nonmutating open operator override private protocol public repeat required rethrows return self set static struct subscript super switch throws try typealias var weak where while willSet", "Any Bool Character Double Float Int Optional String UInt Void");
        add("php", Profile::CLike, "abstract and array as break callable case catch class clone const continue declare default die do echo else elseif empty endfor endforeach endif endswitch endwhile enum eval exit extends final finally fn for foreach function global goto if implements include include_once instanceof interface isset list match namespace new or print private protected public readonly require require_once return static switch throw trait try unset use var while xor yield", "bool float int object string void");
        result["php"].hash = true;
        add("ruby", Profile::Python, "alias and begin break case class def defined do else elsif end ensure false for if in module next nil not or redo rescue retry return self super then true undef unless until when while yield", "Array Hash Integer String Symbol", "puts print require require_relative attr_accessor attr_reader");
        add("bash", Profile::Shell, "case do done elif else esac fi for function if in select then time until while", "", "alias cd echo eval exec exit export printf pwd read readonly return set shift source test trap unset");
        add("powershell", Profile::Shell, "begin break catch class continue data do dynamicparam else elseif end enum exit filter finally for foreach function if in param process return switch throw trap try until using while workflow", "string int bool double hashtable array", "write-host write-output get-content set-content get-childitem where-object select-object foreach-object");
        result["powershell"].insensitive = true;
        add("dockerfile", Profile::Shell, "add arg cmd copy entrypoint env expose from healthcheck label maintainer onbuild run shell stopsignal user volume workdir", "", "echo apt-get apk yum"); result["dockerfile"].insensitive = true;
        add("html", Profile::Markup, ""); add("xml", Profile::Markup, ""); add("vue", Profile::Markup, "");
        add("css", Profile::Css, "important inherit initial unset revert", "", "calc var rgb rgba hsl hsla url linear-gradient");
        result["scss"] = result["css"]; result["scss"].slash = true;
        add("json", Profile::Json, "");
        add("yaml", Profile::Config, ""); add("ini", Profile::Config, ""); add("toml", Profile::Config, "");
        add("diff", Profile::Diff, ""); add("markdown", Profile::Markdown, ""); add("latex", Profile::Latex, "");
        add("mermaid", Profile::Diagram, "graph flowchart sequenceDiagram classDiagram stateDiagram stateDiagram-v2 erDiagram gantt pie journey mindmap timeline subgraph end direction participant actor note loop alt opt par rect activate deactivate class classDef style click linkStyle title section");
        result["javascript"].backtick = result["typescript"].backtick = result["go"].backtick = true;
        result["bash"].backtick = true;
        result["cpp"].literals = words("true false nullptr NULL"); result["c"].literals = words("true false NULL");
        result["python"].literals = words("True False None"); result["go"].literals = words("true false nil");
        result["javascript"].literals.insert("undefined"); result["typescript"].literals.insert("undefined");
        result["ruby"].literals = words("true false nil");
        return result;
    }();
    return all;
}
bool starts(const QString &text, int at, const QString &prefix) { return text.mid(at, prefix.size()) == prefix; }
bool identifier(QChar c, const Profile &p) {
    return c.isLetterOrNumber() || c == '_' || (p.family == Profile::Css || p.family == Profile::Config || p.family == Profile::Shell) && (c == '-' || c == '.');
}
int nonspace(const QString &text, int at) { while (at < text.size() && text[at].isSpace()) ++at; return at; }
void token(QVector<Token> &out, int start, int end, Kind kind) { if (end > start) out.append({start, end - start, kind}); }
int quoted(const QString &text, int at, State &state, QVector<Token> &out) {
    const int start = at;
    while (at < text.size()) {
        if (starts(text, at, state.end)) {
            if (state.doubled && starts(text, at + state.end.size(), state.end)) { at += 2 * state.end.size(); continue; }
            at += state.end.size(); token(out, start, at, state.stringKind); state.mode = State::Normal; state.end.clear(); state.multiline = false; state.doubled = false; state.escapes = true; return at;
        }
        if (state.escapes && text[at] == '\\') at += qMin(2, int(text.size()) - at);
        else ++at;
    }
    token(out, start, at, state.stringKind);
    if (!state.multiline && (text.isEmpty() || !text.endsWith('\\'))) { state.mode = State::Normal; state.end.clear(); }
    return at;
}
Line codeLine(const QString &text, const QString &language, const State &previous) {
    Line result; result.state = previous; auto &state = result.state;
    const auto found = profiles().constFind(language); if (found == profiles().cend()) { state = {}; return result; }
    const auto &p = found.value(); auto &out = result.tokens;
    if (p.family == Profile::Diff) {
        if (text.startsWith("+++") || text.startsWith("---") || text.startsWith("@@") || text.startsWith("diff ")) token(out, 0, text.size(), Kind::Preprocessor);
        else if (text.startsWith('+')) token(out, 0, text.size(), Kind::Inserted);
        else if (text.startsWith('-')) token(out, 0, text.size(), Kind::Deleted);
        return result;
    }
    static const QRegularExpression number("(?:0[xX][0-9a-fA-F](?:[0-9a-fA-F_']*[0-9a-fA-F])?|0[bB][01][01_']*|(?:[0-9][0-9_']*(?:\\.[0-9_]*)?|\\.[0-9_]+)(?:[eE][+-]?[0-9_]+)?)(?:[uUlLfF]+)?");
    int i = 0;
    while (i < text.size()) {
        if (state.mode == State::Comment) {
            const int end = text.indexOf(state.end, i); const int next = end < 0 ? text.size() : end + state.end.size();
            token(out, i, next, Kind::Comment); i = next;
            if (end >= 0) { state.mode = State::Normal; state.end.clear(); } continue;
        }
        if (state.mode == State::String) { i = quoted(text, i, state, out); continue; }
        const QChar c = text[i]; if (c.isSpace()) { ++i; continue; }
        const bool boundary = i == 0 || text[i - 1].isSpace();
        if (p.slash && starts(text, i, "//") || p.family == Profile::Sql && starts(text, i, "--")
            || p.hash && c == '#' && (p.family != Profile::Shell && p.family != Profile::Config || boundary)
            || p.family == Profile::Config && language == "ini" && c == ';'
            || p.family == Profile::Latex && c == '%' || p.family == Profile::Diagram && starts(text, i, "%%")) {
            token(out, i, text.size(), Kind::Comment); break;
        }
        if (p.block && starts(text, i, "/*") || language == "powershell" && starts(text, i, "<#")) {
            state.mode = State::Comment; state.end = language == "powershell" ? "#>" : "*/";
            const int end = text.indexOf(state.end, i + 2); const int next = end < 0 ? text.size() : end + 2;
            token(out, i, next, Kind::Comment); i = next;
            if (end >= 0) { state.mode = State::Normal; state.end.clear(); } continue;
        }
        if ((language == "cpp" || language == "c") && c == '#' && text.left(i).trimmed().isEmpty()) {
            int end = nonspace(text, i + 1); while (end < text.size() && text[end].isLetter()) ++end;
            token(out, i, end, Kind::Preprocessor); i = end;
            if (text.mid(0, end).contains("include")) {
                const int value = nonspace(text, end), close = text.indexOf('>', value + 1);
                if (value < text.size() && text[value] == '<' && close >= 0) { token(out, value, close + 1, Kind::String); i = close + 1; }
            }
            continue;
        }
        if (language == "cpp" && starts(text, i, "R\"")) {
            const int opening = text.indexOf('(', i + 2);
            if (opening >= 0 && opening - i <= 18 && !text.mid(i + 2, opening - i - 2).contains(QRegularExpression("[\\s()\\\\]"))) {
                state.mode = State::String; state.end = ')' + text.mid(i + 2, opening - i - 2) + '"'; state.multiline = true; state.doubled = false; state.escapes = false; state.stringKind = Kind::String;
                token(out, i, opening + 1, Kind::String); i = quoted(text, opening + 1, state, out); continue;
            }
        }
        if (c == '\'' || c == '"' || c == '`' && p.backtick || language == "csharp" && starts(text, i, "@\"")) {
            if (language == "rust" && c == '\'' && i + 2 < text.size() && text[i + 1].isLetter() && text[i + 2] != '\'') { token(out, i, i + 1, Kind::Operator); ++i; continue; }
            const bool verbatim = language == "csharp" && starts(text, i, "@\"");
            const QChar quote = verbatim ? QChar('"') : c;
            const bool triple = language == "python" && starts(text, i, QString(3, quote));
            const int opening = verbatim ? 2 : triple ? 3 : 1;
            state.mode = State::String; state.end = triple ? QString(3, quote) : QString(quote);
            state.multiline = triple || c == '`' || verbatim || p.family == Profile::Sql || language == "powershell" || language == "ruby";
            state.doubled = verbatim || p.family == Profile::Sql;
            state.escapes = !state.doubled && !(language == "go" && c == '`') && !(p.family == Profile::Shell && c == '\'');
            state.stringKind = Kind::String;
            if (p.family == Profile::Json) {
                int closing = i + opening;
                while (closing < text.size()) { if (text[closing] == '\\') closing += 2; else if (text[closing++] == quote) break; }
                const int next = nonspace(text, closing); if (next < text.size() && text[next] == ':') state.stringKind = Kind::Property;
            }
            token(out, i, i + opening, state.stringKind); i = quoted(text, i + opening, state, out); continue;
        }
        if (c == '$' && (p.family == Profile::Shell || language == "php" || language == "scss")) {
            int end = i + 1;
            if (end < text.size() && text[end] == '{') { const int close = text.indexOf('}', end + 1); end = close < 0 ? text.size() : close + 1; }
            else { while (end < text.size() && (identifier(text[end], p) || text[end] == ':' || text[end] == '?')) ++end; }
            token(out, i, end, Kind::Variable); i = end; continue;
        }
        if (p.family == Profile::Latex && c == '\\') {
            int end = i + 1; while (end < text.size() && text[end].isLetter()) ++end;
            if (end == i + 1 && end < text.size()) ++end; token(out, i, end, Kind::Function); i = end; continue;
        }
        if (p.family == Profile::Css && c == '#') {
            int end = i + 1; while (end < text.size() && identifier(text[end], p)) ++end;
            token(out, i, end, state.braces ? Kind::Number : Kind::Selector); i = end; continue;
        }
        if (p.family == Profile::Config && c == '[') {
            const int end = text.indexOf(']', i + 1); if (end >= 0) { token(out, i, end + 1, Kind::Type); i = end + 1; continue; }
        }
        if (c.isDigit() || c == '.' && i + 1 < text.size() && text[i + 1].isDigit()) {
            const auto match = number.match(text, i, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption);
            if (match.hasMatch()) { const int end = i + match.capturedLength(); token(out, i, end, Kind::Number); i = end; continue; }
        }
        if (c.isLetter() || c == '_' || c == '@' && (language == "java" || p.family == Profile::Css)) {
            int end = i + 1; while (end < text.size() && identifier(text[end], p)) ++end;
            const QString word = text.mid(i, end - i), compare = p.insensitive ? word.toLower() : word;
            const int next = nonspace(text, end); Kind kind; bool styled = true;
            if (p.literals.contains(compare)) kind = Kind::Literal;
            else if (p.keywords.contains(compare)) kind = Kind::Keyword;
            else if (p.types.contains(compare)) kind = Kind::Type;
            else if (p.builtins.contains(compare)) kind = Kind::Function;
            else if ((p.family == Profile::Config || p.family == Profile::Css && state.braces) && next < text.size() && (text[next] == ':' || text[next] == '=')) kind = Kind::Property;
            else if (p.family == Profile::Css && !state.braces) kind = c == '@' ? Kind::Preprocessor : Kind::Selector;
            else if (next < text.size() && text[next] == '(') kind = Kind::Function;
            else if (language == "java" && c == '@') kind = Kind::Preprocessor;
            else styled = false;
            if (styled) token(out, i, end, kind); i = end; continue;
        }
        if (p.family == Profile::Markdown && c == '#' && text.left(i).trimmed().isEmpty()) { token(out, i, text.size(), Kind::Keyword); break; }
        if (c == '{') ++state.braces;
        if (c == '}') state.braces = qMax(0, state.braces - 1);
        token(out, i, i + 1, QString("(){}[];,.:?").contains(c) ? Kind::Punctuation : Kind::Operator); ++i;
    }
    if (text.isEmpty() && state.mode == State::String && !state.multiline) { state.mode = State::Normal; state.end.clear(); }
    return result;
}
Line markupLine(const QString &text, const QString &language, const State &previous) {
    Line result; result.state = previous; auto &state = result.state; auto &out = result.tokens; int i = 0;
    while (i < text.size()) {
        if (!state.embedded.isEmpty()) {
            const QString closing = state.embedded == "css" ? "style" : "script";
            const QRegularExpression close("</" + closing + "\\s*>", QRegularExpression::CaseInsensitiveOption);
            const auto match = close.match(text, i); const int end = match.hasMatch() ? match.capturedStart() : text.size();
            const auto code = codeLine(text.mid(i, end - i), state.embedded, state);
            state = code.state; for (const auto &part : code.tokens) out.append({i + part.start, part.length, part.kind}); i = end;
            if (!match.hasMatch()) break;
            state = {}; continue;
        }
        if (state.mode == State::Comment || state.mode == State::CData) {
            const int end = text.indexOf(state.end, i), next = end < 0 ? text.size() : end + state.end.size();
            token(out, i, next, state.mode == State::Comment ? Kind::Comment : Kind::String); i = next;
            if (end >= 0) { state.mode = State::Normal; state.end.clear(); } continue;
        }
        if (state.mode == State::String) {
            i = quoted(text, i, state, out); if (state.mode == State::Normal) state.mode = State::Tag; continue;
        }
        if (state.mode == State::Tag) {
            if (text[i].isSpace()) { ++i; continue; }
            if (text[i] == '"' || text[i] == '\'') {
                state.mode = State::String; state.end = text.mid(i, 1); state.multiline = true; state.doubled = false; state.escapes = false; state.stringKind = Kind::String;
                token(out, i, i + 1, Kind::String); ++i; continue;
            }
            if (text[i] == '>') {
                token(out, i, i + 1, Kind::Punctuation); ++i; state.mode = State::Normal;
                if (language != "xml" && !state.closingTag && (state.tag == "script" || state.tag == "style")) state.embedded = state.tag == "style" ? "css" : "javascript";
                state.tag.clear(); continue;
            }
            if (text[i].isLetterOrNumber() || text[i] == '_' || text[i] == ':' || text[i] == '-' || text[i] == '@') {
                int end = i + 1; while (end < text.size() && (text[end].isLetterOrNumber() || QString("_:-.@").contains(text[end]))) ++end;
                token(out, i, end, Kind::Attribute); i = end; continue;
            }
            if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '>') state.closingTag = true;
            token(out, i, i + 1, text[i] == '=' ? Kind::Operator : Kind::Punctuation); ++i; continue;
        }
        if (starts(text, i, "<!--") || starts(text, i, "<![CDATA[")) {
            const bool comment = starts(text, i, "<!--"); state.mode = comment ? State::Comment : State::CData; state.end = comment ? "-->" : "]]>";
            const int begin = i, end = text.indexOf(state.end, i + (comment ? 4 : 9)); i = end < 0 ? text.size() : end + state.end.size();
            token(out, begin, i, comment ? Kind::Comment : Kind::String);
            if (end >= 0) { state.mode = State::Normal; state.end.clear(); } continue;
        }
        if (text[i] == '<' && i + 1 < text.size() && (text[i + 1].isLetter() || QString("/!?").contains(text[i + 1]))) {
            int name = i + 1; if (QString("/!?").contains(text[name])) ++name;
            int end = name; while (end < text.size() && (text[end].isLetterOrNumber() || QString("_:-.").contains(text[end]))) ++end;
            token(out, i, name, Kind::Punctuation); token(out, name, end, text[i + 1] == '!' || text[i + 1] == '?' ? Kind::Preprocessor : Kind::Tag);
            state.tag = text.mid(name, end - name).toLower(); state.closingTag = text[i + 1] == '/'; state.mode = State::Tag; i = end; continue;
        }
        if (text[i] == '&') {
            static const QRegularExpression entity("&(?:[A-Za-z][A-Za-z0-9]*|#[0-9]+|#[xX][0-9a-fA-F]+);");
            const auto match = entity.match(text, i, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption);
            if (match.hasMatch()) { token(out, i, i + match.capturedLength(), Kind::Entity); i += match.capturedLength(); continue; }
        }
        ++i;
    }
    return result;
}
}
CodeSyntax::Line CodeSyntax::highlightLine(const QString &text, const QString &language, const State &previous) {
    const QString canonical = LanguageCatalog::canonicalName(language);
    const auto found = profiles().constFind(canonical); if (found == profiles().cend()) return {};
    return found->family == Profile::Markup ? markupLine(text, canonical, previous) : codeLine(text, canonical, previous);
}
QColor CodeSyntax::color(Kind kind, bool dark) {
    // Semantic categories follow Prism/highlight.js. Pair each dark color with
    // a readable light-background color rather than assuming a dark theme.
    static const QPair<const char *, const char *> colors[] = {
        {"#b8a6e0", "#7731a5"}, {"#dfc387", "#795000"}, {"#a7cca1", "#28733e"}, {"#899d96", "#54666a"},
        {"#e5bb87", "#955313"}, {"#e5bb87", "#955313"}, {"#8dc8e8", "#006ba6"}, {"#9dd8d8", "#087274"},
        {"#e59fa9", "#a92e45"}, {"#dfc387", "#795000"}, {"#9dd8d8", "#087274"}, {"#bec8d2", "#485764"},
        {"#e3ad8c", "#96521d"}, {"#b5c9ed", "#365f9c"}, {"#e5bb87", "#955313"}, {"#e59fa9", "#a92e45"},
        {"#a7cca1", "#28733e"}, {"#e59fa9", "#a92e45"}
    };
    const auto &pair = colors[int(kind)]; return QColor(dark ? pair.first : pair.second);
}
int CodeSyntax::stateKey(const State &state) {
    const QString key = QString::number(state.mode) + '/' + state.end + '/' + state.tag + '/' + state.embedded + '/'
        + QString::number(int(state.stringKind)) + '/' + QString::number(state.multiline) + QString::number(state.doubled)
        + QString::number(state.closingTag) + QString::number(state.escapes) + '/' + QString::number(state.braces);
    return int(qHash(key) & 0x7fffffff);
}
