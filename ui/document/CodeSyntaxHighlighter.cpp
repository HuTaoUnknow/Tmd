#include "ui/document/CodeSyntaxHighlighter.h"
#include "ui/document/CodeBlockFormat.h"
#include "core/markdown/CodeSyntax.h"
#include "core/markdown/LanguageCatalog.h"
#include "core/markdown/MarkdownCodeBlocks.h"
#include "core/settings/MarkdownSettings.h"
#include <QTextBlock>
#include <QTextDocument>
namespace {
struct SyntaxData : QTextBlockUserData {
    CodeSyntax::State state;
    QString language;
    int id;
};
}
CodeSyntaxHighlighter::CodeSyntaxHighlighter(QTextDocument *document, bool sourceMode)
    : QSyntaxHighlighter(document), m_sourceMode(sourceMode) {
    if (sourceMode) connect(document, &QTextDocument::contentsChanged, this, &CodeSyntaxHighlighter::updateSource);
}
void CodeSyntaxHighlighter::updateSource() {
    const QString source = document()->toRawText().replace(QChar::ParagraphSeparator, '\n');
    if (source == m_source) return;
    m_source = source; QMap<int, Context> next; int id = 0;
    for (const auto &block : MarkdownCodeBlocks::scan(source)) {
        ++id;
        for (const auto &line : block.lines) next.insert(line.number, {block.language, id, line.contentStart - line.start});
    }
    // Ordinary typing keeps line numbers and fence identities stable: Qt can
    // update just the edited line and any changed multiline lexical state.
    if (next == m_contexts) return;
    m_contexts = std::move(next);
    rehighlight();
}
void CodeSyntaxHighlighter::refresh() { if (m_sourceMode) updateSource(); rehighlight(); }
void CodeSyntaxHighlighter::highlightBlock(const QString &text) {
    setCurrentBlockState(-1); setCurrentBlockUserData(nullptr);
    const auto &settings = MarkdownSettingsStore::current();
    if (!settings.enabled(MarkdownSettings::SyntaxColors)) return;
    Context context;
    if (m_sourceMode) {
        const auto found = m_contexts.constFind(currentBlock().blockNumber());
        if (found == m_contexts.cend()) return; context = found.value();
    } else {
        if (!CodeBlockFormat::isCode(currentBlock())) return;
        context = {LanguageCatalog::canonicalName(currentBlock().blockFormat().stringProperty(QTextFormat::BlockCodeLanguage)),
            currentBlock().blockFormat().intProperty(CodeBlockFormat::Id), 0};
    }
    CodeSyntax::State previous;
    const auto *data = dynamic_cast<const SyntaxData *>(currentBlock().previous().userData());
    if (data && data->language == context.language && data->id == context.id) previous = data->state;
    const auto result = CodeSyntax::highlightLine(text.mid(context.offset), context.language, previous);
    const bool dark = settings.color(m_sourceMode ? MarkdownSettings::PageBackground : MarkdownSettings::BlockBackground).lightness() < 128;
    for (const auto &part : result.tokens) setFormat(context.offset + part.start, part.length, CodeSyntax::color(part.kind, dark));
    auto *current = new SyntaxData; current->state = result.state; current->language = context.language; current->id = context.id;
    setCurrentBlockUserData(current); setCurrentBlockState(CodeSyntax::stateKey(result.state));
}
