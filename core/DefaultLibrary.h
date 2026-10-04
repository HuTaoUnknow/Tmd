#pragma once
#include <QString>

class DefaultLibrary {
public:
    // Seed a writable library once. Existing libraries and files are preserved.
    static bool initialize(const QString &libraryRoot, const QString &examplesRoot, QString *error = nullptr);
    static QString installedRoot();
};
