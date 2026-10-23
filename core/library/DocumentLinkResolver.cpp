#include "core/library/DocumentLinkResolver.h"
#include "core/library/LibraryPathPolicy.h"
#include "core/library/LibraryFileIndex.h"
#include <QDir>
#include <QFileInfo>

DocumentLinkResolver::Target DocumentLinkResolver::resolve(const QString &libraryRoot, const QString &documentPath, const QUrl &link) {
    Target result; result.fragment = link.fragment();
    if (!link.isValid() || link.isEmpty()) return result;
    if (link.isRelative() && link.path().isEmpty() && link.hasFragment()) {
        result.kind = Kind::Anchor; result.url = link; return result;
    }
    const bool drive = link.scheme().size() == 1 && (link.path().startsWith('/') || link.path().startsWith('\\'));
    if (!link.scheme().isEmpty() && !link.isLocalFile() && !drive) {
        result.kind = Kind::External; result.url = link; return result;
    }
    const QString local = localPath(libraryRoot, documentPath, link);
    if (local.isEmpty()) return result;
    const QFileInfo file(local);
    if (!file.exists()) { result.error = QStringLiteral("链接目标不存在：%1").arg(local); return result; }
    result.url = QUrl::fromLocalFile(local); result.url.setFragment(result.fragment);
    const auto internal = file.isFile() ? LibraryPathPolicy(libraryRoot).document(local) : QString();
    if (!internal.isEmpty()) { result.kind = Kind::Document; result.path = internal; }
    else { result.kind = Kind::External; result.path = local; }
    return result;
}
QString DocumentLinkResolver::localPath(const QString &libraryRoot, const QString &documentPath, const QUrl &link, const QSet<QString> &knownPaths) {
    const bool drive = link.scheme().size() == 1 && (link.path().startsWith('/') || link.path().startsWith('\\'));
    if (!link.isValid() || (link.isRelative() && link.path().isEmpty()) || (!link.scheme().isEmpty() && !link.isLocalFile() && !drive)) return {};
    const QDir root(libraryRoot);
    QString local = link.isLocalFile() ? link.toLocalFile() : drive ? link.scheme() + ':' + link.path() : link.path();
    local = QDir::fromNativeSeparators(local);
    if (!QDir::isAbsolutePath(local)) {
        const QString current = QDir::isAbsolutePath(documentPath) ? documentPath : root.absoluteFilePath(documentPath);
        const QString relative = QFileInfo(current).dir().absoluteFilePath(local);
        const QString fromRoot = root.absoluteFilePath(local);
        // A library-prefixed index also works from documents in subfolders.
        const QString prefix = QFileInfo(root.absolutePath()).fileName() + '/';
        if (local.startsWith(prefix, Qt::CaseInsensitive)) local = root.absoluteFilePath(local.mid(prefix.size()));
        else {
            const auto exists = [&](const QString &path) { return knownPaths.contains(LibraryFileIndex::key(path)) || QFileInfo::exists(path); };
            local = exists(relative) || !exists(fromRoot) ? relative : fromRoot;
        }
    }
    return QDir::cleanPath(local);
}
