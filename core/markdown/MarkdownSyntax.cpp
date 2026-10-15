#include "core/markdown/MarkdownSyntax.h"
#include "core/markdown/MarkdownParser.h"
#include "core/markdown/MarkdownBlockSyntax.h"
#include "third_party/md4c/md4c.h"
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <algorithm>
#include <cstdint>

namespace {
int lineStart(const QString &s, int p) { return p ? s.lastIndexOf('\n', p - 1) + 1 : 0; }
struct Reader {
    const QString &source;
    MarkdownSyntax::Result result;
    const MD_CHAR *input = nullptr;
    int inputLength = 0, base = 0, lastOffset = 0;
    QVector<int> utf8Offsets;
    QVector<char> claimed;
    struct Span { MD_SPANTYPE type; int first = -1, last = -1; };
    QVector<Span> spans;
    int headingLevel = 0, headingFirst = -1, headingLast = -1, imageDepth = 0;
    QString headingText;
    QList<MarkdownParser::HeadingMarker> markers;
    int offset(const MD_CHAR *p) const {
        const auto address = reinterpret_cast<std::uintptr_t>(p), begin = reinterpret_cast<std::uintptr_t>(input);
        if (address < begin || address > begin + inputLength * sizeof(MD_CHAR)) return -1;
        const int index = int(p - input);
        return base + utf8Offsets.value(index);
    }
    QString text(const MD_CHAR *p, MD_SIZE n) const {
        return QString::fromUtf8(p, n);
    }
    static int enterBlock(MD_BLOCKTYPE type, void *detail, void *data) {
        auto &r = *static_cast<Reader *>(data);
        if (type == MD_BLOCK_H) { r.headingLevel = int(static_cast<MD_BLOCK_H_DETAIL *>(detail)->level); r.headingFirst = r.headingLast = -1; r.headingText.clear(); }
        return 0;
    }
    static int leaveBlock(MD_BLOCKTYPE type, void *, void *data) {
        auto &r = *static_cast<Reader *>(data);
        if (type != MD_BLOCK_H) return 0;
        int start = -1;
        if (r.headingFirst >= 0) start = lineStart(r.source, r.headingFirst);
        else for (const auto &marker : r.markers) {
            if (marker.markerStart < r.lastOffset || marker.level != r.headingLevel) continue;
            start = lineStart(r.source, marker.markerStart); r.lastOffset = marker.end;
            if (!marker.separated) start = -1; // Tmd requires the typed space.
            break;
        }
        if (start >= 0) {
            const int end = r.source.indexOf('\n', qMax(start, r.headingLast));
            if (end >= 0) {
                const int next = r.source.indexOf('\n', end + 1);
                const auto underline = r.source.mid(end + 1, (next < 0 ? r.source.size() : next) - end - 1);
                static const QRegularExpression loneDash("^(?: {0,3}> +)* *- *\\r?$");
                const bool atx = std::any_of(r.markers.cbegin(), r.markers.cend(), [&](const auto &m) { return m.separated && m.markerStart >= start && m.markerStart <= end; });
                if (!atx && loneDash.match(underline).hasMatch()) { r.headingLevel = 0; return 0; }
            }
            r.result.headings.append({r.headingLevel, int(r.source.left(start).count('\n')), start, r.headingText.trimmed()});
            if (end >= 0) {
                const int nextEnd = r.source.indexOf('\n', end + 1);
                const QString underline = r.source.mid(end + 1, (nextEnd < 0 ? r.source.size() : nextEnd) - end - 1);
                static const QRegularExpression setext("^(?: {0,3}>[ \\t]?)*[ \\t]*(?:=+|-+)[ \\t]*$");
                // ATX headings cannot consume a following thematic break.
                static const QRegularExpression atx("^(?: {0,3}(?:>[ \\t]?|(?:[-+*]|[0-9]{1,9}[.)])[ \\t]+))* {0,3}#{1,6}(?:[ \\t]|$)");
                if (!atx.match(r.source.mid(start, end - start)).hasMatch() && setext.match(underline).hasMatch()) r.result.underlineLines.insert(end + 1);
            }
        }
        r.headingLevel = 0;
        return 0;
    }
    static int enterSpan(MD_SPANTYPE type, void *, void *data) {
        auto &r = *static_cast<Reader *>(data);
        if (type == MD_SPAN_IMG) ++r.imageDepth;
        r.spans.append(Span{type}); return 0;
    }
    void finishSpan(const Span &span) {
        if (span.first < 0 || span.last < span.first || imageDepth) return;
        const bool code = span.type == MD_SPAN_CODE;
        const int required = span.type == MD_SPAN_STRONG ? 2 : 1;
        if (!code && span.type != MD_SPAN_EM && span.type != MD_SPAN_STRONG && span.type != MD_SPAN_DEL) return;
        const QString choices = code ? "`" : span.type == MD_SPAN_DEL ? "~" : "*_";
        int opening = -1, count = required;
        for (int p = span.first - 1; p >= base; --p) {
            if (claimed[p] || !choices.contains(source[p])) continue;
            int begin = p; while (begin > base && source[begin - 1] == source[p] && !claimed[begin - 1]) --begin;
            const int size = p - begin + 1;
            if (size >= required) { count = code ? size : span.type == MD_SPAN_DEL ? qMin(2, size) : required; opening = p - count + 1; break; }
            p = begin;
        }
        if (opening < 0) return;
        int closing = -1;
        for (int p = span.last; p < source.size(); ++p) {
            if (source[p] != source[opening] || claimed[p]) continue;
            int end = p; while (end < source.size() && source[end] == source[p] && !claimed[end]) ++end;
            if (end - p >= count) { closing = p; break; }
            p = end - 1;
        }
        if (closing < 0 || opening + count > closing) return;
        std::fill(claimed.begin() + opening, claimed.begin() + opening + count, 1);
        std::fill(claimed.begin() + closing, claimed.begin() + closing + count, 1);
        result.formats.append({opening, closing + count, opening + count, closing, source.mid(opening, count), source.mid(closing, count), code});
    }
    static int leaveSpan(MD_SPANTYPE type, void *, void *data) {
        auto &r = *static_cast<Reader *>(data);
        const auto span = r.spans.takeLast(); r.finishSpan(span);
        if (type == MD_SPAN_IMG) --r.imageDepth;
        return 0;
    }
    static int onText(MD_TEXTTYPE type, const MD_CHAR *text, MD_SIZE size, void *data) {
        auto &r = *static_cast<Reader *>(data);
        const int start = r.offset(text), end = r.offset(text + size);
        if (start >= 0 && end >= start) {
            r.lastOffset = qMax(r.lastOffset, end);
            if (r.headingLevel) { if (r.headingFirst < 0) r.headingFirst = start; r.headingLast = end; }
            for (auto &span : r.spans) { if (span.first < 0) span.first = start; span.last = end; }
        }
        if (r.headingLevel && type != MD_TEXT_HTML) {
            QString value = type == MD_TEXT_BR || type == MD_TEXT_SOFTBR ? QString(" ") : r.text(text, size);
            if (type == MD_TEXT_ENTITY) value = QTextDocumentFragment::fromHtml(value).toPlainText();
            if (type == MD_TEXT_NULLCHAR) value = QChar::ReplacementCharacter;
            r.headingText += value;
        }
        return 0;
    }
};
}
QTextDocument::MarkdownFeatures MarkdownSyntax::renderFeatures() {
    // GFM's underscores are emphasis, not Qt's optional underline extension.
    return QTextDocument::MarkdownFeatures((int(QTextDocument::MarkdownDialectGitHub) & ~MD_FLAG_UNDERLINE) | MD_FLAG_NOHTML | MD_FLAG_NOINDENTEDCODEBLOCKS);
}
int MarkdownSyntax::frontMatterEnd(const QString &source) {
    const int first = source.indexOf('\n');
    if (first < 0 || source.left(first).trimmed() != "---" || source[0] != '-') return 0;
    for (int start = first + 1; start < source.size();) {
        int end = source.indexOf('\n', start); if (end < 0) end = source.size();
        if (source.mid(start, end - start).trimmed() == "---") return qMin(end + 1, int(source.size()));
        start = end + 1;
    }
    return 0;
}
MarkdownSyntax::Result MarkdownSyntax::scan(const QString &source, const MarkdownAnalysis *analysis) {
    Reader r{source}; r.claimed.fill(0, source.size());
    r.markers = analysis ? MarkdownParser::headingMarkers(source, *analysis) : MarkdownParser::headingMarkers(source);
    r.lastOffset = r.base = r.result.frontMatterEnd = frontMatterEnd(source);
    const auto prepared = MarkdownBlockSyntax::prepare(source, analysis);
    const QString body = prepared.text.mid(r.base);
    const QByteArray utf8 = body.toUtf8(); r.input = utf8.constData(); r.inputLength = utf8.size();
    r.utf8Offsets.resize(utf8.size() + 1);
    int units = 0;
    for (int b = 0; b < utf8.size();) {
        const auto c = static_cast<unsigned char>(utf8[b]); const int count = c < 128 ? 1 : c < 224 ? 2 : c < 240 ? 3 : 4;
        for (int i = 0; i < count && b + i < utf8.size(); ++i) r.utf8Offsets[b + i] = prepared.positions.value(r.base + units) - r.base;
        units += count == 4 ? 2 : 1; b += count;
    }
    r.utf8Offsets[utf8.size()] = source.size() - r.base;
    MD_PARSER parser{0, MD_DIALECT_GITHUB | MD_FLAG_NOHTML | MD_FLAG_NOINDENTEDCODEBLOCKS, Reader::enterBlock, Reader::leaveBlock, Reader::enterSpan, Reader::leaveSpan, Reader::onText, nullptr, nullptr};
    r.result.parsed = md_parse(r.input, r.inputLength, &parser, &r) == 0;
    std::sort(r.result.formats.begin(), r.result.formats.end(), [](const auto &a, const auto &b) { return a.start < b.start || (a.start == b.start && a.end > b.end); });
    return r.result;
}
MarkdownSyntax::Edit MarkdownSyntax::leaveEmptyContainer(const QString &source, int caret) {
    Edit result{source, caret}; caret = qBound(0, caret, int(source.size()));
    const int start = lineStart(source, caret), newline = source.indexOf('\n', caret);
    const int end = newline < 0 ? source.size() : newline;
    const QString line = source.mid(start, end - start);
    static const QRegularExpression empty("^([ \\t]*(?:>[ \\t]*)*)(?:(?:[-+*]|[0-9]{1,9}[.)])(?:[ \\t]+(?:\\[[ xX]\\][ \\t]*)?)?)?[ \\t]*$");
    if (!empty.match(line).hasMatch() || (!line.contains('>') && !line.contains(QRegularExpression("[-+*]|[0-9]{1,9}[.)]")))) return result;
    for (const auto &block : MarkdownAnalysis::scan(source).lines) if (block.start == start && block.code) return result;
    int begin = start;
    // Drop the quote-only separator generated by the preceding Enter.
    if (line.contains('>') && start > 0) {
        const int previous = lineStart(source, start - 1);
        const QString separator = source.mid(previous, start - previous - 1);
        static const QRegularExpression quoteOnly("^(?: {0,3}> +)+ *$");
        if (quoteOnly.match(separator).hasMatch() && separator.count('>') == line.count('>')) begin = previous;
    }
    result.source.replace(begin, end - begin, ""); result.caret = begin;
    if (begin > 0 && result.source[begin - 1] == '\n' && (begin < 2 || result.source[begin - 2] != '\n')) { result.source.insert(begin, '\n'); ++result.caret; }
    result.changed = true; return result;
}
