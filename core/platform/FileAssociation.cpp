#include "core/platform/FileAssociation.h"
#include <QDir>
#include <QFileInfo>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#endif

namespace FileAssociation {
namespace {
QString native(const QString &path) { return QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()); }
bool samePath(const QString &left, const QString &right) {
    return QDir::cleanPath(QFileInfo(left).absoluteFilePath()).compare(QDir::cleanPath(QFileInfo(right).absoluteFilePath()), Qt::CaseInsensitive) == 0;
}
QString powershell() {
    QString root = qEnvironmentVariable("SystemRoot");
    if (root.isEmpty()) root = QStringLiteral("C:\\Windows");
    return QDir::toNativeSeparators(QDir(root).filePath(QStringLiteral("System32/WindowsPowerShell/v1.0/powershell.exe")));
}
QString classesKey(const QString &root, const QString &child) { return root + QStringLiteral("\\Classes\\") + child; }
void fail(QString *error, const QString &message) { if (error) *error = message; }
#ifdef Q_OS_WIN
const wchar_t *wide(const QString &text) { return reinterpret_cast<const wchar_t *>(text.utf16()); }
QString systemMessage(LONG code) {
    wchar_t *buffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, DWORD(code), 0,
                   reinterpret_cast<wchar_t *>(&buffer), 0, nullptr);
    const QString text = buffer ? QString::fromWCharArray(buffer).trimmed() : QString::number(code);
    if (buffer) LocalFree(buffer);
    return QStringLiteral("%1 (%2)").arg(text).arg(code);
}
QString readString(const QString &key, const QString &name) {
    HKEY handle = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, wide(key), 0, KEY_QUERY_VALUE, &handle) != ERROR_SUCCESS) return {};
    DWORD type = 0, size = 0;
    QString result;
    if (RegQueryValueExW(handle, name.isEmpty() ? nullptr : wide(name), nullptr, &type, nullptr, &size) == ERROR_SUCCESS
        && (type == REG_SZ || type == REG_EXPAND_SZ) && size > 0) {
        QByteArray data(int(size) + 2, '\0');
        if (RegQueryValueExW(handle, name.isEmpty() ? nullptr : wide(name), nullptr, &type, reinterpret_cast<BYTE *>(data.data()), &size) == ERROR_SUCCESS)
            result = QString::fromWCharArray(reinterpret_cast<const wchar_t *>(data.constData()));
    }
    RegCloseKey(handle);
    return result;
}
LONG deleteValue(const QString &key, const QString &name) {
    HKEY handle = nullptr;
    LONG code = RegOpenKeyExW(HKEY_CURRENT_USER, wide(key), 0, KEY_SET_VALUE, &handle);
    if (code == ERROR_FILE_NOT_FOUND) return ERROR_SUCCESS;
    if (code != ERROR_SUCCESS) return code;
    code = RegDeleteValueW(handle, wide(name));
    RegCloseKey(handle);
    return code == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : code;
}
LONG deleteTree(const QString &key) {
    const LONG code = RegDeleteTreeW(HKEY_CURRENT_USER, wide(key));
    if (code != ERROR_SUCCESS && code != ERROR_FILE_NOT_FOUND) return code;
    const LONG removed = RegDeleteKeyW(HKEY_CURRENT_USER, wide(key));
    return removed == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : removed;
}
#endif
}

