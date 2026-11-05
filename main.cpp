#include "ui/MainWindow.h"
#include "ui/common/Theme.h"
#include "core/library/MarkdownManager.h"
#include "core/markdown/MarkdownImages.h"
#include "core/images/MarkdownExportJob.h"
#include "core/images/ImageStorage.h"
#include "core/library/MarkdownFileIO.h"
#include "core/library/DefaultLibrary.h"
#include "core/platform/ApplicationInstance.h"
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
#include <QQueue>
#include <memory>
#include <algorithm>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QtCore/qscopeguard.h>

namespace {
QString diagnosticLogPath;
QtMessageHandler previousMessageHandler = nullptr;
QMutex diagnosticLogMutex;
void writeDiagnosticMessage(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    {
        const QMutexLocker<QMutex> lock(&diagnosticLogMutex);
        QFile log(diagnosticLogPath);
        if (log.open(QIODevice::WriteOnly | QIODevice::Append)) {
            const auto line = QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
                + QStringLiteral(" [%1] ").arg(int(type)) + message + '\n';
            log.write(line.toUtf8());
        }
    }
    if (previousMessageHandler) previousMessageHandler(type, context, message);
}
}
int main(int argc, char *argv[]) {
    // The Debug launcher provides a per-process log file. Write from the app,
    // so diagnostics survive after the short-lived PowerShell launcher exits.
    diagnosticLogPath = qEnvironmentVariable("TMD_DIAGNOSTIC_LOG");
    if (QDir::isAbsolutePath(diagnosticLogPath)) {
        previousMessageHandler = qInstallMessageHandler(writeDiagnosticMessage);
        qInfo() << "Tmd startup" << TREE_MD_VERSION;
    }
    const auto resetDiagnosticHandler = qScopeGuard([] {
        if (QDir::isAbsolutePath(diagnosticLogPath)) qInstallMessageHandler(previousMessageHandler);
    });
    QApplication application(argc, argv);
    applyTreeMdTheme(application);
    application.setApplicationName("Tree MD"); application.setApplicationDisplayName("Tmd");
    application.setOrganizationName("TreeMD"); application.setApplicationVersion(TREE_MD_VERSION);
    QCommandLineParser parser; parser.addHelpOption(); parser.addVersionOption();
    parser.addPositionalArgument("documents", "Open MD files dropped onto the executable or shortcut", "[documents...]");
    parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsPositionalArguments);
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
    // Import/export and short-lived capture processes intentionally remain
    // independent. Normal Debug shortcut launches share an existing owner.
    std::unique_ptr<ApplicationInstance> instance;
    if (!parser.isSet("smoke-test") && !parser.isSet("screenshot")) {
        QJsonArray files;
        for (const auto &argument : parser.positionalArguments()) {
            const QUrl url(argument);
            files.append(MarkdownFileIO::normalizedPath(url.isLocalFile() ? url.toLocalFile() : argument));
        }
        QJsonObject request{{"files", files}, {"document", parser.value("open-document")}};
        for (const auto &flag : {"source-view", "relations", "knowledge-tree", "settings", "image-settings"})
            request.insert(flag, parser.isSet(flag));
        instance = std::make_unique<ApplicationInstance>(ApplicationInstance::identity(application.applicationFilePath(), dataPath));
        QString error; const auto result = instance->start(request, &error);
        if (result == ApplicationInstance::Result::Forwarded) return 0;
        if (result == ApplicationInstance::Result::Failed) { QMessageBox::warning(nullptr, QStringLiteral("Tmd"), error); return 2; }
    }
    if (!diagnosticLogPath.isEmpty()) qInfo() << "Constructing main window";
    MainWindow window(dataPath); window.show();
    if (!diagnosticLogPath.isEmpty()) qInfo() << "Main window ready";
    QQueue<QJsonObject> pendingLaunches;
    QTimer launchTimer; launchTimer.setSingleShot(true);
    auto activate = [&window] {
        if (window.isMinimized()) window.setWindowState(window.windowState() & ~Qt::WindowMinimized);
        window.raise(); window.activateWindow();
        if (auto *modal = QApplication::activeModalWidget()) { modal->raise(); modal->activateWindow(); }
    };
    bool openingLaunch = false;
    QObject::connect(&launchTimer, &QTimer::timeout, &window, [&] {
        if (openingLaunch) return;
        activate();
        if (QApplication::activeModalWidget()) { launchTimer.start(100); return; }
        openingLaunch = true;
        while (!pendingLaunches.isEmpty()) {
            const auto request = pendingLaunches.dequeue();
            for (const auto &file : request.value("files").toArray()) if (file.isString()) window.openFile(file.toString());
            if (!request.value("document").toString().isEmpty()) window.openDocument(request.value("document").toString());
            if (request.value("source-view").toBool()) window.setSourceMode(true);
            if (request.value("relations").toBool()) window.setRelationsVisible(true);
            if (request.value("knowledge-tree").toBool()) window.openKnowledgeTree();
            for (const auto &action : {qMakePair("settings", "markdownSettingsAction"), qMakePair("image-settings", "imageSettingsAction")})
                if (request.value(action.first).toBool()) window.findChild<QAction *>(action.second)->trigger();
            if (QApplication::activeModalWidget()) break;
        }
        openingLaunch = false;
        if (!pendingLaunches.isEmpty()) launchTimer.start(100);
    });
    if (instance) {
        auto receive = [&] {
            for (const auto &request : instance->takeRequests()) pendingLaunches.enqueue(request);
            if (!pendingLaunches.isEmpty()) { activate(); if (!openingLaunch) launchTimer.start(0); }
        };
        QObject::connect(instance.get(), &ApplicationInstance::requestsAvailable, &window, receive);
        receive();
    }
    const auto documents = parser.positionalArguments();
    if (!documents.isEmpty()) QTimer::singleShot(0, &window, [&window, documents] { for (const auto &path : documents) window.openFile(path); });
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
