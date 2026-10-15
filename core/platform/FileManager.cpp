#include "core/platform/FileManager.h"
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#endif

bool FileManager::openPath(const QString &path, QString *error) {
    const QFileInfo info(path);
    if (!info.exists()) { if (error) *error = QStringLiteral("项目不存在，请刷新列表。"); return false; }
    bool opened = false;
#ifdef Q_OS_WIN
    if (info.isFile()) {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        PIDLIST_ABSOLUTE item = nullptr;
        const QString native = QDir::toNativeSeparators(info.absoluteFilePath());
        if (SUCCEEDED(SHParseDisplayName(reinterpret_cast<LPCWSTR>(native.utf16()), nullptr, &item, 0, nullptr))) {
            opened = SUCCEEDED(SHOpenFolderAndSelectItems(item, 0, nullptr, 0)); CoTaskMemFree(item);
        }
        if (SUCCEEDED(initialized)) CoUninitialize();
    } else
#endif
    opened = QDesktopServices::openUrl(QUrl::fromLocalFile(info.isDir() ? info.absoluteFilePath() : info.absolutePath()));
    if (!opened && error) *error = QStringLiteral("无法打开系统文件管理器。");
    return opened;
}
