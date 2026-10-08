#include "ui/MainWindow.h"
#include "ui/common/Theme.h"
#include "core/library/MarkdownManager.h"
#include "core/markdown/MarkdownImages.h"
#include "core/images/MarkdownExportJob.h"
#include "core/images/ImageStorage.h"
#include "core/library/MarkdownFileIO.h"
#include "core/library/DefaultLibrary.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QAction>
#include <algorithm>
int main(int argc, char *argv[]) {
    QApplication application(argc, argv);
    applyTreeMdTheme(application);
    application.setApplicationName("Tree MD"); application.setApplicationDisplayName("Tmd");
    application.setOrganizationName("TreeMD"); application.setApplicationVersion(TREE_MD_VERSION);
    QCommandLineParser parser; parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"data-dir", "Markdown data directory", "path"});
    parser.addOption({"library-root", "Initialize or open a library containing md_data and md_photo", "path"});
    parser.addOption({"smoke-test", "Start, render, and exit automatically"});
    parser.addOption({"screenshot", "Save the window image and exit", "path"});
    parser.addOption({"relations", "Expand knowledge relations"});
    parser.addOption({"knowledge-tree", "Open the knowledge tree for the current document"});
    parser.addOption({"settings", "Open the Markdown preload settings"});
    parser.addOption({"image-settings", "Open image storage settings"});
    parser.addOption({"import-directory", "Import Markdown recursively and copy local/embedded images", "path"});
    parser.addOption({"report", "Write an operation report as JSON", "path"});
    parser.addOption({"export-document", "Export this relative document path without opening a window", "path"});
    parser.addOption({"export-to", "Export destination Markdown path", "path"});
    parser.addOption({"with-images", "Bundle images beside the exported Markdown"});
    parser.addOption({"open-document", "Initially open this relative document path", "path"});
    parser.addOption({"source-view", "Initially show the plain Markdown source"});
    parser.process(application);
    QString dataPath = parser.value("data-dir");
    const QDir binary(QCoreApplication::applicationDirPath());
    const QDir source(QStringLiteral(TREE_MD_SOURCE_DIR));
    QString examplesPath = binary.filePath("examples");
    if (!QDir(examplesPath).exists()) examplesPath = source.filePath("examples");
    const bool installed = QFileInfo::exists(binary.filePath("tmd-installation.ini"));
    QString initializeRoot;
    if (dataPath.isEmpty() && parser.isSet("library-root")) initializeRoot = parser.value("library-root");
    else if (dataPath.isEmpty() && installed) initializeRoot = DefaultLibrary::installedRoot();
    if (!initializeRoot.isEmpty() || (dataPath.isEmpty() && installed)) {
        QString error;
        if (!DefaultLibrary::initialize(initializeRoot, examplesPath, &error)) {
            if (parser.isSet("smoke-test") || parser.isSet("screenshot")) qCritical("%s", qPrintable(error));
            else QMessageBox::critical(nullptr, QStringLiteral("Tmd"), error);
            return 2;
        }
        dataPath = QDir(initializeRoot).filePath("md_data");
    }
    if (dataPath.isEmpty()) {
        for (const QString &candidate : {binary.filePath("md_data"), binary.filePath("../md_data"), binary.filePath("../../md_data"), QStringLiteral(TREE_MD_SOURCE_DIR "/md_data")}) {
            if (QFileInfo(candidate).isDir()) { dataPath = candidate; break; }
        }
    }
    if (dataPath.isEmpty()) {
        const QString root = QFileInfo::exists(source.filePath("CMakeLists.txt")) ? source.path() : binary.path();
        QString error;
        if (!DefaultLibrary::initialize(root, examplesPath, &error)) {
            if (parser.isSet("smoke-test") || parser.isSet("screenshot")) qCritical("%s", qPrintable(error));
            else QMessageBox::critical(nullptr, QStringLiteral("Tmd"), error);
            return 2;
        }
        dataPath = QDir(root).filePath("md_data");
    }
    auto report = [&](const QJsonObject &result) {
        return !parser.isSet("report") || MarkdownFileIO::writeFile(parser.value("report"), QString::fromUtf8(QJsonDocument(result).toJson()));
    };
    if (parser.isSet("import-directory")) {
        MarkdownManager manager(dataPath); const auto result = manager.importDirectory(parser.value("import-directory"));
        QJsonArray imported, failures;
        for (const auto &path : result.imported) imported.append(path);
        for (auto it = result.failures.cbegin(); it != result.failures.cend(); ++it)
            failures.append(QJsonObject{{"document", it.key()}, {"error", it.value()}});
        if (!report({{"success", result.success()}, {"imported", imported}, {"imageReferences", result.imageReferences},
            {"failures", failures}, {"error", result.error}, {"dataRoot", manager.rootPath()}, {"photoRoot", ImageStorage(dataPath).photoRoot()}})) return 3;
        return result.success() ? 0 : 2;
    }
    if (parser.isSet("export-document")) {
        MarkdownManager manager(dataPath); QString error;
        if (!manager.scan(&error) || !manager.loadNode(parser.value("export-document")) || !parser.isSet("export-to")) {
            report({{"success", false}, {"error", error.isEmpty() ? "Document or export destination missing" : error}}); return 2;
        }
        auto *node = manager.loadNode(parser.value("export-document")); MarkdownExportJob job;
        QObject::connect(&job, &MarkdownExportJob::finished, &application, [&](bool success, const QString &message) {
            const bool saved = report({{"success", success}, {"message", message}}); application.exit(saved ? success ? 0 : 2 : 3);
        });
        QTimer::singleShot(0, &application, [&] { job.start(node->content(), node->filePath(), parser.value("export-to"), parser.isSet("with-images")); });
        return application.exec();
    }
    MainWindow window(dataPath); window.show();
    if (parser.isSet("open-document")) window.openDocument(parser.value("open-document"));
    if (parser.isSet("source-view")) window.setSourceMode(true);
    if (parser.isSet("relations")) window.setRelationsVisible(true);
    if (parser.isSet("knowledge-tree")) QTimer::singleShot(0, &window, &MainWindow::openKnowledgeTree);
    if (parser.isSet("settings")) QTimer::singleShot(0, &window, [&window] { window.findChild<QAction *>("markdownSettingsAction")->trigger(); });
    if (parser.isSet("image-settings")) QTimer::singleShot(0, &window, [&window] { window.findChild<QAction *>("imageSettingsAction")->trigger(); });
    if (parser.isSet("smoke-test") || parser.isSet("screenshot")) {
        QTimer::singleShot(700, &application, [&] {
            QWidget *capture = QApplication::activeModalWidget(); if (!capture) capture = &window;
            if (parser.isSet("screenshot") && !capture->grab().save(parser.value("screenshot"))) application.exit(2);
            else application.quit();
        });
    }
    return application.exec();
}
