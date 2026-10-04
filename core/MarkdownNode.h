#pragma once
#include <QString>
#include <QStringList>
#include <array>

// Parent means a comparable alternative; Child means an explanation or example.
// Neither denotes a filesystem parent/child relationship.
// Alternatives are peers. DetailOf is the internal backlink of a Child relation.
enum class NodeRelationType { Previous, Next, Parent, Child, DetailOf };

class MarkdownNode {
public:
    QString filePath() const { return m_filePath; }
    QString relativePath() const { return m_relativePath; }
    QString title() const;
    QString content() const { return m_content; }
    bool isModified() const { return m_content != m_savedContent; }
    bool hasExternalChange() const { return m_externalChange; }
    QStringList relatedPaths(NodeRelationType type) const;
private:
    friend class MarkdownManager;
    QString m_filePath;
    QString m_relativePath;
    QString m_fileIdentity;
    QString m_content;
    QString m_savedContent;
    bool m_externalChange = false;
    std::array<QStringList, 5> m_relations;
};
