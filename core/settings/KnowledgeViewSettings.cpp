#include "core/settings/KnowledgeViewSettings.h"
#include <QSettings>
namespace { constexpr auto zoomSetting = "knowledgeTree/zoomPercent"; }
KnowledgeViewSettings &KnowledgeViewSettings::shared() { static KnowledgeViewSettings value; return value; }
int KnowledgeViewSettings::zoomPercent() const {
    bool valid = false; const int percent = QSettings().value(zoomSetting, 60).toInt(&valid);
    return valid && percent >= 20 && percent <= 150 ? percent : 60;
}
void KnowledgeViewSettings::setZoomPercent(int percent) {
    percent = qBound(20, percent, 150);
    QSettings settings; settings.setValue(zoomSetting, percent); settings.sync();
    emit zoomChanged(percent);
}
