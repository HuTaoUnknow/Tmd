#include "ui/MainWindow.h"
#include "core/library/MarkdownManager.h"
#include "core/library/MarkdownFileIO.h"
#include "core/library/LibraryPathPolicy.h"
#include "ui/document/MarkdownEditor.h"
#include "ui/document/MarkdownDocumentView.h"
#include "ui/library/DocumentSidebar.h"
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QStatusBar>
#include <QTimer>
#include <QElapsedTimer>
#include <QDebug>

bool MainWindow::openFile(const QString &input) {
    const QUrl url(input);
    const auto path = MarkdownFileIO::normalizedPath(url.isLocalFile() ? url.toLocalFile() : input);
    if (!QFileInfo(path).isFile() || QFileInfo(path).suffix().compare("md",Qt::CaseInsensitive) != 0) {
        showError(QStringLiteral("无法打开 MD 文件：%1").arg(path)); return false;
    }
    if (!LibraryPathPolicy(m_manager->rootPath()).document(path).isEmpty()) {
        QString error;
        if (!m_manager->loadNode(path) && !m_manager->scan(&error)) { showError(error); return false; }
        return openDocument(path);
    }
    QMessageBox prompt(QMessageBox::Question, QStringLiteral("备份文档"),
        QStringLiteral("是否将这份文档备份到知识库根目录？"), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, this);
    prompt.setObjectName("externalDocumentBackupPrompt"); prompt.setTextFormat(Qt::PlainText);
    prompt.setInformativeText(path + QStringLiteral("\n\n是：复制到知识库根目录，原文件保留。\n否：直接阅读和编辑原文件，Ctrl+S 保存到原地址。"));
    prompt.button(QMessageBox::Yes)->setText(QStringLiteral("是，备份"));
    prompt.button(QMessageBox::No)->setText(QStringLiteral("否，直接打开"));
    prompt.button(QMessageBox::Cancel)->setText(QStringLiteral("取消"));
    prompt.setDefaultButton(QMessageBox::No);
    const auto choice = prompt.exec();
    if (choice == QMessageBox::Yes) return backupExternal(path);
    if (choice != QMessageBox::No) return false;
    QString error;
    if (!m_manager->openExternalDocument(path, &error)) { showError(error); return false; }
    return openDocument(path);
}
bool MainWindow::backupExternal(const QString &path) {
    QString error, saved;
    { const QScopedValueRollback<bool> busy(m_busy, true); saved = m_manager->backupExternalDocument(path, &error); }
    if (saved.isEmpty()) { showError(error); return false; }
    const bool opened = openDocument(saved);
    if (opened) statusBar()->showMessage(QStringLiteral("已备份到知识库根目录：%1").arg(saved), 6000);
    return opened;
}
void MainWindow::forgetExternal(const QString &path) {
    auto *node = m_manager->loadNode(path); if (!node || !node->isExternalDocument()) return;
    QElapsedTimer removalTime; removalTime.start();
    qInfo() << "External record removal started:" << path << "missing:" << node->isSourceMissing();
    bool discard = false;
    if (node->isModified()) {
        const auto choice = QMessageBox::question(this, QStringLiteral("移除外部文档记录"),
            QStringLiteral("此文档尚未保存，是否保存后移除记录？原文件不会删除。"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
        if (choice == QMessageBox::Cancel) return;
        QString error;
        if (choice == QMessageBox::Save && !m_manager->saveDocument(path, &error)) { showError(error); return; }
        discard = choice == QMessageBox::Discard;
    }
    QString error; const bool current = path == m_current;
    bool removed = false;
    {
        // Do not refresh the current document between removing its model node
        // and selecting the replacement document in finishRemoval.
        const QScopedValueRollback<bool> busy(m_busy, true);
        removed = m_manager->forgetExternalDocument(path, &error, discard);
    }
    if (!removed) { showError(error); return; }
    finishRemoval(current);
    qInfo() << "External record removal completed in" << removalTime.elapsed() << "ms";
}
bool MainWindow::eventFilter(QObject *object, QEvent *event) {
    const QMimeData *mime = nullptr;
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove)
        mime = static_cast<QDragMoveEvent *>(event)->mimeData();
    else if (event->type() == QEvent::Drop) mime = static_cast<QDropEvent *>(event)->mimeData();
    QStringList documents;
    if (mime) for (const auto &url : mime->urls())
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).suffix().compare("md",Qt::CaseInsensitive) == 0) documents.append(url.toLocalFile());
    if (!documents.isEmpty()) {
        if (event->type() == QEvent::Drop) {
            static_cast<QDropEvent *>(event)->acceptProposedAction();
            QTimer::singleShot(0, this, [this, documents] { for (const auto &path : documents) openFile(path); });
        } else static_cast<QDragMoveEvent *>(event)->acceptProposedAction();
        return true;
    }
    return QMainWindow::eventFilter(object,event);
}
