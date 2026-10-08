#include "core/library/DefaultLibrary.h"
#include "core/library/MarkdownFileIO.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class DefaultLibraryTests : public QObject {
    Q_OBJECT
private slots:
    void firstLaunchAndDeletedExamplesStayDeleted();
    void existingLibraryIsPreserved();
    void failedInitializationCanBeRetried();
};

void DefaultLibraryTests::firstLaunchAndDeletedExamplesStayDeleted() {
    QTemporaryDir temporary; QVERIFY(temporary.isValid()); const QDir root(temporary.path());
    const QString examples = root.filePath("examples"), library = root.filePath("user/Tmd");
    QVERIFY(QDir().mkpath(QDir(examples).filePath(QStringLiteral("md_data/markdown学习"))));
    QVERIFY(QDir().mkpath(QDir(examples).filePath(QStringLiteral("md_photo/markdown学习"))));
    const QString relative = QStringLiteral("md_data/markdown学习/入门.md");
    QVERIFY(MarkdownFileIO::writeFile(QDir(examples).filePath(relative), QStringLiteral("# 入门\n自己的例子\n")));
    QVERIFY(MarkdownFileIO::writeFile(QDir(examples).filePath("md_data/.tree-md-relations.json"), "{\"version\":2,\"relations\":[]}"));
    QVERIFY(MarkdownFileIO::writeFile(QDir(examples).filePath(QStringLiteral("md_photo/markdown学习/photo.png")), "image fixture"));
    QString error; QVERIFY2(DefaultLibrary::initialize(library, examples, &error), qPrintable(error));
    QString content; QVERIFY(MarkdownFileIO::readFile(QDir(library).filePath(relative), content));
    QCOMPARE(content, QStringLiteral("# 入门\n自己的例子\n"));
    QVERIFY(QFileInfo::exists(QDir(library).filePath("md_data/.tree-md-relations.json")));
    QVERIFY(QFileInfo::exists(QDir(library).filePath(QStringLiteral("md_photo/markdown学习/photo.png"))));
    QVERIFY(QFile::remove(QDir(library).filePath(relative)));
    QVERIFY2(DefaultLibrary::initialize(library, examples, &error), qPrintable(error));
    QVERIFY(!QFileInfo::exists(QDir(library).filePath(relative)));
}

void DefaultLibraryTests::existingLibraryIsPreserved() {
    QTemporaryDir temporary; const QDir root(temporary.path()); const QString library = root.filePath("library");
    QVERIFY(QDir().mkpath(QDir(library).filePath("md_data")));
    const QString userFile = QDir(library).filePath(QStringLiteral("md_data/我的笔记.md"));
    const QString metadata = QDir(library).filePath("md_data/.tree-md-relations.json");
    QVERIFY(MarkdownFileIO::writeFile(userFile, QStringLiteral("# 个人笔记\n保留内容\n")));
    QVERIFY(MarkdownFileIO::writeFile(metadata, "user relation snapshot"));
    QString error; QVERIFY2(DefaultLibrary::initialize(library, root.filePath("absent-examples"), &error), qPrintable(error));
    QString content; QVERIFY(MarkdownFileIO::readFile(userFile, content)); QCOMPARE(content, QStringLiteral("# 个人笔记\n保留内容\n"));
    QVERIFY(MarkdownFileIO::readFile(metadata, content)); QCOMPARE(content, QString("user relation snapshot"));
    QCOMPARE(QDir(QDir(library).filePath("md_data")).entryList(QDir::Files | QDir::Hidden).size(), 2);
}

void DefaultLibraryTests::failedInitializationCanBeRetried() {
    QTemporaryDir temporary; const QDir root(temporary.path());
    const QString examples = root.filePath("examples"), library = root.filePath("library");
    QVERIFY(QDir().mkpath(QDir(examples).filePath("md_data")));
    QVERIFY(MarkdownFileIO::writeFile(QDir(examples).filePath("md_data/first.md"), "# First"));
    QString error; QVERIFY(!DefaultLibrary::initialize(library, examples, &error)); QVERIFY(!error.isEmpty());
    QVERIFY(!QFileInfo::exists(QDir(library).filePath("md_data/first.md")));
    QVERIFY(!QFileInfo::exists(QDir(library).filePath(".tmd-library-initialized")));
    QVERIFY(QDir().mkpath(QDir(examples).filePath("md_photo")));
    QVERIFY2(DefaultLibrary::initialize(library, examples, &error), qPrintable(error));
    QVERIFY(QFileInfo::exists(QDir(library).filePath("md_data/first.md")));
}

QTEST_GUILESS_MAIN(DefaultLibraryTests)
#include "DefaultLibraryTests.moc"
