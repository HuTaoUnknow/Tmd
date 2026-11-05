#pragma once
#include <QList>
#include <QString>

// Per-user ".md" registration for Windows. Windows 10/11 does not let a
// program silently become the default handler; Tmd registers itself under
// HKCU\Software\Classes and then lets the user confirm it in Windows Settings
// or the "Open with" picker.
namespace FileAssociation {
inline constexpr auto ProgId = "Tmd.Markdown";
inline constexpr auto ApplicationName = "Tmd";
inline constexpr auto RealSoftwareRoot = "Software";

enum class Mode { Installed, DebugLauncher, Portable };
struct Launch {
    Mode mode = Mode::Portable;
    QString executable;   // tree_md.exe that will finally run
    QString launcher;     // scripts/Start-Debug.ps1 for Debug builds
    QString command;      // registry shell\open\command, "%1" is the document
    QString icon;         // DefaultIcon value
};
struct Entry {
    QString key;          // relative to HKEY_CURRENT_USER
    QString name;         // empty means the default value
    QString value;
};

// Debug builds have no deployed Qt runtime beside tree_md.exe; they are
// started through scripts/Start-Debug.ps1, exactly like the desktop shortcut.
Launch launchFor(const QString &applicationFilePath);
QString modeText(Mode mode);
QList<Entry> entries(const Launch &launch, const QString &softwareRoot = RealSoftwareRoot);

bool registerForCurrentUser(const Launch &launch, QString *error = nullptr, const QString &softwareRoot = RealSoftwareRoot);
bool unregisterForCurrentUser(QString *error = nullptr, const QString &softwareRoot = RealSoftwareRoot);
// Command stored in <root>\Classes\Tmd.Markdown\shell\open\command, empty when absent.
QString registeredCommand(const QString &softwareRoot = RealSoftwareRoot);
// Read-only: the ProgId Windows currently uses for .md (UserChoice), empty when none.
QString currentUserChoice();
bool isTmdChoice(const QString &progId);

bool openDefaultAppsSettings(QString *error = nullptr);
// Windows "Open with" picker for one Markdown file; nativeParent may be 0.
bool showOpenWithDialog(const QString &file, quintptr nativeParent, QString *error = nullptr);
}
