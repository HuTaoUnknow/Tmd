#pragma once
#include <QString>
class LibraryPathPolicy {
public:
    explicit LibraryPathPolicy(QString root);
    QString document(const QString &path, QString *error = nullptr) const;
    QString directory(const QString &path, QString *error = nullptr) const;
private:
    QString containedPath(const QString &candidate) const;
    QString m_root;
};
