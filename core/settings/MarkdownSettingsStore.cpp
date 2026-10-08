#include "core/settings/MarkdownSettingsCodec.h"
#include "core/library/MarkdownFileIO.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
namespace {
struct Cache { MarkdownSettings value; QString path, error; bool loaded = false; };
Cache &cache() { static Cache value; return value; }
bool fail(QString *error, const QString &text) { if (error) *error = text; return false; }
}
bool MarkdownSettingsStore::validate(const MarkdownSettings &settings, QString *error) { return MarkdownSettingsCodec::validate(settings, error); }
QString MarkdownSettingsStore::encode(const MarkdownSettings &settings) { return MarkdownSettingsCodec::encode(settings, false); }
bool MarkdownSettingsStore::decode(const QString &text, MarkdownSettings &settings, QString *error) { return MarkdownSettingsCodec::decode(text, settings, error); }
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
bool MarkdownSettingsStore::read(const QString &path, MarkdownSettings &settings, QString *error) {
    if (QFileInfo(path).size() > MarkdownSettingsCodec::MaximumTextSize) return fail(error, QStringLiteral("设置文件过大。"));
    QString text; return MarkdownFileIO::readFile(path, text, error) && decode(text, settings, error);
}
bool MarkdownSettingsStore::write(const QString &path, const MarkdownSettings &settings, QString *error) {
    if (!validate(settings, error)) return false;
    return MarkdownFileIO::writeFile(path, encode(settings), error);
}
bool MarkdownSettingsStore::save(const MarkdownSettings &settings, QString *error) {
    if (!QDir().mkpath(QFileInfo(storagePath()).absolutePath())) return fail(error, QStringLiteral("无法创建设置目录。"));
    const QString text = MarkdownSettingsCodec::encode(settings, true, error);
    if (text.isEmpty() || !MarkdownFileIO::writeFile(storagePath(), text, error)) return false;
    cache().value = settings; cache().loaded = true; cache().error.clear(); return true;
}

bool MarkdownSettingsStore::rememberHostedLink(const QString &name, const QString &url, QString *error) {
    auto settings = current();
    for (const auto &link : settings.hostedLinks) if (link.url == url) return true;
    if (settings.hostedLinks.size() >= 32) return true;
    settings.hostedLinks.append({name, url});
    if (settings.defaultHostedLink < 0) settings.defaultHostedLink = settings.hostedLinks.size() - 1;
    return save(settings, error);
}
