#include "MarkdownTypography.h"
#include "core/MarkdownSettings.h"
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
    const auto &settings = MarkdownSettingsStore::current();
    font.setFamilies({settings.fonts[0], settings.fonts[1], "Segoe UI"});
    font.setPointSizeF(settings.number(MarkdownSettings::BodySize) * qBound(50, zoomPercent, 200) / 100.0); return font;
}
QFont MarkdownTypography::codeFont(int zoomPercent) {
    initializeTypography(); QFont font;
    const auto &settings = MarkdownSettingsStore::current();
    font.setFamilies({settings.fonts[2], "Consolas", settings.fonts[1]});
    font.setStyleHint(QFont::Monospace); font.setFixedPitch(true);
    font.setPointSizeF(settings.number(MarkdownSettings::CodeSize) * qBound(50, zoomPercent, 200) / 100.0); return font;
}
qreal MarkdownTypography::headingPointSize(int level, int zoomPercent) {
    const auto key = static_cast<MarkdownSettings::Number>(MarkdownSettings::H1 + qBound(1, level, 6) - 1);
    return MarkdownSettingsStore::current().number(key) * qBound(50, zoomPercent, 200) / 100.0;
}
QColor MarkdownTypography::codeBackground(const QColor &bodyBackground, bool inlineCode) {
    Q_UNUSED(bodyBackground);
    return MarkdownSettingsStore::current().color(inlineCode ? MarkdownSettings::InlineBackground : MarkdownSettings::BlockBackground);
}
int MarkdownTypography::wheelSteps(const QWheelEvent *event, int &remainder) {
    if (event->angleDelta().y()) {
        remainder += event->angleDelta().y(); const int steps = remainder / 120;
        remainder %= 120; return steps;
    }
    return event->pixelDelta().y() > 0 ? 1 : event->pixelDelta().y() < 0 ? -1 : 0;
}
