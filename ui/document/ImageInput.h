#pragma once
#include <QImage>
#include <QUrl>
#include <QList>
class QMimeData;
class ImageInput {
public:
    static QList<QUrl> urls(const QMimeData *data, bool hostedLinks = false);
    static QImage image(const QMimeData *data);
    static bool canInsert(const QMimeData *data, bool hostedLinks = false);
};
