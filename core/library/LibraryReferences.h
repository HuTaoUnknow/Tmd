#pragma once
#include <QString>
#include <QStringList>
#include <QMap>
#include <QSet>

class LibraryReferences {
public:
    static QString rewrite(const QString &content, const QString &root, const QString &oldDocument,
                           const QString &newDocument, const QMap<QString,QString> &moves,
                           const QSet<QString> &knownPaths);
    static QStringList missing(const QString &content, const QString &root, const QString &document);
    static int mapPosition(const QString &before, const QString &after, int position);
};