Launch launchFor(const QString &applicationFilePath) {
    Launch launch;
    launch.executable = native(applicationFilePath);
    const QDir binary = QFileInfo(applicationFilePath).absoluteDir();
    const QString project = QDir::cleanPath(binary.absoluteFilePath(QStringLiteral("../..")));
    const QString launcher = QDir(project).filePath(QStringLiteral("scripts/Start-Debug.ps1"));
    const QString launchedByScript = QDir(project).filePath(QStringLiteral("build/Desktop_Qt_6_11_1_MinGW_64_bit_Debug/tree_md.exe"));
    if (QFileInfo::exists(binary.filePath(QStringLiteral("tmd-installation.ini")))) launch.mode = Mode::Installed;
    else if (QFileInfo(launcher).isFile() && samePath(launchedByScript, applicationFilePath)) launch.mode = Mode::DebugLauncher;
    if (launch.mode == Mode::DebugLauncher) {
        launch.launcher = native(launcher);
        launch.command = QStringLiteral("\"%1\" -NoLogo -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File \"%2\" \"%3\"")
            .arg(powershell(), launch.launcher, QStringLiteral("%1"));
        const QString icon = QDir(project).filePath(QStringLiteral("resources/tmd.ico"));
        launch.icon = QFileInfo::exists(icon) ? native(icon) + QStringLiteral(",0") : launch.executable + QStringLiteral(",0");
    } else {
        launch.command = QStringLiteral("\"%1\" \"%2\"").arg(launch.executable, QStringLiteral("%1"));
        launch.icon = launch.executable + QStringLiteral(",0");
    }
    return launch;
}

QString modeText(Mode mode) {
    switch (mode) {
    case Mode::Installed: return QStringLiteral("安装版");
    case Mode::DebugLauncher: return QStringLiteral("Debug 版（通过 scripts\\Start-Debug.ps1 启动，与桌面“Tmd Debug”快捷方式相同）");
    case Mode::Portable: return QStringLiteral("便携版（直接启动 tree_md.exe）");
    }
    return {};
}

QList<Entry> entries(const Launch &launch, const QString &root) {
    const QString prog = QString::fromLatin1(ProgId), app = QStringLiteral("Applications\\tree_md.exe");
    const QString capabilities = root + QStringLiteral("\\Tmd\\Capabilities");
    return {
        {classesKey(root, prog), {}, QStringLiteral("Markdown 文档")},
        {classesKey(root, prog), QStringLiteral("FriendlyTypeName"), QStringLiteral("Markdown 文档")},
        {classesKey(root, prog + QStringLiteral("\\DefaultIcon")), {}, launch.icon},
        {classesKey(root, prog + QStringLiteral("\\shell\\open\\command")), {}, launch.command},
        {classesKey(root, QStringLiteral(".md\\OpenWithProgids")), prog, {}},
        {classesKey(root, app), QStringLiteral("FriendlyAppName"), QString::fromLatin1(ApplicationName)},
        {classesKey(root, app + QStringLiteral("\\DefaultIcon")), {}, launch.icon},
        {classesKey(root, app + QStringLiteral("\\shell\\open\\command")), {}, launch.command},
        {classesKey(root, app + QStringLiteral("\\SupportedTypes")), QStringLiteral(".md"), {}},
        {capabilities, QStringLiteral("ApplicationName"), QString::fromLatin1(ApplicationName)},
        {capabilities, QStringLiteral("ApplicationDescription"), QStringLiteral("基于 Markdown 的本地知识树")},
        {capabilities, QStringLiteral("ApplicationIcon"), launch.icon},
        {capabilities + QStringLiteral("\\FileAssociations"), QStringLiteral(".md"), prog},
        {root + QStringLiteral("\\RegisteredApplications"), QString::fromLatin1(ApplicationName), capabilities},
    };
}

bool registerForCurrentUser(const Launch &launch, QString *error, const QString &root) {
    if (launch.command.isEmpty()) { fail(error, QStringLiteral("无法确定 Tmd 的启动命令。")); return false; }
#ifdef Q_OS_WIN
    for (const auto &entry : entries(launch, root)) {
        HKEY handle = nullptr;
        LONG code = RegCreateKeyExW(HKEY_CURRENT_USER, wide(entry.key), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &handle, nullptr);
        if (code == ERROR_SUCCESS) {
            const auto bytes = DWORD((entry.value.size() + 1) * sizeof(wchar_t));
            code = RegSetValueExW(handle, entry.name.isEmpty() ? nullptr : wide(entry.name), 0, REG_SZ,
                                  reinterpret_cast<const BYTE *>(wide(entry.value)), bytes);
            RegCloseKey(handle);
        }
        if (code != ERROR_SUCCESS) {
            fail(error, QStringLiteral("写入 HKEY_CURRENT_USER\\%1 失败：%2").arg(entry.key, systemMessage(code)));
            return false;
        }
    }
    if (root == QLatin1String(RealSoftwareRoot)) SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
#else
    Q_UNUSED(root); fail(error, QStringLiteral("只有 Windows 支持设置默认打开方式。")); return false;
#endif
}

