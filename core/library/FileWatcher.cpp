#include "core/library/FileWatcher.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#ifdef Q_OS_WIN
#include <QWinEventNotifier>
#include <array>
#include <functional>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// A recursive root handle with delete sharing leaves descendant directories movable.
// QFileSystemWatcher's FindFirstChangeNotification handles lock watched directories.
class WindowsDirectoryWatch {
public:
    WindowsDirectoryWatch(const QString &path, bool recursive, std::function<void()> changed)
        : m_recursive(recursive), m_changed(std::move(changed)) {
        m_directory = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
        if (m_directory == INVALID_HANDLE_VALUE) return;
        GetFileInformationByHandle(m_directory, &m_identity);
        m_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!m_event) return;
        m_notifier = std::make_unique<QWinEventNotifier>(m_event);
        QObject::connect(m_notifier.get(), &QWinEventNotifier::activated, m_notifier.get(), [this] {
            DWORD bytes = 0;
            const bool complete = GetOverlappedResult(m_directory, &m_overlapped, &bytes, FALSE);
            if (!complete && GetLastError() == ERROR_IO_INCOMPLETE) return;
            m_pending = false;
            if (!complete || !arm()) m_notifier->setEnabled(false);
            m_changed(); // Zero bytes also requires a rescan after buffer overflow.
        });
        if (!arm()) m_notifier->setEnabled(false);
    }
    ~WindowsDirectoryWatch() {
        m_notifier.reset();
        if (m_directory != INVALID_HANDLE_VALUE) {
            if (m_pending) {
                CancelIoEx(m_directory, &m_overlapped);
                DWORD bytes = 0; GetOverlappedResult(m_directory, &m_overlapped, &bytes, TRUE);
            }
            CloseHandle(m_directory);
        }
        if (m_event) CloseHandle(m_event);
    }
    bool matches(const QString &path) const {
        if (!m_pending) return false;
        const HANDLE current = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (current == INVALID_HANDLE_VALUE) return false;
        BY_HANDLE_FILE_INFORMATION identity{}; const bool found = GetFileInformationByHandle(current, &identity); CloseHandle(current);
        return found && identity.dwVolumeSerialNumber == m_identity.dwVolumeSerialNumber
            && identity.nFileIndexHigh == m_identity.nFileIndexHigh && identity.nFileIndexLow == m_identity.nFileIndexLow;
    }
private:
    bool arm() {
        ResetEvent(m_event); m_overlapped = {}; m_overlapped.hEvent = m_event;
        m_pending = ReadDirectoryChangesW(m_directory, m_buffer.data(), DWORD(m_buffer.size()), m_recursive,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
            nullptr, &m_overlapped, nullptr);
        return m_pending;
    }
    HANDLE m_directory = INVALID_HANDLE_VALUE, m_event = nullptr;
    OVERLAPPED m_overlapped{};
    BY_HANDLE_FILE_INFORMATION m_identity{};
    alignas(DWORD) std::array<char, 65536> m_buffer{};
    bool m_recursive, m_pending = false;
    std::function<void()> m_changed;
    std::unique_ptr<QWinEventNotifier> m_notifier;
};
#endif

FileWatcher::FileWatcher(QString root, QObject *parent) : QObject(parent), m_root(std::move(root)) {
    m_debounce.setSingleShot(true); m_debounce.setInterval(280);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, [this] { emit changed(); });
#ifdef Q_OS_WIN
    m_retry.setInterval(1000);
    connect(&m_retry, &QTimer::timeout, this, [this] {
        rebuild();
        if (m_rootWatch && m_rootWatch->matches(m_root)) m_debounce.start();
    });
#endif
    rebuild();
}
FileWatcher::~FileWatcher() = default;
void FileWatcher::rebuild() {
#ifdef Q_OS_WIN
    const auto notify = [this] { m_debounce.start(); };
    const auto ensure = [&](std::unique_ptr<WindowsDirectoryWatch> &watch, const QString &path, bool recursive) {
        if (watch && watch->matches(path)) return;
        watch.reset();
        if (QFileInfo(path).isDir()) watch = std::make_unique<WindowsDirectoryWatch>(path, recursive, notify);
    };
    ensure(m_rootWatch, m_root, true);
    const QString parent = QFileInfo(m_root).absolutePath(); ensure(m_parentWatch, parent, false);
    if (!m_rootWatch || !m_rootWatch->matches(m_root) || !m_parentWatch || !m_parentWatch->matches(parent)) m_retry.start();
    else m_retry.stop();
#else
    QSet<QString> wanted;
    if (QFileInfo(m_root).isDir()) wanted.insert(m_root);
    wanted.insert(QFileInfo(m_root).absolutePath());
    QDirIterator it(m_root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        if (!info.isSymLink()) wanted.insert(path);
    }
    const QStringList watched = m_watcher.files() + m_watcher.directories();
    const QSet<QString> current(watched.cbegin(), watched.cend());
    const auto remove = current - wanted, add = wanted - current;
    if (!remove.isEmpty()) m_watcher.removePaths(remove.values());
    if (!add.isEmpty()) m_watcher.addPaths(add.values());
#endif
}
void FileWatcher::suspend() {
    m_debounce.stop();
#ifdef Q_OS_WIN
    m_retry.stop(); m_rootWatch.reset(); m_parentWatch.reset();
#else
    const QStringList paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty()) m_watcher.removePaths(paths);
#endif
}
