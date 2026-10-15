#pragma once
#include <QTreeWidget>
#include <QPersistentModelIndex>
#include <QSet>

class QRubberBand;
class QTimer;

class DocumentFileTree : public QTreeWidget {
    Q_OBJECT
public:
    explicit DocumentFileTree(QWidget *parent = nullptr);
    QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override;
    void setLibraryRoot(const QString &root);
    void restoreDirectoryStates();
    void remapDirectoryState(const QString &from, const QString &to);
    void finishTransientExpansion();
    QRect textRect(QTreeWidgetItem *item) const;
signals:
    void moveRequested(const QString &path, bool directory, const QString &destinationDirectory);
    void moveDocumentsRequested(const QStringList &paths, const QString &destinationDirectory);
protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void startDrag(Qt::DropActions actions) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void updateGeometries() override;
private:
    struct Target { QStringList sources; QString directoryPath; bool directory = false, valid = false; QPersistentModelIndex index; };
    Target target(const QMimeData *mime, const QPoint &position) const;
    QString m_origin;
    Target m_preview;
    bool m_updatingGeometries = false;
    void saveDirectoryStates();
    void updateBoxSelection();
    QSet<QString> m_collapsed, m_autoExpanded, m_boxInitial;
    QString m_settingsKey;
    bool m_restoring = false, m_dragEligible = false, m_boxSelecting = false;
    QPoint m_boxOrigin, m_boxPosition;
    QRubberBand *m_rubberBand;
    QTimer *m_boxScrollTimer;
};
