#pragma once
#include <QTextBlock>
#include <QTextFormat>
namespace CodeBlockFormat {
constexpr int Id = QTextFormat::UserProperty + 140;
inline bool isCode(const QTextBlock &block) {
    return block.isValid() && (block.blockFormat().nonBreakableLines() || block.blockFormat().hasProperty(QTextFormat::BlockCodeFence));
}
inline bool samePanel(const QTextBlock &a, const QTextBlock &b) {
    return isCode(a) && isCode(b) && a.blockFormat().intProperty(Id) == b.blockFormat().intProperty(Id)
        && a.blockFormat().stringProperty(QTextFormat::BlockCodeLanguage) == b.blockFormat().stringProperty(QTextFormat::BlockCodeLanguage);
}
}
