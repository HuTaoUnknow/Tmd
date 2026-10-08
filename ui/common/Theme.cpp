#include "ui/common/Theme.h"
#include "ui/common/AppIcon.h"
#include <QApplication>
#include <QEvent>
#include <QMenu>
#include <QPalette>
#include <QPainter>

namespace {
class MenuPopupTheme : public QObject {
public:
    explicit MenuPopupTheme(QObject *parent) : QObject(parent) {}
protected:
    bool eventFilter(QObject *object, QEvent *event) override {
        if (event->type() == QEvent::Polish) {
            auto *popup = qobject_cast<QWidget *>(object);
            if (popup && (qobject_cast<QMenu *>(popup) || popup->objectName() == "fenceLanguagePopup")) {
                // Windows needs a frameless, translucent surface for genuinely transparent corners.
                popup->setWindowFlag(Qt::FramelessWindowHint, true);
                popup->setAttribute(Qt::WA_TranslucentBackground);
            }
        } else if (event->type() == QEvent::Paint && object->objectName() == "fenceLanguagePopup") {
            // The item view paints its viewport; its translucent outer window
            // also needs an opaque menu surface between the rounded corners.
            if (auto *popup = qobject_cast<QWidget *>(object)) {
                QPainter painter(popup); painter.setRenderHint(QPainter::Antialiasing);
                painter.setPen(QPen(QColor("#333333"), 1)); painter.setBrush(Qt::black);
                painter.drawRoundedRect(QRectF(popup->rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
            }
        }
        return QObject::eventFilter(object, event);
    }
};
}

void applyTreeMdTheme(QApplication &application) {
    if (!application.findChild<QObject *>("treeMdMenuTheme")) {
        auto *menuTheme = new MenuPopupTheme(&application);
        menuTheme->setObjectName("treeMdMenuTheme"); application.installEventFilter(menuTheme);
    }
    application.setWindowIcon(treeMdIcon());
    application.setStyle("Fusion");
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#212121"));
    palette.setColor(QPalette::WindowText, QColor("#e7e7e7"));
    palette.setColor(QPalette::Base, QColor("#212121"));
    palette.setColor(QPalette::AlternateBase, QColor("#282828"));
    palette.setColor(QPalette::Text, QColor("#e7e7e7"));
    palette.setColor(QPalette::PlaceholderText, QColor("#808780"));
    palette.setColor(QPalette::Button, QColor("#2c2c2c"));
    palette.setColor(QPalette::ButtonText, QColor("#e7e7e7"));
    palette.setColor(QPalette::Highlight, QColor("#375347"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::ToolTipBase, QColor("#303a34"));
    palette.setColor(QPalette::ToolTipText, QColor("#ededed"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#707070"));
    application.setPalette(palette);
    application.setStyleSheet(QStringLiteral(R"(
        QMainWindow, QDialog { background: #212121; color: #e7e7e7; }
        QToolBar { background: #1b1b1b; border: none; spacing: 5px; padding: 8px 12px; }
        QToolButton { border: 1px solid transparent; border-radius: 7px; padding: 7px 10px; }
        QToolButton:hover, QPushButton:hover { background: #333b36; }
        QToolButton:checked { background: #31483d; border-color: #526b5d; }
        QToolButton#quickDocumentSearchButton { background: #28332c; border: 1px solid #526b5d; border-radius: 20px; padding: 0; }
        QToolButton#quickDocumentSearchButton:hover { background: #3a5848; }
        QToolButton#quickDocumentSearchButton:focus { border-color: #9bc8af; }
        QDialog#quickDocumentPopup { background: #212121; border: 1px solid #526357; border-radius: 9px; }
        QLabel#quickSearchTitle { font-size: 16px; font-weight: 600; color: #c4dfd0; }
        QLabel#quickDocumentCount { color: #a5ada8; }
        QListWidget#quickDocumentResults { background: #1c1c1c; border: none; padding: 4px; }
        QListWidget#quickDocumentResults::item { padding: 8px; border-radius: 5px; }
        QListWidget#quickDocumentResults::item:hover { background: #292d2a; }
        QListWidget#quickDocumentResults::item:selected { background: #304339; color: #eeeeee; }
        QFrame#documentModeFrame { background: #111613; border: 1px solid #506258; border-radius: 9px; }
        QFrame#documentModeFrame QToolButton { padding: 7px 16px; border-radius: 6px; color: #aab6ae; }
        QFrame#documentModeFrame QToolButton:checked { background: #3a5848; border-color: #749781; color: #ffffff; font-weight: 600; }
        QToolBar::separator { background: #3a3a3a; width: 1px; margin: 8px; }
        QWidget#documentSidebar { background: #171717; }
        QLabel#brand { font-size: 21px; font-weight: 600; color: #b4d1c3; padding: 4px 0 15px; }
        QLabel#brandIcon { padding: 0; }
        QLabel#documentTitle { font-size: 20px; font-weight: 700; color: #f4f4f4; padding: 4px 20px 10px; }
        QLabel#editorZoom { color: #bfc7c2; padding: 3px 12px; }
        QLabel#knowledgeTreeCurrent { font-size: 18px; font-weight: 600; color: #c4dfd0; }
        QLabel#availableDocumentsHeading { font-size: 15px; font-weight: 600; padding: 3px 0 6px; }
        QLabel#knowledgeTreeHint { color: #b7cfc0; padding: 8px 12px; background: #252e28; border-radius: 6px; }
        QLabel#notice { color: #ddbe83; background: #363126; padding: 9px; border-radius: 6px; }
        QTreeWidget { border: none; background: #1c1c1c; padding: 6px; outline: 0; }
        QTreeWidget#documentTree { background: #171717; }
        QTreeWidget::item { padding: 7px 4px; border-radius: 5px; }
        QTreeWidget::item:hover { background: #292d2a; }
        QTreeWidget::item:selected { background: #304339; color: #eeeeee; }
        QHeaderView::section { border: none; background: #1c1c1c; padding: 13px 8px; color: #a5ada8; }
        QTextEdit#markdownEditor { border: none; background: #212121; selection-background-color: #3b5c4b; }
        QPlainTextEdit, QListWidget { border: 1px solid #3b3b3b; border-radius: 7px; padding: 8px; }
        QLineEdit, QComboBox { background: #252525; border: 1px solid #3a3a3a; border-radius: 6px; padding: 8px; }
        QLineEdit:focus, QComboBox:focus { border-color: #7a9e8d; }
        QPushButton { background: #303732; border: 1px solid #48514b; border-radius: 6px; padding: 8px 13px; }
        QSplitter::handle { background: #303030; width: 2px; height: 2px; }
        QStatusBar { background: #1b1b1b; color: #a0a0a0; }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 1px; }
        QScrollBar:horizontal { background: transparent; height: 10px; margin: 1px; }
        QScrollBar::handle { background: #505752; border-radius: 4px; min-height: 24px; min-width: 24px; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
        QToolTip { background: #303a34; color: #ededed; border: 1px solid #526357; padding: 6px; }
        QTextBrowser#markdownDocument { border: none; background: #212121; selection-background-color: #3b5c4b; }
        QMenu { background: #000000; color: #e7e7e7; border: 1px solid #333333; border-radius: 8px; padding: 6px; }
        QMenu::item { padding: 9px 32px 9px 22px; border-radius: 4px; }
        QMenu::item:selected { background: #242424; }
        QMenu::separator { background: #333333; height: 1px; margin: 5px 8px; }
    )"));
}
