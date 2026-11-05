#include "ui/document/MarkdownTableStyle.h"
#include "core/settings/MarkdownSettings.h"
#include <QTextDocument>
#include <QTextTable>
#include <QTextBlock>

namespace {
QColor background(const MarkdownSettings &settings, MarkdownSettings::Color role) {
    const auto color = settings.color(role);
    // Earlier profiles used a filled green grid by default. Give that default
    // the new neutral appearance without replacing custom table colors.
    const QRgb old = role == MarkdownSettings::TableHeader ? 0xff344239
        : role == MarkdownSettings::TableOdd ? 0xff242b27 : 0xff2b332e;
    return color.rgb() == old ? settings.color(MarkdownSettings::PageBackground) : color;
}
void applyFrame(QTextFrame *frame, const MarkdownSettings &settings) {
    for (auto *child : frame->childFrames()) {
        auto *table = qobject_cast<QTextTable *>(child);
        if (!table) { applyFrame(child, settings); continue; }
        auto format = table->format();
        format.setBorder(0); format.setCellSpacing(0); format.setBorderCollapse(true);
        format.setCellPadding(settings.number(MarkdownSettings::TablePadding));
        format.setHeaderRowCount(1); format.setWidth(QTextLength(QTextLength::PercentageLength, 100)); table->setFormat(format);
        auto rule = settings.color(MarkdownSettings::TableBorder);
        if (rule.rgb() == QRgb(0xff46554d)) {
            rule = settings.color(MarkdownSettings::TextColor); rule.setAlpha(55);
        }
        for (int row = 0; row < table->rows(); ++row) for (int col = 0; col < table->columns(); ++col) {
            auto cell = table->cellAt(row, col); auto cellFormat = cell.format().toTableCellFormat();
            cellFormat.setTopBorder(0); cellFormat.setLeftBorder(0); cellFormat.setRightBorder(0);
            cellFormat.setBottomBorder(row == 0 ? 1.25 : .65);
            cellFormat.setBottomBorderStyle(QTextFrameFormat::BorderStyle_Solid); cellFormat.setBottomBorderBrush(rule);
            const auto role = row == 0 ? MarkdownSettings::TableHeader
                : settings.enabled(MarkdownSettings::StripedTables) && row % 2 ? MarkdownSettings::TableOdd : MarkdownSettings::TableEven;
            cellFormat.setBackground(background(settings, role)); cell.setFormat(cellFormat);
            for (auto block = cell.firstCursorPosition().block(); block.isValid() && block.position() <= cell.lastCursorPosition().position(); block = block.next()) {
                QTextCursor cursor(block); auto blockFormat = block.blockFormat();
                blockFormat.setTopMargin(0); blockFormat.setBottomMargin(0); cursor.setBlockFormat(blockFormat);
            }
            if (!row) {
                auto header = cell.firstCursorPosition(); header.setPosition(cell.lastCursorPosition().position(), QTextCursor::KeepAnchor);
                QTextCharFormat bold; bold.setFontWeight(QFont::Bold); header.mergeCharFormat(bold);
            }
        }
        applyFrame(table, settings);
    }
}
}
void MarkdownTableStyle::apply(QTextDocument *document, const MarkdownSettings &settings) { applyFrame(document->rootFrame(), settings); }
