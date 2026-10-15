#include "core/markdown/MarkdownTableEditing.h"
#include "core/markdown/MarkdownAnalysis.h"
#include "core/markdown/MarkdownBlockSyntax.h"
#include "third_party/md4c/md4c.h"
#include <QByteArray>
#include <QRegularExpression>
#include <cstdint>

namespace {
using Cell = MarkdownTableEditing::Cell;
bool escaped(const QString &source, int position) {
    int count = 0;
    while (position > 0 && source[--position] == '\\') ++count;
    return count % 2;
}
bool padding(QChar character) { return character == ' ' || character == '\t' || character == '\r'; }
QString cellSource(const QString &text) {
    QString normalized = text; normalized.replace("\r\n", "\n"); normalized.replace('\r', '\n');
    QString result;
    for (int i = 0; i < normalized.size(); ++i) {
        if (normalized[i] == '\n' || normalized[i] == QChar::LineSeparator) result += "<br>";
        else { if (normalized[i] == '|' && !escaped(normalized, i)) result += '\\'; result += normalized[i]; }
    }
    return result;
}
QVector<Cell> cells(const QString &source, const MarkdownAnalysis::Line &line, bool *hasPipe = nullptr) {
    QVector<int> pipes;
    for (int p = line.contentStart; p < line.end; ++p)
        if (source[p] == '|' && !escaped(source, p)) pipes.append(p);
    if (hasPipe) *hasPipe = !pipes.isEmpty();
    int start = line.contentStart, end = line.end;
    while (start < end && padding(source[start])) ++start;
    while (end > start && padding(source[end - 1])) --end;
    const bool leading = !pipes.isEmpty() && pipes.front() == start;
    const bool trailing = !pipes.isEmpty() && pipes.back() == end - 1;
    int first = leading ? pipes.front() + 1 : line.contentStart;
    const int last = trailing ? pipes.back() : line.end;
    QVector<Cell> result;
    auto append = [&](int begin, int finish) {
        Cell cell{begin, finish, begin, finish};
        while (cell.contentStart < cell.contentEnd && padding(source[cell.contentStart])) ++cell.contentStart;
        while (cell.contentEnd > cell.contentStart && padding(source[cell.contentEnd - 1])) --cell.contentEnd;
        result.append(cell);
    };
    for (int pipe : pipes) if (pipe >= first && pipe < last) { append(first, pipe); first = pipe + 1; }
    append(first, qMax(first, last));
    return result;
}
bool literal(const MarkdownAnalysis::Line &line) { return line.code || line.fenceMarker || line.html || line.comment; }

// The renderer already uses MD4C. Its table events confirm candidates in the
// complete document, including paragraph/container context and empty cells.
struct ParsedTable {
    int columns = 0, rows = 0, row = -1, anchorRow = -1, anchorByte = -1, previousByte = 0;
};
struct ParseContext {
    const QByteArray *bytes = nullptr;
    QVector<ParsedTable> tables;
    int active = -1, lastByte = 0;
};
int enterBlock(MD_BLOCKTYPE type, void *detail, void *user) {
    auto &context = *static_cast<ParseContext *>(user);
    if (type == MD_BLOCK_TABLE) {
        const auto &table = *static_cast<MD_BLOCK_TABLE_DETAIL *>(detail);
        context.tables.append({int(table.col_count), int(table.head_row_count + table.body_row_count), -1, -1, -1, context.lastByte});
        context.active = context.tables.size() - 1;
    } else if (type == MD_BLOCK_TR && context.active >= 0) ++context.tables[context.active].row;
    return 0;
}
int leaveBlock(MD_BLOCKTYPE type, void *, void *user) {
    if (type == MD_BLOCK_TABLE) static_cast<ParseContext *>(user)->active = -1;
    return 0;
}
int span(MD_SPANTYPE, void *, void *) { return 0; }
int text(MD_TEXTTYPE, const MD_CHAR *characters, MD_SIZE length, void *user) {
    auto &context = *static_cast<ParseContext *>(user);
    const auto begin = reinterpret_cast<std::uintptr_t>(context.bytes->constData());
    const auto pointer = reinterpret_cast<std::uintptr_t>(characters);
    if (!length || pointer < begin || pointer + length > begin + context.bytes->size()) return 0;
    const int offset = int(pointer - begin);
    context.lastByte = qMax(context.lastByte, offset + int(length));
    if (context.active >= 0) {
        auto &table = context.tables[context.active];
        if (table.anchorByte < 0) { table.anchorByte = offset; table.anchorRow = table.row; }
    }
    return 0;
}
}

