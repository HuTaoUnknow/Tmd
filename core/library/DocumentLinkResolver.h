#pragma once
#include <QString>
#include <QUrl>
#include <QSet>

// Resolve links against the current document without opening files or apps.
class DocumentLinkResolver {
public:
    enum class Kind { Invalid, Anchor, Document, External };
    struct Target {
        Kind kind = Kind::Invalid;
        QUrl url;
        QString path, fragment, error;
    };
    static Target resolve(const QString &libraryRoot, const QString &documentPath, const QUrl &link);
    static QString localPath(const QString &libraryRoot, const QString &documentPath, const QUrl &link,
                             const QSet<QString> &knownPaths = {});
};
