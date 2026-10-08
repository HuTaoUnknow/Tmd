#pragma once
#include "core/knowledge/KnowledgeIndex.h"
#include "ui/knowledge/KnowledgeTreeLayout.h"
#include <QGraphicsView>
#include <QMap>

class MarkdownManager;
class KnowledgeNodeWidget;
class QGraphicsPathItem;
class QGraphicsRectItem;
class QSpinBox;
struct KnowledgeDropPreview {
    bool valid = false;
    QString source, target, message;
    NodeRelationType type = NodeRelationType::Next;
};
class KnowledgeTreeCanvas : public QGraphicsView {
    Q_OBJECT
public:
    explicit KnowledgeTreeCanvas(QWidget *parent = nullptr);
    static QString mimeType() { return QStringLiteral("application/x-tree-md-document"); }
    void setGraph(MarkdownManager *manager, const QString &root);
    void updateTitle(const QString &path, const QString &title);
    void setReadOnly(bool readOnly);
    int visibleNodeCount() const { return m_index.paths.size(); }
    const KnowledgeIndexResult &index() const { return m_index; }
    QRectF nodeRect(const QString &path) const;
    KnowledgeDropPreview preview() const { return m_preview; }
    QString selectedNode() const { return m_selectedNode; }
    bool hasSelectedEdge() const { return m_selectedEdge >= 0; }
    int zoomPercent() const { return m_zoomPercent; }
    void setZoomPercent(int percent);
    QSpinBox *createZoomControl(QWidget *parent);
    void restoreView();
    void centerCurrent();
    void removeSelectedRelation();
signals:
    void nodeActivated(const QString &path);
    void relationRequested(const QString &source, const QString &target, NodeRelationType type);
    void relationRemoveRequested(const QString &source, const QString &target);
    void rootRequested(const QString &path);
    void selectionChanged();
    void previewChanged(const QString &message);
    void zoomPercentChanged(int percent);
protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void showEvent(QShowEvent *event) override;
private:
    void applyZoomPercent(int percent);
    struct EdgeVisual { KnowledgeEdge edge; QGraphicsPathItem *line; };
    KnowledgeNodeWidget *cardAt(QPointF point) const;
    KnowledgeNodeWidget *nearestCard(QPointF point, const QString &exclude) const;
    void selectNode(const QString &path);
    void selectEdge(int index);
    void previewExternal(const QString &path, QPointF point);
    void previewConnection(QPointF point);
    void showPreview(const QString &source, const QString &target, NodeRelationType type, const QRectF &sourceRect, const QRectF &targetRect, const QString &message);
    void clearPreview();
    void commitPreview();
    quint64 m_graphRevision = ~quint64(0);
    MarkdownManager *m_manager = nullptr;
    KnowledgeIndexResult m_index;
    KnowledgeTreeLayoutResult m_layout;
    QByteArray m_layoutSignature;
    QMap<QString, KnowledgeNodeWidget *> m_cards;
    QList<EdgeVisual> m_edges;
    QList<QGraphicsItem *> m_previewItems;
    KnowledgeDropPreview m_preview;
    QString m_selectedNode, m_dragSource;
    int m_selectedEdge = -1, m_port = -1;
    QPoint m_pressPosition;
    bool m_dragging = false;
    bool m_readOnly = false;
    int m_zoomPercent = 60;
    QString m_pressedPath;
};
