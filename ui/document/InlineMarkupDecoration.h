#pragma once
#include "core/markdown/MarkdownSourceMap.h"
#include <QObject>
#include <QMap>
#include <QTextBlock>
#include <QTextCharFormat>
class QTextEdit;
class QPainter;
class QTimer;

// Glyphs and caret positions are presentation only; source is never duplicated.
class InlineMarkupDecoration : public QObject {
public:
    struct Hit {
        int displayPosition = -1, sourcePosition = -1;
        bool valid() const { return displayPosition >= 0; }
    };
    explicit InlineMarkupDecoration(QTextEdit *view);
    void clear();
    void translateEdit(int position, int removed, int added);
    void refresh(const MarkdownSourceMap &mapping);
    void setSourceSelection(int anchor, int caret, bool frozen);
    void paintCodeBackground(QPainter &painter, const MarkdownSourceMap &mapping) const;
    void paint(QPainter &painter) const;
    QStringList visibleMarkers() const;
    QStringList visibleBlockMarkers() const;
    bool hasBlockPrefix(int displayPosition) const;
    QRect caretRect(const QRect &native) const;
    Hit hitTest(const QPoint &point) const;
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    struct Piece {
        int first, last;
        QString marker;
        QFont font;
        QBrush background;
        bool selected;
        bool block = false;
    };
    struct Slot { int position; bool opening = false, blockPrefix = false; QVector<Piece> pieces; qreal gap = 0; };
    QTextEdit *m_view;
    QMap<int, QTextCharFormat> m_originalFormats;
    QList<QPair<QTextBlock, qreal>> m_originalMargins;
    QVector<Slot> m_slots;
    QString m_signature;
    QTimer *m_caretTimer;
    int m_caretSource = -1, m_caretPosition = -1, m_cursorWidth;
    bool m_customCaret = false, m_caretVisible = true;
    bool m_frozen = false;
    int m_selectionAnchor = -1, m_selectionCaret = -1;
    QVector<QRectF> codePanels(const MarkdownSourceMap &mapping) const;
    void addSpacing(int position, qreal extra);
    void updateCaret(const MarkdownSourceMap &mapping);
    const Slot *caretSlot() const;
    qreal markerLeft(const Slot &slot, const QRect &native) const;
};
