#include "core/library/FileWatcher.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QCoreApplication>
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
    // The kernel owns these addresses until the overlapped request completes,
    // including after CancelIoEx has merely requested cancellation.
    struct Request {
        HANDLE directory = INVALID_HANDLE_VALUE, event = nullptr;
        OVERLAPPED overlapped{};
        alignas(DWORD) std::array<char, 65536> buffer{};
        ~Request() {
            if (directory != INVALID_HANDLE_VALUE) CloseHandle(directory);
            if (event) CloseHandle(event);
        }
    };
    static void retire(Request *request, int attempts = 0) {
        DWORD bytes = 0;
        if (GetOverlappedResult(request->directory, &request->overlapped, &bytes, FALSE)
            || GetLastError() != ERROR_IO_INCOMPLETE) { delete request; return; }
        // Never wait on the GUI thread. Keep the buffer, OVERLAPPED and handles
        // alive while Windows finishes cancellation. At process shutdown an
        // undelivered callback deliberately leaves them for OS reclamation.
        if (attempts == 100) qWarning("Directory watch cancellation is still pending; UI cleanup remains asynchronous.");
        if (auto *application = QCoreApplication::instance())
            QTimer::singleShot(attempts < 100 ? 10 : 100, application, [request, attempts] { retire(request, attempts + 1); });
    }
public:
    WindowsDirectoryWatch(const QString &path, bool recursive, std::function<void()> changed)
        : m_recursive(recursive), m_changed(std::move(changed)) {
        m_request->directory = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
        if (m_request->directory == INVALID_HANDLE_VALUE) return;
        GetFileInformationByHandle(m_request->directory, &m_identity);
        m_request->event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!m_request->event) return;
        m_notifier = std::make_unique<QWinEventNotifier>(m_request->event);
        QObject::connect(m_notifier.get(), &QWinEventNotifier::activated, m_notifier.get(), [this] {
            DWORD bytes = 0;
            const bool complete = GetOverlappedResult(m_request->directory, &m_request->overlapped, &bytes, FALSE);
            if (!complete && GetLastError() == ERROR_IO_INCOMPLETE) return;
            m_pending = false;
            if (!complete || !arm()) m_notifier->setEnabled(false);
            m_changed(); // Zero bytes also requires a rescan after buffer overflow.
        });
        if (!arm()) m_notifier->setEnabled(false);
    }
    ~WindowsDirectoryWatch() {
        m_notifier.reset();
        if (m_pending) {
            auto *request = m_request.release();
            CancelIoEx(request->directory, &request->overlapped);
            retire(request);
        }
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
        ResetEvent(m_request->event); m_request->overlapped = {}; m_request->overlapped.hEvent = m_request->event;
        m_pending = ReadDirectoryChangesW(m_request->directory, m_request->buffer.data(), DWORD(m_request->buffer.size()), m_recursive,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
            nullptr, &m_request->overlapped, nullptr);
        return m_pending;
    }
    std::unique_ptr<Request> m_request = std::make_unique<Request>();
    BY_HANDLE_FILE_INFORMATION m_identity{};
    bool m_recursive, m_pending = false;
    std::function<void()> m_changed;
    std::unique_ptr<QWinEventNotifier> m_notifier;
};
#endif

FileWatcher::FileWatcher(QString root, QObject *parent, bool recursive)
    : QObject(parent), m_root(std::move(root)), m_recursive(recursive) {
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
    ensure(m_rootWatch, m_root, m_recursive);
    const QString parent = QFileInfo(m_root).absolutePath(); ensure(m_parentWatch, parent, false);
    if (!m_rootWatch || !m_rootWatch->matches(m_root) || !m_parentWatch || !m_parentWatch->matches(parent)) m_retry.start();
    else m_retry.stop();
#else
    QSet<QString> wanted;
    if (QFileInfo(m_root).isDir()) wanted.insert(m_root);
    wanted.insert(QFileInfo(m_root).absolutePath());
    QDirIterator it(m_root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                    m_recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags);
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