QVector<MarkdownTableEditing::Table> MarkdownTableEditing::tables(const QString &source) {
    const auto analysis = MarkdownAnalysis::forRendering(source);
    struct Candidate { int header = 0, columns = 0; };
    QVector<Candidate> candidates;
    static const QRegularExpression separator("^:?-+:?$");
    for (int i = 1; i < analysis.lines.size(); ++i) {
        const auto &header = analysis.lines[i - 1], &divider = analysis.lines[i];
        if (literal(header) || literal(divider) || source.mid(header.contentStart, header.end - header.contentStart).trimmed().isEmpty()) continue;
        bool pipe = false;
        const auto divided = cells(source, divider, &pipe);
        if (!pipe || divided.isEmpty()) continue;
        bool valid = true;
        for (const auto &cell : divided)
            if (!separator.match(source.mid(cell.contentStart, cell.contentEnd - cell.contentStart)).hasMatch()) { valid = false; break; }
        if (valid) candidates.append({i - 1, int(divided.size())});
    }
    if (candidates.isEmpty()) return {};
    const auto prepared = MarkdownBlockSyntax::prepare(source, &analysis);
    const QByteArray bytes = prepared.text.toUtf8(); ParseContext context; context.bytes = &bytes;
    MD_PARSER parser{}; parser.flags = MD_DIALECT_GITHUB | MD_FLAG_NOHTML | MD_FLAG_NOINDENTEDCODEBLOCKS;
    parser.enter_block = enterBlock; parser.leave_block = leaveBlock;
    parser.enter_span = span; parser.leave_span = span; parser.text = text;
    if (md_parse(bytes.constData(), MD_SIZE(bytes.size()), &parser, &context)) return {};
    auto originalPosition = [&](int byte) {
        const int position = QString::fromUtf8(bytes.constData(), byte).size();
        return prepared.positions.value(position, source.size());
    };
    QVector<Table> result; int nextCandidate = 0;
    for (const auto &parsed : context.tables) {
        int expectedHeader = -1;
        if (parsed.anchorByte >= 0) {
            const int position = originalPosition(parsed.anchorByte);
            int rowLine = 0;
            while (rowLine < analysis.lines.size() && analysis.lines[rowLine].end < position) ++rowLine;
            expectedHeader = rowLine - parsed.anchorRow - (parsed.anchorRow > 0 ? 1 : 0);
        }
        const int lower = originalPosition(parsed.previousByte);
        int found = -1;
        for (int c = nextCandidate; c < candidates.size(); ++c) {
            const auto &candidate = candidates[c];
            if (candidate.columns != parsed.columns || candidate.header + parsed.rows >= analysis.lines.size()) continue;
            if (expectedHeader >= 0 ? candidate.header == expectedHeader : analysis.lines[candidate.header].start >= lower) { found = c; break; }
        }
        if (found < 0) continue;
        nextCandidate = found + 1; const int header = candidates[found].header;
        Table table; table.start = analysis.lines[header].start; table.end = analysis.lines[header + 1].end; bool valid = true;
        for (int row = 0; row < parsed.rows; ++row) {
            const auto &line = analysis.lines[header + row + (row > 0 ? 1 : 0)];
            if (literal(line)) { valid = false; break; }
            auto rowCells = cells(source, line);
            table.rows.append(rowCells); table.end = qMax(table.end, line.end);
        }
        int width = parsed.columns;
        for (const auto &row : table.rows) width = qMax(width, int(row.size()));
        for (auto &row : table.rows) while (row.size() < width) {
            int position = source.indexOf('\n', row.back().end); if (position < 0) position = source.size();
            row.append({position, position, position, position, true});
        }
        if (valid) result.append(table);
    }
    return result;
}

