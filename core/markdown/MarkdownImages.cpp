#include "core/markdown/MarkdownImages.h"
#include "core/markdown/MarkdownAnalysis.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>
#include <algorithm>

namespace {
bool escaped(const QString &s, int i) {
    int n = 0; while (i > 0 && s[--i] == '\\') ++n; return n % 2;
}
QString unescape(QString s) {
    static const QRegularExpression punctuation(R"(\\([!"#$%&'()*+,\-./:;<=>?@\[\]\\^_`{|}~]))");
    s.replace(punctuation, "\\1");
    return s.replace("&amp;", "&").replace("&quot;", "\"").replace("&#39;", "'");
}
QString label(QString s) { return unescape(s).simplified().toCaseFolded(); }
bool destination(const QString &s, int p, int limit, int &start, int &end, int &after) {
    while (p < limit && s[p].isSpace()) ++p;
    if (p >= limit) return false;
    if (s[p] == '<') {
        start = ++p;
        while (p < limit && (s[p] != '>' || escaped(s, p)) && s[p] != '\n') ++p;
        if (p == limit || s[p] != '>') return false;
        end = p; after = p + 1; return end > start;
    }
    start = p; int depth = 0;
    while (p < limit) {
        if (!escaped(s, p)) {
            if (s[p].isSpace()) break;
            if (s[p] == '(') ++depth;
            if (s[p] == ')') { if (!depth) break; --depth; }
        }
        ++p;
    }
    end = after = p; return end > start && !depth;
}
}
QVector<MarkdownImage> MarkdownImages::parse(const QString &s) {
    return parse(s, MarkdownAnalysis::scan(s));
}
QVector<MarkdownImage> MarkdownImages::parse(const QString &s, const MarkdownAnalysis &analysis) {
    const auto code = analysis.literalMask(s.size());
    QMap<QString, MarkdownImage> definitions;
    static const QRegularExpression definition(R"(^ {0,3}\[([^\]\n]+)\]:[ \t]*)", QRegularExpression::MultilineOption);
    auto matches = definition.globalMatch(s);
    while (matches.hasNext()) {
        const auto m = matches.next(); if (code[m.capturedStart()]) continue;
        int start, end, after, limit = s.indexOf('\n', m.capturedEnd()); if (limit < 0) limit = s.size();
        if (destination(s, m.capturedEnd(), limit, start, end, after) && !definitions.contains(label(m.captured(1))))
            definitions.insert(label(m.captured(1)), {0, start, end - start, unescape(s.mid(start, end - start)), {}});
    }
    QVector<MarkdownImage> result;
    for (int i = 0; i + 2 < s.size(); ++i) {
        if (code[i] || s[i] != '!' || s[i + 1] != '[' || escaped(s, i)) continue;
        int end = i + 2, depth = 1;
        for (; end < s.size(); ++end) if (!escaped(s, end)) {
            if (s[end] == '[') ++depth;
            if (s[end] == ']' && --depth == 0) break;
        }
        if (end >= s.size()) continue;
        const QString alt = unescape(s.mid(i + 2, end - i - 2));
        int p = end + 1;
        if (p < s.size() && s[p] == '(') {
            int start, urlEnd, after;
            if (!destination(s, p + 1, s.size(), start, urlEnd, after)) continue;
            while (after < s.size() && s[after].isSpace()) ++after;
            if (after < s.size() && (s[after] == '"' || s[after] == '\'' || s[after] == '(')) {
                const QChar close = s[after] == '(' ? ')' : s[after]; ++after;
                while (after < s.size() && (s[after] != close || escaped(s, after))) ++after;
                if (after < s.size()) ++after;
                while (after < s.size() && s[after].isSpace()) ++after;
            }
            if (after < s.size() && s[after] == ')') {
                result.append({i, start, urlEnd - start, unescape(s.mid(start, urlEnd - start)), alt}); i = after;
            }
        } else {
            QString id = alt;
            if (p < s.size() && s[p] == '[') {
                const int close = s.indexOf(']', p + 1);
                if (close < 0) continue;
                if (close > p + 1) id = s.mid(p + 1, close - p - 1);
                p = close + 1;
            }
            if (definitions.contains(label(id))) {
                auto image = definitions.value(label(id)); image.position = i; image.alt = alt;
                result.append(image); i = p - 1;
            }
        }
    }
    static const QRegularExpression html(R"(<img\b[^>]*\bsrc\s*=\s*(["'])(.*?)\1[^>]*>)", QRegularExpression::CaseInsensitiveOption);
    matches = html.globalMatch(s);
    while (matches.hasNext()) {
        const auto m = matches.next(); if (code[m.capturedStart()]) continue;
        result.append({int(m.capturedStart()), int(m.capturedStart(2)), int(m.capturedLength(2)), unescape(m.captured(2)), {}});
    }
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) { return a.position < b.position; });
    return result;
}
QString MarkdownImages::replace(const QString &s, const QVector<MarkdownImage> &images, const QMap<QString, QString> &sources) {
    QMap<int, QPair<int, QString>> spans;
    for (const auto &image : images) if (sources.contains(image.source)) spans.insert(image.start, {image.length, sources.value(image.source)});
    QString result = s;
    auto it = spans.cend();
    while (it != spans.cbegin()) { --it; result.replace(it.key(), it->first, it->second); }
    return result;
}
QString MarkdownImages::encodedPath(const QString &path) {
    return QString::fromLatin1(QUrl::toPercentEncoding(QDir::fromNativeSeparators(path), "/:"));
}
bool MarkdownImages::isRemote(const QString &s) {
    const QUrl url(s); return url.isValid() && (url.scheme().compare("http", Qt::CaseInsensitive) == 0 || url.scheme().compare("https", Qt::CaseInsensitive) == 0) && !url.host().isEmpty();
}
bool MarkdownImages::isData(const QString &s) { return s.startsWith("data:image/", Qt::CaseInsensitive); }
QString MarkdownImages::localPath(const QString &s, const QString &document) {
    if (isRemote(s) || isData(s)) return {};
    QString path;
    if (s.startsWith("file:", Qt::CaseInsensitive)) path = QUrl(s).toLocalFile();
    else {
        static const QRegularExpression drive(R"(^[A-Za-z]:[/\\])");
        if (!drive.match(s).hasMatch() && !QUrl(s).scheme().isEmpty()) return {};
        path = QUrl::fromPercentEncoding(s.toUtf8());
    }
    path = QDir::fromNativeSeparators(path);
    return QDir::cleanPath(QDir::isAbsolutePath(path) ? path : QDir(QFileInfo(document).absolutePath()).absoluteFilePath(path));
}
QString MarkdownImages::markup(const QString &source, QString alt) {
    alt.replace('\\', "\\\\").replace('[', "\\[").replace(']', "\\]").replace('\n', ' ');
    return "![" + alt + "](<" + source + ">)";
}
