#pragma once
#include <QString>

struct MarkdownEdit {
    QString source;
    int anchor = 0, caret = 0;
    bool handled = false, changed = false;
};