namespace {
struct Grid {
    QVector<QStringList> rows;
    QStringList divider, prefixes;
    QString dividerPrefix;
};
int startOfLine(const QString &source, int position) { return position ? source.lastIndexOf('\n', position - 1) + 1 : 0; }
Grid grid(const QString &source, const MarkdownTableEditing::Table &table) {
    Grid result;
    for (const auto &row : table.rows) {
        QStringList values;
        for (const auto &cell : row) values.append(cell.virtualCell ? QString("   ") : source.mid(cell.start, cell.end - cell.start));
        result.rows.append(values);
        const int start = startOfLine(source, row.front().start);
        int pipe = row.front().start - 1;
        result.prefixes.append(source.mid(start, (pipe >= start && source[pipe] == '|' ? pipe : row.front().start) - start));
    }
    const int firstEnd = source.indexOf('\n', table.start), dividerStart = firstEnd + 1;
    int dividerEnd = source.indexOf('\n', dividerStart); if (dividerEnd < 0) dividerEnd = source.size();
    MarkdownAnalysis::Line line{dividerStart, dividerEnd, dividerStart};
    const auto analysis = MarkdownAnalysis::forRendering(source);
    for (const auto &candidate : analysis.lines) if (candidate.start == dividerStart) { line = candidate; break; }
    const auto divided = cells(source, line);
    for (const auto &cell : divided) result.divider.append(source.mid(cell.start, cell.end - cell.start));
    if (!divided.isEmpty()) {
        const int pipe = divided.front().start - 1;
        if (pipe >= dividerStart && source[pipe] == '|') result.dividerPrefix = source.mid(dividerStart, pipe - dividerStart);
    }
    return result;
}
QString serialize(Grid value) {
    int width = value.divider.size();
    for (const auto &row : value.rows) width = qMax(width, int(row.size()));
    while (value.divider.size() < width) value.divider.append(" --- ");
    QStringList lines;
    for (int r = 0; r < value.rows.size(); ++r) {
        while (value.rows[r].size() < width) value.rows[r].append("   ");
        lines.append(value.prefixes.value(r) + '|' + value.rows[r].join('|') + '|');
        if (r == 0) lines.append(value.dividerPrefix + '|' + value.divider.join('|') + '|');
    }
    return lines.join('\n');
}
}

QString MarkdownTableEditing::normalize(const QString &source) {
    QString result = source;
    const auto parsed = tables(source);
    for (auto it = parsed.crbegin(); it != parsed.crend(); ++it) {
        bool needed = false;
        const auto value = grid(source, *it);
        const int width = it->rows.front().size();
        if (value.divider.size() != width) needed = true;
        for (const auto &row : it->rows) for (const auto &cell : row) if (cell.virtualCell) needed = true;
        if (needed) result.replace(it->start, it->end - it->start, serialize(value));
    }
    return result;
}

MarkdownEdit MarkdownTableEditing::create(const QString &source, int caret) {
    MarkdownEdit edit{source, caret, caret};
    const int start = startOfLine(source, caret);
    if (source.mid(start, caret - start) != "| ") return edit;
    for (const auto &line : MarkdownAnalysis::forRendering(source).lines)
        if (line.start == start && literal(line)) return edit;
    for (const auto &table : tables(source)) if (start >= table.start && start <= table.end) return edit;
    int end = source.indexOf('\n', start); if (end < 0) end = source.size();
    edit.source.replace(start, end - start, "| " + source.mid(caret, end - caret) + " |\n| --- |");
    edit.anchor = edit.caret = start + 2; edit.handled = edit.changed = true; return edit;
}

