#include "ui/document/ImageInput.h"
#include "core/markdown/MarkdownImages.h"
#include <QMimeData>
#include <QImageReader>
#include <QFileInfo>
#include <QPixmap>
#include <QRegularExpression>

QList<QUrl> ImageInput::urls(const QMimeData *data, bool hostedLinks) {
    QList<QUrl> result;
    auto append = [&](const QUrl &url, bool imageContext = false) {
        const QByteArray extension = QFileInfo(url.isLocalFile() ? url.toLocalFile() : url.path()).suffix().toLower().toLatin1();
        const bool knownImage = QImageReader::supportedImageFormats().contains(extension) || extension == "jpg";
        if ((url.isLocalFile() && knownImage) || (MarkdownImages::isRemote(url.toString()) && (knownImage || hostedLinks || imageContext)))
            if (!result.contains(url)) result.append(url);
    };
    for (const auto &url : data->urls()) append(url, data->hasImage());
    if (result.isEmpty() && data->hasHtml()) {
        static const QRegularExpression imageTag(R"(<img\b[^>]*\bsrc\s*=\s*(["'])(.*?)\1)", QRegularExpression::CaseInsensitiveOption);
        const auto match = imageTag.match(data->html());
        QString text = data->html(); text.remove(QRegularExpression("<[^>]+>"));
        if (match.hasMatch() && (data->hasImage() || text.trimmed().isEmpty())) append(QUrl(match.captured(2).replace("&amp;", "&")), true);
    }
    if (result.isEmpty() && data->hasText() && !data->hasImage()) {
        const QString text = data->text().trimmed();
        if (!text.contains('\n') && !text.contains(' ')) append(QUrl(text));
    }
    return result;
}
QImage ImageInput::image(const QMimeData *data) {
    if (!data->hasImage()) return {};
    const QVariant value = data->imageData(); const QImage image = qvariant_cast<QImage>(value);
    return image.isNull() ? qvariant_cast<QPixmap>(value).toImage() : image;
}
bool ImageInput::canInsert(const QMimeData *data, bool hostedLinks) {
    return !urls(data, hostedLinks).isEmpty() || data->hasImage();
}
