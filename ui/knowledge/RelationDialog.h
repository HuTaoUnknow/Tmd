#pragma once
#include <QDialog>
class MarkdownManager;
class KnowledgeTreeCanvas;
class QTreeWidget;
class QLineEdit;
class QLabel;

class RelationDialog : public QDialog {
    Q_OBJECT
public:
    RelationDialog(MarkdownManager *manager, QString source, QWidget *parent = nullptr);
    void setCurrentNode(const QString &path);
private:
    void refresh();
    void filter();
    MarkdownManager *m_manager;
    QString m_source;
    QTreeWidget *m_available;
    QLineEdit *m_search;
    KnowledgeTreeCanvas *m_canvas;
    QLabel *m_current, *m_count, *m_status;
};
