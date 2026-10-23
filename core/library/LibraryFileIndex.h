#pragma once
#include <QString>
#include <QStringList>
#include <QMap>

// Persistent identities and digests distinguish moves from actual deletion.
class LibraryFileIndex {
public:
    struct Entry {
        QString path, identity, digest;
        qint64 size = 0, modified = 0;
        bool directory = false;
        qint64 created = 0;
    };
    using Entries = QMap<QString, Entry>;
    static QString key(const QString &path);
    static Entries capture(const QStringList &roots, const Entries &previous);
    static QMap<QString,QString> moves(const Entries &previous, const Entries &current);
    bool load(const QString &file, QString *error);
    bool store(const QString &file, const Entries &current, const QMap<QString,QString> &moves, QString *error);
    Entries entries;
    QMap<QString,QString> redirects;
private:
    QString m_serialized;
};
