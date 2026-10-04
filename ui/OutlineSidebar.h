#pragma once
#include "core/MarkdownOutline.h"
#include <QTreeWidget>

class OutlineSidebar : public QTreeWidget {
    Q_OBJECT
public:
    explicit OutlineSidebar(QWidget *parent = nullptr);
    void setOutline(const MarkdownOutline &outline);
signals:
    void headingActivated(int position);
};
