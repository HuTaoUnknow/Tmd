#include "core/library/MarkdownManager.h"
#include "core/markdown/MarkdownImages.h"
#include "core/images/ImageStorage.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QScopedValueRollback>
#include <algorithm>
LibraryImportResult MarkdownManager::importDirectory(const QString &sourceDirectory) {
    LibraryImportResult result; const QDir source(sourceDirectory);
    if (!source.exists()) { result.error = QStringLiteral("导入目录不存在。"); return result; }
    if (!scan(&result.error) || !ImageStorage(m_root).ensureRoot(&result.error)) return result;
    QStringList files; QDirIterator iterator(source.path(), QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        if (QFileInfo(path).suffix().compare("md", Qt::CaseInsensitive) == 0) files.append(path);
    }
    std::sort(files.begin(), files.end());
    {
        const QScopedValueRollback<bool> batch(m_batchImport, true);
        for (const auto &file : files) {
            const QString relative = source.relativeFilePath(file); QString error;
            if (importDocument(file, relative, &error)) result.imported.append(relative);
            else result.failures.insert(relative, error);
        }
    }
    // Partial successes are retained, matching single-document import. Publish
    // one complete collection after the batch instead of rescanning per file.
    if (!scan(&result.error)) return result;
    for (const auto &relative : result.imported)
        if (const auto *node = loadNode(relative)) result.imageReferences += MarkdownImages::parse(node->content()).size();
    return result;
}
