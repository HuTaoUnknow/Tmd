#include "core/markdown/MarkdownCodeBlocks.h"
#include "core/markdown/LanguageCatalog.h"
#include <QRegularExpression>

QVector<MarkdownCodeBlocks::Block> MarkdownCodeBlocks::scan(const QString &source, const MarkdownAnalysis *provided) {
    const auto owned = provided ? MarkdownAnalysis{} : MarkdownAnalysis::forRendering(source);
    const auto &analysis = provided ? *provided : owned;
    QVector<Block> result; int nextLine = 0;
    for (const auto &fence : analysis.fences) {
        if (!fence.closed()) continue;
        const int infoStart = fence.start + fence.indent.size() + fence.delimiter.size();
        const QString info = source.mid(infoStart, fence.headerEnd - infoStart);
        Block block; block.fence = fence; block.info = info.trimmed().section(QRegularExpression("\\s+"), 0, 0);
        block.language = LanguageCatalog::canonicalName(info); block.label = LanguageCatalog::displayName(info);
        while (nextLine < analysis.lines.size() && analysis.lines[nextLine].start < fence.bodyStart) ++nextLine;
        while (nextLine < analysis.lines.size() && analysis.lines[nextLine].start < fence.closeStart) {
            const auto &line = analysis.lines[nextLine++];
            block.lines.append(line);
            block.code += source.mid(line.contentStart, line.end - line.contentStart);
            if (line.end < source.size() && source[line.end] == '\n') block.code += '\n';
        }
        result.append(block);
    }
    return result;
}
