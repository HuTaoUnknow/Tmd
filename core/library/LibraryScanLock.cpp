#include "core/library/LibraryScanLock.h"
#include "core/library/LibraryFileIndex.h"
#include <QCryptographicHash>
#include <QDir>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <QMap>
#include <QElapsedTimer>
#include <QFile>
#include <QThread>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
namespace {
struct Held { int fd = -1, count = 0; };
thread_local QMap<QString,Held> held;
}
#endif

LibraryScanLock::LibraryScanLock(const QString &root) : m_key(LibraryFileIndex::key(root)) {
#ifdef Q_OS_WIN
    const QString name="Local\\Tmd-Library-"+QString::fromLatin1(QCryptographicHash::hash(m_key.toUtf8(),QCryptographicHash::Sha256).toHex());
    const HANDLE handle=CreateMutexW(nullptr,FALSE,reinterpret_cast<LPCWSTR>(name.utf16()));
    if (!handle) return;
    const DWORD wait=WaitForSingleObject(handle,5000);
    if (wait==WAIT_OBJECT_0 || wait==WAIT_ABANDONED) { m_handle=handle; m_locked=true; }
    else CloseHandle(handle);
#else
    if (held.contains(m_key)) { ++held[m_key].count; m_locked=true; return; }
    const QByteArray path=QFile::encodeName(QDir(root).filePath(".tree-md-files.lock"));
    const int fd=::open(path.constData(),O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);
    if (fd<0) return;
    QElapsedTimer timer; timer.start();
    do {
        if (::flock(fd,LOCK_EX|LOCK_NB)==0) { held.insert(m_key,{fd,1}); m_locked=true; return; }
        QThread::msleep(10);
    } while (timer.elapsed()<5000);
    ::close(fd);
#endif
}
LibraryScanLock::~LibraryScanLock() {
    if (!m_locked) return;
#ifdef Q_OS_WIN
    ReleaseMutex(static_cast<HANDLE>(m_handle)); CloseHandle(static_cast<HANDLE>(m_handle));
#else
    auto found=held.find(m_key);
    if (found!=held.end() && !--found->count) { ::flock(found->fd,LOCK_UN); ::close(found->fd); held.erase(found); }
#endif
}
