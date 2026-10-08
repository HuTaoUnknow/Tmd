#pragma once
#include <QString>
#include <QVector>

struct MarkdownHeading {
    int level = 1;
    int line = 0;
    int position = 0;
    QString text;
};
using MarkdownOutline = QVector<MarkdownHeading>;
