#include "core/library/LibraryReferences.h"
#include "core/library/DocumentLinkResolver.h"
#include "core/library/LibraryFileIndex.h"
#include "core/markdown/MarkdownReferences.h"
#include "core/markdown/editing/TextPatch.h"
#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace {
QString localPath(const MarkdownReference &ref, const QString &root, const QString &document, const QSet<QString> &known = {}) {
    const QUrl url(ref.source);
    if (!ref.image) return DocumentLinkResolver::localPath(root,document,url,known);
    if (!url.isValid() || url.path().isEmpty()) return {};
    const bool drive = url.scheme().size()==1 && (url.path().startsWith('/') || url.path().startsWith('\\'));
    if (!url.scheme().isEmpty() && !url.isLocalFile() && !drive) return {};
    QString path=url.isLocalFile() ? url.toLocalFile() : drive ? url.scheme()+':'+url.path() : url.path();
    path=QDir::fromNativeSeparators(path);
    return QDir::cleanPath(QDir::isAbsolutePath(path) ? path : QFileInfo(document).dir().absoluteFilePath(path));
}
QString mapped(const QString &path, const QMap<QString,QString> &moves) {
    const QString key=LibraryFileIndex::key(path);
    if (moves.contains(key)) return moves.value(key);
    QString best, target;
    for (auto it=moves.cbegin();it!=moves.cend();++it)
        if (key.startsWith(it.key()+'/') && it.key().size()>best.size()) { best=it.key(); target=it.value(); }
    return best.isEmpty() ? path : target+path.mid(best.size());
}
QString spelling(const QString &original, const QString &target, const QString &document) {
    const QUrl url(original);
    QString path;
    if (url.isLocalFile()) path=QUrl::fromLocalFile(target).toString(QUrl::FullyEncoded);
    else {
        const bool drive=url.scheme().size()==1 && (url.path().startsWith('/') || url.path().startsWith('\\'));
        const bool absolute=drive || QDir::isAbsolutePath(url.path());
        path=QString::fromLatin1(QUrl::toPercentEncoding(absolute ? target : QFileInfo(document).dir().relativeFilePath(target),"/:"));
    }
    if (url.hasQuery()) path+='?'+url.query(QUrl::FullyEncoded);
    if (url.hasFragment()) path+='#'+url.fragment(QUrl::FullyEncoded);
    return path;
}
}
QString LibraryReferences::rewrite(const QString &content, const QString &root, const QString &oldDocument,
                                    const QString &newDocument, const QMap<QString,QString> &moves, const QSet<QString> &known) {
    auto result=content; const auto refs=MarkdownReferences::parse(content);
    for (auto it=refs.crbegin();it!=refs.crend();++it) {
        const QString oldTarget=localPath(*it,root,oldDocument,known);
        if (oldTarget.isEmpty()) continue;
        const QString target=mapped(oldTarget,moves);
        const QUrl url(it->source);
        if (LibraryFileIndex::key(oldTarget)==LibraryFileIndex::key(target)
            && (!url.isRelative() || QDir::isAbsolutePath(url.path()))) continue;
        if (LibraryFileIndex::key(oldTarget)==LibraryFileIndex::key(target)
            && LibraryFileIndex::key(oldDocument)==LibraryFileIndex::key(newDocument)) continue;
        // A missing asset is never recreated or retargeted to a guessed file.
        if (!QFileInfo::exists(target) && LibraryFileIndex::key(oldDocument)==LibraryFileIndex::key(newDocument)) continue;
        const auto replacement=spelling(it->source,target,newDocument);
        if (replacement!=it->source) result.replace(it->start,it->length,replacement);
    }
    return result;
}
QStringList LibraryReferences::missing(const QString &content, const QString &root, const QString &document) {
    QStringList result;
    for (const auto &ref : MarkdownReferences::parse(content)) {
        const auto path=localPath(ref,root,document);
        if (!path.isEmpty() && (!QFileInfo::exists(path) || (ref.image && !QFileInfo(path).isFile())))
            result.append((ref.image ? QStringLiteral("图片目标不存在：") : QStringLiteral("链接目标不存在："))+ref.source);
    }
    result.removeDuplicates(); return result;
}
int LibraryReferences::mapPosition(const QString &before, const QString &after, int position) {
    position=qBound(0,position,int(before.size()));
    const auto oldRefs=MarkdownReferences::parse(before), newRefs=MarkdownReferences::parse(after);
    if (oldRefs.size()==newRefs.size()) {
        int delta=0;
        for (int i=0;i<oldRefs.size();++i) {
            const auto &old=oldRefs[i], &next=newRefs[i];
            if (position<old.start) return qBound(0,position+delta,int(after.size()));
            if (position<=old.start+old.length) return next.start+qMin(position-old.start,next.length);
            delta=next.start+next.length-old.start-old.length;
        }
        return qBound(0,position+delta,int(after.size()));
    }
    const auto patch=tmd::TextPatch::between(before,after);
    const int mapped=position<=patch.start ? position : position>=patch.start+patch.removed ? position+int(patch.inserted.size())-patch.removed : qMin(position,patch.start+int(patch.inserted.size()));
    return qBound(0,mapped,int(after.size()));
}