bool unregisterForCurrentUser(QString *error, const QString &root) {
#ifdef Q_OS_WIN
    const QList<QPair<QString, QString>> values{
        {classesKey(root, QStringLiteral(".md\\OpenWithProgids")), QString::fromLatin1(ProgId)},
        {root + QStringLiteral("\\RegisteredApplications"), QString::fromLatin1(ApplicationName)},
    };
    for (const auto &value : values) {
        const LONG code = deleteValue(value.first, value.second);
        if (code != ERROR_SUCCESS) { fail(error, QStringLiteral("删除 %1 失败：%2").arg(value.first, systemMessage(code))); return false; }
    }
    for (const QString &key : {classesKey(root, QString::fromLatin1(ProgId)), classesKey(root, QStringLiteral("Applications\\tree_md.exe")),
                               root + QStringLiteral("\\Tmd\\Capabilities")}) {
        const LONG code = deleteTree(key);
        if (code != ERROR_SUCCESS) { fail(error, QStringLiteral("删除 %1 失败：%2").arg(key, systemMessage(code))); return false; }
    }
    RegDeleteKeyW(HKEY_CURRENT_USER, wide(root + QStringLiteral("\\Tmd")));  // only succeeds when empty
    if (root == QLatin1String(RealSoftwareRoot)) SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
#else
    Q_UNUSED(root); fail(error, QStringLiteral("只有 Windows 支持设置默认打开方式。")); return false;
#endif
}

QString registeredCommand(const QString &root) {
#ifdef Q_OS_WIN
    return readString(classesKey(root, QString::fromLatin1(ProgId) + QStringLiteral("\\shell\\open\\command")), {});
#else
    Q_UNUSED(root); return {};
#endif
}

QString currentUserChoice() {
#ifdef Q_OS_WIN
    const QString base = QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.md\\");
    for (const QString &key : {QStringLiteral("UserChoiceLatest"), QStringLiteral("UserChoice")}) {
        const QString value = readString(base + key, QStringLiteral("ProgId"));
        if (!value.isEmpty()) return value;
    }
#endif
    return {};
}

bool isTmdChoice(const QString &progId) {
    return progId.compare(QLatin1String(ProgId), Qt::CaseInsensitive) == 0
        || progId.compare(QStringLiteral("Applications\\tree_md.exe"), Qt::CaseInsensitive) == 0;
}

bool openDefaultAppsSettings(QString *error) {
#ifdef Q_OS_WIN
    for (const QString &uri : {QStringLiteral("ms-settings:defaultapps?registeredAppUser=Tmd"), QStringLiteral("ms-settings:defaultapps")}) {
        const auto result = reinterpret_cast<quintptr>(ShellExecuteW(nullptr, L"open", wide(uri), nullptr, nullptr, SW_SHOWNORMAL));
        if (result > 32) return true;
    }
    fail(error, QStringLiteral("无法打开 Windows“默认应用”设置。请手动打开：设置 → 应用 → 默认应用。"));
    return false;
#else
    fail(error, QStringLiteral("只有 Windows 支持设置默认打开方式。")); return false;
#endif
}

bool showOpenWithDialog(const QString &file, quintptr nativeParent, QString *error) {
#ifdef Q_OS_WIN
    const QString path = native(file);
    OPENASINFO info{};
    info.pcszFile = wide(path);
    info.oaifInFlags = OAIF_ALLOW_REGISTRATION | OAIF_REGISTER_EXT;
    const HRESULT result = SHOpenWithDialog(reinterpret_cast<HWND>(nativeParent), &info);
    if (SUCCEEDED(result) || result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return true;
    fail(error, QStringLiteral("无法打开 Windows“打开方式”窗口（0x%1）。").arg(quint32(result), 8, 16, QLatin1Char('0')));
    return false;
#else
    Q_UNUSED(file); Q_UNUSED(nativeParent); fail(error, QStringLiteral("只有 Windows 支持设置默认打开方式。")); return false;
#endif
}
}
