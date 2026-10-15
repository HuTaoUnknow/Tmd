#pragma once
#include <QString>

namespace FileManager {
// Open directories, or reveal a file in its containing directory.
bool openPath(const QString &path, QString *error = nullptr);
}
