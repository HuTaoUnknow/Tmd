#include "core/library/MarkdownManager.h"
#include "core/library/MarkdownFileIO.h"
#include "core/library/FileWatcher.h"
#include "core/images/ImageStorage.h"
#include <QDir>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QtCore/qscopeguard.h>

bool MarkdownManager::moveDocuments(const QStringList &paths, const QString &destinationDirectory, QString *error) {
    const auto fail = [&](const QString &message) { if (error) *error = message; return false; };
    if (paths.isEmpty()) return fail(QStringLiteral("请选择要移动的文档。"));
    if (!scan(error)) return false;
    const QString parent = destinationDirectory.isEmpty() ? m_root : safeDirectoryPath(destinationDirectory, error);
    if (parent.isEmpty()) return false;
    if (!QFileInfo(parent).isDir()) return fail(QStringLiteral("目标文件夹不存在。"));
    struct Move {
        QString from, to, oldRelative, newRelative, oldKey, newKey, original, rewritten;
        MarkdownNode *node = nullptr;
        bool moved = false, pathUpdated = false;
    };
    QList<Move> moves; QSet<QString> sources, targets; QMap<QString, QString> keys;
    for (const QString &path : paths) {
        const QString from = safePath(path, error);
        if (from.isEmpty()) return false;
        const QString oldKey = keyFor(path);
        if (sources.contains(oldKey)) continue;
        sources.insert(oldKey);
        auto *node = loadNode(path);
        if (!node || !QFileInfo(from).isFile()) return fail(QStringLiteral("文档不存在：%1").arg(path));
        const QString to = QDir(parent).filePath(QFileInfo(from).fileName());
        const QString relative = QDir(m_root).relativeFilePath(to), newKey = keyFor(relative);
        if (newKey == oldKey) continue;
        if (node->isModified()) return fail(QStringLiteral("请先保存文档，再移动：%1").arg(path));
        if (QFileInfo::exists(to) || targets.contains(newKey)) return fail(QStringLiteral("目标目录存在同名文档：%1").arg(QFileInfo(to).fileName()));
        targets.insert(newKey); keys.insert(oldKey, newKey);
        moves.append({from, to, node->relativePath(), relative, oldKey, newKey, {}, {}, node});
    }
    if (moves.isEmpty()) return true;
    const QSignalBlocker watcherBlocker(m_watcher);
    if (m_watcher) m_watcher->suspend();
    const auto resumeWatching = qScopeGuard([this] { if (m_watcher) m_watcher->rebuild(); });
    ImageStorage storage(m_root); QStringList created;
    for (auto &move : moves) {
        QStringList images;
        if (!MarkdownFileIO::readFile(move.from, move.original, error)
            || !storage.prepareRelocation(move.from, move.newRelative, move.original, move.rewritten, images, error)) {
            storage.rollback(created); return false;
        }
        created.append(images);
    }
    RelationStore next;
    for (auto source = m_relations.cbegin(); source != m_relations.cend(); ++source)
        for (auto target = source->cbegin(); target != source->cend(); ++target)
            next[keys.value(source.key(), source.key())].insert(keys.value(target.key(), target.key()), target.value());
    const auto setPath = [&](Move &move, bool forward) {
        if (move.pathUpdated == forward) return;
        auto node = m_nodes.extract(move.pathUpdated ? move.newKey : move.oldKey);
        if (node.empty()) return;
        node.key() = forward ? move.newKey : move.oldKey;
        node.mapped()->m_filePath = forward ? move.to : move.from;
        node.mapped()->m_relativePath = forward ? move.newRelative : move.oldRelative;
        m_nodes.insert(std::move(node)); move.pathUpdated = forward;
    };
    const auto rollback = [&] {
        QStringList details; bool restored = true;
        for (auto it = moves.rbegin(); it != moves.rend(); ++it) {
            auto &move = *it; if (!move.moved) continue;
            QString reason;
            if (move.rewritten != move.original && !MarkdownFileIO::writeFile(move.to, move.original, &reason)) { restored = false; details.append(reason); }
            if (MarkdownFileIO::renameFile(move.to, move.from, &reason)) move.moved = false;
            else { restored = false; details.append(reason); }
            setPath(move, move.moved);
        }
        if (restored) storage.rollback(created);
        else {
            if (error) *error += QStringLiteral("；部分还原失败，已保留文件和图片：%1").arg(details.join(';'));
            for (const auto &move : moves) if (move.moved) emit documentRenamed(move.oldRelative, move.newRelative);
            emit documentsChanged();
        }
    };
    for (auto &move : moves) {
        if (!MarkdownFileIO::renameFile(move.from, move.to, error)) { rollback(); return false; }
        move.moved = true;
        if (move.rewritten != move.original && !MarkdownFileIO::writeFile(move.to, move.rewritten, error)) { rollback(); return false; }
    }
    for (auto &move : moves) setPath(move, true);
    if (!persistRelations(next, error)) { rollback(); return false; }
    for (auto &move : moves) {
        move.node->m_content = move.node->m_savedContent = move.rewritten;
        move.node->m_fileIdentity = MarkdownFileIO::fileIdentity(move.to); move.node->m_externalChange = false;
    }
    m_relations = next; applyRelations(); ++m_graphRevision;
    for (const auto &move : moves) emit documentRenamed(move.oldRelative, move.newRelative);
    emit documentsChanged(); emit relationsChanged(); return true;
}
bool MarkdownManager::movePath(const QString &path, const QString &destinationDirectory, QString *error) {
    const QString parent = destinationDirectory.isEmpty() ? m_root : safeDirectoryPath(destinationDirectory, error);
    if (parent.isEmpty()) return false;
    const QString to = QDir(parent).filePath(QFileInfo(path).fileName());
    return relocatePath(path, QDir(m_root).relativeFilePath(to), error);
}
bool MarkdownManager::renameDirectory(const QString &path, const QString &newRelativePath, QString *error) {
    const QString from = safeDirectoryPath(path, error);
    if (from.isEmpty()) return false;
    if (!QFileInfo(from).isDir()) {
        if (error) *error = QStringLiteral("文件夹不存在，请刷新列表。");
        return false;
    }
    return relocatePath(path, newRelativePath, error);
}
bool MarkdownManager::relocatePath(const QString &path, const QString &newRelativePath, QString *error) {
    const auto fail = [&](const QString &message) { if (error) *error = message; return false; };
    if (!scan(error)) return false;
    const bool directory = QFileInfo(QDir(m_root).absoluteFilePath(path)).isDir();
    const QString from = directory ? safeDirectoryPath(path, error) : safePath(path, error);
    if (from.isEmpty()) return false;
    if (!QFileInfo::exists(from)) return fail(QStringLiteral("要移动的项目不存在，请刷新列表。"));
    const QString to = directory ? safeDirectoryPath(newRelativePath, error) : safePath(newRelativePath, error);
    if (to.isEmpty()) return false;
    if (!QFileInfo(QFileInfo(to).absolutePath()).isDir()) return fail(QStringLiteral("目标文件夹不存在。"));
#ifdef Q_OS_WIN
    const auto sensitivity = Qt::CaseInsensitive;
#else
    const auto sensitivity = Qt::CaseSensitive;
#endif
    if (from.compare(to, sensitivity) == 0) return true;
    if (directory && to.startsWith(from + '/', sensitivity)) return fail(QStringLiteral("不能将文件夹移动到自身或其下级目录。"));
    if (QFileInfo::exists(to)) return fail(QStringLiteral("目标目录中存在同名项目，请先重命名。"));
    const QSignalBlocker watcherBlocker(m_watcher);
    if (m_watcher) m_watcher->suspend();
    const auto resumeWatching = qScopeGuard([this] { if (m_watcher) m_watcher->rebuild(); });
    const QString toRelative = QDir(m_root).relativeFilePath(to);
    if (!directory) return renameDocument(path, toRelative, error);

    struct Document {
        MarkdownNode *node;
        QString oldPath, newPath, oldRelative, newRelative, oldKey, newKey, original, rewritten;
        bool rewrittenOnDisk = false;
    };
    QList<Document> documents;
    QMap<QString, QString> keys;
    for (auto *node : allNodes()) {
        if (!node->filePath().startsWith(from + '/', sensitivity)) continue;
        if (node->isModified()) return fail(QStringLiteral("请先保存文件夹内的文档，再移动：%1").arg(node->relativePath()));
        const QString target = to + node->filePath().mid(from.size());
        const QString relative = QDir(m_root).relativeFilePath(target);
        documents.append({node, node->filePath(), target, node->relativePath(), relative,
            keyFor(node->relativePath()), keyFor(relative), {}, {}});
        keys.insert(documents.back().oldKey, documents.back().newKey);
    }
    ImageStorage storage(m_root); QStringList created;
    for (auto &document : documents) {
        QStringList images;
        if (!MarkdownFileIO::readFile(document.oldPath, document.original, error)
            || !storage.prepareRelocation(document.oldPath, document.newRelative, document.original, document.rewritten, images, error, from, to)) {
            storage.rollback(created); return false;
        }
        created.append(images);
    }
    RelationStore next;
    for (auto source = m_relations.cbegin(); source != m_relations.cend(); ++source)
        for (auto target = source->cbegin(); target != source->cend(); ++target)
            next[keys.value(source.key(), source.key())].insert(keys.value(target.key(), target.key()), target.value());
    bool sourceIntact = true;
    if (!MarkdownFileIO::renameDirectory(from, to, error, &sourceIntact)) {
        if (sourceIntact) storage.rollback(created);
        else emit documentsChanged();
        return false;
    }

    const auto setNodePaths = [&](bool forward) {
        for (auto &document : documents) {
            auto node = m_nodes.extract(forward ? document.oldKey : document.newKey);
            if (node.empty()) continue;
            node.key() = forward ? document.newKey : document.oldKey;
            node.mapped()->m_filePath = forward ? document.newPath : document.oldPath;
            node.mapped()->m_relativePath = forward ? document.newRelative : document.oldRelative;
            m_nodes.insert(std::move(node));
        }
    };
    const auto rollback = [&] {
        QString details; bool restored = true;
        for (const auto &document : documents) {
            if (!document.rewrittenOnDisk) continue;
            QString reason;
            if (!MarkdownFileIO::writeFile(document.newPath, document.original, &reason)) { restored = false; details += reason + ';'; }
        }
        QString moveError; const bool movedBack = MarkdownFileIO::renameDirectory(to, from, &moveError);
        if (!movedBack) details += moveError;
        setNodePaths(!movedBack);
        if (restored && movedBack) storage.rollback(created);
        else {
            if (error) *error += QStringLiteral("；还原未完成，文档保留在 %1：%2").arg(movedBack ? from : to, details);
            if (!movedBack) for (const auto &document : documents) emit documentRenamed(document.oldRelative, document.newRelative);
        }
    };
    for (auto &document : documents) {
        if (document.original != document.rewritten) {
            if (!MarkdownFileIO::writeFile(document.newPath, document.rewritten, error)) { rollback(); return false; }
            document.rewrittenOnDisk = true;
        }
    }
    // Portable relation serialization uses the updated display paths.
    setNodePaths(true);
    if (!persistRelations(next, error)) { rollback(); return false; }
    for (const auto &document : documents) {
        document.node->m_content = document.node->m_savedContent = document.rewritten;
        document.node->m_fileIdentity = MarkdownFileIO::fileIdentity(document.newPath);
        document.node->m_externalChange = false;
    }
    m_relations = next; applyRelations(); ++m_graphRevision;
    for (const auto &document : documents) emit documentRenamed(document.oldRelative, document.newRelative);
    emit documentsChanged(); emit relationsChanged(); return true;
}
