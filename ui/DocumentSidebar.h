#pragma once
#include <QWidget>
#include <QList>
class MarkdownNode;
class QLineEdit;
class QTreeWidget;
class QStackedWidget;
class QToolButton;
class QDialog;
class QListWidget;
class QLabel;

class DocumentSidebar : public QWidget {
    Q_OBJECT
public:
    enum class DisplayMode { Expanded, Compact, Hidden };
    static constexpr int compactWidth = 64, compactThreshold = 180, hiddenThreshold = 36;
    explicit DocumentSidebar(QWidget *parent = nullptr);
    void setDocuments(const QList<MarkdownNode *> &nodes);
    void selectPath(const QString &path);
    DisplayMode displayMode() const { return isHidden() ? DisplayMode::Hidden : m_mode; }
    void openQuickSearch();
    QSize minimumSizeHint() const override { return {0, 0}; }
    QSize sizeHint() const override { return {245, 400}; }
signals:
    void documentActivated(const QString &path);
protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    void filter();
    void updateDisplayMode();
    void refreshQuickResults();
    void activateQuickResult();
    struct SearchEntry { QString path, title; };
    QList<SearchEntry> m_entries;
    QString m_current;
    QStackedWidget *m_pages;
    QToolButton *m_quickButton;
    QDialog *m_quickPopup = nullptr;
    QLineEdit *m_quickSearch = nullptr;
    QListWidget *m_quickResults = nullptr;
    QLabel *m_quickCount = nullptr;
    DisplayMode m_mode = DisplayMode::Expanded;
    QLineEdit *m_search;
    QTreeWidget *m_tree;
};
