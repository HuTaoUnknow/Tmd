#include "core/library/LibraryPathPolicy.h"
#include "core/library/MarkdownFileIO.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
namespace {
QString reject(QString *error, const QString &message) { if (error) *error = message; return {}; }
}
LibraryPathPolicy::LibraryPathPolicy(QString root) : m_root(MarkdownFileIO::normalizedPath(root)) {}
QString LibraryPathPolicy::containedPath(const QString &candidate) const {
#ifdef Q_OS_WIN
    const auto sensitivity = Qt::CaseInsensitive;
#else
    const auto sensitivity = Qt::CaseSensitive;
#endif
    const QString resolved = MarkdownFileIO::normalizedPath(candidate);
    QFileInfo ancestor(candidate);
    while (!ancestor.exists() && ancestor.absoluteFilePath() != ancestor.absolutePath()) ancestor = QFileInfo(ancestor.absolutePath());
    const QString ancestorPath = MarkdownFileIO::normalizedPath(ancestor.absoluteFilePath());
    if (!resolved.startsWith(m_root + '/', sensitivity)
        || (ancestorPath.compare(m_root, sensitivity) != 0 && !ancestorPath.startsWith(m_root + '/', sensitivity))) return {};
    return resolved;
}
QString LibraryPathPolicy::document(const QString &path, QString *error) const {
    const QString resolved = containedPath(QDir::isAbsolutePath(path) ? path : QDir(m_root).absoluteFilePath(path));
    if (resolved.isEmpty() || QFileInfo(resolved).suffix().compare("md", Qt::CaseInsensitive) != 0)
        return reject(error, QStringLiteral("请选择 md_data 内的 .md 文件，路径不能越过知识库目录。"));
    return resolved;
}
QString LibraryPathPolicy::directory(const QString &path, QString *error) const {
    const QString candidate = QDir::isAbsolutePath(path) ? path : QDir(m_root).absoluteFilePath(path);
    const QString relative = QDir(m_root).relativeFilePath(QDir::cleanPath(candidate));
    if (path.isEmpty() || relative == "." || QDir::isAbsolutePath(relative))
        return reject(error, QStringLiteral("请选择 md_data 内的文件夹，不能操作知识库根目录。"));
    for (const auto &part : QDir::fromNativeSeparators(relative).split('/')) {
        if (part.isEmpty() || part == "." || part == "..") return reject(error, QStringLiteral("文件夹路径不能越过知识库目录。"));
#ifdef Q_OS_WIN
        static const QRegularExpression reserved("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\.|$)", QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression invalid("[<>:\"\\\\|?*\\x00-\\x1f]");
        if (part.endsWith('.') || part.endsWith(' ') || invalid.match(part).hasMatch() || reserved.match(part).hasMatch())
            return reject(error, QStringLiteral("文件夹名称包含 Windows 不允许的字符或保留名称。"));
#endif
    }
    const QString resolved = containedPath(candidate);
    if (resolved.isEmpty() || QFileInfo(candidate).isSymLink())
        return reject(error, QStringLiteral("请选择知识库内的真实文件夹，不能越界或操作目录链接。"));
    return resolved;
}
