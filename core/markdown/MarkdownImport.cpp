#include "core/markdown/MarkdownImport.h"
#include "core/markdown/MarkdownAnalysis.h"
#include "core/markdown/MarkdownCodeEditing.h"
#include "third_party/md4c/md4c.h"
#include <QRegularExpression>
#include <cstdint>
namespace {
struct Conversion { int start, end; QString replacement; };
struct Reader {
    QByteArray bytes; const QString &source;
    bool indented = false; int first = -1, last = -1; QString body;
    QVector<Conversion> changes;
    static int enter(MD_BLOCKTYPE type, void *detail, void *user) {
        auto &r = *static_cast<Reader *>(user);
        if (type == MD_BLOCK_CODE) { r.indented = !static_cast<MD_BLOCK_CODE_DETAIL *>(detail)->fence_char; r.first = r.last = -1; r.body.clear(); }
        return 0;
    }
    static int text(MD_TEXTTYPE, const MD_CHAR *p, MD_SIZE count, void *user) {
        auto &r = *static_cast<Reader *>(user); if (!r.indented) return 0;
        r.body += QString::fromUtf8(p, count);
        const auto address = reinterpret_cast<std::uintptr_t>(p), base = reinterpret_cast<std::uintptr_t>(r.bytes.constData());
        if (count && address >= base && address + count <= base + r.bytes.size()) {
            const int first = QString::fromUtf8(r.bytes.constData(), int(address - base)).size();
            const int last = first + QString::fromUtf8(p, count).size();
            if (r.first < 0) r.first = first; r.last = qMax(r.last, last);
        }
        return 0;
    }
    static int leave(MD_BLOCKTYPE type, void *, void *user) {
        auto &r = *static_cast<Reader *>(user);
        if (type != MD_BLOCK_CODE || !r.indented) return 0;
        r.indented = false; if (r.first < 0) return 0;
        const int start = r.first ? r.source.lastIndexOf('\n', r.first - 1) + 1 : 0;
        int end = r.source.indexOf('\n', qMax(r.first, r.last - 1)); if (end < 0) end = r.source.size();
        QString prefix = r.source.mid(start, r.first - start); int columns = 0;
        while (!prefix.isEmpty() && columns < 4 && (prefix.back() == ' ' || prefix.back() == '\t')) {
            columns += prefix.back() == '\t' ? 4 : 1; prefix.chop(1);
        }
        int length = 3; const auto runs = QRegularExpression("`+").globalMatch(r.body);
        auto matches = runs; while (matches.hasNext()) length = qMax(length, int(matches.next().capturedLength()) + 1);
        const QString fence(length, '`'), continuation = MarkdownCodeEditing::continuationPrefix(prefix);
        QString body = r.body; if (body.endsWith('\n')) body.chop(1);
        const auto lines = body.split('\n'); QStringList output{prefix + fence};
        for (const auto &line : lines) output.append(continuation + line);
        output.append(continuation + fence); r.changes.append({start, end, output.join('\n')}); return 0;
    }
    static int span(MD_SPANTYPE, void *, void *) { return 0; }
};
}
QString MarkdownImport::convertIndentedCode(const QString &source) {
    Reader reader{source.toUtf8(), source}; MD_PARSER parser{}; parser.flags = MD_DIALECT_GITHUB;
    parser.enter_block = Reader::enter; parser.leave_block = Reader::leave; parser.enter_span = Reader::span; parser.leave_span = Reader::span; parser.text = Reader::text;
    if (md_parse(reader.bytes.constData(), reader.bytes.size(), &parser, &reader)) return source;
    QString result = source;
    for (auto it = reader.changes.crbegin(); it != reader.changes.crend(); ++it) result.replace(it->start, it->end - it->start, it->replacement);
    return result;
}
QString MarkdownImport::paste(const QString &document, int caret, const QString &text) {
    for (const auto &line : MarkdownAnalysis::forRendering(document).lines)
        if (caret >= line.start && caret <= line.end && line.code && !line.fenceMarker) return text;
    return convertIndentedCode(text);
}
