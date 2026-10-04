#include "MarkdownNode.h"
#include "MarkdownParser.h"
#include <QFileInfo>
#include <QRegularExpression>

QString MarkdownNode::title() const {
    const auto headings = MarkdownParser::outline(m_content);
    if (!headings.isEmpty() && !headings.front().text.isEmpty()) return headings.front().text;
    return QFileInfo(m_relativePath).completeBaseName();
}
QStringList MarkdownNode::relatedPaths(NodeRelationType type) const {
    const int index = static_cast<int>(type);
    return index >= 0 && index < 5 ? m_relations[index] : QStringList{};
}
