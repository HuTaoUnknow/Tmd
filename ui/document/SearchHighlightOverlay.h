#pragma once
#include <QWidget>
#include <QPointer>
#include <QTextFormat>
class QTextEdit;
class QTimer;

// A mouse-transparent layer paints rounded search panels and redraws their
// glyphs with the existing QTextLayout. It never changes document formats.
class SearchHighlightOverlay : public QWidget {
public:
    static constexpr int Tag = QTextFormat::UserProperty + 250;
    static constexpr int Background = QTextFormat::UserProperty + 251;
    static constexpr int Held = QTextFormat::UserProperty + 252;
    explicit SearchHighlightOverlay(QTextEdit *editor);
    void refresh();
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
private:
    QPointer<QTextEdit> m_editor;
    QTimer *m_caretTimer;
    bool m_caretVisible = true;
};
