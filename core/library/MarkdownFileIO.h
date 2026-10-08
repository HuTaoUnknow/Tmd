#pragma once
#include <QString>

// All Markdown disk operations live here. Writes replace files atomically.
class MarkdownFileIO {
public:
    static bool readFile(const QString &path, QString &content, QString *error = nullptr);
    static bool writeFile(const QString &path, const QString &content, QString *error = nullptr);
    static bool exists(const QString &path);
    static bool moveToTrash(const QString &path, QString *error = nullptr, QString *pathInTrash = nullptr);
    static bool renameFile(const QString &from, const QString &to, QString *error = nullptr);
    static QString normalizedPath(const QString &path);
    static QString fileIdentity(const QString &path);
};
