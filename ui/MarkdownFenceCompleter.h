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
    using ReplaceLanguage = std::function<void(int, int, const QString &)>;
    MarkdownFenceCompleter(QTextEdit *editor, ReadContext read, ReplaceLanguage replace);
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
    ReplaceLanguage m_replace;
    QString m_dismissed;
    bool m_scheduled = false;
};
