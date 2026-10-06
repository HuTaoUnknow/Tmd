#pragma once
#include <QString>
#include <QStringList>
#include <QImage>

class ImageStorage {
public:
    explicit ImageStorage(QString dataRoot, QString photoRoot = {});
    static QString absolutePhotoRoot(const QString &dataRoot);
    QString photoRoot() const { return m_photoRoot; }
    bool ensureRoot(QString *error = nullptr) const;
    bool store(const QString &documentRelativePath, const QByteArray &bytes, QString &reference,
               QStringList *created = nullptr, QString *error = nullptr) const;
    bool storeFile(const QString &documentRelativePath, const QString &source, QString &reference,
                   QStringList *created = nullptr, QString *error = nullptr) const;
    bool storeImage(const QString &documentRelativePath, const QImage &image, QString &reference,
                    QString *error = nullptr) const;
    bool prepareImport(const QString &sourceDocument, const QString &targetRelativePath, const QString &content,
                       QString &rewritten, QStringList &created, QString *error = nullptr) const;
    bool prepareRelocation(const QString &sourceDocument, const QString &targetRelativePath, const QString &content,
                           QString &rewritten, QStringList &created, QString *error = nullptr) const;
    void rollback(const QStringList &created) const;
    static bool writeAsset(const QString &directory, const QByteArray &bytes, const QString &extension,
                           QString &path, bool &created, QString *error = nullptr);
private:
    QString m_dataRoot, m_photoRoot;
};
