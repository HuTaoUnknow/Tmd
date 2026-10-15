#pragma once
#include "core/markdown/MarkdownEdit.h"
#include <QVector>

namespace MarkdownTableEditing {
// UTF-16 source offsets. Cell ranges exclude structural pipes and retain
// padding; content ranges exclude padding and may be empty.
struct Cell { int start = 0, end = 0, contentStart = 0, contentEnd = 0; bool virtualCell = false; };
struct Table { int start = 0, end = 0; QVector<QVector<Cell>> rows; };
QVector<Table> tables(const QString &source);
// Source anchor for Qt's empty paragraph after a table. Typing there must
// preserve enough blank separators to keep the body outside the table.
int paragraphAfter(const QString &source, const Table &table);
MarkdownEdit insertParagraph(const QString &source, int caret, const QString &text);
MarkdownEdit newline(const QString &source, int anchor, int caret);
MarkdownEdit newline(const QString &source, int anchor, int caret, int tableIndex, int row, int column);
MarkdownEdit pasteCell(const QString &source, int anchor, int caret, int tableIndex, int row, int column, const QString &text);
enum class Action { AddColumn, AddRow, DeleteColumn, DeleteRow, NextCell, Leave, Paste };
MarkdownEdit operate(const QString &source, int tableIndex, int row, int column, Action action, const QString &text = {});
MarkdownEdit create(const QString &source, int caret);
QString normalize(const QString &source);
}
