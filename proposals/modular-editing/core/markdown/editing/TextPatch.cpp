#include "core/markdown/editing/TextPatch.h"

namespace tmd::proposal {
namespace {
bool splitsSurrogate(const QString &text, int position) {
    return position > 0 && position < text.size()
        && text[position - 1].isHighSurrogate() && text[position].isLowSurrogate();
}
}

TextPatch TextPatch::between(const QString &before, const QString &after) {
    if (before == after) return {};
    int first = 0, oldEnd = int(before.size()), newEnd = int(after.size());
    while (first < oldEnd && first < newEnd && before[first] == after[first]) ++first;
    // Equal high surrogates may belong to different characters. Replace the
    // complete pair so the QString passed to QTextCursor is also well formed.
    if (splitsSurrogate(before, first) || splitsSurrogate(after, first)) --first;
    while (oldEnd > first && newEnd > first && before[oldEnd - 1] == after[newEnd - 1]) {
        --oldEnd;
        --newEnd;
    }
    if (splitsSurrogate(before, oldEnd) || splitsSurrogate(after, newEnd)) {
        ++oldEnd;
        ++newEnd;
    }
    return {first, oldEnd - first, after.mid(first, newEnd - first)};
}
}
