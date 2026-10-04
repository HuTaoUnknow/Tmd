#pragma once
#include <QFont>
#include <QColor>
class QWheelEvent;
namespace MarkdownTypography {
QFont bodyFont(int zoomPercent = 100);
QFont codeFont(int zoomPercent = 100);
qreal headingPointSize(int level, int zoomPercent);
QColor codeBackground(const QColor &bodyBackground, bool inlineCode = false);
int wheelSteps(const QWheelEvent *event, int &remainder);
}
