#pragma once
#include <QList>
#include <QPair>
#include <QString>
namespace LanguageCatalog {
const QList<QPair<QString, QString>> &entries();
QString canonicalName(const QString &info);
QString displayName(const QString &info);
}
