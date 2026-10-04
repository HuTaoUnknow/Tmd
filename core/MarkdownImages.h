#pragma once
#include <QString>
#include <QVector>
#include <QMap>

struct MarkdownImage {
    int position = 0;       // Image occurrence, for navigation in the editor.
    int start = 0;          // URL span, including reference definitions when applicable.
    int length = 0;
    QString source, alt;
};
class MarkdownImages {
public:
    static QVector<MarkdownImage> parse(const QString &markdown);
    static QString replace(const QString &markdown, const QVector<MarkdownImage> &images,
                           const QMap<QString, QString> &sources);
    static QString encodedPath(const QString &path);
    static QString localPath(const QString &source, const QString &documentPath);
    static bool isRemote(const QString &source);
    static bool isData(const QString &source);
    static QString markup(const QString &source, QString alt = {});
};
