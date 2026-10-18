#pragma once
#include "core/markdown/MarkdownEdit.h"
#include <optional>

namespace tmd {
enum class ViewMode { Source, Rendered };
enum class EditCommand { Newline, Indent, Outdent, Backspace, DeleteForward,
                         NextCell, AddColumn, AddRow };
enum class EditHandler { None, Table, Code, List, Prefix, Paragraph };
struct SourceSelection { int anchor = 0, caret = 0; };
struct TableCellAddress { int table = 0, row = 0, column = 0; };
struct EditRequest {
    QString source;
    SourceSelection selection;
    EditCommand command = EditCommand::Newline;
    ViewMode mode = ViewMode::Source;
    std::optional<TableCellAddress> tableCell;
    // Qt maps a caret at the end of hidden inline markup before dispatch.
    std::optional<SourceSelection> paragraphSelection;
    // The rendered view distinguishes whitespace-only display blocks.
    bool listBackspaceAllowed = true;
};
struct DispatchResult {
    MarkdownEdit edit;
    EditHandler handler = EditHandler::None;
};
class EditCommandRouter final {
public:
    static DispatchResult dispatch(const EditRequest &request);
    static std::optional<TableCellAddress> tableCellAt(const QString &source, int caret);
};
}
