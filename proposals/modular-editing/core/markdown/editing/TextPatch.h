#pragma once
#include <QString>

namespace tmd::proposal {
// One contiguous UTF-16 replacement, suitable for one Qt undo transaction.
struct TextPatch {
    int start = 0;
    int removed = 0;
    QString inserted;
    bool changed() const { return removed != 0 || !inserted.isEmpty(); }
    static TextPatch between(const QString &before, const QString &after);
};
}
