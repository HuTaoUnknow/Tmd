#include "core/markdown/MarkdownReferences.h"
#include "core/markdown/MarkdownAnalysis.h"
#include <QMap>
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <algorithm>

namespace {
bool escaped(const QString &s, int p) {
    int count = 0; while (p > 0 && s[--p] == '\\') ++count; return count % 2;
}
QString decoded(QString s) {
    s.replace(QRegularExpression(R"(\\([!"#$%&'()*+,\-./:;<=>?@\[\]\\^_`{|}~]))"), "\\1");
    const QRegularExpression entity("&(?:#[0-9]+|#[xX][0-9a-fA-F]+|[A-Za-z][A-Za-z0-9]+);");
    auto iterator = entity.globalMatch(s); QVector<QRegularExpressionMatch> matches;
    while (iterator.hasNext()) matches.append(iterator.next());
    for (auto i = matches.crbegin(); i != matches.crend(); ++i)
        s.replace(i->capturedStart(), i->capturedLength(), QTextDocumentFragment::fromHtml(i->captured()).toPlainText());
    return s;
}
QString label(const QString &s) { return decoded(s).simplified().toCaseFolded(); }
bool destination(const QString &s, int p, int limit, MarkdownReference &reference, int &after) {
    int newlines=0;
    while (p < limit && s[p].isSpace()) { if (s[p]=='\n' && ++newlines>1) return false; ++p; }
    int start = p;
    if (p < limit && s[p] == '<') {
        start = ++p;
        while (p < limit && s[p] != '\n' && (s[p] != '>' || escaped(s,p))) ++p;
        if (p >= limit || s[p] != '>') return false;
        after = p + 1;
    } else {
        int depth = 0;
        while (p < limit) {
            if (!escaped(s,p)) {
                if (s[p].isSpace()) break;
                if (s[p] == '(') ++depth;
                if (s[p] == ')') { if (!depth) break; --depth; }
            }
            ++p;
        }
        if (depth) return false;
        after = p;
    }
    if (p == start) return false;
    reference = {start, p-start, decoded(s.mid(start,p-start)), false}; return true;
}
bool closesLink(const QString &s, int p, int &end) {
    while (p < s.size() && s[p].isSpace()) ++p;
    if (p < s.size() && (s[p] == '\'' || s[p] == '"' || s[p] == '(')) {
        const QChar close = s[p] == '(' ? QChar(')') : s[p]; ++p;
        while (p < s.size() && (s[p] != close || escaped(s,p))) ++p;
        if (p == s.size()) return false;
        ++p; while (p < s.size() && s[p].isSpace()) ++p;
    }
    if (p >= s.size() || s[p] != ')') return false;
    end = p+1; return true;
}
}
QVector<MarkdownReference> MarkdownReferences::parse(const QString &s) {
    const auto analysis = MarkdownAnalysis::forRendering(s);
    auto literal = analysis.literalMask(s.size());
    QMap<QString, MarkdownReference> definitions;
    QMap<int, MarkdownReference> destinations;
    const QRegularExpression definition(R"(^ {0,3}\[([^\]\n]+)\]:[ \t]*)");
    for (const auto &line : analysis.lines) {
        if (line.code || line.html || line.comment) continue;
        const int at = line.contentStart;
        if (at >= s.size() || literal[at]) continue;
        const auto match = definition.match(s.mid(at, line.end-at));
        if (!match.hasMatch()) continue;
        MarkdownReference ref; int after;
        int limit=line.end;
        if (s.mid(at+match.capturedEnd(),line.end-at-match.capturedEnd()).trimmed().isEmpty() && line.end<s.size()) {
            limit=s.indexOf('\n',line.end+1); if (limit<0) limit=s.size();
        }
        if (destination(s,at+match.capturedEnd(),limit,ref,after)) {
            const auto key = label(match.captured(1));
            if (!definitions.contains(key)) definitions.insert(key,ref);
            destinations.insert(ref.start,ref);
        }
        std::fill(literal.begin()+line.start,literal.begin()+limit,1);
    }
    for (int i = 0; i < s.size(); ++i) {
        if (literal[i] || escaped(s,i)) continue;
        if (s[i] == '<') {
            const int end = s.indexOf('>',i+1);
            if (end > i && !s.mid(i,end-i).contains('\n')) {
                const auto value = s.mid(i+1,end-i-1);
                if (value.startsWith("file:",Qt::CaseInsensitive)) destinations.insert(i+1,{i+1,end-i-1,value,false});
            }
        }
        if (s[i] != '[') continue;
        const bool image = i > 0 && s[i-1] == '!' && !escaped(s,i-1);
        int depth = 1, close = i+1;
        for (; close < s.size(); ++close) {
            if (literal[close] || escaped(s,close)) continue;
            if (s[close] == '[') ++depth;
            else if (s[close] == ']' && !--depth) break;
        }
        if (close >= s.size()) continue;
        int after = close+1;
        if (after < s.size() && s[after] == '(') {
            MarkdownReference ref; int next, end;
            if (destination(s,after+1,s.size(),ref,next) && closesLink(s,next,end)) {
                ref.image=image; destinations.insert(ref.start,ref);
                // Continue inside the label so linked images retain both targets.
            }
        } else {
            QString key = s.mid(i+1,close-i-1);
            if (after < s.size() && s[after] == '[') {
                const int end = s.indexOf(']',after+1);
                if (end < 0) continue;
                if (end > after+1) key=s.mid(after+1,end-after-1);
            }
            auto found=definitions.constFind(label(key));
            if (found != definitions.cend()) {
                auto ref=found.value(); ref.image=image || destinations.value(ref.start).image; destinations.insert(ref.start,ref);
            }
        }
    }
    return destinations.values().toVector();
}
