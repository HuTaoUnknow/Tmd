#include "core/library/LibraryFileIndex.h"
#include "core/library/MarkdownFileIO.h"
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

QString LibraryFileIndex::key(const QString &path) {
    QString result=QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
#ifdef Q_OS_WIN
    result=result.toCaseFolded();
#endif
    return result;
}
LibraryFileIndex::Entries LibraryFileIndex::capture(const QStringList &roots, const Entries &previous) {
    Entries result;
    for (const auto &root : roots) {
        QDirIterator iterator(root,QDir::Files|QDir::Dirs|QDir::Hidden|QDir::NoDotAndDotDot|QDir::NoSymLinks,QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const auto path=iterator.next(); const QFileInfo info(path);
            if (info.fileName().startsWith(".tree-md-") || info.fileName().endsWith(".tmp")) continue;
            const QString k=key(path); if (result.contains(k)) continue;
            Entry entry{QDir::fromNativeSeparators(info.absoluteFilePath()),MarkdownFileIO::fileIdentity(path),{},info.size(),info.lastModified().toMSecsSinceEpoch(),info.isDir()};
            entry.created=info.birthTime().isValid() ? info.birthTime().toMSecsSinceEpoch() : 0;
            // Directory timestamps change when the inventory itself is saved.
            // Paths/identities describe structure; child files describe contents.
            if (entry.directory) { entry.size=0; entry.modified=0; }
            const auto old=previous.constFind(k);
            if (!entry.directory) {
                if (old!=previous.cend() && old->identity==entry.identity && old->size==entry.size && old->modified==entry.modified)
                    entry.digest=old->digest;
                else {
                    QFile file(path); if (file.open(QIODevice::ReadOnly)) {
                        QCryptographicHash hash(QCryptographicHash::Sha256);
                        if (hash.addData(&file)) entry.digest=QString::fromLatin1(hash.result().toHex());
                    }
                }
            }
            result.insert(k,entry);
        }
    }
    return result;
}
QMap<QString,QString> LibraryFileIndex::moves(const Entries &previous, const Entries &current) {
    QMap<QString,QStringList> oldIds,newIds,oldHashes,newHashes;
    for (auto it=previous.cbegin();it!=previous.cend();++it) if (!current.contains(it.key())) {
        if (!it->identity.isEmpty()) oldIds[it->identity].append(it.key());
        if (!it->directory && it->size>0 && !it->digest.isEmpty()) oldHashes[it->digest].append(it.key());
    }
    for (auto it=current.cbegin();it!=current.cend();++it) if (!previous.contains(it.key())) {
        if (!it->identity.isEmpty()) newIds[it->identity].append(it.key());
        if (!it->directory && it->size>0 && !it->digest.isEmpty()) newHashes[it->digest].append(it.key());
    }
    QMap<QString,QString> result; QSet<QString> claimed;
    for (auto it=oldIds.cbegin();it!=oldIds.cend();++it) if (it->size()==1 && newIds.value(it.key()).size()==1) {
        const QString target=newIds.value(it.key()).front();
        const auto &old=previous[it->front()]; const auto &next=current[target];
        // Deleted file IDs can be reused. An unchanged identity alone is insufficient for a different body.
        const bool sameAge=old.created>0 && old.created==next.created;
        const bool sameBody=!old.directory && !old.digest.isEmpty() && old.digest==next.digest;
        const bool directoryWithoutBirthTime=old.directory && (old.created<=0 || next.created<=0);
        if (old.directory==next.directory && (sameBody || sameAge || directoryWithoutBirthTime)) {
            result.insert(it->front(),next.path); claimed.insert(target);
        }
    }
    for (auto it=oldHashes.cbegin();it!=oldHashes.cend();++it) if (it->size()==1 && newHashes.value(it.key()).size()==1 && !result.contains(it->front())) {
        const QString target=newHashes.value(it.key()).front();
        if (!claimed.contains(target)) { result.insert(it->front(),current[target].path); claimed.insert(target); }
    }
    return result;
}
bool LibraryFileIndex::load(const QString &file, QString *error) {
    if (!QFileInfo::exists(file)) { entries.clear(); redirects.clear(); m_serialized.clear(); return true; }
    QString text; if (!MarkdownFileIO::readFile(file,text,error)) return false;
    QJsonParseError parse; const auto document=QJsonDocument::fromJson(text.toUtf8(),&parse);
    if (parse.error!=QJsonParseError::NoError || !document.isObject() || document.object().value("version").toInt()!=1) {
        if (error) *error=QStringLiteral("文件结构记录损坏，原记录已保留。"); return false;
    }
    const auto object=document.object(); Entries found;
    for (const auto &value : object.value("files").toArray()) {
        const auto item=value.toObject(); const auto path=item.value("path").toString();
        if (!QDir::isAbsolutePath(path)) { if (error) *error=QStringLiteral("文件结构记录含无效路径。"); return false; }
        found.insert(key(path),{path,item.value("identity").toString(),item.value("digest").toString(),item.value("size").toVariant().toLongLong(),item.value("modified").toVariant().toLongLong(),item.value("directory").toBool(),item.value("created").toVariant().toLongLong()});
    }
    entries=found; redirects.clear();
    const auto paths=object.value("redirects").toObject();
    for (auto it=paths.begin();it!=paths.end();++it) redirects.insert(it.key(),it.value().toString());
    m_serialized=text; return true;
}
bool LibraryFileIndex::store(const QString &file, const Entries &current, const QMap<QString,QString> &moved, QString *error) {
    Entries retained=current;
    // Keep missing entries until a later scan can identify their destination or restoration.
    for (auto it=entries.cbegin();it!=entries.cend();++it)
        if (!current.contains(it.key()) && !moved.contains(it.key())) retained.insert(it.key(),it.value());
    auto nextRedirects=redirects;
    for (auto it=moved.cbegin();it!=moved.cend();++it) {
        for (auto redirect=nextRedirects.begin();redirect!=nextRedirects.end();++redirect)
            if (key(redirect.value())==it.key()) redirect.value()=it.value();
        nextRedirects.insert(it.key(),it.value());
    }
    QJsonArray files;
    for (const auto &entry : retained) files.append(QJsonObject{{"path",entry.path},{"identity",entry.identity},{"digest",entry.digest},{"size",entry.size},{"modified",entry.modified},{"directory",entry.directory},{"created",entry.created}});
    QJsonObject redirectObject; for (auto it=nextRedirects.cbegin();it!=nextRedirects.cend();++it) redirectObject.insert(it.key(),it.value());
    const QString text=QString::fromUtf8(QJsonDocument(QJsonObject{{"version",1},{"files",files},{"redirects",redirectObject}}).toJson());
    if (text!=m_serialized && !MarkdownFileIO::writeFile(file,text,error)) return false;
    entries=retained; redirects=nextRedirects; m_serialized=text; return true;
}
