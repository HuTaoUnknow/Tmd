#pragma once
#include "core/markdown/MarkdownOutline.h"
#include "core/knowledge/NodeRelationType.h"
#include <QString>
#include <QStringList>
#include <array>

class MarkdownNode {
public:
    QString filePath() const { return m_filePath; }
    QString relativePath() const { return m_relativePath; }
    QString title() const;
    const MarkdownOutline &outline() const;
    QString content() const { return m_content; }
    bool isModified() const { return m_content != m_savedContent; }
    bool hasExternalChange() const { return m_externalChange; }
    bool isExternalDocument() const { return m_externalDocument; }
    bool isSourceMissing() const { return m_sourceMissing; }
    QStringList missingReferences() const { return m_missingReferences; }
    bool hasMissingReferences() const { return !m_missingReferences.isEmpty(); }
    QStringList relatedPaths(NodeRelationType type) const;
private:
    friend class MarkdownManager;
    friend class ExternalDocuments;
    QString m_filePath;
    QString m_relativePath;
    QString m_fileIdentity;
    QString m_content;
    QString m_savedContent;
    QStringList m_missingReferences;
    mutable QString m_outlineSource;
    mutable MarkdownOutline m_outline;
    mutable bool m_outlineReady = false;
    bool m_externalChange = false;
    bool m_externalDocument = false, m_sourceMissing = false;
    std::array<QStringList, 5> m_relations;
};
