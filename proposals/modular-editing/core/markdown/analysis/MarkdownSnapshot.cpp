#include "core/markdown/analysis/MarkdownSnapshot.h"
#include <utility>

namespace tmd::proposal {
MarkdownSnapshot::MarkdownSnapshot(QString source, Mode mode)
    : m_source(std::move(source)), m_mode(mode) {}

const MarkdownAnalysis &MarkdownSnapshot::analysis() const {
    if (!m_analysis) {
        m_analysis = m_mode == Mode::Rendering ? MarkdownAnalysis::forRendering(m_source)
                                             : MarkdownAnalysis::scan(m_source);
        ++m_counts.analysis;
    }
    return *m_analysis;
}
const MarkdownSyntax::Result &MarkdownSnapshot::syntax() const {
    if (!m_syntax) {
        m_syntax = MarkdownSyntax::scan(m_source, &analysis());
        ++m_counts.syntax;
    }
    return *m_syntax;
}
const QVector<MarkdownImage> &MarkdownSnapshot::images() const {
    if (!m_images) {
        m_images = MarkdownImages::parse(m_source, analysis());
        ++m_counts.images;
    }
    return *m_images;
}
const QVector<MarkdownCodeBlocks::Block> &MarkdownSnapshot::codeBlocks() const {
    if (!m_codeBlocks) {
        m_codeBlocks = MarkdownCodeBlocks::scan(m_source, &analysis());
        ++m_counts.codeBlocks;
    }
    return *m_codeBlocks;
}
}
