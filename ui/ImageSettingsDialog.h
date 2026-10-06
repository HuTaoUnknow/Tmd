#pragma once
#include "core/MarkdownSettings.h"
#include <QDialog>

class QLineEdit;
class QListWidget;
class QComboBox;
class QPlainTextEdit;
class QTabWidget;
class QLabel;
class ImageSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImageSettingsDialog(const QString &dataRoot, QWidget *parent = nullptr);
signals:
    void settingsApplied();
    void imageLinkRequested(const QString &url, const QString &name);
private:
    bool apply();
    bool saveLink();
    void refreshLinks(int selected = -1);
    void refreshLocation();
    MarkdownSettings m_values;
    QString m_dataRoot;
    QTabWidget *m_modes;
    QLineEdit *m_relative, *m_absolute, *m_linkName, *m_linkUrl;
    QLineEdit *m_endpoint, *m_fileField, *m_urlPath, *m_authName, *m_authPrefix, *m_token;
    QListWidget *m_links;
    QComboBox *m_authKind;
    QPlainTextEdit *m_fields;
    QLabel *m_relativeLocation, *m_absoluteLocation, *m_status;
};
