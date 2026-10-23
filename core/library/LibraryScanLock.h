#pragma once
#include <QString>

// A recursive, process-shared library transaction lock. No watched file is
// created/deleted on each acquisition, so locking cannot cause refresh loops.
class LibraryScanLock {
public:
    explicit LibraryScanLock(const QString &root);
    ~LibraryScanLock();
    LibraryScanLock(const LibraryScanLock &) = delete;
    LibraryScanLock &operator=(const LibraryScanLock &) = delete;
    bool locked() const { return m_locked; }
    QString error() const { return QStringLiteral("知识库正在由其他窗口同步，或无法取得同步锁，请稍后重试。"); }
private:
    QString m_key;
    void *m_handle = nullptr;
    bool m_locked = false;
};
