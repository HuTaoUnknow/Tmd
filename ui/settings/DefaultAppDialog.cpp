#include "ui/settings/DefaultAppDialog.h"
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

DefaultAppDialog::DefaultAppDialog(const QString &currentDocument, QWidget *parent, const QString &applicationFilePath, const QString &softwareRoot)
    : QDialog(parent), m_launch(FileAssociation::launchFor(applicationFilePath.isEmpty() ? QCoreApplication::applicationFilePath() : applicationFilePath)),
      m_document(currentDocument), m_root(softwareRoot) {
    setObjectName("defaultAppDialog"); setWindowTitle(QStringLiteral("设为默认打开方式")); resize(720, 0);
    openSystemSettings = [](QString *error) { return FileAssociation::openDefaultAppsSettings(error); };
    openWithPicker = [this](const QString &file, QString *error) { return FileAssociation::showOpenWithDialog(file, quintptr(winId()), error); };
    userChoice = [] { return FileAssociation::currentUserChoice(); };
    auto *layout = new QVBoxLayout(this);
    auto *description = new QLabel(QStringLiteral(
        "让双击 .md 文件时直接用 Tmd 打开。<br>"
        "Windows 10/11 不允许程序自行修改默认应用，因此分两步：<br>"
        "① Tmd 先把自己登记为 .md 的可选程序（只写入当前用户的注册表 HKEY_CURRENT_USER，无需管理员权限）；<br>"
        "② 随后打开 Windows“默认应用”设置，请在其中找到 <b>Tmd</b>，把 <b>.md</b> 设为 Tmd。<br>"
        "之后在资源管理器中打开的 .md 会交给已运行的 Tmd 窗口；知识库外的文档会先询问是否备份到知识库根目录。"), this);
    description->setObjectName("defaultAppDescription"); description->setWordWrap(true); description->setTextFormat(Qt::RichText);
    layout->addWidget(description);
    auto *form = new QFormLayout;
    auto *mode = new QLabel(FileAssociation::modeText(m_launch.mode), this); mode->setObjectName("defaultAppMode"); mode->setWordWrap(true);
    m_command = new QLineEdit(m_launch.command, this); m_command->setObjectName("defaultAppCommand"); m_command->setReadOnly(true); m_command->setCursorPosition(0);
    m_registration = new QLabel(this); m_registration->setObjectName("defaultAppRegistration"); m_registration->setWordWrap(true);
    m_choice = new QLabel(this); m_choice->setObjectName("defaultAppChoice"); m_choice->setWordWrap(true);
    form->addRow(QStringLiteral("当前程序"), mode);
    form->addRow(QStringLiteral("打开命令"), m_command);
    form->addRow(QStringLiteral("登记状态"), m_registration);
    form->addRow(QStringLiteral(".md 默认程序"), m_choice);
    layout->addLayout(form);
    if (m_launch.mode == FileAssociation::Mode::DebugLauncher) {
        auto *debug = new QLabel(QStringLiteral("Debug 版通过 scripts\\Start-Debug.ps1 启动，请保持项目目录和 Debug 构建目录不变；移动项目后需要重新登记。"), this);
        debug->setObjectName("defaultAppDebugNote"); debug->setWordWrap(true); layout->addWidget(debug);
    }
    m_message = new QLabel(this); m_message->setObjectName("defaultAppMessage"); m_message->setWordWrap(true); layout->addWidget(m_message);
    auto *actions = new QHBoxLayout;
    auto *registerButton = new QPushButton(QStringLiteral("登记并打开系统设置"), this); registerButton->setObjectName("registerDefaultAppButton"); registerButton->setDefault(true);
    m_openWith = new QPushButton(QStringLiteral("用“打开方式”选择…"), this); m_openWith->setObjectName("openWithDefaultAppButton");
    m_openWith->setToolTip(QStringLiteral("对当前文档弹出 Windows“打开方式”窗口，选择 Tmd 并勾选“始终使用此应用”"));
    m_openWith->setEnabled(!m_document.isEmpty() && QFileInfo(m_document).isFile());
    m_remove = new QPushButton(QStringLiteral("移除登记"), this); m_remove->setObjectName("removeDefaultAppButton");
    auto *refresh = new QPushButton(QStringLiteral("刷新状态"), this); refresh->setObjectName("refreshDefaultAppButton");
    for (auto *button : {registerButton, m_openWith, m_remove, refresh}) actions->addWidget(button);
    actions->addStretch(1); layout->addLayout(actions);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(registerButton, &QPushButton::clicked, this, &DefaultAppDialog::registerAndOpenSettings);
    connect(m_openWith, &QPushButton::clicked, this, &DefaultAppDialog::chooseWithOpenWith);
    connect(m_remove, &QPushButton::clicked, this, &DefaultAppDialog::removeRegistration);
    connect(refresh, &QPushButton::clicked, this, &DefaultAppDialog::refreshStatus);
    refreshStatus();
}

