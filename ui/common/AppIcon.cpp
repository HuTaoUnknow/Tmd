#include "ui/common/AppIcon.h"
#include <QResource>

static void initializeBranding() { Q_INIT_RESOURCE(branding); }
QIcon treeMdIcon() {
    static const QIcon icon = [] { initializeBranding(); return QIcon(QStringLiteral(":/branding/tmd.ico")); }();
    return icon;
}
