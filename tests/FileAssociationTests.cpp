#include "core/platform/FileAssociation.h"
#include "ui/settings/DefaultAppDialog.h"
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QUuid>
#include <QtCore/qscopeguard.h>
#include <QtTest>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
bool touch(const QString &path) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path); return file.open(QIODevice::WriteOnly) && file.write("x") == 1;
}
QString debugExecutable(const QString &project) { return QDir(project).filePath("build/Desktop_Qt_6_11_1_MinGW_64_bit_Debug/tree_md.exe"); }
QString native(const QString &path) { return QDir::toNativeSeparators(path); }
#ifdef Q_OS_WIN
// Some sandboxes and security tools deny freshly built executables any HKCU write;
// skip honestly instead of reporting a product failure there.
bool isolatedHiveWritable(const QString &root) {
    HKEY handle = nullptr;
    const LONG code = RegCreateKeyExW(HKEY_CURRENT_USER, reinterpret_cast<const wchar_t *>(root.utf16()), 0, nullptr,
                                      REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &handle, nullptr);
    if (handle) RegCloseKey(handle);
    return code != ERROR_ACCESS_DENIED;
}
#define TMD_REQUIRE_WRITABLE_HIVE(root) \
    if (!isolatedHiveWritable(root)) QSKIP("HKEY_CURRENT_USER writes are denied for this process (sandbox or security software).")
#endif
QString isolatedRoot() { return QStringLiteral("Software\\TmdAssociationTest-") + QUuid::createUuid().toString(QUuid::Id128); }
void removeIsolatedRoot(const QString &root) {
    QSettings software(QStringLiteral("HKEY_CURRENT_USER\\Software"), QSettings::NativeFormat);
    software.remove(root.section('\\', 1)); software.sync();
}
}

