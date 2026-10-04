#include "MarkdownTypography.h"
#include <QFontDatabase>
#include <QResource>
#include <QWheelEvent>

static void initializeTypography() {
    static const bool loaded = [] {
        Q_INIT_RESOURCE(typography);
        for (const auto &file : {"OpenSans-Regular.ttf", "OpenSans-SemiBold.ttf", "OpenSans-Bold.ttf", "OpenSans-Italic.ttf", "DejaVuSansMono.ttf", "DejaVuSansMono-Bold.ttf"})
            QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/") + file);
        return true;
    }();
    Q_UNUSED(loaded);
}
QFont MarkdownTypography::bodyFont(int zoomPercent) {
    initializeTypography(); QFont font;
    font.setFamilies({"Open Sans", "Microsoft YaHei", "Segoe UI"});
    font.setPointSizeF(12.0 * qBound(50, zoomPercent, 200) / 100.0); return font;
}
QFont MarkdownTypography::codeFont(int zoomPercent) {
    initializeTypography(); QFont font;
    font.setFamilies({"DejaVu Sans Mono", "Consolas", "Microsoft YaHei"});
    font.setStyleHint(QFont::Monospace); font.setFixedPitch(true);
    font.setPointSizeF(10.5 * qBound(50, zoomPercent, 200) / 100.0); return font;
}
qreal MarkdownTypography::headingPointSize(int level, int zoomPercent) {
    // Ratios from the locally installed MarkText editor's heading styles.
    const qreal ratios[] = {1.0, 1.875, 1.5, 1.375, 1.25, 1.125, 1.0};
    return bodyFont(zoomPercent).pointSizeF() * ratios[qBound(1, level, 6)];
}
QColor MarkdownTypography::codeBackground(const QColor &bodyBackground, bool inlineCode) {
    return bodyBackground.darker(inlineCode ? 120 : 135);
}
int MarkdownTypography::wheelSteps(const QWheelEvent *event, int &remainder) {
    if (event->angleDelta().y()) {
        remainder += event->angleDelta().y(); const int steps = remainder / 120;
        remainder %= 120; return steps;
    }
    return event->pixelDelta().y() > 0 ? 1 : event->pixelDelta().y() < 0 ? -1 : 0;
}
