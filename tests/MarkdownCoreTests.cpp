#include "core/markdown/MarkdownAnalysis.h"
#include "core/markdown/MarkdownParser.h"
#include "core/markdown/MarkdownImages.h"
#include "core/markdown/MarkdownSourceMap.h"
#include "core/images/ImageStorage.h"
#include "core/images/ImageInsertJob.h"
#include "core/library/MarkdownManager.h"
#include "core/library/MarkdownFileIO.h"
#include <QTemporaryDir>
#include <QDir>
#include <QtTest>

class MarkdownCoreTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_settingsDirectory;
private slots:
    void initTestCase() {
        QVERIFY(m_settingsDirectory.isValid());
        MarkdownSettingsStore::setStoragePath(m_settingsDirectory.filePath("settings.txt"));
    }
    void quotedFencesAndComments_data() {
        QTest::addColumn<QString>("source");
        QTest::newRow("longer-close") << QString("> ```md\n> # literal\n> ![example](missing.png)\n> ````\n\n# Real\n![real](real.png)");
        QTest::newRow("container-ends") << QString("> ```md\n> # literal\n> ![example](missing.png)\n\n# Real\n![real](real.png)");
        QTest::newRow("comment") << QString("<!--\n# literal\n![example](missing.png)\n-->\n# Real\n![real](real.png)");
        QTest::newRow("raw-html") << QString("<pre>\n# literal\n![example](missing.png)\n</pre>\n# Real\n![real](real.png)");
        QTest::newRow("list-code") << QString("- ```md\n  # literal\n  ![example](missing.png)\n  ````\n\n# Real\n![real](real.png)");
    }
    void quotedFencesAndComments() {
        QFETCH(QString, source);
        const auto analysis = MarkdownAnalysis::scan(source);
        const auto outline = MarkdownParser::outline(source, analysis);
        QCOMPARE(outline.size(), 1); QCOMPARE(outline.front().text, QString("Real"));
        const auto markers = MarkdownParser::headingMarkers(source, analysis);
        QCOMPARE(markers.size(), 1);
        const auto images = MarkdownImages::parse(source, analysis);
        QCOMPARE(images.size(), 1); QCOMPARE(images.front().source, QString("real.png"));
    }
    void importDoesNotReadLiteralImages() {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        const QString source = "> ```md\n> ![example](missing.png)\n> ````\n<!-- ![hidden](missing2.png) -->\n# Real\n";
        ImageStorage storage(QDir(temp.path()).filePath("md_data"));
        QString rewritten, error; QStringList created;
        QVERIFY2(storage.prepareImport(QDir(temp.path()).filePath("source.md"), "guide.md", source, rewritten, created, &error), qPrintable(error));
        QCOMPARE(rewritten, source); QVERIFY(created.isEmpty());
    }
    void inlineCommentDelimitersAreLiteral() {
        const QString source = "`<!--` ![real](real.png) `-->`\n<!-- ![hidden](missing.png) -->";
        const auto analysis = MarkdownAnalysis::scan(source);
        QCOMPARE(analysis.inlineCodes.size(), 2);
        const auto images = MarkdownImages::parse(source, analysis);
        QCOMPARE(images.size(), 1); QCOMPARE(images.front().source, QString("real.png"));
        QCOMPARE(analysis.opaque.size(), 1);
    }
    void exactInlineDelimiterRuns() {
        const QString source = "``a ` b`` and `c`";
        MarkdownSourceMap mapping; mapping.rebuild(source, "a ` b and c");
        const auto first = mapping.inlineCodeAt(2), second = mapping.inlineCodeAt(10);
        QVERIFY(first.valid()); QCOMPARE(first.sourceEnd, 9); QVERIFY(second.valid());
        QCOMPARE(mapping.edited(2, 0, "x", nullptr), QString("``a x` b`` and `c`"));
    }
    void inlineRangesFollowIncrementalEdits() {
        MarkdownSourceMap mapping; mapping.rebuild("`one` and `two`", "one and two");
        QCOMPARE(mapping.inlineCodeAt(9).sourceStart, 10);
        QCOMPARE(mapping.applyEdit(0, 0, "x", "x", nullptr), QString("`xone` and `two`"));
        QCOMPARE(mapping.inlineCodeAt(10).sourceStart, 11);
        QCOMPARE(mapping.applyEdit(1, 3, "", "", nullptr), QString("`x` and `two`"));
        QVERIFY(mapping.inlineCodeAt(0).valid()); QCOMPARE(mapping.inlineCodeAt(7).sourceStart, 8);
    }
    void batchImportScansOnceAndRetainsPartialSuccess() {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        const QString source = QDir(temp.path()).filePath("source"); QVERIFY(QDir().mkpath(QDir(source).filePath("group")));
        for (int i = 0; i < 30; ++i) QVERIFY(MarkdownFileIO::writeFile(QDir(source).filePath(QString("group/%1.md").arg(i)), "# Document\n"));
        MarkdownManager manager(QDir(temp.path()).filePath("md_data"));
        QSignalSpy changed(&manager, &MarkdownManager::documentsChanged);
        const auto result = manager.importDirectory(source);
        QVERIFY2(result.success(), qPrintable(result.error)); QCOMPARE(result.imported.size(), 30);
        QCOMPARE(manager.allNodes().size(), 30); QCOMPARE(changed.size(), 2);
        QVERIFY(MarkdownFileIO::writeFile(QDir(source).filePath("new.md"), "# New\n"));
        const auto again = manager.importDirectory(source);
        QVERIFY(!again.success()); QCOMPARE(again.failures.size(), 30); QCOMPARE(again.imported, QStringList{"new.md"});
        QVERIFY(manager.loadNode("new.md"));
    }
    void imageInsertionCapturesConfiguration() {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        MarkdownSettings settings; settings.relativePhotoPath = "../photos-one";
        const QString root = QDir(temp.path()).filePath("md_data"); QVERIFY(QDir().mkpath(root));
        ImageInsertJob job(root, "group/test.md", settings);
        QSignalSpy ready(&job, &ImageInsertJob::ready), failed(&job, &ImageInsertJob::failed);
        settings.relativePhotoPath = "../photos-two";
        QImage image(8, 8, QImage::Format_ARGB32); image.fill(Qt::blue); job.start(image);
        QCOMPARE(failed.size(), 0); QCOMPARE(ready.size(), 1);
        const QString file = MarkdownImages::localPath(ready[0][0].toString(), QDir(root).filePath("group/test.md"));
        QVERIFY(QFileInfo::exists(file)); QVERIFY(file.contains("photos-one/group/"));
    }
    void copyRelocatesImagesAndPreservesUnsavedContent() {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        const QString root = temp.filePath("md_data");
        MarkdownManager manager(root); QString error;
        QVERIFY(manager.scan(&error)); QVERIFY(manager.createDocument("source/a.md", "# Saved\n", &error));
        QImage image(8, 8, QImage::Format_ARGB32); image.fill(Qt::red);
        ImageStorage storage(root); QString reference;
        QVERIFY2(storage.storeImage("source/a.md", image, reference, &error), qPrintable(error));
        const QString content = "# Unsaved\n\n" + MarkdownImages::markup(reference);
        manager.updateContent("source/a.md", content);
        QVERIFY2(manager.copyDocument("source/a.md", "target/b.md", content, &error), qPrintable(error));
        auto *copy = manager.loadNode("target/b.md"); QVERIFY(copy); QCOMPARE(copy->title(), QString("Unsaved"));
        const auto images = MarkdownImages::parse(copy->content()); QCOMPARE(images.size(), 1);
        const QString file = MarkdownImages::localPath(images.front().source, copy->filePath());
        QVERIFY(QFileInfo::exists(file)); QVERIFY(file.contains("md_photo/target/"));
        QVERIFY(manager.loadNode("source/a.md")->isModified()); QCOMPARE(manager.loadNode("source/a.md")->content(), content);
        const QString before = copy->content();
        QVERIFY(!manager.copyDocument("source/a.md", "target/b.md", "replacement", &error));
        QCOMPARE(manager.loadNode("target/b.md")->content(), before);
        QVERIFY(!manager.copyDocument("source/a.md", "../escape.md", content, &error));
        QVERIFY(!QFileInfo::exists(temp.filePath("escape.md")));
    }
};
QTEST_GUILESS_MAIN(MarkdownCoreTests)
#include "MarkdownCoreTests.moc"