MarkdownEdit MarkdownTableEditing::operate(const QString &source, int index, int row, int column, Action action, const QString &text) {
    MarkdownEdit edit{source, 0, 0}; const auto parsed = tables(source);
    if (index < 0 || index >= parsed.size()) return edit;
    const auto &table = parsed[index];
    if (row < 0 || row >= table.rows.size() || column < 0 || column >= table.rows[row].size()) return edit;
    edit.handled = true;
    auto value = grid(source, table); const int width = table.rows.front().size();
    while (value.divider.size() < width) value.divider.append(" --- ");
    if (action == Action::NextCell && (row + 1 < value.rows.size() || column + 1 < width)) {
        column = (column + 1) % width; if (!column) ++row;
        edit.anchor = edit.caret = table.rows[row][column].contentStart; return edit;
    }
    if (action == Action::Leave || action == Action::NextCell) {
        int after = table.end;
        if (after < source.size() && source[after] == '\n') ++after;
        else { edit.source.insert(after, "\n\n"); after += 2; edit.changed = true; }
        if (after == edit.source.size() && !edit.source.endsWith("\n\n")) { edit.source += '\n'; ++after; edit.changed = true; }
        edit.anchor = edit.caret = after; return edit;
    }
    if ((action == Action::DeleteRow && value.rows.size() == 1) || (action == Action::DeleteColumn && width == 1)) {
        edit.source.remove(table.start, table.end - table.start); edit.anchor = edit.caret = table.start; edit.changed = true; return edit;
    }
    if (action == Action::AddColumn) {
        ++column;
        for (auto &cells : value.rows) cells.insert(column, "   ");
        value.divider.insert(column, " --- ");
    } else if (action == Action::AddRow) {
        ++row; value.rows.insert(row, QStringList());
        value.prefixes.insert(row, value.dividerPrefix);
    } else if (action == Action::DeleteColumn) {
        for (auto &cells : value.rows) cells.removeAt(column);
        value.divider.removeAt(column); column = qMin(column, width - 2);
    } else if (action == Action::DeleteRow) {
        value.rows.removeAt(row); value.prefixes.removeAt(row); row = qMin(row, int(value.rows.size()) - 1);
    } else if (action == Action::Paste) {
        QString pasted = text; pasted.replace("\r\n", "\n"); pasted.replace('\r', '\n');
        const auto lines = pasted.split('\n'); int targetWidth = width;
        for (const auto &line : lines) targetWidth = qMax(targetWidth, column + int(line.split('\t').size()));
        while (value.rows.size() < row + lines.size()) { value.rows.append(QStringList()); value.prefixes.append(value.dividerPrefix); }
        for (auto &cells : value.rows) while (cells.size() < targetWidth) cells.append("   ");
        for (int r = 0; r < lines.size(); ++r) {
            const auto columns = lines[r].split('\t');
            for (int c = 0; c < columns.size(); ++c) {
                const auto cell = cellSource(columns[c]);
                value.rows[row + r][column + c] = ' ' + cell + ' ';
            }
        }
    }
    edit.source.replace(table.start, table.end - table.start, serialize(value));
    const auto updated = tables(edit.source);
    if (index < updated.size() && row < updated[index].rows.size() && column < updated[index].rows[row].size())
        edit.anchor = edit.caret = updated[index].rows[row][column].contentStart;
    else edit.anchor = edit.caret = table.start;
    edit.changed = edit.source != source; return edit;
}

int MarkdownTableEditing::paragraphAfter(const QString &source, const Table &table) {
    int blankRows = 0, caret = -1;
    for (int start = table.end + 1; start < source.size();) {
        int end = source.indexOf('\n', start);
        if (end < 0) end = source.size();
        if (!source.mid(start, end - start).trimmed().isEmpty()) break;
        ++blankRows;
        if (blankRows == 1) caret = end;
        // Prefer a position with an existing empty separator on both sides.
        if (blankRows == 2) caret = start;
        if (end == source.size()) break;
        start = end + 1;
    }
    return caret;
}

