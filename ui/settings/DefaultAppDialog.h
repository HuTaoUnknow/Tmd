#pragma once
#include "core/platform/FileAssociation.h"
#include <QDialog>
#include <functional>

class QLabel;
class QLineEdit;
class QPushButton;

// Settings → 设为默认打开方式. Registers Tmd for .md under HKEY_CURRENT_USER
// and hands the final choice to Windows, which owns the default-app decision.
class DefaultAppDialog : public QDialog {
    Q_OBJECT
public:
    explicit DefaultAppDialog(const QString &currentDocument, QWidget *parent = nullptr,
                              const QString &applicationFilePath = {},
                              const QString &softwareRoot = QString::fromLatin1(FileAssociation::RealSoftwareRoot));
    // Replaceable so tests never open Windows Settings or the Open-with picker.
    std::function<bool(QString *)> openSystemSettings;
    std::function<bool(const QString &, QString *)> openWithPicker;
    std::function<QString()> userChoice;
    const FileAssociation::Launch &launch() const { return m_launch; }
    void refreshStatus();
    bool registerAndOpenSettings();
    bool chooseWithOpenWith();
    bool removeRegistration();
private:
    void showStatus(const QString &text, bool error);
    FileAssociation::Launch m_launch;
    QString m_document, m_root;
    QLabel *m_registration = nullptr, *m_choice = nullptr, *m_message = nullptr;
    QLineEdit *m_command = nullptr;
    QPushButton *m_openWith = nullptr, *m_remove = nullptr;
};
