#include "MarkdownOutline.h"

namespace {
QVector<MarkdownOutlineItem> *childrenAtPath(QVector<MarkdownOutlineItem> &items,
                                              const QVector<int> &path)
{
    QVector<MarkdownOutlineItem> *current = &items;
    for (int index : path)
        current = &(*current)[index].children;
    return current;
}

const MarkdownOutlineItem &itemAtPath(const QVector<MarkdownOutlineItem> &items,
                                      const QVector<int> &path)
{
    const QVector<MarkdownOutlineItem> *current = &items;
    const MarkdownOutlineItem *item = nullptr;
    for (int index : path) {
        item = &(*current)[index];
        current = &item->children;
    }
    return *item;
}
}

QVector<MarkdownOutlineItem> MarkdownOutline::fromMarkdown(const QString &markdown)
{
    QVector<MarkdownOutlineItem> roots;
    QVector<int> parentPath;

    for (const MarkdownHeading &heading : MarkdownParser::parseHeadings(markdown)) {
        while (!parentPath.isEmpty()
               && itemAtPath(roots, parentPath).level >= heading.level) {
            parentPath.removeLast();
        }

        MarkdownOutlineItem item;
        item.text = heading.text;
        item.level = heading.level;
        item.lineNumber = heading.lineNumber;

        QVector<MarkdownOutlineItem> *siblings = childrenAtPath(roots, parentPath);
        const int itemIndex = siblings->size();
        siblings->append(std::move(item));
        parentPath.append(itemIndex);
    }

    return roots;
}
