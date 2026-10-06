#pragma once
#include <QDialog>
#include "core/MarkdownSettings.h"
#include <QMap>

class QFontComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;
class MarkdownSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit MarkdownSettingsDialog(QWidget *parent = nullptr);
signals:
    void settingsApplied();
private:
    void setValues(const MarkdownSettings &settings);
    MarkdownSettings values() const;
    bool apply();
    void importSettings();
    void exportSettings();
    void refreshColor(int index);
    MarkdownSettings m_values;
    std::array<QFontComboBox *, 3> m_fonts;
    std::array<QDoubleSpinBox *, MarkdownSettings::NumberCount> m_numbers;
    std::array<QPushButton *, MarkdownSettings::ColorCount> m_colors;
    QMap<int, QCheckBox *> m_flags;
    QLabel *m_status;
};
