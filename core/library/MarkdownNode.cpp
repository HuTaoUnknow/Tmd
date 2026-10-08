#include "core/library/MarkdownNode.h"
#include "core/markdown/MarkdownParser.h"
#include <QFileInfo>
#include <QRegularExpression>

const MarkdownOutline &MarkdownNode::outline() const {
    if (!m_outlineReady || m_outlineSource != m_content) {
        m_outline = MarkdownParser::outline(m_content); m_outlineSource = m_content; m_outlineReady = true;
    }
    return m_outline;
}
QString MarkdownNode::title() const {
    const auto &headings = outline();
    if (!headings.isEmpty() && !headings.front().text.isEmpty()) return headings.front().text;
    return QFileInfo(m_relativePath).completeBaseName();
}
QStringList MarkdownNode::relatedPaths(NodeRelationType type) const {
    const int index = static_cast<int>(type);
    return index >= 0 && index < 5 ? m_relations[index] : QStringList{};
}