void DefaultAppDialog::refreshStatus() {
    const QString registered = FileAssociation::registeredCommand(m_root);
    if (registered.isEmpty()) m_registration->setText(QStringLiteral("尚未登记"));
    else if (registered.compare(m_launch.command, Qt::CaseInsensitive) == 0) m_registration->setText(QStringLiteral("已登记为当前程序"));
    else m_registration->setText(QStringLiteral("已登记，但指向其他位置：%1（点击“登记并打开系统设置”改为当前程序）").arg(registered));
    m_remove->setEnabled(!registered.isEmpty());
    const QString choice = userChoice ? userChoice() : QString();
    if (choice.isEmpty()) m_choice->setText(QStringLiteral("Windows 尚未记录用户选择"));
    else if (FileAssociation::isTmdChoice(choice)) m_choice->setText(QStringLiteral("Tmd ✓"));
    else m_choice->setText(QStringLiteral("其他程序（%1）").arg(choice));
}

void DefaultAppDialog::showStatus(const QString &text, bool error) {
    m_message->setText(text);
    m_message->setStyleSheet(error ? QStringLiteral("color: #e58b8b;") : QString());
}

bool DefaultAppDialog::registerAndOpenSettings() {
    QString error;
    if (!FileAssociation::registerForCurrentUser(m_launch, &error, m_root)) { showStatus(error, true); refreshStatus(); return false; }
    refreshStatus();
    if (!openSystemSettings || !openSystemSettings(&error)) {
        showStatus(QStringLiteral("已登记 Tmd，但未能打开系统设置：%1").arg(error), true); return false;
    }
    showStatus(QStringLiteral("已登记 Tmd。请在打开的“默认应用”窗口中选择 Tmd，把 .md 设为 Tmd；完成后点击“刷新状态”。"), false);
    return true;
}

bool DefaultAppDialog::chooseWithOpenWith() {
    QString error;
    if (m_document.isEmpty() || !QFileInfo(m_document).isFile()) { showStatus(QStringLiteral("请先打开一篇 .md 文档。"), true); return false; }
    if (!FileAssociation::registerForCurrentUser(m_launch, &error, m_root)) { showStatus(error, true); refreshStatus(); return false; }
    if (!openWithPicker || !openWithPicker(m_document, &error)) { showStatus(error, true); refreshStatus(); return false; }
    refreshStatus();
    showStatus(QStringLiteral("如果在“打开方式”中选择了 Tmd 并勾选“始终使用此应用”，.md 将默认用 Tmd 打开。"), false);
    return true;
}

bool DefaultAppDialog::removeRegistration() {
    QString error;
    if (!FileAssociation::unregisterForCurrentUser(&error, m_root)) { showStatus(error, true); return false; }
    refreshStatus();
    showStatus(QStringLiteral("已移除 Tmd 的登记。若 Windows 仍把 .md 指向 Tmd，请在“默认应用”中改选其他程序。"), false);
    return true;
}
