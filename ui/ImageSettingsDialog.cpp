#include "ImageSettingsDialog.h"
#include "core/ImageStorage.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QUrl>
#include <QButtonGroup>
#include <QToolButton>

ImageSettingsDialog::ImageSettingsDialog(const QString &dataRoot, QWidget *parent)
    : QDialog(parent), m_values(MarkdownSettingsStore::current()), m_dataRoot(dataRoot), m_modes(new QTabWidget(this)) {
    setObjectName("imageSettingsDialog"); setWindowTitle(QStringLiteral("图片保存方式")); resize(760, 680);
    auto *outer = new QVBoxLayout(this); m_modes->setObjectName("imageStorageModes");
    auto *modeRow = new QHBoxLayout; auto *modeButtons = new QButtonGroup(this); modeButtons->setExclusive(true);
    const QStringList modeNames = {QStringLiteral("相对路径"), QStringLiteral("绝对路径"), QStringLiteral("图床")};
    for (int i = 0; i < 3; ++i) {
        auto *button = new QToolButton(this); button->setObjectName(QString("imageStorageMode%1").arg(i)); button->setText(modeNames[i]);
        button->setCheckable(true); button->setMinimumHeight(40); button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setStyleSheet("QToolButton { background: #292e2b; border: 1px solid #424a45; border-radius: 6px; } QToolButton:checked { background: #314b3d; border-color: #90b9a0; }");
        modeButtons->addButton(button, i); modeRow->addWidget(button, 1);
    }
    outer->addLayout(modeRow); m_modes->tabBar()->hide(); m_modes->setDocumentMode(true); outer->addWidget(m_modes, 1);
    QWidget *relativePage = new QWidget(m_modes), *absolutePage = new QWidget(m_modes), *hostPage = new QWidget(m_modes);
    m_modes->addTab(relativePage, QStringLiteral("相对路径")); m_modes->addTab(absolutePage, QStringLiteral("绝对路径")); m_modes->addTab(hostPage, QStringLiteral("图床"));
    m_modes->setCurrentIndex(m_values.imageMode);
    modeButtons->button(m_values.imageMode)->setChecked(true);
    connect(modeButtons, &QButtonGroup::idClicked, m_modes, &QTabWidget::setCurrentIndex);
    connect(m_modes, &QTabWidget::currentChanged, this, [modeButtons](int index) { if (auto *button = modeButtons->button(index)) button->setChecked(true); });
    auto pathPage = [&](QWidget *page, bool relative) {
        auto *layout = new QVBoxLayout(page);
        auto *description = new QLabel(relative ? QStringLiteral("相对位置以 md_data 为基准。图片子目录与文档目录保持一致，有图片时才创建。现有图片保持原地址。")
            : QStringLiteral("指定目录后，新插入的图片会复制到该位置，并在 Markdown 中使用绝对路径。留空时，本地文件引用原位置，截图使用默认图片目录。"), page);
        description->setWordWrap(true); layout->addWidget(description);
        auto *form = new QFormLayout; auto *row = new QWidget(page); auto *line = new QLineEdit(row); auto *browse = new QPushButton(QStringLiteral("选择目录…"), row);
        auto *rowLayout = new QHBoxLayout(row); rowLayout->setContentsMargins(0, 0, 0, 0); rowLayout->addWidget(line, 1); rowLayout->addWidget(browse);
        auto *location = new QLabel(page); location->setWordWrap(true); form->addRow(relative ? QStringLiteral("相对位置") : QStringLiteral("保存位置"), row);
        form->addRow(QStringLiteral("实际位置"), location); layout->addLayout(form); layout->addStretch(1);
        if (relative) { m_relative = line; m_relativeLocation = location; line->setObjectName("relativeImageDirectory"); line->setText(m_values.relativePhotoPath); }
        else { m_absolute = line; m_absoluteLocation = location; line->setObjectName("absoluteImageDirectory"); line->setText(m_values.absolutePhotoPath); line->setPlaceholderText(QStringLiteral("留空：本地文件引用原位置")); }
        connect(browse, &QPushButton::clicked, this, [this, line, relative] {
            const QString initial = relative ? QDir(m_dataRoot).absoluteFilePath(line->text()) : line->text();
            const QString directory = QFileDialog::getExistingDirectory(this, QStringLiteral("选择图片目录"), initial);
            if (!directory.isEmpty()) line->setText(relative ? QDir(m_dataRoot).relativeFilePath(directory) : QDir::cleanPath(directory));
        });
    };
    pathPage(relativePage, true); pathPage(absolutePage, false);
    connect(m_relative, &QLineEdit::textChanged, this, &ImageSettingsDialog::refreshLocation);
    connect(m_absolute, &QLineEdit::textChanged, this, &ImageSettingsDialog::refreshLocation); refreshLocation();
    auto *hostLayout = new QVBoxLayout(hostPage); auto *scroll = new QScrollArea(hostPage); scroll->setWidgetResizable(true);
    auto *host = new QWidget(scroll); auto *hostContent = new QVBoxLayout(host); hostLayout->addWidget(scroll); scroll->setWidget(host);
    auto *linkGroup = new QGroupBox(QStringLiteral("图床图片链接管理"), host); auto *linkLayout = new QVBoxLayout(linkGroup); hostContent->addWidget(linkGroup);
    m_links = new QListWidget(linkGroup); m_links->setObjectName("hostedImageLinks"); m_links->setMinimumHeight(110); m_links->setMaximumHeight(160); linkLayout->addWidget(m_links);
    auto *linkForm = new QFormLayout; m_linkName = new QLineEdit(linkGroup); m_linkName->setObjectName("hostedImageName");
    m_linkUrl = new QLineEdit(linkGroup); m_linkUrl->setObjectName("hostedImageUrl"); m_linkUrl->setPlaceholderText("https://example.com/image.png");
    linkForm->addRow(QStringLiteral("名称"), m_linkName); linkForm->addRow(QStringLiteral("图片链接"), m_linkUrl); linkLayout->addLayout(linkForm);
    auto *linkButtons = new QHBoxLayout;
    auto button = [&](const QString &text, const char *name) { auto *b = new QPushButton(text, linkGroup); b->setObjectName(name); linkButtons->addWidget(b); return b; };
    auto *add = button(QStringLiteral("新增"), "newHostedImageLink"), *save = button(QStringLiteral("保存链接"), "saveHostedImageLink"),
        *remove = button(QStringLiteral("删除"), "deleteHostedImageLink"), *use = button(QStringLiteral("设为默认"), "defaultHostedImageLink"), *insert = button(QStringLiteral("插入链接"), "insertHostedImageLink");
    linkLayout->addLayout(linkButtons);
    connect(m_links, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= m_values.hostedLinks.size()) { m_linkName->clear(); m_linkUrl->clear(); return; }
        m_linkName->setText(m_values.hostedLinks[row].name); m_linkUrl->setText(m_values.hostedLinks[row].url);
    });
    connect(add, &QPushButton::clicked, this, [this] { m_links->setCurrentRow(-1); m_links->clearSelection(); m_linkName->clear(); m_linkUrl->clear(); m_linkUrl->setFocus(); });
    connect(save, &QPushButton::clicked, this, [this] { saveLink(); });
    connect(remove, &QPushButton::clicked, this, [this] {
        const int row = m_links->currentRow(); if (row < 0) return;
        m_values.hostedLinks.removeAt(row);
        if (m_values.defaultHostedLink == row) m_values.defaultHostedLink = m_values.hostedLinks.isEmpty() ? -1 : 0;
        else if (m_values.defaultHostedLink > row) --m_values.defaultHostedLink;
        refreshLinks(qMin(row, int(m_values.hostedLinks.size()) - 1));
    });
    connect(use, &QPushButton::clicked, this, [this] { if (saveLink()) { m_values.defaultHostedLink = m_links->currentRow(); refreshLinks(m_values.defaultHostedLink); } });
    connect(insert, &QPushButton::clicked, this, [this] {
        if (!saveLink() || !apply()) return;
        const auto &link = m_values.hostedLinks[m_links->currentRow()]; emit imageLinkRequested(link.url, link.name); accept();
    });
    auto *uploadGroup = new QGroupBox(QStringLiteral("自动上传接口"), host); auto *uploadForm = new QFormLayout(uploadGroup); hostContent->addWidget(uploadGroup);
    auto *explanation = new QLabel(QStringLiteral("填写接口后，在图床方式下粘贴或拖入图片会自动上传。使用 multipart/form-data 的 POST 接口；返回链接按 JSON 字段路径读取，例如 data.url。"), uploadGroup);
    explanation->setWordWrap(true); uploadForm->addRow(explanation);
    auto edit = [&](const QString &label, const QString &value, const char *name) { auto *line = new QLineEdit(value, uploadGroup); line->setObjectName(name); uploadForm->addRow(label, line); return line; };
    m_endpoint = edit(QStringLiteral("上传接口 URL"), m_values.uploadEndpoint, "uploadEndpoint"); m_endpoint->setPlaceholderText("https://example.com/api/upload");
    m_fileField = edit(QStringLiteral("图片文件字段"), m_values.uploadFileField, "uploadFileField");
    m_urlPath = edit(QStringLiteral("返回图片链接字段"), m_values.uploadUrlPath, "uploadUrlPath");
    m_authKind = new QComboBox(uploadGroup); m_authKind->setObjectName("uploadAuthKind");
    m_authKind->addItems({"无需鉴权", "HTTP 请求头", "表单字段", "查询参数"}); m_authKind->setCurrentIndex(m_values.uploadAuthKind); uploadForm->addRow(QStringLiteral("鉴权方式"), m_authKind);
    m_authName = edit(QStringLiteral("鉴权参数名称"), m_values.uploadAuthName, "uploadAuthName");
    m_authPrefix = edit(QStringLiteral("凭据前缀"), m_values.uploadAuthPrefix, "uploadAuthPrefix");
    m_token = edit(QStringLiteral("API Token / Key"), m_values.uploadToken, "uploadToken"); m_token->setEchoMode(QLineEdit::Password);
    m_fields = new QPlainTextEdit(uploadGroup); m_fields->setObjectName("uploadExtraFields"); m_fields->setMaximumHeight(90); m_fields->setPlaceholderText(QStringLiteral("可选，每行一个：参数名=参数值"));
    QStringList fields; for (auto it = m_values.uploadFields.cbegin(); it != m_values.uploadFields.cend(); ++it) fields.append(it.key() + '=' + it.value()); m_fields->setPlainText(fields.join('\n'));
    uploadForm->addRow(QStringLiteral("附加表单参数"), m_fields);
    auto updateAuth = [this] { const bool enabled = m_authKind->currentIndex() != 0; for (auto *line : {m_authName, m_authPrefix, m_token}) line->setEnabled(enabled); };
    connect(m_authKind, &QComboBox::currentIndexChanged, this, [updateAuth] { updateAuth(); }); updateAuth();
    auto *note = new QLabel(QStringLiteral("Token/Key 输入框的凭据使用 Windows 本机保护保存，不随“导出配置”转移。链接管理只维护记录，删除记录不会删除服务器图片或文档中的链接。"), host); note->setWordWrap(true); hostContent->addWidget(note); hostContent->addStretch(1);
    m_status = new QLabel(QStringLiteral("点击“应用”保存。修改图片位置只影响之后插入的图片。"), this); m_status->setWordWrap(true); m_status->setObjectName("imageSettingsStatus"); outer->addWidget(m_status);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this); outer->addWidget(buttons);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定")); buttons->button(QDialogButtonBox::Apply)->setText(QStringLiteral("应用")); buttons->button(QDialogButtonBox::Apply)->setObjectName("applyImageSettings"); buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this] { apply(); });
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { if (apply()) accept(); }); connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    refreshLinks(m_values.defaultHostedLink);
}
void ImageSettingsDialog::refreshLocation() {
    m_relativeLocation->setText(QDir::cleanPath(QDir(m_dataRoot).absoluteFilePath(m_relative->text())));
    m_absoluteLocation->setText(m_absolute->text().isEmpty() ? QStringLiteral("截图目录：%1").arg(m_relativeLocation->text()) : QDir::cleanPath(m_absolute->text()));
}
void ImageSettingsDialog::refreshLinks(int selected) {
    m_links->clear();
    for (int i = 0; i < m_values.hostedLinks.size(); ++i) {
        const auto &link = m_values.hostedLinks[i]; auto *item = new QListWidgetItem(link.name + (i == m_values.defaultHostedLink ? QStringLiteral("（默认）") : QString{}), m_links); item->setToolTip(link.url);
    }
    m_links->setCurrentRow(selected);
}
bool ImageSettingsDialog::saveLink() {
    const QString url = m_linkUrl->text().trimmed(); const QUrl address(url);
    QString name = m_linkName->text().trimmed(); if (name.isEmpty()) name = QFileInfo(address.path()).fileName(); if (name.isEmpty()) name = address.host();
    auto candidate = m_values; int row = m_links->currentRow();
    if (row < 0) { row = candidate.hostedLinks.size(); candidate.hostedLinks.append({name, url}); if (candidate.defaultHostedLink < 0) candidate.defaultHostedLink = row; }
    else candidate.hostedLinks[row] = {name, url};
    QString error; if (!MarkdownSettingsStore::validate(candidate, &error)) { QMessageBox::warning(this, QStringLiteral("无法保存图床链接"), error); return false; }
    m_values = candidate; refreshLinks(row); return true;
}
bool ImageSettingsDialog::apply() {
    if (!m_linkUrl->text().trimmed().isEmpty()) {
        const int row = m_links->currentRow();
        if (row < 0 || m_values.hostedLinks[row].url != m_linkUrl->text().trimmed() || m_values.hostedLinks[row].name != m_linkName->text().trimmed()) if (!saveLink()) return false;
    }
    auto settings = MarkdownSettingsStore::current(); settings.imageMode = m_modes->currentIndex();
    settings.relativePhotoPath = QDir::fromNativeSeparators(m_relative->text().trimmed()); settings.absolutePhotoPath = QDir::fromNativeSeparators(m_absolute->text().trimmed());
    settings.hostedLinks = m_values.hostedLinks; settings.defaultHostedLink = m_values.defaultHostedLink;
    settings.uploadEndpoint = m_endpoint->text().trimmed(); settings.uploadFileField = m_fileField->text().trimmed(); settings.uploadUrlPath = m_urlPath->text().trimmed();
    settings.uploadAuthKind = m_authKind->currentIndex(); settings.uploadAuthName = m_authName->text().trimmed(); settings.uploadAuthPrefix = m_authPrefix->text(); settings.uploadToken = m_token->text().trimmed();
    settings.uploadFields.clear();
    for (const auto &line : m_fields->toPlainText().split('\n')) {
        if (line.trimmed().isEmpty()) continue; const int equal = line.indexOf('='); const QString key = line.left(equal).trimmed();
        if (equal < 1 || settings.uploadFields.contains(key)) { QMessageBox::warning(this, QStringLiteral("附加参数无效"), QStringLiteral("每行使用参数名=参数值，参数名不能重复。")); return false; }
        settings.uploadFields[key] = line.mid(equal + 1);
    }
    QString error; if (!MarkdownSettingsStore::save(settings, &error)) { QMessageBox::warning(this, QStringLiteral("无法保存图片设置"), error); return false; }
    m_values = settings; emit settingsApplied(); m_status->setText(QStringLiteral("已保存并应用图片设置。")); return true;
}
