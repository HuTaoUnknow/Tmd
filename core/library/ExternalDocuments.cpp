#include "core/library/ExternalDocuments.h"
#include "core/library/MarkdownNode.h"
#include "core/library/MarkdownFileIO.h"
#include "core/library/LibraryPathPolicy.h"
#include "core/library/LibraryReferences.h"
#include "core/library/LibraryScanLock.h"
#include "core/library/FileWatcher.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTimer>

namespace {
bool fail(QString *error, const QString &message) { if (error) *error = message; return false; }
}
QString ExternalDocuments::key(const QString &path) {
    if (!QDir::isAbsolutePath(path)) return {};
    auto result = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef Q_OS_WIN
    result = result.toCaseFolded();
#endif
    return result;
}
ExternalDocuments::ExternalDocuments(const QString &root, QObject *parent)
    : QObject(parent), m_root(root), m_record(QDir(root).filePath(".tree-md-external.json")),
      m_debounce(new QTimer(this)), m_poll(new QTimer(this)) {
    m_debounce->setSingleShot(true); m_debounce->setInterval(80);
    m_poll->setInterval(2000);
    connect(m_debounce, &QTimer::timeout, this, &ExternalDocuments::refresh);
    connect(m_poll, &QTimer::timeout, this, &ExternalDocuments::refresh);
    if (!QFileInfo::exists(m_record)) return;
    QString text;
    if (!MarkdownFileIO::readFile(m_record, text, &m_loadError)) return;
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(text.toUtf8(), &parse);
    const auto object = document.object();
    if (parse.error != QJsonParseError::NoError || object.value("version").toInt() != 1 || !object.value("files").isArray()) {
        m_loadError = QStringLiteral("外部文档记录无法读取，原记录已保留。"); return;
    }
    for (const auto &value : object.value("files").toArray()) {
        const QString path = QDir::cleanPath(QDir::fromNativeSeparators(value.toString()));
        if (!QDir::isAbsolutePath(path) || QFileInfo(path).suffix().compare("md", Qt::CaseInsensitive) != 0
            || !LibraryPathPolicy(m_root).document(path).isEmpty()) continue;
        auto node = std::make_unique<MarkdownNode>();
        node->m_filePath = node->m_relativePath = path; node->m_externalDocument = true;
        QString error;
        MarkdownFileIO::readFile(path, node->m_content, &error);
        node->m_savedContent = node->m_content;
        updateStatus(node.get()); m_nodes.emplace(key(path), std::move(node));
    }
}
ExternalDocuments::~ExternalDocuments() = default;
MarkdownNode *ExternalDocuments::find(const QString &path) const {
    const auto found = m_nodes.find(key(path));
    return found == m_nodes.end() ? nullptr : found->second.get();
}
QList<MarkdownNode *> ExternalDocuments::nodes() const {
    QList<MarkdownNode *> result;
    for (const auto &entry : m_nodes) result.append(entry.second.get());
    return result;
}
void ExternalDocuments::updateStatus(MarkdownNode *node) {
    node->m_sourceMissing = !QFileInfo(node->filePath()).isFile();
    node->m_missingReferences = LibraryReferences::missing(node->content(), m_root, node->filePath());
    if (node->m_sourceMissing) node->m_missingReferences.prepend(QStringLiteral("外部文件不存在：") + node->filePath());
}
bool ExternalDocuments::persist(QString *error, const QString &added, const QString &removed) {
    const LibraryScanLock transaction(m_root);
    if (!transaction.locked()) return fail(error, transaction.error());
    if (!m_loadError.isEmpty()) return fail(error, m_loadError);
    QJsonArray files;
    if (QFileInfo::exists(m_record)) {
        QString text;
        if (!MarkdownFileIO::readFile(m_record, text, error)) return false;
        QJsonParseError parse;
        const auto object = QJsonDocument::fromJson(text.toUtf8(), &parse).object();
        if (parse.error != QJsonParseError::NoError || object.value("version").toInt() != 1 || !object.value("files").isArray())
            return fail(error, QStringLiteral("外部文档记录格式错误，未覆盖原记录。"));
        files = object.value("files").toArray();
    }
    QJsonArray updated; bool present = false;
    for (const auto &value : files) {
        if (!removed.isEmpty() && key(value.toString()) == key(removed)) continue;
        if (!added.isEmpty() && key(value.toString()) == key(added)) present = true;
        updated.append(value);
    }
    if (!added.isEmpty() && !present) updated.append(added);
    return MarkdownFileIO::writeFile(m_record, QString::fromUtf8(QJsonDocument(QJsonObject{{"version", 1}, {"files", updated}}).toJson()), error);
}
MarkdownNode *ExternalDocuments::open(const QString &input, QString *error) {
    const QString path = MarkdownFileIO::normalizedPath(input);
    if (auto *node = find(path)) return node;
    if (!QFileInfo(path).isFile() || QFileInfo(path).suffix().compare("md", Qt::CaseInsensitive) != 0
        || !LibraryPathPolicy(m_root).document(path).isEmpty()) {
        fail(error, QStringLiteral("请选择知识库以外存在的 MD 文件。")); return nullptr;
    }
    auto node = std::make_unique<MarkdownNode>();
    node->m_filePath = node->m_relativePath = path; node->m_externalDocument = true;
    if (!MarkdownFileIO::readFile(path, node->m_content, error)) return nullptr;
    node->m_savedContent = node->m_content; updateStatus(node.get());
    const auto id = key(path); m_nodes.emplace(id, std::move(node));
    if (!persist(error, path)) { m_nodes.erase(id); return nullptr; }
    rebuildWatches(); emit documentsChanged(); return find(path);
}
bool ExternalDocuments::forget(const QString &path, QString *error, bool discard) {
    auto found = m_nodes.find(key(path));
    if (found == m_nodes.end()) return fail(error, QStringLiteral("外部文档记录不存在。"));
    if (found->second->isModified() && !discard) return fail(error, QStringLiteral("请先保存或放弃编辑，再移除记录。"));
    auto held = std::move(found->second); m_nodes.erase(found);
    if (!persist(error, {}, path)) { m_nodes.emplace(key(path), std::move(held)); return false; }
    rebuildWatches(); emit documentsChanged(); return true;
}
void ExternalDocuments::updateContent(const QString &path, const QString &content) {
    auto *node = find(path); if (!node || node->m_content == content) return;
    const auto title = node->title(); node->m_content = content; updateStatus(node);
    emit documentChanged(node->relativePath());
    if (node->title() != title) emit titleChanged(node->relativePath(), node->title());
}
bool ExternalDocuments::save(const QString &path, QString *error, bool overwrite) {
    auto *node = find(path); if (!node) return fail(error, QStringLiteral("外部文档记录不存在。"));
    QString disk;
    if (!MarkdownFileIO::readFile(node->filePath(), disk, error)) { updateStatus(node); emit documentsChanged(); return false; }
    if (disk != node->m_savedContent && !overwrite) {
        node->m_externalChange = true;
        return fail(error, QStringLiteral("原文件已被其他程序修改。请重新加载，或明确选择覆盖。"));
    }
    if (!MarkdownFileIO::writeFile(node->filePath(), node->content(), error)) return false;
    node->m_savedContent = node->m_content; node->m_externalChange = false; updateStatus(node);
    rebuildWatches(); emit documentChanged(node->relativePath()); return true;
}
bool ExternalDocuments::reload(const QString &path, QString *error) {
    auto *node = find(path); if (!node) return fail(error, QStringLiteral("外部文档记录不存在。"));
    QString content;
    if (!MarkdownFileIO::readFile(node->filePath(), content, error)) return false;
    const auto title = node->title(); node->m_content = node->m_savedContent = content;
    node->m_externalChange = false; updateStatus(node);
    emit documentChanged(node->relativePath());
    if (node->title() != title) emit titleChanged(node->relativePath(), node->title());
    emit documentsChanged(); return true;
}
void ExternalDocuments::enableWatching() { m_watching = true; rebuildWatches(); }
void ExternalDocuments::rebuildWatches() {
    if (!m_watching) return;
    QStringList wanted;
    for (const auto *node : nodes()) {
        const QFileInfo directory(QFileInfo(node->filePath()).absolutePath());
        if (directory.isDir()) wanted.append(directory.absoluteFilePath());
    }
    wanted.removeDuplicates();
    for (const auto &path : m_watchers.keys()) if (!wanted.contains(path)) delete m_watchers.take(path);
    for (const auto &path : wanted) if (!m_watchers.contains(path)) {
        // Use the existing delete-sharing Windows watcher: opening a document must not lock its folder.
        auto *watcher = new FileWatcher(path, this, false);
        connect(watcher, &FileWatcher::changed, m_debounce, qOverload<>(&QTimer::start));
        m_watchers.insert(path, watcher);
    }
    if (m_nodes.empty()) m_poll->stop(); else m_poll->start();
}
void ExternalDocuments::refresh() {
    bool changed = false;
    for (auto *node : nodes()) {
        const auto title = node->title(), before = node->content();
        const auto missing = node->missingReferences(); const bool conflict = node->hasExternalChange();
        QString disk, error;
        if (MarkdownFileIO::readFile(node->filePath(), disk, &error)) {
            if (!node->isModified()) node->m_content = node->m_savedContent = disk;
            node->m_externalChange = disk != node->m_savedContent;
        }
        updateStatus(node);
        if (!error.isEmpty() && !node->m_sourceMissing) node->m_missingReferences.prepend(QStringLiteral("无法读取原文件：") + error);
        if (before != node->content() || missing != node->missingReferences() || conflict != node->hasExternalChange()) {
            changed = true; emit documentChanged(node->relativePath());
            if (title != node->title()) emit titleChanged(node->relativePath(), node->title());
        }
    }
    rebuildWatches(); if (changed) emit documentsChanged();
}