MarkdownEdit MarkdownTableEditing::insertParagraph(const QString &source, int caret, const QString &text) {
    MarkdownEdit edit{source, caret, caret};
    if (text.isEmpty()) return edit;
    for (const auto &table : tables(source)) {
        if (paragraphAfter(source, table) != caret) continue;
        int next = caret;
        while (next < source.size()) {
            int end = source.indexOf('\n', next);
            if (end < 0) end = source.size();
            if (!source.mid(next, end - next).trimmed().isEmpty()) break;
            next = end < source.size() ? end + 1 : end;
        }
        const int beforeBreaks = source.mid(table.end, caret - table.end).count('\n');
        const QString before(qMax(0, 2 - beforeBreaks), '\n');
        const int afterBreaks = source.mid(caret, next - caret).count('\n');
        static const QRegularExpression heading("^ {0,3}#{1,6} ");
        const bool nextHeading = heading.match(source.mid(next)).hasMatch();
        const QString after(next < source.size() && !nextHeading ? qMax(0, 2 - afterBreaks) : 0, '\n');
        edit.source.insert(caret, before + text + after);
        edit.anchor = edit.caret = caret + before.size() + text.size();
        edit.handled = edit.changed = true;
        return edit;
    }
    return edit;
}

MarkdownEdit MarkdownTableEditing::newline(const QString &source, int anchor, int caret) {
    MarkdownEdit edit{source, anchor, caret};
    const int begin = qMin(anchor, caret), end = qMax(anchor, caret);
    if (begin < 0 || end > source.size()) return edit;
    for (const auto &table : tables(source)) for (const auto &row : table.rows) for (const auto &cell : row) {
        if (cell.virtualCell || begin < cell.start || end > cell.end) continue;
        edit.source.replace(begin, end - begin, "<br>");
        edit.anchor = edit.caret = begin + 4; edit.handled = edit.changed = true; return edit;
    }
    return edit;
}

MarkdownEdit MarkdownTableEditing::newline(const QString &source, int anchor, int caret, int tableIndex, int row, int column) {
    MarkdownEdit edit{source, anchor, caret};
    const auto scanned = tables(source);
    if (tableIndex < 0 || tableIndex >= scanned.size() || row < 0 || row >= scanned[tableIndex].rows.size()) return edit;
    const auto &cells = scanned[tableIndex].rows[row];
    if (column < 0 || column >= cells.size()) return edit;
    const auto &cell = cells[column];
    const int begin = qMin(anchor, caret), end = qMax(anchor, caret);
    if (!cell.virtualCell) {
        if (begin < cell.start || end > cell.end) return edit;
        edit.source.replace(begin, end - begin, "<br>");
        edit.anchor = edit.caret = begin + 4; edit.handled = edit.changed = true; return edit;
    }
    if (anchor != caret || caret != cell.end) return edit;
    int existing = 0; while (existing < cells.size() && !cells[existing].virtualCell) ++existing;
    int trimmedEnd = caret;
    while (trimmedEnd > 0 && padding(source[trimmedEnd - 1])) --trimmedEnd;
    const bool trailingPipe = trimmedEnd > 0 && source[trimmedEnd - 1] == '|' && !escaped(source, trimmedEnd - 1);
    QString inserted = trailingPipe ? QString() : QString(" |");
    for (int missing = existing; missing < column; ++missing) inserted += " |";
    inserted += " <br> |";
    edit.source.insert(caret, inserted);
    edit.anchor = edit.caret = caret + inserted.indexOf("<br>") + 4;
    edit.handled = edit.changed = true; return edit;
}
MarkdownEdit MarkdownTableEditing::pasteCell(const QString &source, int anchor, int caret, int tableIndex, int row, int column, const QString &text) {
    auto edit = newline(source, anchor, caret, tableIndex, row, column);
    if (!edit.handled) return edit;
    const auto inserted = cellSource(text);
    edit.source.replace(edit.caret - 4, 4, inserted);
    edit.anchor = edit.caret = edit.caret - 4 + inserted.size(); edit.changed = edit.source != source; return edit;
}