class FileAssociationTests : public QObject {
    Q_OBJECT
private slots:
    void debugBuildsUseTheDesktopLauncher() {
        QTemporaryDir project; QVERIFY(project.isValid());
        const QString exe = debugExecutable(project.path());
        QVERIFY(touch(exe)); QVERIFY(touch(project.filePath("scripts/Start-Debug.ps1"))); QVERIFY(touch(project.filePath("resources/tmd.ico")));
        const auto launch = FileAssociation::launchFor(exe);
        QCOMPARE(launch.mode, FileAssociation::Mode::DebugLauncher);
        QCOMPARE(launch.launcher, native(project.filePath("scripts/Start-Debug.ps1")));
        QVERIFY(launch.command.startsWith('"')); QVERIFY(launch.command.contains(QStringLiteral("powershell.exe\" -NoLogo -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File \"")));
        QVERIFY(launch.command.endsWith(QStringLiteral("Start-Debug.ps1\" \"%1\"")));
        QCOMPARE(launch.icon, native(project.filePath("resources/tmd.ico")) + QStringLiteral(",0"));
        QVERIFY(FileAssociation::modeText(launch.mode).contains(QStringLiteral("Start-Debug.ps1")));
    }
    void releaseBuildsOpenTheExecutableDirectly() {
        QTemporaryDir root; QVERIFY(root.isValid());
        const QString portable = root.filePath(QStringLiteral("便携 Tmd/tree_md.exe")); QVERIFY(touch(portable));
        auto launch = FileAssociation::launchFor(portable);
        QCOMPARE(launch.mode, FileAssociation::Mode::Portable);
        QCOMPARE(launch.command, QStringLiteral("\"%1\" \"%2\"").arg(native(portable), QStringLiteral("%1")));
        QCOMPARE(launch.icon, native(portable) + QStringLiteral(",0"));
        QVERIFY(touch(root.filePath(QStringLiteral("便携 Tmd/tmd-installation.ini"))));
        QCOMPARE(FileAssociation::launchFor(portable).mode, FileAssociation::Mode::Installed);
        // An installed copy wins even when it sits in a Debug-shaped directory.
        QTemporaryDir project; const QString exe = debugExecutable(project.path());
        QVERIFY(touch(exe)); QVERIFY(touch(project.filePath("scripts/Start-Debug.ps1")));
        QVERIFY(touch(QFileInfo(exe).absoluteDir().filePath("tmd-installation.ini")));
        launch = FileAssociation::launchFor(exe);
        QCOMPARE(launch.mode, FileAssociation::Mode::Installed); QVERIFY(!launch.command.contains("powershell"));
    }
    void entriesDescribeProgIdCapabilitiesAndOpenWith() {
        FileAssociation::Launch launch; launch.command = "\"C:\\Tmd\\tree_md.exe\" \"%1\""; launch.icon = "C:\\Tmd\\tree_md.exe,0";
        const auto entries = FileAssociation::entries(launch, "Software\\Probe");
        auto value = [&](const QString &key, const QString &name) {
            for (const auto &entry : entries) if (entry.key == key && entry.name == name) return entry.value;
            return QStringLiteral("<missing>");
        };
        QCOMPARE(value("Software\\Probe\\Classes\\Tmd.Markdown\\shell\\open\\command", {}), launch.command);
        QCOMPARE(value("Software\\Probe\\Classes\\Applications\\tree_md.exe\\shell\\open\\command", {}), launch.command);
        QCOMPARE(value("Software\\Probe\\Classes\\.md\\OpenWithProgids", "Tmd.Markdown"), QString());
        QCOMPARE(value("Software\\Probe\\Tmd\\Capabilities\\FileAssociations", ".md"), QStringLiteral("Tmd.Markdown"));
        QCOMPARE(value("Software\\Probe\\RegisteredApplications", "Tmd"), QStringLiteral("Software\\Probe\\Tmd\\Capabilities"));
        for (const auto &entry : entries) QVERIFY2(entry.key.startsWith("Software\\Probe\\"), qPrintable(entry.key));
        QVERIFY(FileAssociation::isTmdChoice("Tmd.Markdown")); QVERIFY(FileAssociation::isTmdChoice("Applications\\tree_md.exe"));
        QVERIFY(!FileAssociation::isTmdChoice("VSCode.md"));
    }
#ifdef Q_OS_WIN
    void isolatedRegistryRoundTrip() {
        const QString root = isolatedRoot(); const auto cleanup = qScopeGuard([&] { removeIsolatedRoot(root); });
        TMD_REQUIRE_WRITABLE_HIVE(root);
        QTemporaryDir dir; const QString exe = dir.filePath(QStringLiteral("中文 目录/tree_md.exe")); QVERIFY(touch(exe));
        const auto launch = FileAssociation::launchFor(exe); QString error;
        QCOMPARE(FileAssociation::registeredCommand(root), QString());
        QVERIFY2(FileAssociation::registerForCurrentUser(launch, &error, root), qPrintable(error));
        QCOMPARE(FileAssociation::registeredCommand(root), launch.command);
        QSettings hive(QStringLiteral("HKEY_CURRENT_USER\\") + root, QSettings::NativeFormat);
        QCOMPARE(hive.value("RegisteredApplications/Tmd").toString(), root + QStringLiteral("\\Tmd\\Capabilities"));
        QCOMPARE(hive.value("Tmd/Capabilities/FileAssociations/.md").toString(), QStringLiteral("Tmd.Markdown"));
        QVERIFY(hive.childKeys().isEmpty());
        hive.beginGroup("Classes/.md/OpenWithProgids"); QVERIFY(hive.childKeys().contains("Tmd.Markdown")); hive.endGroup();
        QCOMPARE(hive.value("Classes/Tmd.Markdown/FriendlyTypeName").toString(), QStringLiteral("Markdown 文档"));
        QVERIFY2(FileAssociation::unregisterForCurrentUser(&error, root), qPrintable(error));
        QCOMPARE(FileAssociation::registeredCommand(root), QString());
        hive.sync();
        QVERIFY(!hive.contains("RegisteredApplications/Tmd")); QVERIFY(!hive.childGroups().contains("Tmd"));
        hive.beginGroup("Classes/.md/OpenWithProgids"); QVERIFY(!hive.childKeys().contains("Tmd.Markdown")); hive.endGroup();
        QVERIFY2(FileAssociation::unregisterForCurrentUser(&error, root), qPrintable(error));  // idempotent
    }
    void dialogRegistersAndDelegatesToWindows() {
        const QString root = isolatedRoot(); const auto cleanup = qScopeGuard([&] { removeIsolatedRoot(root); });
        TMD_REQUIRE_WRITABLE_HIVE(root);
        QTemporaryDir project; const QString exe = debugExecutable(project.path());
        QVERIFY(touch(exe)); QVERIFY(touch(project.filePath("scripts/Start-Debug.ps1")));
        DefaultAppDialog dialog({}, nullptr, exe, root); int opened = 0; QString picked;
        dialog.openSystemSettings = [&](QString *) { ++opened; return true; };
        dialog.openWithPicker = [&](const QString &file, QString *) { picked = file; return true; };
        dialog.userChoice = [] { return QStringLiteral("Tmd.Markdown"); };
        QCOMPARE(dialog.windowTitle(), QStringLiteral("设为默认打开方式"));
        QCOMPARE(dialog.findChild<QLineEdit *>("defaultAppCommand")->text(), dialog.launch().command);
        QVERIFY(dialog.findChild<QLabel *>("defaultAppDebugNote"));
        QVERIFY(!dialog.findChild<QPushButton *>("openWithDefaultAppButton")->isEnabled());
        QVERIFY(!dialog.findChild<QPushButton *>("removeDefaultAppButton")->isEnabled());
        QCOMPARE(dialog.findChild<QLabel *>("defaultAppRegistration")->text(), QStringLiteral("尚未登记"));
        dialog.findChild<QPushButton *>("registerDefaultAppButton")->click();
        QCOMPARE(opened, 1); QCOMPARE(FileAssociation::registeredCommand(root), dialog.launch().command);
        QCOMPARE(dialog.findChild<QLabel *>("defaultAppRegistration")->text(), QStringLiteral("已登记为当前程序"));
        QCOMPARE(dialog.findChild<QLabel *>("defaultAppChoice")->text(), QStringLiteral("Tmd ✓"));
        QVERIFY(dialog.findChild<QLabel *>("defaultAppMessage")->text().contains(QStringLiteral("默认应用")));
        dialog.findChild<QPushButton *>("removeDefaultAppButton")->click();
        QCOMPARE(FileAssociation::registeredCommand(root), QString());
        QCOMPARE(dialog.findChild<QLabel *>("defaultAppRegistration")->text(), QStringLiteral("尚未登记"));
        QVERIFY(picked.isEmpty());
        const QString document = project.filePath(QStringLiteral("笔记.md")); QVERIFY(touch(document));
        DefaultAppDialog withDocument(document, nullptr, exe, root);
        withDocument.openWithPicker = [&](const QString &file, QString *) { picked = file; return true; };
        withDocument.userChoice = [] { return QStringLiteral("VSCode.md"); };
        QVERIFY(withDocument.findChild<QPushButton *>("openWithDefaultAppButton")->isEnabled());
        withDocument.findChild<QPushButton *>("openWithDefaultAppButton")->click();
        QCOMPARE(picked, document); QCOMPARE(FileAssociation::registeredCommand(root), withDocument.launch().command);
        QCOMPARE(withDocument.findChild<QLabel *>("defaultAppChoice")->text(), QStringLiteral("其他程序（VSCode.md）"));
    }
#endif
};
QTEST_MAIN(FileAssociationTests)
#include "FileAssociationTests.moc"
