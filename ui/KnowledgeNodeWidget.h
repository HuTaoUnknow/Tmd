#pragma once
#include <QGraphicsObject>

class KnowledgeNodeWidget : public QGraphicsObject {
    Q_OBJECT
public:
    KnowledgeNodeWidget(QString path, QString title, QString role, bool current = false, int hierarchyLevel = 0, int nestedDepth = 0);
    static QSizeF cardSize(int hierarchyLevel, int nestedDepth, bool current = false);
    static qreal titlePointSize(int hierarchyLevel);
    int hierarchyLevel() const { return m_hierarchyLevel; }
    int nestedDepth() const { return m_nestedDepth; }
    void setCardMetrics(QSizeF size, qreal titleSize);
    qreal titleFontSize() const { return m_titleSize; }
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override;
    QString path() const { return m_path; }
    void setConnectionPorts(bool visible) { m_ports = visible; update(); }
signals:
    void activated(const QString &path);
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
private:
    QString m_path, m_title, m_role;
    int m_hierarchyLevel, m_nestedDepth;
    QSizeF m_cardSize;
    qreal m_titleSize;
    bool m_current, m_hover = false, m_ports = false;
};
