#pragma once
#include "core/markdown/MarkdownAnalysis.h"

namespace MarkdownCodeBlocks {
struct Block {
    MarkdownAnalysis::Fence fence;
    QString info, language, label, code;
    QVector<MarkdownAnalysis::Line> lines;
};
QVector<Block> scan(const QString &source, const MarkdownAnalysis *analysis = nullptr);
}
