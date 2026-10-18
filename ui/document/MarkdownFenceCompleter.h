#pragma once
#include <QObject>
#include <QPair>
#include <QString>
#include <functional>

class QTextEdit;
class QCompleter;

// Both editable views use source positions so Markdown punctuation stays intact.
class MarkdownFenceCompleter : public QObject {
public:
    using Context = QPair<QString, int>;
    using ReadContext = std::function<Context()>;
    using ReplaceFence = std::function<void(int, int, const QString &, int)>;
    MarkdownFenceCompleter(QTextEdit *editor, ReadContext read, ReplaceFence replace, bool separateElements = false);
    bool isEditingFence() const;
    void reset();
    static bool isOpeningFence(const QString &source, int cursor);
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    void scheduleUpdate();
    void update(bool force = false);
    void complete(const QString &language);
    QTextEdit *m_editor;
    QCompleter *m_completer;
    ReadContext m_read;
    ReplaceFence m_replace;
    QString m_dismissed;
    bool m_scheduled = false;
    bool m_choiceNavigated = false;
    bool m_separateElements = false;
    int m_newFenceLineStart = -1;
};
