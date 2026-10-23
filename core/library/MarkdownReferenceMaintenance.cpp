#include "core/library/MarkdownManager.h"
#include "core/library/MarkdownFileIO.h"
#include "core/library/LibraryReferences.h"
#include "core/images/ImageStorage.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

void MarkdownManager::updateReferenceStatus(MarkdownNode *node) {
    if (node) node->m_missingReferences=LibraryReferences::missing(node->content(),m_root,node->filePath());
}
void MarkdownManager::rememberSavedFile(MarkdownNode *node) {
    if (!node || !m_fileIndexLoaded || !m_fileIndexWritable) return;
    QString reloadError;
    if (!m_fileIndex.load(QDir(m_root).filePath(".tree-md-files.json"),&reloadError)) {
        m_warnings.append(reloadError); return;
    }
    const auto captured=LibraryFileIndex::capture({QFileInfo(node->filePath()).absolutePath()},m_fileIndex.entries);
    const auto key=LibraryFileIndex::key(node->filePath());
    auto updated=m_fileIndex.entries;
    if (captured.contains(key)) updated.insert(key,captured.value(key));
    QString error;
    if (!m_fileIndex.store(QDir(m_root).filePath(".tree-md-files.json"),updated,{},&error))
        m_warnings.append(QStringLiteral("文件结构记录保存失败：%1").arg(error));
}
QMap<QString,QStringList> MarkdownManager::duplicateDocuments() const {
    QMap<QString,QStringList> grouped;
    for (const auto *node : allNodes()) grouped[QFileInfo(node->relativePath()).fileName().normalized(QString::NormalizationForm_C).toCaseFolded()].append(node->relativePath());
    for (auto it=grouped.begin();it!=grouped.end();) {
        if (it->size()<2) it=grouped.erase(it);
        else { it->sort(); ++it; }
    }
    return grouped;
}
bool MarkdownManager::validateDocumentName(const QString &path, QString *error, const QString &except) const {
    const QString absolute=safePath(path,error);
    if (absolute.isEmpty()) return false;
    const QString name=QFileInfo(absolute).fileName().normalized(QString::NormalizationForm_C).toCaseFolded();
    const QString ignored=except.isEmpty() ? QString() : LibraryFileIndex::key(safePath(except));
    QDirIterator iterator(m_root,QDir::Files|QDir::Hidden|QDir::NoSymLinks,QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString file=iterator.next();
        if (QFileInfo(file).suffix().compare("md",Qt::CaseInsensitive)!=0 || LibraryFileIndex::key(file)==ignored) continue;
        if (QFileInfo(file).fileName().normalized(QString::NormalizationForm_C).toCaseFolded()==name) {
            if (error) *error=QStringLiteral("整个 md_data 中不允许同名 MD 文件。\n已有：%1\n请修改文件名后重试。").arg(QDir(m_root).relativeFilePath(file));
            return false;
        }
    }
    return true;
}
void MarkdownManager::maintainReferences(const QMap<QString,QString> &moves, const LibraryFileIndex::Entries &inventory,
                                        const QMap<QString,QString> &bufferOrigins) {
    QSet<QString> known;
    for (auto it=m_fileIndex.entries.cbegin();it!=m_fileIndex.entries.cend();++it) known.insert(it.key());
    for (auto it=m_fileIndex.redirects.cbegin();it!=m_fileIndex.redirects.cend();++it) known.insert(it.key());
    auto redirects=moves;
    for (auto it=m_fileIndex.redirects.cbegin();it!=m_fileIndex.redirects.cend();++it)
        if (!QFileInfo::exists(it.key()) && !redirects.contains(it.key())) redirects.insert(it.key(),it.value());
    QVector<QPair<QString,QPair<QString,QString>>> notifications;
    for (auto *node : allNodes()) {
        QString origin=node->filePath();
        for (auto it=moves.cbegin();it!=moves.cend();++it)
            if (LibraryFileIndex::key(it.value())==LibraryFileIndex::key(node->filePath())) {
                const auto previous=m_fileIndex.entries.value(it.key());
                const auto current=inventory.value(LibraryFileIndex::key(node->filePath()));
                bool ownMove=false;
                for (const auto &target : m_pendingReferenceMoves)
                    if (LibraryFileIndex::key(node->filePath())==LibraryFileIndex::key(target) || LibraryFileIndex::key(node->filePath()).startsWith(LibraryFileIndex::key(target)+'/')) ownMove=true;
                // A different body at a moved path may already contain another
                // window's repaired URLs. Rebase only a known original body.
                if (ownMove || (!previous.digest.isEmpty() && previous.digest==current.digest)) origin=previous.path.isEmpty() ? it.key() : previous.path;
                break;
            }
        const QString before=node->m_content;
        const QString bufferOrigin=bufferOrigins.value(LibraryFileIndex::key(node->filePath()),origin);
        const auto rewrite=[&](const QString &source) { return LibraryReferences::rewrite(source,m_root,bufferOrigin,node->filePath(),redirects,known); };
        const QString rewritten=rewrite(before);
        QString disk, error;
        if (MarkdownFileIO::readFile(node->filePath(),disk,&error)) {
            const QString newDisk=LibraryReferences::rewrite(disk,m_root,origin,node->filePath(),redirects,known);
            bool saved=true;
            if (newDisk!=disk) {
                QString latest;
                saved=MarkdownFileIO::readFile(node->filePath(),latest,&error) && latest==disk;
                if (!saved && error.isEmpty()) error=QStringLiteral("文件在同步引用期间再次发生变化。");
                if (saved) saved=MarkdownFileIO::writeFile(node->filePath(),newDisk,&error);
            }
            if (saved) {
                // Patch the saved baseline separately; never save the user's pending text.
                node->m_savedContent=rewrite(node->m_savedContent);
                node->m_externalChange=newDisk!=node->m_savedContent;
                node->m_fileIdentity=MarkdownFileIO::fileIdentity(node->filePath());
            } else m_warnings.append(QStringLiteral("引用自动保存失败：%1：%2").arg(node->relativePath(),error));
        } else m_warnings.append(node->relativePath()+": "+error);
        node->m_content=rewritten;
        updateReferenceStatus(node);
        if (before!=rewritten) notifications.append({node->relativePath(),{before,rewritten}});
    }
    const auto conflicts=duplicateDocuments();
    for (auto it=conflicts.cbegin();it!=conflicts.cend();++it)
        m_warnings.append(QStringLiteral("MD 文件重名，请修改文件名：%1").arg(it->join(QStringLiteral("、"))));
    if (m_fileIndexWritable) {
        const auto updated=LibraryFileIndex::capture({m_root,ImageStorage(m_root).photoRoot()},inventory);
        QString error;
        if (!m_fileIndex.store(QDir(m_root).filePath(".tree-md-files.json"),updated,moves,&error))
            m_warnings.append(QStringLiteral("文件结构记录保存失败：%1").arg(error));
    }
    for (const auto &notification : notifications) emit referencesRewritten(notification.first,notification.second.first,notification.second.second);
}
