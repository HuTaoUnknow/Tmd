#pragma once
#include <QObject>
#include <QFont>
#include <QRectF>
#include <QVector>
class QTextEdit;
class QToolButton;
class QPainter;
class CodeBlockDecoration : public QObject {
    Q_OBJECT
public:
    explicit CodeBlockDecoration(QTextEdit *edit);
    void setFont(const QFont &font) { m_font = font; }
    qreal headerHeight() const;
    void paintBackground(QPainter &painter);
    void paintHeaders(QPainter &painter);
signals:
    void copyRequested(int codeIndex, int displayPosition);
private:
    struct Panel { QRectF area, header; QString label; int codeIndex, position; };
    void sync();
    QTextEdit *m_edit;
    QFont m_font;
    QVector<Panel> m_panels;
    QVector<QToolButton *> m_buttons;
};
