#pragma once
#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <memory>
#ifdef Q_OS_WIN
class WindowsDirectoryWatch;
#endif

class FileWatcher : public QObject {
    Q_OBJECT
public:
    explicit FileWatcher(QString root, QObject *parent = nullptr);
    ~FileWatcher() override;
    void rebuild();
    void suspend();
signals:
    void changed();
private:
    QString m_root;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
#ifdef Q_OS_WIN
    std::unique_ptr<WindowsDirectoryWatch> m_rootWatch, m_parentWatch;
    QTimer m_retry;
#endif
};
