#include "MarkdownSettings.h"
#include "MarkdownFileIO.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QUrl>
#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {
struct Cache { MarkdownSettings value; QString path, error; bool loaded = false; };
Cache &cache() { static Cache value; return value; }
bool fail(QString *error, const QString &text) { if (error) *error = text; return false; }
constexpr quint16 allowedFlags = 511;
constexpr int maximumTextSize = 524288;
bool validText(const QString &value, int limit, bool allowEmpty = true) {
    if ((!allowEmpty && value.trimmed().isEmpty()) || value.toUtf8().size() > limit) return false;
    for (auto character : value) if (character.unicode() < 32) return false;
    return true;
}
bool webAddress(const QString &value) {
    const QUrl url(value, QUrl::StrictMode);
    return url.isValid() && !url.host().isEmpty() && (url.scheme() == "https" || url.scheme() == "http");
}
bool protectToken(const QByteArray &input, QByteArray &output, bool encrypt, QString *error) {
    if (input.isEmpty()) { output.clear(); return true; }
#ifdef Q_OS_WIN
    DATA_BLOB source{DWORD(input.size()), reinterpret_cast<BYTE *>(const_cast<char *>(input.constData()))}, result{};
    const BOOL success = encrypt ? CryptProtectData(&source, L"Tmd image upload", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &result)
        : CryptUnprotectData(&source, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &result);
    if (!success) return fail(error, QStringLiteral("无法读取或保护本机图床凭据，请重新填写。"));
    output = QByteArray(reinterpret_cast<const char *>(result.pbData), result.cbData); LocalFree(result.pbData); return true;
#else
    Q_UNUSED(output); Q_UNUSED(encrypt);
    return fail(error, QStringLiteral("当前平台不支持保存受保护的图床凭据。"));
#endif
}
}
const std::array<MarkdownNumberSpec, MarkdownSettings::NumberCount> &markdownNumberSpecs() {
    static const std::array<MarkdownNumberSpec, MarkdownSettings::NumberCount> specs{{
        {"正文字号", " pt", 60, 480, 10, 0}, {"代码字号", " pt", 60, 480, 10, 0},
        {"一级标题 H1", " pt", 60, 960, 10, 1}, {"二级标题 H2", " pt", 60, 960, 10, 1},
        {"三级标题 H3", " pt", 60, 960, 10, 1}, {"四级标题 H4", " pt", 60, 960, 10, 1},
        {"五级标题 H5", " pt", 60, 960, 10, 1}, {"六级标题 H6", " pt", 60, 960, 10, 1},
        {"正文行高", " %", 100, 250, 1, 2}, {"标题行高", " %", 100, 250, 1, 1},
        {"段落上下间距", " px", 0, 48, 1, 2}, {"标题段前间距", " px", 0, 80, 1, 1},
        {"标题段后间距", " px", 0, 80, 1, 1}, {"分点上下间距", " px", 0, 32, 1, 2},
        {"每级分点缩进", " px", 8, 96, 1, 2}, {"每级引用缩进", " px", 8, 80, 1, 2},
        {"代码块左右留白", " px", 0, 48, 1, 3}, {"代码块上下间距", " px", 0, 48, 1, 3},
        {"Tab 宽度", " px", 8, 128, 1, 3}, {"表格单元格留白", " px", 0, 32, 1, 4},
        {"编辑后预加载延迟", " ms", 150, 2000, 1, 4}
    }};
    return specs;
}
const std::array<MarkdownColorSpec, MarkdownSettings::ColorCount> &markdownColorSpecs() {
    static const std::array<MarkdownColorSpec, MarkdownSettings::ColorCount> specs{{
        {"文档背景", 0}, {"正文颜色", 0}, {"标题颜色", 1}, {"代码块背景（三个反引号）", 3},
        {"行内代码背景（单反引号）", 3}, {"引用背景", 2}, {"链接颜色", 4}, {"表格边框", 4},
        {"表格表头背景", 4}, {"表格偶数行背景", 4}, {"表格奇数行背景", 4}
    }};
    return specs;
}
qreal MarkdownSettings::number(Number key) const { return qreal(numbers[key]) / markdownNumberSpecs()[key].scale; }
QString MarkdownSettingsStore::storagePath() {
    if (!cache().path.isEmpty()) return cache().path;
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)).filePath("Tmd-settings.txt");
}
void MarkdownSettingsStore::setStoragePath(const QString &path) { cache() = {}; cache().path = path; }
const MarkdownSettings &MarkdownSettingsStore::current() {
    auto &state = cache();
    if (!state.loaded) {
        state.loaded = true;
        if (QFileInfo::exists(storagePath())) read(storagePath(), state.value, &state.error);
    }
    return state.value;
}
QString MarkdownSettingsStore::loadError() { current(); return cache().error; }
bool MarkdownSettingsStore::validate(const MarkdownSettings &settings, QString *error) {
    if (settings.fonts.size() != 3) return fail(error, QStringLiteral("字体设置不完整。"));
    for (const auto &font : settings.fonts) {
        if (font.trimmed().isEmpty() || font.toUtf8().size() > 192) return fail(error, QStringLiteral("字体名称无效或过长。"));
        for (const auto c : font) if (c.unicode() < 32) return fail(error, QStringLiteral("字体名称包含控制字符。"));
    }
    for (int i = 0; i < MarkdownSettings::NumberCount; ++i)
        if (settings.numbers[i] < markdownNumberSpecs()[i].minimum || settings.numbers[i] > markdownNumberSpecs()[i].maximum)
            return fail(error, QStringLiteral("%1 超出允许范围。").arg(markdownNumberSpecs()[i].label));
    if ((settings.flags & ~allowedFlags) || settings.imageMode > 2) return fail(error, QStringLiteral("设置包含未知选项。"));
    for (auto color : settings.colors) if (qAlpha(color) != 255) return fail(error, QStringLiteral("背景和文字颜色必须是不透明颜色。"));
    if (!validText(settings.relativePhotoPath, 1024, false) || QDir::isAbsolutePath(settings.relativePhotoPath)
        || settings.relativePhotoPath.contains(QRegularExpression("[:*?\"<>|]"))) return fail(error, QStringLiteral("相对图片位置必须是相对 md_data 的目录路径。"));
    if (!validText(settings.absolutePhotoPath, 1024) || (!settings.absolutePhotoPath.isEmpty() && !QDir::isAbsolutePath(settings.absolutePhotoPath)))
        return fail(error, QStringLiteral("绝对图片位置必须是完整目录路径，或留空使用默认位置。"));
    if (settings.hostedLinks.size() > 32 || settings.defaultHostedLink < -1 || settings.defaultHostedLink >= settings.hostedLinks.size())
        return fail(error, QStringLiteral("图床链接列表或默认选项无效（最多 32 条）。"));
    for (const auto &link : settings.hostedLinks) if (!validText(link.name, 128, false) || !validText(link.url, 2048, false) || !webAddress(link.url))
        return fail(error, QStringLiteral("图床链接需要名称和有效的 HTTP/HTTPS 图片地址。"));
    if (!validText(settings.uploadEndpoint, 2048) || (!settings.uploadEndpoint.isEmpty() && !webAddress(settings.uploadEndpoint)))
        return fail(error, QStringLiteral("上传接口需要有效的 HTTP/HTTPS 地址。"));
    const QRegularExpression field("^[A-Za-z0-9_.-]{1,64}$"), header("^[A-Za-z0-9-]{1,64}$");
    if (!field.match(settings.uploadFileField).hasMatch() || !validText(settings.uploadUrlPath, 256, false) || settings.uploadAuthKind > 3
        || !validText(settings.uploadAuthName, 64) || !validText(settings.uploadAuthPrefix, 128) || !validText(settings.uploadToken, 4096)
        || (settings.uploadAuthKind && !(settings.uploadAuthKind == 1 ? header : field).match(settings.uploadAuthName).hasMatch()))
        return fail(error, QStringLiteral("上传文件字段、鉴权参数或返回链接字段无效。"));
    if (settings.uploadFields.size() > 16) return fail(error, QStringLiteral("上传附加表单字段最多 16 个。"));
    for (auto it = settings.uploadFields.cbegin(); it != settings.uploadFields.cend(); ++it)
        if (!field.match(it.key()).hasMatch() || it.key() == settings.uploadFileField || !validText(it.value(), 2048))
            return fail(error, QStringLiteral("上传附加字段无效，不能覆盖图片文件字段。"));
    if (settings.uploadAuthKind == 2 && (settings.uploadAuthName == settings.uploadFileField || settings.uploadFields.contains(settings.uploadAuthName)))
        return fail(error, QStringLiteral("鉴权表单字段不能与图片或附加字段重名。"));
    if (error) error->clear(); return true;
}
static QString encodeSettings(const MarkdownSettings &settings, bool local, QString *error = nullptr) {
    if (!MarkdownSettingsStore::validate(settings, error)) return {};
    QByteArray bytes("TMDP"); QDataStream stream(&bytes, QIODevice::Append);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << quint16(2) << quint16(settings.flags | (settings.imageMode << 10));
    for (auto value : settings.numbers) stream << value;
    for (auto color : settings.colors) {
        stream << quint8(qRed(color)) << quint8(qGreen(color)) << quint8(qBlue(color));
    }
    for (const auto &font : settings.fonts) {
        const auto utf8 = font.toUtf8(); stream << quint16(utf8.size()); stream.writeRawData(utf8.constData(), utf8.size());
    }
    auto string = [&](const QString &value) { const auto utf8 = value.toUtf8(); stream << quint16(utf8.size()); stream.writeRawData(utf8.constData(), utf8.size()); };
    string(settings.relativePhotoPath); string(settings.absolutePhotoPath);
    stream << quint16(settings.hostedLinks.size()) << quint16(settings.defaultHostedLink < 0 ? 65535 : settings.defaultHostedLink);
    for (const auto &link : settings.hostedLinks) { string(link.name); string(link.url); }
    string(settings.uploadEndpoint); string(settings.uploadFileField); string(settings.uploadUrlPath);
    stream << settings.uploadAuthKind; string(settings.uploadAuthName); string(settings.uploadAuthPrefix);
    stream << quint16(settings.uploadFields.size());
    for (auto it = settings.uploadFields.cbegin(); it != settings.uploadFields.cend(); ++it) { string(it.key()); string(it.value()); }
    QByteArray credential;
    if (local && !protectToken(settings.uploadToken.toUtf8(), credential, true, error)) return {};
    stream << quint16(credential.size()); stream.writeRawData(credential.constData(), credential.size());
    bytes += QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).left(4);
    const auto hex = bytes.toHex().toUpper(); QString text;
    for (int i = 0; i < hex.size(); i += 4) {
        if (i) text += i % 64 ? ' ' : '\n';
        text += QString::fromLatin1(hex.mid(i, 4));
    }
    return text + '\n';
}
QString MarkdownSettingsStore::encode(const MarkdownSettings &settings) { return encodeSettings(settings, false); }
bool MarkdownSettingsStore::decode(const QString &text, MarkdownSettings &settings, QString *error) {
    if (text.size() > maximumTextSize) return fail(error, QStringLiteral("设置文件过大。"));
    QByteArray hex;
    for (const auto c : text) {
        if (c.isSpace() || (hex.isEmpty() && c == QChar::ByteOrderMark)) continue;
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return fail(error, QStringLiteral("设置文件必须是十六进制 TXT。"));
        hex += c.toLatin1();
    }
    if (hex.size() % 2 || hex.size() < 24) return fail(error, QStringLiteral("设置文件不完整。"));
    const auto bytes = QByteArray::fromHex(hex), payload = bytes.first(bytes.size() - 4);
    if (!payload.startsWith("TMDP") || QCryptographicHash::hash(payload, QCryptographicHash::Sha256).left(4) != bytes.last(4))
        return fail(error, QStringLiteral("设置文件校验失败，内容未导入。"));
    QDataStream stream(payload.mid(4)); stream.setByteOrder(QDataStream::BigEndian);
    quint16 version, packed; stream >> version >> packed;
    if (version != 1 && version != 2) return fail(error, QStringLiteral("不支持的设置文件版本。"));
    if (packed & ~(allowedFlags | quint16(3 << 10))) return fail(error, QStringLiteral("设置文件包含未知标志。"));
    MarkdownSettings next; next.flags = packed & allowedFlags; next.imageMode = (packed >> 10) & 3;
    for (auto &value : next.numbers) stream >> value;
    for (auto &color : next.colors) { quint8 r, g, b; stream >> r >> g >> b; color = qRgb(r, g, b); }
    next.fonts.clear();
    for (int i = 0; i < 3; ++i) {
        quint16 size = 0; stream >> size;
        if (!size || size > 192) return fail(error, QStringLiteral("设置文件的字体数据无效。"));
        QByteArray name(size, '\0');
        if (stream.readRawData(name.data(), size) != size || QString::fromUtf8(name).toUtf8() != name)
            return fail(error, QStringLiteral("设置文件的字体编码无效。"));
        next.fonts.append(QString::fromUtf8(name));
    }
    if (version == 2) {
        auto string = [&](QString &value, int limit) {
            quint16 size = 0; stream >> size;
            if (size > limit) return false;
            QByteArray utf8(size, '\0');
            if (stream.readRawData(utf8.data(), size) != size || QString::fromUtf8(utf8).toUtf8() != utf8) return false;
            value = QString::fromUtf8(utf8); return true;
        };
        quint16 count = 0, selected = 65535;
        if (!string(next.relativePhotoPath, 1024) || !string(next.absolutePhotoPath, 1024)) return fail(error, QStringLiteral("图片位置数据无效。"));
        stream >> count >> selected; if (count > 32) return fail(error, QStringLiteral("图床链接列表过长。"));
        next.defaultHostedLink = selected == 65535 ? -1 : selected;
        for (int i = 0; i < count; ++i) { HostedImageLink link; if (!string(link.name, 128) || !string(link.url, 2048)) return fail(error, QStringLiteral("图床链接编码无效。")); next.hostedLinks.append(link); }
        if (!string(next.uploadEndpoint, 2048) || !string(next.uploadFileField, 64) || !string(next.uploadUrlPath, 256)) return fail(error, QStringLiteral("上传接口数据无效。"));
        stream >> next.uploadAuthKind;
        if (!string(next.uploadAuthName, 64) || !string(next.uploadAuthPrefix, 128)) return fail(error, QStringLiteral("鉴权参数数据无效。"));
        stream >> count; if (count > 16) return fail(error, QStringLiteral("上传表单字段过多。"));
        for (int i = 0; i < count; ++i) { QString key, value; if (!string(key, 64) || !string(value, 2048) || next.uploadFields.contains(key)) return fail(error, QStringLiteral("上传表单字段无效或重复。")); next.uploadFields[key] = value; }
        stream >> count; if (count > 12000) return fail(error, QStringLiteral("凭据数据过长。"));
        QByteArray credential(count, '\0'), token;
        if (stream.readRawData(credential.data(), count) != count) return fail(error, QStringLiteral("凭据数据不完整。"));
        if (!protectToken(credential, token, false, error)) return false;
        next.uploadToken = QString::fromUtf8(token);
        if (next.uploadToken.toUtf8() != token) return fail(error, QStringLiteral("凭据编码无效。"));
    }
    if (stream.status() != QDataStream::Ok || !stream.atEnd()) return fail(error, QStringLiteral("设置文件长度或字段格式不正确。"));
    if (!validate(next, error)) return false;
    settings = next; return true;
}
bool MarkdownSettingsStore::read(const QString &path, MarkdownSettings &settings, QString *error) {
    if (QFileInfo(path).size() > maximumTextSize) return fail(error, QStringLiteral("设置文件过大。"));
    QString text; return MarkdownFileIO::readFile(path, text, error) && decode(text, settings, error);
}
bool MarkdownSettingsStore::write(const QString &path, const MarkdownSettings &settings, QString *error) {
    if (!validate(settings, error)) return false;
    return MarkdownFileIO::writeFile(path, encode(settings), error);
}
bool MarkdownSettingsStore::save(const MarkdownSettings &settings, QString *error) {
    if (!QDir().mkpath(QFileInfo(storagePath()).absolutePath())) return fail(error, QStringLiteral("无法创建设置目录。"));
    const QString text = encodeSettings(settings, true, error);
    if (text.isEmpty() || !MarkdownFileIO::writeFile(storagePath(), text, error)) return false;
    cache().value = settings; cache().loaded = true; cache().error.clear(); return true;
}
