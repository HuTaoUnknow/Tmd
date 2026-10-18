#include "application/editing/EditCommandRouter.h"
#include "core/markdown/MarkdownBlockSyntax.h"
#include "core/markdown/MarkdownCodeEditing.h"
#include "core/markdown/MarkdownListEditing.h"
#include "core/markdown/MarkdownParagraphEditing.h"
#include "core/markdown/MarkdownTableEditing.h"
#include <utility>

namespace tmd::proposal {
std::optional<TableCellAddress> EditCommandRouter::tableCellAt(const QString &source, int caret) {
    const auto tables = MarkdownTableEditing::tables(source);
    for (int t = 0; t < tables.size(); ++t)
        for (int r = 0; r < tables[t].rows.size(); ++r)
            for (int c = 0; c < tables[t].rows[r].size(); ++c) {
                const auto &cell = tables[t].rows[r][c];
                if (caret >= cell.start && caret <= cell.end) return TableCellAddress{t, r, c};
            }
    return std::nullopt;
}

DispatchResult EditCommandRouter::dispatch(const EditRequest &r) {
    const int anchor = r.selection.anchor, caret = r.selection.caret;
    DispatchResult result{{r.source, anchor, caret}, EditHandler::None};
    const auto valid = [&r](SourceSelection s) {
        return s.anchor >= 0 && s.caret >= 0 && s.anchor <= r.source.size() && s.caret <= r.source.size();
    };
    if (!valid(r.selection) || (r.paragraphSelection && !valid(*r.paragraphSelection))) return result;
    const auto take = [&result](MarkdownEdit edit, EditHandler handler) {
        if (!edit.handled) return false;
        result = {std::move(edit), handler};
        return true;
    };
    using Action = MarkdownTableEditing::Action;
    const bool tableAction = r.command == EditCommand::NextCell || r.command == EditCommand::AddColumn || r.command == EditCommand::AddRow;
    if (tableAction) {
        if (!r.tableCell) return result;
        const auto cell = *r.tableCell;
        const auto tables = MarkdownTableEditing::tables(r.source);
        if (cell.table < 0 || cell.table >= tables.size() || cell.row < 0 || cell.row >= tables[cell.table].rows.size()
            || cell.column < 0 || cell.column >= tables[cell.table].rows[cell.row].size()) return result;
        const auto action = r.command == EditCommand::AddRow ? Action::AddRow
            : r.command == EditCommand::AddColumn ? Action::AddColumn : Action::NextCell;
        take(MarkdownTableEditing::operate(r.source, cell.table, cell.row, cell.column, action), EditHandler::Table);
        return result;
    }
    const bool newline = r.command == EditCommand::Newline;
    const bool indent = r.command == EditCommand::Indent || r.command == EditCommand::Outdent;
    const bool outdent = r.command == EditCommand::Outdent;
    const bool rendered = r.mode == ViewMode::Rendered;
    if (newline && r.tableCell) {
        const auto cell = *r.tableCell;
        take(MarkdownTableEditing::newline(r.source, anchor, caret, cell.table, cell.row, cell.column), EditHandler::Table);
        return result;
    }
    if (newline && !rendered && take(MarkdownTableEditing::newline(r.source, anchor, caret), EditHandler::Table)) return result;
    if (newline || indent) {
        if (take(indent ? MarkdownCodeEditing::indent(r.source, anchor, caret, outdent)
                        : MarkdownCodeEditing::newline(r.source, anchor, caret), EditHandler::Code)) return result;
        if (take(indent ? MarkdownListEditing::indent(r.source, anchor, caret, outdent)
                        : MarkdownListEditing::newline(r.source, anchor, caret), EditHandler::List)) return result;
        if (newline) {
            const auto paragraph = r.paragraphSelection.value_or(r.selection);
            take(MarkdownParagraphEditing::newline(r.source, paragraph.anchor, paragraph.caret, rendered), EditHandler::Paragraph);
        }
        return result;
    }
    if (r.command == EditCommand::Backspace && anchor == caret) {
        if (take(MarkdownListEditing::backspaceIndent(r.source, caret), EditHandler::List)) return result;
        // Preserve the existing, deliberately different source/rendered order.
        if (rendered && take(MarkdownBlockSyntax::backspaceEmptyPrefix(r.source, caret), EditHandler::Prefix)) return result;
        if (take(MarkdownCodeEditing::backspace(r.source, caret), EditHandler::Code)) return result;
        if (!rendered && take(MarkdownBlockSyntax::backspaceEmptyPrefix(r.source, caret), EditHandler::Prefix)) return result;
        if (take(MarkdownParagraphEditing::backspace(r.source, caret, rendered), EditHandler::Paragraph)) return result;
        if (!rendered || r.listBackspaceAllowed)
            take(MarkdownListEditing::backspace(r.source, caret), EditHandler::List);
    } else if (r.command == EditCommand::DeleteForward && rendered && anchor == caret) {
        take(MarkdownParagraphEditing::deleteForward(r.source, caret), EditHandler::Paragraph);
    }
    return result;
}
}
