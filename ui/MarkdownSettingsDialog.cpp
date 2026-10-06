#include "MarkdownSettingsDialog.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QToolButton>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

MarkdownSettingsDialog::MarkdownSettingsDialog(QWidget *parent) : QDialog(parent), m_status(new QLabel(this)) {
    setObjectName("markdownSettingsDialog"); setWindowTitle(QStringLiteral("MD加载样式")); resize(680, 560);
    auto *outer = new QVBoxLayout(this);
    auto *description = new QLabel(QStringLiteral("设置文档排版、原文字体和代码提示。字号基于 100% 显示倍率，Ctrl+滚轮仍可缩放。"), this);
    description->setWordWrap(true); outer->addWidget(description);
    auto *scroll = new QScrollArea(this); scroll->setWidgetResizable(true);
    auto *content = new QWidget(scroll); auto *layout = new QVBoxLayout(content);
    std::array<QFormLayout *, 5> sections;
    const QStringList titles = {"字体与正文", "标题", "段落、分点与引用", "代码块与行内代码", "链接、表格与预加载"};
    for (int i = 0; i < 5; ++i) {
        auto *group = new QFrame(content); auto *groupLayout = new QVBoxLayout(group); groupLayout->setContentsMargins(0, 0, 0, 0);
        auto *header = new QToolButton(group); header->setObjectName(QString("markdownSettingsSection%1").arg(i));
        header->setText(titles[i]); header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); header->setArrowType(Qt::DownArrow);
        header->setCheckable(true); header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); header->setMinimumHeight(42);
        header->setStyleSheet("QToolButton { text-align: left; background: #292e2b; border: 1px solid #424a45; border-radius: 6px; padding: 6px 12px; font-weight: 600; }");
        auto *body = new QWidget(group); body->setObjectName(QString("markdownSettingsSectionBody%1").arg(i)); sections[i] = new QFormLayout(body);
        sections[i]->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow); groupLayout->addWidget(header); groupLayout->addWidget(body); body->hide();
        connect(header, &QToolButton::toggled, body, [header, body](bool expanded) { body->setVisible(expanded); header->setArrowType(expanded ? Qt::UpArrow : Qt::DownArrow); });
        layout->addWidget(group);
    }
    const QStringList fontLabels = {"首选字体（正文）", "次选字体（回退）", "代码字体（等宽）"};
    for (int i = 0; i < 3; ++i) {
        m_fonts[i] = new QFontComboBox(content); m_fonts[i]->setObjectName(QString("markdownSettingFont%1").arg(i));
        if (i == 2) m_fonts[i]->setFontFilters(QFontComboBox::MonospacedFonts);
        sections[0]->addRow(fontLabels[i], m_fonts[i]);
    }
    for (int i = 0; i < MarkdownSettings::NumberCount; ++i) {
        const auto &spec = markdownNumberSpecs()[i]; auto *spin = new QDoubleSpinBox(content);
        spin->setObjectName(QString("markdownSettingNumber%1").arg(i)); spin->setDecimals(spec.scale == 10 ? 1 : 0);
        spin->setRange(qreal(spec.minimum) / spec.scale, qreal(spec.maximum) / spec.scale);
        spin->setSuffix(spec.suffix); spin->setKeyboardTracking(false); m_numbers[i] = spin;
        sections[spec.section]->addRow(spec.label, spin);
    }
    for (int i = 0; i < MarkdownSettings::ColorCount; ++i) {
        auto *button = new QPushButton(content); button->setObjectName(QString("markdownSettingColor%1").arg(i));
        m_colors[i] = button; const auto &spec = markdownColorSpecs()[i]; sections[spec.section]->addRow(spec.label, button);
        connect(button, &QPushButton::clicked, this, [this, i] {
            const QColor chosen = QColorDialog::getColor(m_values.color(static_cast<MarkdownSettings::Color>(i)), this, markdownColorSpecs()[i].label);
            if (chosen.isValid()) { m_values.colors[i] = chosen.rgb(); refreshColor(i); }
        });
    }
    struct Toggle { int flag, section; const char *label; };
    const Toggle toggles[] = {
        {MarkdownSettings::BoldHeadings, 1, "标题加粗"}, {MarkdownSettings::HeadingRules, 1, "一级、二级标题下划线"},
        {MarkdownSettings::LanguageCompletion, 3, "三个反引号后的语言提示与补全"}, {MarkdownSettings::SyntaxColors, 3, "代码语法高亮"},
        {MarkdownSettings::CodeBorder, 3, "代码块边框"}, {MarkdownSettings::WrapCode, 3, "代码自动换行"},
        {MarkdownSettings::UnderlineLinks, 4, "链接下划线"}, {MarkdownSettings::StripedTables, 4, "表格交替行背景"},
        {MarkdownSettings::AutoRender, 4, "编辑时自动预加载排版"}
    };
    for (const auto &toggle : toggles) {
        auto *check = new QCheckBox(QString::fromUtf8(toggle.label), content);
        check->setObjectName(QString("markdownSettingFlag%1").arg(toggle.flag)); m_flags[toggle.flag] = check;
        sections[toggle.section]->addRow(check);
    }
    layout->addStretch(1); scroll->setWidget(content); outer->addWidget(scroll, 1);
    m_status->setWordWrap(true); m_status->setText(QStringLiteral("设置保存到：%1\n导入后可检查，点击“应用”保存。导出包含配置，不包含文档、图片或 Token/Key 凭据。").arg(MarkdownSettingsStore::storagePath()));
    outer->addWidget(m_status);
    auto *footer = new QHBoxLayout;
    auto *import = new QPushButton(QStringLiteral("导入配置…"), this); import->setObjectName("importMarkdownSettings");
    auto *exportButton = new QPushButton(QStringLiteral("导出配置…"), this); exportButton->setObjectName("exportMarkdownSettings");
    auto *defaults = new QPushButton(QStringLiteral("恢复默认"), this); defaults->setObjectName("resetMarkdownSettings");
    footer->addWidget(import); footer->addWidget(exportButton); footer->addWidget(defaults); footer->addStretch(1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定")); buttons->button(QDialogButtonBox::Apply)->setText(QStringLiteral("应用"));
    buttons->button(QDialogButtonBox::Apply)->setObjectName("applyMarkdownSettings"); buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    footer->addWidget(buttons); outer->addLayout(footer);
    connect(import, &QPushButton::clicked, this, &MarkdownSettingsDialog::importSettings);
    connect(exportButton, &QPushButton::clicked, this, &MarkdownSettingsDialog::exportSettings);
    connect(defaults, &QPushButton::clicked, this, [this] { setValues(MarkdownSettings{}); });
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this] { apply(); });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { if (apply()) accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    setValues(MarkdownSettingsStore::current());
    if (!MarkdownSettingsStore::loadError().isEmpty()) m_status->setText(MarkdownSettingsStore::loadError() + QStringLiteral(" 已使用默认设置，可应用后重新保存。"));
}
void MarkdownSettingsDialog::refreshColor(int index) {
    const QColor color = m_values.color(static_cast<MarkdownSettings::Color>(index));
    m_colors[index]->setText(color.name().toUpper());
    m_colors[index]->setStyleSheet(QString("background: %1; color: %2;").arg(color.name(), color.lightness() > 140 ? "#111111" : "#ffffff"));
}
void MarkdownSettingsDialog::setValues(const MarkdownSettings &settings) {
    m_values = settings;
    for (int i = 0; i < 3; ++i) m_fonts[i]->setCurrentText(settings.fonts[i]);
    for (int i = 0; i < MarkdownSettings::NumberCount; ++i) m_numbers[i]->setValue(settings.number(static_cast<MarkdownSettings::Number>(i)));
    for (int i = 0; i < MarkdownSettings::ColorCount; ++i) refreshColor(i);
    for (auto it = m_flags.cbegin(); it != m_flags.cend(); ++it) it.value()->setChecked(settings.flags & it.key());
}
MarkdownSettings MarkdownSettingsDialog::values() const {
    auto settings = m_values;
    for (int i = 0; i < 3; ++i) settings.fonts[i] = m_fonts[i]->currentText().trimmed();
    for (int i = 0; i < MarkdownSettings::NumberCount; ++i) settings.numbers[i] = qRound(m_numbers[i]->value() * markdownNumberSpecs()[i].scale);
    settings.flags = 0;
    for (auto it = m_flags.cbegin(); it != m_flags.cend(); ++it) if (it.value()->isChecked()) settings.flags |= it.key();
    return settings;
}
bool MarkdownSettingsDialog::apply() {
    QString error;
    if (!MarkdownSettingsStore::save(values(), &error)) { QMessageBox::warning(this, QStringLiteral("无法保存设置"), error); return false; }
    emit settingsApplied(); m_status->setText(QStringLiteral("已保存并应用：%1").arg(MarkdownSettingsStore::storagePath())); return true;
}
void MarkdownSettingsDialog::importSettings() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("导入配置"), {}, QStringLiteral("配置文本 (*.txt);;所有文件 (*)"));
    if (path.isEmpty()) return;
    MarkdownSettings settings; QString error;
    if (!MarkdownSettingsStore::read(path, settings, &error)) { QMessageBox::warning(this, QStringLiteral("无法导入设置"), error); return; }
    const auto &current = MarkdownSettingsStore::current();
    if (settings.uploadToken.isEmpty() && settings.uploadEndpoint == current.uploadEndpoint && settings.uploadAuthKind == current.uploadAuthKind
        && settings.uploadAuthName == current.uploadAuthName && settings.uploadAuthPrefix == current.uploadAuthPrefix) settings.uploadToken = current.uploadToken;
    setValues(settings); m_status->setText(QStringLiteral("已读取 %1，点击“应用”保存并使用。").arg(QFileInfo(path).fileName()));
}
void MarkdownSettingsDialog::exportSettings() {
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出配置"), "Tmd-settings.txt", QStringLiteral("配置文本 (*.txt)"));
    if (path.isEmpty()) return; if (QFileInfo(path).suffix().isEmpty()) path += ".txt";
    QString error;
    if (!MarkdownSettingsStore::write(path, values(), &error)) { QMessageBox::warning(this, QStringLiteral("无法导出设置"), error); return; }
    m_status->setText(QStringLiteral("已导出十六进制设置：%1").arg(path));
}
