#pragma once
#include "core/markdown/MarkdownAnalysis.h"
#include "core/markdown/MarkdownSyntax.h"
#include "core/markdown/MarkdownImages.h"
#include "core/markdown/MarkdownCodeBlocks.h"
#include <optional>

namespace tmd::proposal {
// Immutable source, lazy derived values. One owner on the GUI thread. Raw and
// rendering analysis intentionally remain distinct, including unpaired fences.
class MarkdownSnapshot final {
public:
    enum class Mode { Raw, Rendering };
    struct BuildCounts { int analysis = 0, syntax = 0, images = 0, codeBlocks = 0; };
    explicit MarkdownSnapshot(QString source, Mode mode);
    MarkdownSnapshot(const MarkdownSnapshot &) = delete;
    MarkdownSnapshot &operator=(const MarkdownSnapshot &) = delete;
    const QString &source() const { return m_source; }
    Mode mode() const { return m_mode; }
    const MarkdownAnalysis &analysis() const;
    const MarkdownSyntax::Result &syntax() const;
    const QVector<MarkdownImage> &images() const;
    const QVector<MarkdownCodeBlocks::Block> &codeBlocks() const;
    BuildCounts buildCounts() const { return m_counts; }
private:
    const QString m_source;
    const Mode m_mode;
    mutable std::optional<MarkdownAnalysis> m_analysis;
    mutable std::optional<MarkdownSyntax::Result> m_syntax;
    mutable std::optional<QVector<MarkdownImage>> m_images;
    mutable std::optional<QVector<MarkdownCodeBlocks::Block>> m_codeBlocks;
    mutable BuildCounts m_counts;
};
}
