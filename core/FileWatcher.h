#pragma once
#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>

class FileWatcher : public QObject {
    Q_OBJECT
public:
    explicit FileWatcher(QString root, QObject *parent = nullptr);
    void rebuild();
    void suspend();
signals:
    void changed();
private:
    QString m_root;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
