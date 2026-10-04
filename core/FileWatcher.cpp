#include "FileWatcher.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

FileWatcher::FileWatcher(QString root, QObject *parent) : QObject(parent), m_root(std::move(root)) {
    m_debounce.setSingleShot(true); m_debounce.setInterval(280);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, [this] { emit changed(); rebuild(); });
    rebuild();
}
void FileWatcher::rebuild() {
    QSet<QString> wanted;
    if (QFileInfo(m_root).isDir()) wanted.insert(m_root);
    wanted.insert(QFileInfo(m_root).absolutePath());
    QDirIterator it(m_root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        if (!info.isSymLink() && (info.isDir() || info.suffix().compare("md", Qt::CaseInsensitive) == 0 || info.fileName() == ".tree-md-relations.json")) wanted.insert(path);
    }
    const QStringList watched = m_watcher.files() + m_watcher.directories();
    const QSet<QString> current(watched.cbegin(), watched.cend());
    const auto remove = current - wanted, add = wanted - current;
    if (!remove.isEmpty()) m_watcher.removePaths(remove.values());
    if (!add.isEmpty()) m_watcher.addPaths(add.values());
}
