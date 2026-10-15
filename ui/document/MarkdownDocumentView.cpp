#include "ui/document/MarkdownDocumentView.h"
#include "ui/document/ImageInput.h"
#include "ui/document/MarkdownTypography.h"
#include "ui/document/MarkdownFenceCompleter.h"
#include "ui/document/MarkdownViewport.h"
#include "ui/document/CodeBlockDecoration.h"
#include "ui/document/CodeBlockFormat.h"
#include "core/markdown/MarkdownCodeBlocks.h"
#include "core/settings/MarkdownSettings.h"
#include "core/images/ImageLoader.h"
#include "core/markdown/MarkdownParser.h"
#include "core/markdown/MarkdownCodeEditing.h"
#include "core/markdown/MarkdownListEditing.h"
#include "core/markdown/MarkdownSyntax.h"
#include "core/markdown/MarkdownBlockSyntax.h"
#include "core/markdown/MarkdownTableEditing.h"
#include "core/markdown/MarkdownParagraphEditing.h"
#include "core/markdown/MarkdownImport.h"
#include "ui/document/InlineMarkupDecoration.h"
#include <QAbstractTextDocumentLayout>
#include <QClipboard>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QInputMethodEvent>
#include <QMenu>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextFragment>
#include <QTextTable>
#include <QTextList>
#include <QTextBoundaryFinder>
#include "ui/document/CodeSyntaxHighlighter.h"
#include <QRegularExpression>
#include <QFontDatabase>
#include <QTimer>
#include <QScopeGuard>
#include <QFileInfo>
#include <QMimeData>
#include <QMouseEvent>
#include <QWheelEvent>
#include <algorithm>
namespace {
constexpr int tableSourceIndex = QTextFormat::UserProperty + 141;
QList<QTextTable *> documentTables(QTextFrame *frame) {
    QList<QTextTable *> result;
    for (auto *child : frame->childFrames()) {
        if (auto *table = qobject_cast<QTextTable *>(child)) result.append(table);
        result += documentTables(child);
    }
    return result;
}
QString documentPlaceholder() { return QStringLiteral("直接编辑文档，或切换到原文编辑 Markdown。可粘贴、拖入图片。"); }
}

MarkdownDocumentView::MarkdownDocumentView(QWidget *parent) : QTextBrowser(parent), m_markupTimer(new QTimer(this)) {
    setObjectName("markdownDocument"); setReadOnly(false); setAcceptRichText(false); setUndoRedoEnabled(false);
    setAcceptDrops(true); viewport()->setAcceptDrops(true); setOpenLinks(false); setOpenExternalLinks(false);
    setTextInteractionFlags(Qt::TextEditorInteraction);
    setFont(MarkdownTypography::bodyFont()); setTabStopDistance(36); document()->setDocumentMargin(32);
    setPlaceholderText(documentPlaceholder());
    m_viewport = new MarkdownViewport(this);
    m_highlighter = new CodeSyntaxHighlighter(document());
    m_codeDecoration = new CodeBlockDecoration(this);
    connect(m_codeDecoration, &CodeBlockDecoration::copyRequested, this, [this](int index, int position) {
        const auto blocks = MarkdownCodeBlocks::scan(m_source);
        const int sourcePosition = m_sourceMap.sourcePosition(position);
        if (index >= 0 && index < blocks.size() && sourcePosition >= blocks[index].fence.bodyStart && sourcePosition <= blocks[index].fence.closeStart) {
            QApplication::clipboard()->setText(blocks[index].code); return;
        }
        for (const auto &block : blocks) if (sourcePosition >= block.fence.bodyStart && sourcePosition <= block.fence.closeStart) {
            QApplication::clipboard()->setText(block.code); return;
        }
        if (index >= 0 && index < blocks.size()) QApplication::clipboard()->setText(blocks[index].code);
    });
    m_inlineDecorations = new InlineMarkupDecoration(this);
    m_fenceCompleter = new MarkdownFenceCompleter(this,
        [this] {
            const auto format = textCursor().blockFormat();
            const bool code = format.nonBreakableLines() || format.hasProperty(QTextFormat::BlockCodeFence);
            return MarkdownFenceCompleter::Context(m_source, code ? -1 : m_sourceMap.sourcePosition(textCursor().position()));
        },
        [this](int start, int length, const QString &replacement, int caret) {
            emit sourceEditStarted(inlineCursorSourcePosition());
            QString source = m_source; source.replace(start, length, replacement);
            setMarkdownSource(m_path, source);
            QTextCursor cursor(document()); cursor.setPosition(m_sourceMap.displayPosition(caret)); setTextCursor(cursor);
            emit sourceEdited(m_source, m_sourceMap.sourcePosition(cursor.position())); ensureCursorVisible();
        });
    connect(document(), &QTextDocument::contentsChange, this, &MarkdownDocumentView::documentEdited);
    connect(this, &QTextEdit::cursorPositionChanged, this, &MarkdownDocumentView::refreshInlineCursorFormat);
    connect(this, &QTextEdit::selectionChanged, this, &MarkdownDocumentView::refreshInlineCursorFormat);
    m_markupTimer->setSingleShot(true); m_markupTimer->setInterval(450);
    connect(m_markupTimer, &QTimer::timeout, this, [this] {
        // An empty paragraph is an editing position, even though Markdown
        // discards empty paragraphs. Keep it until the user has entered text.
        if (m_selecting) { m_markupTimer->start(); return; }
        if (!MarkdownSettingsStore::current().enabled(MarkdownSettings::AutoRender)
            || textCursor().block().text().trimmed().isEmpty() || m_fenceCompleter->isEditingFence()) return;
        m_markupRefresh = true; setMarkdownSource(m_path, m_source); m_markupRefresh = false;
    });
}
QString MarkdownDocumentView::displayedText() const {
    QString result = document()->toRawText(); result.replace(QChar::ParagraphSeparator, '\n'); return result;
}
int MarkdownDocumentView::viewportSourcePosition() const {
    return m_sourceMap.sourcePosition(m_viewport->anchorPosition());
}
void MarkdownDocumentView::alignSourcePosition(int position) {
    m_viewport->alignPosition(m_sourceMap.displayPosition(position));
}
QImage MarkdownDocumentView::placeholder(const QString &message) const {
    QImage image(420, 68, QImage::Format_ARGB32_Premultiplied); image.fill(QColor("#2b312e"));
    QPainter painter(&image); painter.setPen(QColor("#c0c9c3")); painter.setFont(QFont("Segoe UI", 10));
    painter.drawText(image.rect().adjusted(12, 8, -12, -8), Qt::AlignVCenter | Qt::TextWordWrap, message); return image;
}
void MarkdownDocumentView::setMarkdownSource(const QString &path, const QString &inputSource, bool force) {
    const QString source = MarkdownTableEditing::normalize(inputSource);
    if (!force && !m_markupRefresh && path == m_path && source == m_source) return;
    const bool wasRebuilding = m_rebuilding; m_rebuilding = true;
    const auto rebuilt = qScopeGuard([this, wasRebuilding] { m_rebuilding = wasRebuilding; });
    if (path != m_path) m_revealed = {};
    if (m_revealed.valid() && source != m_source) {
        int first = 0; while (first < source.size() && first < m_source.size() && source[first] == m_source[first]) ++first;
        const int delta = source.size() - m_source.size();
        if (first < m_revealed.start) { m_revealed.start += delta; m_revealed.end += delta; }
        else if (first <= m_revealed.end) m_revealed.end = qMax(m_revealed.start, m_revealed.end + delta);
    }
    m_sourceMap.setLiteralRanges(m_revealed.valid() ? QVector<MarkdownSourceMap::Span>{{m_revealed.start, m_revealed.end}} : QVector<MarkdownSourceMap::Span>());
    const bool restoreSelection = path == m_path && source == m_source && hasSourceSelection();
    const int selectionAnchor = m_selectionAnchor, selectionCaret = m_selectionCaret;
    clearSourceSelection();
    m_fenceCompleter->reset();
    const bool sameDocument = path == m_path; const int scroll = verticalScrollBar()->value();
    const int caret = restoreSelection ? selectionCaret : m_sourceMap.sourcePosition(textCursor().position());
    const int anchor = restoreSelection ? selectionAnchor : m_sourceMap.sourcePosition(textCursor().anchor());
    const auto oldInline = m_sourceMap.inlineFormatAt(textCursor().position());
    const bool outsideInline = oldInline.valid() && (caret == oldInline.sourceStart || caret == oldInline.sourceEnd);
    QMap<QString, QImage> cached;
    if (sameDocument) for (auto it = m_loaded.cbegin(); it != m_loaded.cend(); ++it) cached.insert(m_sources.value(it.key()), it.value());
    m_markupTimer->stop(); ++m_generation;
    for (auto *loader : m_pending) { loader->cancel(); loader->deleteLater(); }
    m_pending.clear(); m_sources.clear(); m_images.clear(); m_loaded.clear(); m_errors.clear(); m_requestedSizes.clear();
    const auto analysis = MarkdownAnalysis::forRendering(source);
    const auto sourceSyntax = MarkdownSyntax::scan(source, &analysis);
    m_path = path; m_source = source; m_references = MarkdownImages::parse(source, analysis);
    m_references.erase(std::remove_if(m_references.begin(), m_references.end(), [&](const auto &image) {
        return source.mid(image.position, 2) != "![" || (m_revealed.valid() && ((image.position >= m_revealed.start && image.position < m_revealed.end)
            || (image.start >= m_revealed.start && image.start < m_revealed.end)));
    }), m_references.end());
    setPlaceholderText(source.trimmed().isEmpty() ? documentPlaceholder() : QString());
    QMap<QString, QString> replacements; int count = 0;
    for (const auto &image : m_references) if (!replacements.contains(image.source)) {
        const QUrl url(QString("tree-md-image:/%1").arg(++count)); m_sources.insert(url, image.source);
        if (cached.contains(image.source) && !force) { m_loaded.insert(url, cached.value(image.source)); m_images.insert(url, cached.value(image.source)); }
        else m_images.insert(url, placeholder(QStringLiteral("正在加载图片…")));
        replacements.insert(image.source, url.toString());
    }
    m_preparing = true; m_inlineDecorations->clear();
    document()->setBaseUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath() + '/'));
    auto headings = MarkdownParser::headingMarkers(source, analysis);
    headings.erase(std::remove_if(headings.begin(), headings.end(), [](const auto &h) { return !h.separated; }), headings.end());
    QString renderSource = MarkdownImages::replace(source, m_references, replacements);
    if (m_revealed.valid()) {
        QMap<int, int> deltas;
        for (const auto &image : m_references) if (image.start < m_revealed.start)
            deltas[image.start] = replacements.value(image.source).size() - image.length;
        int delta = 0; for (int difference : deltas) delta += difference;
        QString raw; const QString punctuation = m_revealed.kind == MarkdownSpelling::Kind::Link ? QString("\\[]()!<>&")
            : m_revealed.kind == MarkdownSpelling::Kind::Underline ? QString("-=") : QString("\\`*_{}[]()#+-.!<>|&=~;");
        for (int p = m_revealed.start; p < qMin(m_revealed.end, int(source.size())); ++p) {
            if (punctuation.contains(source[p])) raw += '\\'; raw += source[p];
        }
        renderSource.replace(m_revealed.start + delta, m_revealed.end - m_revealed.start, raw);
    }
    QString bodyPrefix = QChar(0xe100) + QStringLiteral("tmd-row:");
    while (source.contains(bodyPrefix)) bodyPrefix += ':';
    QVector<QPair<QString, int>> bodyTokens;
    auto renderedRows = renderSource.split('\n');
    const auto originalRows = source.split('\n');
    const int metadataEnd = sourceSyntax.frontMatterEnd;
    if (metadataEnd > 0) {
        renderedRows[0] = "```yaml";
        for (const auto &line : analysis.lines) if (line.start > 0 && line.start < metadataEnd && line.fenceMarker)
            renderedRows[line.number] = "```";
    }
    for (int i = 0, start = 0; i < originalRows.size() && i < renderedRows.size(); start += originalRows[i++].size() + 1) {
        if (!originalRows[i].trimmed().isEmpty() || source.isEmpty()) continue;
        bool literal = false, separator = false;
        if (i < analysis.lines.size()) literal = analysis.lines[i].code || analysis.lines[i].fenceMarker;
        if (literal) continue;
        const bool afterHeading = i > 0 && i < analysis.lines.size()
            && (sourceSyntax.underlineLines.contains(analysis.lines[i - 1].start)
                || std::any_of(sourceSyntax.headings.cbegin(), sourceSyntax.headings.cend(), [&](const auto &heading) { return heading.position == analysis.lines[i - 1].start; }));
        if (i > 0 && i < analysis.lines.size() && !afterHeading && !analysis.lines[i - 1].code
            && (analysis.lines[i - 1].listMarkerStart >= 0 || originalRows[i - 1].startsWith("> "))) {
            int next = i + 1; while (next < originalRows.size() && originalRows[next].trimmed().isEmpty()) ++next;
            separator = next >= originalRows.size() ? i + 1 < originalRows.size() : next < analysis.lines.size() && analysis.lines[next].listMarkerStart < 0
                && !originalRows[next].startsWith("> ") && !QRegularExpression("^ {0,3}#{1,6} ").match(originalRows[next]).hasMatch();
        }
        if (separator) continue;
        const QString token = bodyPrefix + QString::number(bodyTokens.size()) + QChar(0xe101);
        bodyTokens.append({token, start}); renderedRows[i] = '\n' + renderedRows[i] + token + '\n';
    }
    renderSource = renderedRows.join('\n');
    renderSource = MarkdownBlockSyntax::prepare(renderSource).text;
    const auto renderHeadings = MarkdownParser::headingMarkers(renderSource);
    QString emptyHeadingPrefix = QChar(0xe000) + QStringLiteral("tmd-heading:");
    while (renderSource.contains(emptyHeadingPrefix)) emptyHeadingPrefix += ':';
    auto emptyHeadingToken = [&](int index) { return emptyHeadingPrefix + QString::number(index) + QChar(0xe001); };
    for (int i = renderHeadings.size() - 1; i >= 0; --i) {
        const auto &heading = renderHeadings[i];
        if (heading.separated && heading.contentStart == heading.end) renderSource.insert(heading.contentStart, emptyHeadingToken(i));
    }
    // Qt can attach an empty imported list item to the preceding paragraph.
    // Give every empty container its own block, then remove the import token.
    QString emptyContainerPrefix = emptyHeadingPrefix + "container:";
    const auto originalMarkers = MarkdownBlockSyntax::markers(source, &analysis);
    const auto preparedMarkers = MarkdownBlockSyntax::markers(renderSource);
    QVector<int> emptyContainers;
    const auto quoteSeparator = [](const QString &text, const MarkdownBlockSyntax::Marker &marker) {
        static const QRegularExpression quoteOnly("^(?: {0,3}> +)+ *$");
        const auto row = text.mid(marker.lineStart, marker.lineEnd - marker.lineStart);
        if (!quoteOnly.match(row).hasMatch() || marker.lineEnd >= text.size()) return false;
        const int nextStart = marker.lineEnd + 1; int nextEnd = text.indexOf('\n', nextStart); if (nextEnd < 0) nextEnd = text.size();
        const auto nextMarkers = MarkdownBlockSyntax::markers(text.mid(nextStart, nextEnd - nextStart));
        return !nextMarkers.isEmpty() && nextMarkers.front().text.count('>') == row.count('>');
    };
    for (const auto &marker : originalMarkers) if (marker.prefix && !marker.heading && source.mid(marker.end, marker.lineEnd - marker.end).trimmed().isEmpty()
        && !quoteSeparator(source, marker)) emptyContainers.append(marker.end);
    int containerIndex = emptyContainers.size();
    for (auto it = preparedMarkers.crbegin(); it != preparedMarkers.crend(); ++it)
        if (it->prefix && !it->heading && renderSource.mid(it->end, it->lineEnd - it->end).trimmed().isEmpty() && containerIndex > 0
            && !quoteSeparator(renderSource, *it))
            renderSource.insert(it->end, emptyContainerPrefix + QString::number(--containerIndex) + QChar(0xe001));
    // Qt may discard a fence with no body line. Keep a virtual empty line
    // for editing, while source positions continue to refer to the real fence.
    const auto renderFences = MarkdownParser::codeFences(renderSource);
    QVector<QPair<int, QString>> emptyCodeLines; QStringList emptyCodeTokens;
    for (const auto &fence : renderFences) if (fence.closed()) {
        const QString prefix = MarkdownCodeEditing::continuationPrefix(fence.indent);
        for (int start = fence.bodyStart; start < fence.closeStart;) {
            const int end = renderSource.indexOf('\n', start); if (end < 0 || end >= fence.closeStart) break;
            QString line = renderSource.mid(start, end - start);
            if (!prefix.isEmpty() && line.startsWith(prefix)) line.remove(0, prefix.size());
            if (!line.trimmed().isEmpty()) { start = end + 1; continue; }
            const QString token = emptyHeadingPrefix + "code:" + QString::number(emptyCodeTokens.size()) + QChar(0xe001);
            emptyCodeTokens.append(token); emptyCodeLines.append({end, token}); start = end + 1;
        }
    }
    for (const auto &fence : renderFences) if (fence.closed() && fence.bodyStart == fence.closeStart) {
        const QString token = emptyHeadingPrefix + "code:" + QString::number(emptyCodeTokens.size()) + QChar(0xe001);
        emptyCodeTokens.append(token);
        emptyCodeLines.append({fence.bodyStart, MarkdownCodeEditing::continuationPrefix(fence.indent) + token + '\n'});
    }
    // Preserve all empty code rows, including trailing ones, as real cursor
    // positions and independent fenced panels.
    // Apply all insertions together so their offsets refer to the same source.
    std::sort(emptyCodeLines.begin(), emptyCodeLines.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
    for (const auto &line : emptyCodeLines) renderSource.insert(line.first, line.second);
    // Qt's Markdown importer drops inline HTML and can swallow the remainder
    // of a table cell after <br>. Import it as text, then restore a native
    // line separator without changing the saved Markdown.
    struct TableBreak { int start, length; QString token; };
    QVector<TableBreak> tableBreaks;
    const auto renderTables = MarkdownTableEditing::tables(renderSource);
    const auto renderLiteral = MarkdownAnalysis::forRendering(renderSource).literalMask(renderSource.size());
    static const QRegularExpression br("<br\\s*/?\\s*>", QRegularExpression::CaseInsensitiveOption);
    auto breaks = br.globalMatch(renderSource);
    while (breaks.hasNext()) {
        const auto match = breaks.next(); const int start = match.capturedStart();
        if (renderLiteral[start]) continue;
        int slashes = 0; for (int p = start - 1; p >= 0 && renderSource[p] == '\\'; --p) ++slashes;
        if (slashes % 2) continue;
        bool cellBreak = false;
        for (const auto &table : renderTables) for (const auto &row : table.rows) for (const auto &cell : row)
            if (start >= cell.start && start + match.capturedLength() <= cell.end) cellBreak = true;
        if (cellBreak) tableBreaks.append({start, int(match.capturedLength()), emptyHeadingPrefix + "table-br:" + QString::number(tableBreaks.size()) + QChar(0xe001)});
    }
    for (auto it = tableBreaks.crbegin(); it != tableBreaks.crend(); ++it) renderSource.replace(it->start, it->length, it->token);
    document()->setMarkdown(renderSource, MarkdownSyntax::renderFeatures());
    QVector<QPair<QTextCursor, int>> bodyCursors;
    for (const auto &entry : bodyTokens) {
        auto cursor = document()->find(entry.first);
        if (!cursor.isNull()) { cursor.removeSelectedText(); bodyCursors.append({cursor, entry.second}); }
    }
    for (const auto &line : tableBreaks) {
        auto cursor = document()->find(line.token);
        if (!cursor.isNull()) cursor.insertText(QString(QChar::LineSeparator));
    }
    for (const auto &token : emptyCodeTokens) {
        auto cursor = document()->find(token);
        if (!cursor.isNull()) {
            const auto format = cursor.blockFormat(); cursor.removeSelectedText(); cursor.setBlockFormat(format);
        }
    }
    // Qt can omit consecutive empty headings. Give each an import-only token,
    // then remove it while retaining the editable heading block and source caret.
    QList<QPair<QTextCursor, int>> emptyHeadingCursors;
    for (int i = 0; i < headings.size(); ++i) if (headings[i].separated && headings[i].contentStart == headings[i].end) {
        auto cursor = document()->find(emptyHeadingToken(i));
        if (!cursor.isNull()) {
            cursor.removeSelectedText(); int body = headings[i].contentStart;
            for (const auto &marker : originalMarkers) if (marker.prefix && marker.heading && marker.lineStart <= headings[i].markerStart && marker.lineEnd == headings[i].end) { body = marker.end; break; }
            emptyHeadingCursors.append({cursor, body});
        }
    }
    QVector<QPair<int, int>> blockBoundaries;
    for (int i = 0; i < emptyContainers.size(); ++i) {
        auto cursor = document()->find(emptyContainerPrefix + QString::number(i) + QChar(0xe001));
        if (!cursor.isNull()) { cursor.removeSelectedText(); blockBoundaries.append({cursor.position(), emptyContainers[i]}); }
    }
    for (const auto &heading : emptyHeadingCursors) blockBoundaries.append({heading.first.position(), heading.second});
    const auto &fences = analysis.fences;
    const auto sourceCodeBlocks = MarkdownCodeBlocks::scan(source, &analysis);
    // Blank lines outside a fence must not steal an empty code line's source
    // position. Match the code lines Qt retained before applying our styles.
    QList<QTextBlock> codeBlocks; QList<QPair<int, int>> codeBoundaries;
    for (auto block = document()->begin(); block.isValid(); block = block.next())
        if (block.blockFormat().hasProperty(QTextFormat::BlockCodeFence)) codeBlocks.append(block);
    if (metadataEnd > 0) {
        int index = 0;
        for (const auto &line : analysis.lines) if (line.start > 0 && line.start < metadataEnd && !line.fenceMarker && index < codeBlocks.size()) {
            codeBoundaries.append({codeBlocks[index].position(), line.start});
            auto format = codeBlocks[index].blockFormat(); format.setProperty(CodeBlockFormat::Id, -1); QTextCursor(codeBlocks[index++]).setBlockFormat(format);
        }
        if (!index && !codeBlocks.isEmpty()) codeBoundaries.append({codeBlocks.front().position(), source.indexOf('\n') + 1});
    }
    int nextCode = 0, codeIndex = 0;
    for (const auto &fence : fences) {
        ++codeIndex;
        QList<QPair<QString, int>> lines;
        for (const auto &line : sourceCodeBlocks[codeIndex - 1].lines) {
            lines.append({source.mid(line.contentStart, line.end - line.contentStart), line.contentStart});
        }
        if (lines.isEmpty()) lines.append({QString(), fence.bodyStart});
        // Import tokens above keep leading empty lines in the same source order.
        const QString language = sourceCodeBlocks[codeIndex - 1].info;
        for (int candidate = nextCode; candidate + lines.size() <= codeBlocks.size(); ++candidate) {
            bool matches = true;
            for (int line = 0; line < lines.size(); ++line) {
                const auto block = codeBlocks[candidate + line]; const auto format = block.blockFormat();
                if (block.text() != lines[line].first || format.stringProperty(QTextFormat::BlockCodeLanguage) != language
                    || format.stringProperty(QTextFormat::BlockCodeFence) != QString(fence.delimiter[0])) { matches = false; break; }
            }
            if (!matches) continue;
            for (int line = 0; line < lines.size(); ++line) {
                const auto block = codeBlocks[candidate + line]; codeBoundaries.append({block.position(), lines[line].second});
                auto format = block.blockFormat(); format.setProperty(CodeBlockFormat::Id, codeIndex); QTextCursor(block).setBlockFormat(format);
            }
            nextCode = candidate + lines.size(); break;
        }
    }
    // Qt drops trailing blank paragraphs. Keep an editable body paragraph after
    // a list/quote exit or a completed code block.
    bool trailingCodeParagraph = false;
    const auto lastFormat = document()->lastBlock().blockFormat();
    if (source.endsWith("\n\n") && (lastFormat.nonBreakableLines() || lastFormat.hasProperty(QTextFormat::BlockCodeFence))) {
        if (!fences.isEmpty() && fences.back().closed() && source.mid(fences.back().end).trimmed().isEmpty()) {
            QTextCursor end(document()); end.movePosition(QTextCursor::End); end.insertBlock(QTextBlockFormat(), QTextCharFormat());
            trailingCodeParagraph = true;
        }
    }
    if (source.endsWith("\n\n") && !trailingCodeParagraph
        && (document()->lastBlock().textList() || lastFormat.intProperty(QTextFormat::BlockQuoteLevel) > 0)) {
        QTextCursor end(document()); end.movePosition(QTextCursor::End); end.insertBlock(QTextBlockFormat(), QTextCharFormat());
        trailingCodeParagraph = true;
    }
    blockBoundaries += codeBoundaries;
    for (const auto &entry : bodyCursors) blockBoundaries.append({entry.first.block().position(), entry.second});
    if (trailingCodeParagraph) blockBoundaries.append({document()->lastBlock().position(), source.size()});
    // Importing Markdown trims editable line-end whitespace. Restore only the
    // mapped text gaps after parsing, leaving syntax and hard breaks intact.
    m_sourceMap.rebuild(source, displayedText(), blockBoundaries);
    const auto sourceTables = MarkdownTableEditing::tables(source);
    const auto tables = documentTables(document()->rootFrame());
    QSet<int> matchedTables;
    for (auto *table : tables) {
        const int position = m_sourceMap.sourcePosition(table->firstCursorPosition().position());
        int index = -1;
        for (int i = 0; i < sourceTables.size(); ++i)
            if (!matchedTables.contains(i) && table->rows() == sourceTables[i].rows.size()
                && !sourceTables[i].rows.isEmpty() && table->columns() == sourceTables[i].rows.front().size()
                && (position >= sourceTables[i].start && position <= sourceTables[i].end)) { index = i; break; }
        if (index < 0 && tables.size() == sourceTables.size())
            for (int i = 0; i < sourceTables.size(); ++i)
                if (!matchedTables.contains(i) && table->rows() == sourceTables[i].rows.size()
                    && !sourceTables[i].rows.isEmpty() && table->columns() == sourceTables[i].rows.front().size()) { index = i; break; }
        if (index < 0) continue;
        matchedTables.insert(index);
        auto format = table->format(); format.setProperty(tableSourceIndex, index + 1); table->setFormat(format);
        for (int row = 0; row < table->rows(); ++row) for (int column = 0; column < table->columns(); ++column)
            blockBoundaries.append({table->cellAt(row, column).firstCursorPosition().position(), sourceTables[index].rows[row][column].contentStart});
        const int paragraph = MarkdownTableEditing::paragraphAfter(source, sourceTables[index]);
        const auto body = document()->findBlock(table->lastCursorPosition().position() + 1);
        if (paragraph >= 0 && body.isValid() && body.text().trimmed().isEmpty()
            && !QTextCursor(body).currentTable())
            blockBoundaries.append({body.position(), paragraph});
    }
    if (!matchedTables.isEmpty()) m_sourceMap.rebuild(source, displayedText(), blockBoundaries);
    if (m_revealed.valid() && m_revealed.kind == MarkdownSpelling::Kind::Underline) {
        for (const auto &heading : sourceSyntax.headings) {
            const int end = source.indexOf('\n', heading.position);
            if (end + 1 != m_revealed.start) continue;
            const auto block = document()->findBlock(m_sourceMap.displayPosition(heading.position));
            auto format = block.blockFormat(); format.setHeadingLevel(heading.level); QTextCursor(block).setBlockFormat(format);
        }
    }
    // Qt omits the quote level on imported headings. Recover their outer
    // container from the original source rather than losing its appearance.
    const auto sourceHeadings = MarkdownParser::outline(source, analysis);
    int headingIndex = 0;
    for (auto block = document()->begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().headingLevel() <= 0 || headingIndex >= sourceHeadings.size()) continue;
        const int position = sourceHeadings[headingIndex++].position;
        int depth = 0;
        for (const auto &marker : originalMarkers) {
            if (!marker.prefix || marker.lineStart != position) continue;
            depth = marker.text.count('>');
            break;
        }
        auto format = block.blockFormat();
        format.setProperty(QTextFormat::BlockQuoteLevel, depth);
        QTextCursor(block).setBlockFormat(format);
    }
    const auto whitespaceEdits = m_sourceMap.editableWhitespaceEdits();
    if (!whitespaceEdits.isEmpty()) {
        QVector<QPair<QTextCursor, int>> trackedBoundaries;
        for (const auto &boundary : blockBoundaries) {
            QTextCursor cursor(document()); cursor.setPosition(boundary.first); cursor.setKeepPositionOnInsert(true);
            trackedBoundaries.append({cursor, boundary.second});
        }
        for (auto edit = whitespaceEdits.crbegin(); edit != whitespaceEdits.crend(); ++edit) {
            QTextCursor cursor(document()); cursor.setPosition(edit->position);
            cursor.setPosition(edit->position + edit->removed, QTextCursor::KeepAnchor); cursor.insertText(edit->text);
        }
        blockBoundaries.clear();
        for (const auto &boundary : trackedBoundaries) blockBoundaries.append({boundary.first.position(), boundary.second});
    }
    for (const auto &marker : originalMarkers) if (marker.prefix) {
        const auto number = QRegularExpression("(?:^|[ >])([0-9]{1,9})([.)]) ").match(marker.text);
        if (!number.hasMatch()) continue;
        const auto block = document()->findBlock(m_sourceMap.displayPosition(marker.end));
        if (!block.textList()) continue;
        auto format = block.textList()->format(); format.setStart(number.captured(1).toInt()); format.setNumberSuffix(number.captured(2));
        QTextCursor(block).createList(format);
    }
    applyMarkdownStyles(); document()->setModified(false); m_preparing = false; fitImages();
    m_display = displayedText();
    if (!whitespaceEdits.isEmpty()) m_sourceMap.rebuild(m_source, m_display, blockBoundaries);
    if (sameDocument) {
        QTextCursor cursor(document()); cursor.setPosition(qBound(0, m_sourceMap.displayPosition(anchor), document()->characterCount() - 1));
        cursor.setPosition(qBound(0, m_sourceMap.displayPosition(caret), document()->characterCount() - 1), QTextCursor::KeepAnchor); setTextCursor(cursor);
        const auto boundary = m_sourceMap.markerBoundaryAt(cursor.position());
        if (!cursor.hasSelection() && boundary.valid() && caret >= boundary.sourceStart && caret <= boundary.sourceEnd)
            m_sourceMap.setBoundary(cursor.position(), caret);
    } else {
        moveCursor(QTextCursor::Start);
        const auto boundary = m_sourceMap.markerBoundaryAt(0);
        if (boundary.valid()) {
            int start = boundary.sourceStart;
            for (const auto &marker : m_sourceMap.blockMarkers()) if (marker.prefix && marker.displayPosition == 0) start = marker.sourceEnd;
            m_sourceMap.setBoundary(0, start);
        }
    }
    if (sameDocument && outsideInline) {
        const auto code = m_sourceMap.inlineFormatAt(textCursor().position());
        if (code.valid() && (caret == code.sourceStart || caret == code.sourceEnd)) setInlineBoundary(code, caret == code.sourceEnd, true);
    }
    if (restoreSelection) {
        m_selectionAnchor = selectionAnchor; m_selectionCaret = selectionCaret;
        m_selectionDisplayAnchor = textCursor().anchor(); m_selectionDisplayCaret = textCursor().position();
        m_inlineDecorations->setSourceSelection(selectionAnchor, selectionCaret, false);
    }
    refreshInlineCursorFormat();
    verticalScrollBar()->setValue(sameDocument ? scroll : 0);
    for (auto it = m_sources.cbegin(); it != m_sources.cend(); ++it) if (!m_loaded.contains(it.key())) {
        const QUrl url = it.key(); const int generation = m_generation;
        QTimer::singleShot(0, this, [this, url, generation] { loadImage(url, generation); });
    }
    if (m_loaded.size() == m_sources.size()) emit imagesSettled();
    if (source != inputSource) {
        const int generation = m_generation;
        QTimer::singleShot(0, this, [this, generation] {
            if (generation != m_generation) return;
            emit sourceEditStarted(inlineCursorSourcePosition());
            emit sourceEdited(m_source, inlineCursorSourcePosition()); emit sourceEditFinished();
        });
    }
}
void MarkdownDocumentView::applyMarkdownStyles() {
    m_inlineDecorations->clear();
    const auto &settings = MarkdownSettingsStore::current();
    document()->setIndentWidth(settings.number(MarkdownSettings::ListIndent));
    const QFont body = MarkdownTypography::bodyFont(m_zoomPercent), fixed = MarkdownTypography::codeFont(m_zoomPercent);
    QFont labelFont = fixed; labelFont.setPointSizeF(qMax(8.0, fixed.pointSizeF() * 0.85)); m_codeDecoration->setFont(labelFont);
    // Qt ignores the first block's top margin. Reserve its header in the root
    // frame as well, so a document beginning with a fence keeps the controls.
    auto root = document()->rootFrame()->frameFormat(); root.setBottomMargin(24);
    root.setTopMargin(document()->documentMargin() + (document()->begin().blockFormat().intProperty(CodeBlockFormat::Id) > 0 ? m_codeDecoration->headerHeight() : 0));
    document()->rootFrame()->setFrameFormat(root);
    const QColor base = palette().color(QPalette::Base), codeBackground = MarkdownTypography::codeBackground();
    for (auto block = document()->begin(); block.isValid(); block = block.next()) {
        QTextCursor cursor(block); auto format = block.blockFormat();
        const bool code = format.nonBreakableLines() || format.hasProperty(QTextFormat::BlockCodeFence);
        const int heading = format.headingLevel();
        bool hasImage = false;
        for (auto it = block.begin(); !it.atEnd(); ++it) if (it.fragment().isValid() && it.fragment().charFormat().isImageFormat()) hasImage = true;
        format.setTopMargin(settings.number(block.textList() ? MarkdownSettings::ListMargin : MarkdownSettings::ParagraphMargin));
        format.setBottomMargin(format.topMargin());
        // Text line spacing must not multiply the height of an inline image.
        format.setLineHeight(hasImage ? 100 : settings.number(heading ? MarkdownSettings::HeadingLineHeight : MarkdownSettings::LineHeight), QTextBlockFormat::ProportionalHeight);
        if (heading) { format.setTopMargin(settings.number(heading == 1 ? MarkdownSettings::ParagraphMargin : MarkdownSettings::HeadingTop)); format.setBottomMargin(settings.number(MarkdownSettings::HeadingBottom)); }
        if (hasImage) { format.setTopMargin(4); format.setBottomMargin(8); }
        if (code) {
            if (!format.hasProperty(QTextFormat::BlockCodeFence)) format.setProperty(QTextFormat::BlockCodeFence, QStringLiteral("```"));
            format.setBackground(codeBackground); format.setLeftMargin(settings.number(MarkdownSettings::CodePadding)); format.setRightMargin(format.leftMargin());
            format.setNonBreakableLines(!settings.enabled(MarkdownSettings::WrapCode));
            format.setTopMargin(CodeBlockFormat::samePanel(block.previous(), block) ? 0 : settings.number(MarkdownSettings::CodeMargin)
                + (format.intProperty(CodeBlockFormat::Id) > 0 ? m_codeDecoration->headerHeight() : 0));
            format.setBottomMargin(CodeBlockFormat::samePanel(block, block.next()) ? 0 : settings.number(MarkdownSettings::CodeMargin));
        } else if (format.intProperty(QTextFormat::BlockQuoteLevel) > 0) {
            format.setLeftMargin(settings.number(MarkdownSettings::QuoteIndent) * format.intProperty(QTextFormat::BlockQuoteLevel)); format.setBackground(settings.color(MarkdownSettings::QuoteBackground));
        }
        cursor.setBlockFormat(format);
        if (block.text().isEmpty()) {
            QTextCharFormat character; character.setFont(code ? fixed : body);
            character.setForeground(settings.color(heading ? MarkdownSettings::HeadingColor : MarkdownSettings::TextColor));
            if (heading) { character.setFontPointSize(MarkdownTypography::headingPointSize(heading, m_zoomPercent)); character.setFontWeight(settings.enabled(MarkdownSettings::BoldHeadings) ? QFont::Bold : QFont::Normal); }
            cursor.setBlockCharFormat(character);
        }
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment(); if (!fragment.isValid() || fragment.charFormat().isImageFormat()) continue;
            auto character = fragment.charFormat();
            if (code || character.fontFixedPitch()) {
                character.setFontFamilies(fixed.families()); character.setFontPointSize(!code && heading ? MarkdownTypography::headingPointSize(heading, m_zoomPercent) * 0.8 : fixed.pointSizeF()); character.setForeground(settings.color(MarkdownSettings::TextColor));
                if (!code) character.setBackground(QBrush(Qt::NoBrush));
            } else {
                character.setFontFamilies(body.families()); character.setFontPointSize(heading ? MarkdownTypography::headingPointSize(heading, m_zoomPercent) : body.pointSizeF());
                character.setForeground(settings.color(MarkdownSettings::TextColor));
                if (heading) { character.setFontWeight(settings.enabled(MarkdownSettings::BoldHeadings) ? QFont::Bold : QFont::Normal); character.setForeground(settings.color(MarkdownSettings::HeadingColor)); }
            }
            if (character.isAnchor()) { character.setForeground(settings.color(MarkdownSettings::LinkColor)); character.setFontUnderline(settings.enabled(MarkdownSettings::UnderlineLinks)); }
            cursor.setPosition(fragment.position()); cursor.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor); cursor.setCharFormat(character);
        }
    }
    m_highlighter->refresh();
    for (auto *frame : document()->rootFrame()->childFrames()) if (auto *table = qobject_cast<QTextTable *>(frame)) {
        auto format = table->format(); format.setBorder(1); format.setBorderBrush(settings.color(MarkdownSettings::TableBorder)); format.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
        format.setCellPadding(settings.number(MarkdownSettings::TablePadding)); format.setCellSpacing(0); format.setHeaderRowCount(1); format.setWidth(QTextLength(QTextLength::PercentageLength, 100)); table->setFormat(format);
        for (int row = 0; row < table->rows(); ++row) for (int col = 0; col < table->columns(); ++col) {
            auto cell = table->cellAt(row, col); auto cellFormat = cell.format(); cellFormat.setBackground(settings.color(row == 0 ? MarkdownSettings::TableHeader : settings.enabled(MarkdownSettings::StripedTables) && row % 2 ? MarkdownSettings::TableOdd : MarkdownSettings::TableEven)); cell.setFormat(cellFormat);
            if (row == 0) { QTextCursor header = cell.firstCursorPosition(); header.setPosition(cell.lastCursorPosition().position(), QTextCursor::KeepAnchor); QTextCharFormat bold; bold.setFontWeight(QFont::Bold); header.mergeCharFormat(bold); }
        }
    }
}
void MarkdownDocumentView::refreshAppearance() {
    const auto &settings = MarkdownSettingsStore::current(); const bool modified = document()->isModified();
    const int scroll = verticalScrollBar()->value(); m_preparing = true;
    setStyleSheet(QString("QTextBrowser#markdownDocument { background: %1; color: %2; }")
        .arg(settings.color(MarkdownSettings::PageBackground).name(), settings.color(MarkdownSettings::TextColor).name()));
    auto colors = palette(); colors.setColor(QPalette::Base, settings.color(MarkdownSettings::PageBackground));
    colors.setColor(QPalette::Text, settings.color(MarkdownSettings::TextColor)); setPalette(colors);
    setFont(MarkdownTypography::bodyFont(m_zoomPercent)); setTabStopDistance(settings.number(MarkdownSettings::TabWidth));
    m_markupTimer->setInterval(settings.numbers[MarkdownSettings::RenderDelay]); applyMarkdownStyles();
    if (auto *highlighter = findChild<QSyntaxHighlighter *>()) highlighter->rehighlight();
    m_preparing = false; document()->setModified(modified); fitImages(); verticalScrollBar()->setValue(scroll);
    m_fenceCompleter->reset(); refreshInlineCursorFormat(); viewport()->update();
}
void MarkdownDocumentView::setZoomPercent(int percent) {
    percent = qBound(50, percent, 200); if (percent == m_zoomPercent) return;
    const QTextCursor anchor = cursorForPosition(QPoint(32, viewport()->height() / 3)); const int y = cursorRect(anchor).top();
    const bool modified = document()->isModified(); m_preparing = true; m_zoomPercent = percent;
    setFont(MarkdownTypography::bodyFont(percent)); applyMarkdownStyles(); m_preparing = false; fitImages(); refreshInlineCursorFormat();
    document()->setModified(modified); verticalScrollBar()->setValue(verticalScrollBar()->value() + cursorRect(anchor).top() - y); viewport()->update();
}
void MarkdownDocumentView::wheelEvent(QWheelEvent *event) {
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        const int steps = MarkdownTypography::wheelSteps(event, m_wheelRemainder);
        if (steps) emit zoomRequested(qBound(50, m_zoomPercent + steps * 10, 200));
        event->accept(); return;
    }
    QTextBrowser::wheelEvent(event);
}
void MarkdownDocumentView::documentEdited(int position, int removed, int added) {
    if (m_preparing) return;
    const QString display = displayedText(); if (display == m_display) return;
    // Qt may report a replaced paragraph including its separator. The text
    // difference narrows it to the actual user edit before touching the source.
    int prefix = 0; while (prefix < m_display.size() && prefix < display.size() && m_display[prefix] == display[prefix]) ++prefix;
    int oldEnd = m_display.size(), newEnd = display.size();
    while (oldEnd > prefix && newEnd > prefix && m_display[oldEnd - 1] == display[newEnd - 1]) { --oldEnd; --newEnd; }
    if (position >= 0 && position <= m_display.size() && position <= display.size()) {
        const int oldLength = qMin(removed, int(m_display.size()) - position), newLength = qMin(added, int(display.size()) - position);
        if (oldLength == oldEnd - prefix && newLength == newEnd - prefix && m_display.left(position) + display.mid(position, newLength) + m_display.mid(position + oldLength) == display) {
            prefix = position; oldEnd = position + oldLength; newEnd = position + newLength;
        }
    }
    if (m_paragraphInsertion >= 0) {
        // Qt can trim the space before closing heading hashes on Enter. Its
        // display replacement must not move those source hashes into the body.
        const int sourceCursor = m_paragraphInsertion + 1;
        m_source.insert(m_paragraphInsertion, "\n"); m_paragraphInsertion = -1;
        m_inlineDecorations->translateEdit(prefix, oldEnd - prefix, newEnd - prefix);
        m_display = display; m_sourceMap.rebuild(m_source, m_display, {{newEnd, sourceCursor}});
        refreshInlineCursorFormat(); emit sourceEdited(m_source, sourceCursor); m_markupTimer->start(); return;
    }
    QString inserted = display.mid(prefix, newEnd - prefix);
    const QTextBlock block = document()->findBlock(prefix);
    if (inserted.contains('\n') && block.textList() && !block.blockFormat().nonBreakableLines()) {
        const int raw = m_sourceMap.sourcePosition(prefix); const int lineStart = m_source.lastIndexOf('\n', qMax(0, raw - 1)) + 1;
        const auto match = QRegularExpression("^( *)([-+*]|[0-9]+[.)]) +(?:\\[[ xX]\\] +)?").match(m_source.mid(lineStart));
        if (match.hasMatch()) {
            QString marker = match.captured(2); const auto number = QRegularExpression("^([0-9]+)([.)])$").match(marker);
            if (number.hasMatch()) marker = QString::number(number.captured(1).toInt() + 1) + number.captured(2);
            inserted.replace("\n", "\n" + match.captured(1) + marker + " ");
        }
    } else if (inserted.contains('\n') && !block.blockFormat().nonBreakableLines() && !block.blockFormat().hasProperty(QTextFormat::BlockCodeFence)
        && !MarkdownFenceCompleter::isOpeningFence(m_source, m_sourceMap.sourcePosition(prefix))) {
        const int quoteLevel = m_headingExit ? 0 : block.blockFormat().intProperty(QTextFormat::BlockQuoteLevel);
        const QString quote = QString("> ").repeated(quoteLevel);
        inserted.replace("\n", quoteLevel ? "\n" + quote : "\n");
    }
    inserted.replace(QChar::LineSeparator, block.blockFormat().nonBreakableLines() || block.blockFormat().hasProperty(QTextFormat::BlockCodeFence) ? "\n" : "  \n");
    int paragraphCaret = -1;
    if (oldEnd == prefix && !inserted.isEmpty() && block.position() == prefix
        && !QTextCursor(block).currentTable() && block.previous().isValid()
        && QTextCursor(block.previous()).currentTable()) {
        const int raw = m_sourceMap.sourcePosition(prefix);
        const auto paragraph = MarkdownTableEditing::insertParagraph(m_source, raw, inserted);
        if (paragraph.handled) {
            inserted = paragraph.source.mid(raw, paragraph.source.size() - m_source.size());
            paragraphCaret = paragraph.caret;
        }
    }
    m_inlineDecorations->translateEdit(prefix, oldEnd - prefix, newEnd - prefix);
    const QString before = m_source;
    int sourceCursor = 0; m_source = m_sourceMap.applyEdit(prefix, oldEnd - prefix, inserted, display.mid(prefix, newEnd - prefix), &sourceCursor);
    if (m_revealed.valid() && !m_sourceMap.literalRanges().isEmpty()) {
        m_revealed.start = m_sourceMap.literalRanges().front().start; m_revealed.end = m_sourceMap.literalRanges().front().end;
    }
    const auto metadata = MarkdownCodeEditing::removeEmptyMetadata(before, m_source, sourceCursor);
    if (metadata.handled) { applySourceEdit(metadata); return; }
    if (oldEnd > prefix && m_display.mid(prefix, oldEnd - prefix).contains('\n')) {
        m_markupRefresh = true; setMarkdownSource(m_path, m_source); m_markupRefresh = false;
        restoreSourceCursor(sourceCursor, sourceCursor); emit sourceEdited(m_source, sourceCursor); return;
    }
    if (paragraphCaret >= 0) {
        sourceCursor = paragraphCaret;
        m_sourceMap.setBoundary(newEnd, sourceCursor);
    }
    m_display = display;
    refreshInlineCursorFormat();
    emit sourceEdited(m_source, sourceCursor); m_markupTimer->start();
}
QVariant MarkdownDocumentView::loadResource(int type, const QUrl &url) {
    if (type == QTextDocument::ImageResource) return m_images.value(url, placeholder(QStringLiteral("图片地址无法读取。")));
    return {};
}
void MarkdownDocumentView::loadImage(const QUrl &url, int generation) {
    if (generation != m_generation) return;
    auto *loader = new ImageLoader(this); m_pending.insert(url, loader);
    auto finish = [this, url, generation, loader](const QImage &image, const QString &error) {
        loader->deleteLater(); if (generation != m_generation) return;
        m_pending.remove(url);
        if (error.isEmpty()) { m_loaded.insert(url, image); m_images.insert(url, image); }
        else { m_errors.insert(url, error); m_images.insert(url, placeholder(QStringLiteral("图片读取失败：") + error)); }
        document()->addResource(QTextDocument::ImageResource, url, m_images.value(url));
        fitImages(); document()->markContentsDirty(0, document()->characterCount()); viewport()->update();
        if (m_loaded.size() + m_errors.size() == m_sources.size()) emit imagesSettled();
    };
    connect(loader, &ImageLoader::loaded, this, [finish](const QByteArray &, const QImage &image, const QString &) { finish(image, {}); });
    connect(loader, &ImageLoader::failed, this, [finish](const QString &error) { finish({}, error); });
    loader->load(m_sources.value(url), m_path);
}
void MarkdownDocumentView::fitImages() {
    if (m_preparing) return; m_preparing = true;
    const qreal available = qMax(80, viewport()->width() - 2 * int(document()->documentMargin()));
    struct ImageSpan { int position, length; QTextImageFormat format; }; QList<ImageSpan> spans;
    for (auto block = document()->begin(); block.isValid(); block = block.next()) for (auto it = block.begin(); !it.atEnd(); ++it) {
        const auto fragment = it.fragment(); if (!fragment.isValid() || !fragment.charFormat().isImageFormat()) continue;
        auto format = fragment.charFormat().toImageFormat(); const QUrl url(format.name()); if (!m_images.contains(url)) continue;
        if (!m_requestedSizes.contains(fragment.position())) m_requestedSizes.insert(fragment.position(), {format.width(), format.height()});
        const QImage image = m_images.value(url); const QSizeF requested = m_requestedSizes.value(fragment.position());
        qreal width = requested.width() > 0 ? requested.width() : image.width();
        qreal height = requested.height() > 0 ? requested.height() : width * image.height() / qMax(1, image.width());
        if (width > available) { height *= available / width; width = available; }
        if (qAbs(format.width() - width) < 0.5 && qAbs(format.height() - height) < 0.5) continue;
        format.setWidth(width); format.setHeight(height); spans.append({fragment.position(), fragment.length(), format});
    }
    for (const auto &span : spans) { QTextCursor cursor(document()); cursor.setPosition(span.position); cursor.setPosition(span.position + span.length, QTextCursor::KeepAnchor); cursor.setCharFormat(span.format); }
    document()->setModified(false); m_preparing = false;
}
void MarkdownDocumentView::resizeEvent(QResizeEvent *event) {
    QTextBrowser::resizeEvent(event);
    const bool preparing = m_preparing; m_preparing = true;
    auto root = document()->rootFrame()->frameFormat(); root.setBottomMargin(24); document()->rootFrame()->setFrameFormat(root);
    m_preparing = preparing; fitImages();
}
void MarkdownDocumentView::paintEvent(QPaintEvent *event) {
    const int offset = verticalScrollBar()->value();
    const auto &settings = MarkdownSettingsStore::current();
    { QPainter background(viewport()); m_codeDecoration->paintBackground(background); }
    { QPainter background(viewport()); m_inlineDecorations->paintCodeBackground(background, m_sourceMap); }
    QTextBrowser::paintEvent(event); QPainter painter(viewport()); m_codeDecoration->paintHeaders(painter); m_inlineDecorations->paint(painter); painter.setPen(QPen(QColor("#83a88f"), 3));
    for (auto block = document()->begin(); block.isValid(); block = block.next()) if (block.blockFormat().intProperty(QTextFormat::BlockQuoteLevel) > 0 && !m_inlineDecorations->hasBlockPrefix(block.position())) {
        const QRectF rect = document()->documentLayout()->blockBoundingRect(block).translated(0, -offset);
        if (rect.bottom() < 0 || rect.top() > viewport()->height()) continue;
        painter.drawLine(QPointF(rect.left() - 10, rect.top()), QPointF(rect.left() - 10, rect.bottom()));
    }
    painter.setPen(QPen(palette().color(QPalette::Base).lighter(180), 1));
    for (auto block = document()->begin(); block.isValid(); block = block.next()) if (settings.enabled(MarkdownSettings::HeadingRules) && block.blockFormat().headingLevel() > 0 && block.blockFormat().headingLevel() <= 2) {
        const QRectF rect = document()->documentLayout()->blockBoundingRect(block).translated(-horizontalScrollBar()->value(), -offset);
        if (rect.bottom() < 0 || rect.top() > viewport()->height()) continue;
        painter.drawLine(QPointF(rect.left(), rect.bottom() + 5), QPointF(rect.right(), rect.bottom() + 5));
    }
}
void MarkdownDocumentView::goToSourcePosition(int position) {
    QTextCursor cursor(document()); cursor.setPosition(qBound(0, m_sourceMap.displayPosition(position), document()->characterCount() - 1));
    // Heading source positions include the hidden '#'; use the block start.
    const auto headings = MarkdownParser::outline(m_source); int index = -1;
    for (int i = 0; i < headings.size(); ++i) if (headings[i].position <= position) index = i;
    if (index >= 0) { int heading = 0; for (auto block = document()->begin(); block.isValid(); block = block.next()) if (block.blockFormat().headingLevel() > 0 && heading++ == index) { cursor = QTextCursor(block); break; } }
    setTextCursor(cursor); ensureCursorVisible();
    verticalScrollBar()->setValue(verticalScrollBar()->value() + cursorRect(cursor).top() - viewport()->height() * 0.28); setFocus();
}
void MarkdownDocumentView::insertImages(const QMimeData *data) {
    if (!ImageInput::canInsert(data, m_hostedLinks)) return;
    emit imageInputPositionRequested(m_sourceMap.sourcePosition(textCursor().position()));
    const auto urls = ImageInput::urls(data, m_hostedLinks);
    if (!urls.isEmpty()) emit imageFilesInserted(urls);
    else { const QImage image = ImageInput::image(data); if (!image.isNull()) emit clipboardImageInserted(image); }
}
bool MarkdownDocumentView::canInsertFromMimeData(const QMimeData *data) const { return ImageInput::canInsert(data, m_hostedLinks) || data->hasText(); }
void MarkdownDocumentView::insertFromMimeData(const QMimeData *data) {
    emit sourceEditStarted(hasSourceSelection() ? m_selectionCaret : inlineCursorSourcePosition());
    const auto finished = qScopeGuard([this] { emit sourceEditFinished(); });
    if (ImageInput::canInsert(data, m_hostedLinks)) insertImages(data);
    else if (data->hasText()) {
        if (auto *table = textCursor().currentTable()) {
            const auto cell = table->cellAt(textCursor());
            const int index = table->format().intProperty(tableSourceIndex) - 1;
            if (data->text().contains('\t')) applySourceEdit(MarkdownTableEditing::operate(m_source, index,
                cell.row(), cell.column(), MarkdownTableEditing::Action::Paste, data->text()));
            else applySourceEdit(MarkdownTableEditing::pasteCell(m_source,
                hasSourceSelection() ? m_selectionAnchor : m_sourceMap.sourcePosition(textCursor().anchor()),
                hasSourceSelection() ? m_selectionCaret : inlineCursorSourcePosition(), index, cell.row(), cell.column(), data->text()));
            return;
        }
        const QString pasted = MarkdownImport::paste(m_source, inlineCursorSourcePosition(), data->text());
        if (hasSourceSelection()) replaceSourceSelection(pasted);
        else if (!textCursor().hasSelection() && m_sourceMap.isInsideBlockMarker(inlineCursorSourcePosition())) applySourceInsertion(pasted);
        else {
            int caret = 0;
            MarkdownEdit edit; edit.source = m_sourceMap.edited(textCursor().selectionStart(), textCursor().selectionEnd() - textCursor().selectionStart(), pasted, &caret);
            edit.anchor = edit.caret = caret; edit.handled = true; edit.changed = edit.source != m_source; applySourceEdit(edit);
        }
    }
}
void MarkdownDocumentView::pasteImages() { insertFromMimeData(QApplication::clipboard()->mimeData()); }
void MarkdownDocumentView::setInlineBoundary(const MarkdownSourceMap::InlineCode &code, bool atEnd, bool outside) {
    m_sourceMap.setBoundary(textCursor().position(), atEnd ? (outside ? code.sourceEnd : code.contentEnd) : (outside ? code.sourceStart : code.contentStart));
    refreshInlineCursorFormat();
}
void MarkdownDocumentView::refreshInlineCursorFormat() {
    if (m_preparing) return;
    if (!m_selecting && m_selectionAnchor >= 0 && (textCursor().anchor() != m_selectionDisplayAnchor || textCursor().position() != m_selectionDisplayCaret)) clearSourceSelection();
    const bool modified = document()->isModified(); m_preparing = true;
    m_inlineDecorations->refresh(m_sourceMap); m_preparing = false; document()->setModified(modified);
    if (textCursor().hasSelection()) return;
    if (!m_rebuilding && !m_selecting && !m_revealScheduled) {
        const int caret = inlineCursorSourcePosition(); MarkdownSpelling::Region target;
        if (m_revealed.valid() && caret >= m_revealed.start && caret <= m_revealed.end) target = m_revealed;
        else for (const auto &region : MarkdownSpelling::regions(m_source)) {
            if ((region.kind == MarkdownSpelling::Kind::Link || region.kind == MarkdownSpelling::Kind::Entity)
                && caret >= region.contentStart && caret <= region.contentEnd) { target = region; break; }
        }
        if (target.start != m_revealed.start || target.end != m_revealed.end) {
            m_revealScheduled = true; const QString snapshot = m_source;
            QTimer::singleShot(0, this, [this, target, snapshot] {
                m_revealScheduled = false;
                if (m_source != snapshot) { refreshInlineCursorFormat(); return; }
                revealSyntax(target, inlineCursorSourcePosition());
            });
        }
    }
    const auto code = m_sourceMap.inlineFormatAt(textCursor().position()); if (!code.valid()) return;
    const int source = m_sourceMap.sourcePosition(textCursor().position());
    if (source == code.sourceStart || source == code.sourceEnd) {
        QTextCharFormat format; const auto &settings = MarkdownSettingsStore::current(); format.setFont(MarkdownTypography::bodyFont(m_zoomPercent));
        format.setFontFixedPitch(false); format.setBackground(QBrush(Qt::NoBrush)); format.setForeground(settings.color(MarkdownSettings::TextColor));
        const int heading = textCursor().blockFormat().headingLevel();
        if (heading) { format.setFontPointSize(MarkdownTypography::headingPointSize(heading, m_zoomPercent)); format.setFontWeight(settings.enabled(MarkdownSettings::BoldHeadings) ? QFont::Bold : QFont::Normal); format.setForeground(settings.color(MarkdownSettings::HeadingColor)); }
        setCurrentCharFormat(format);
    } else {
        QTextCursor sample(document()); sample.setPosition(code.displayStart); sample.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        auto format = sample.charFormat(); format.clearProperty(QTextFormat::FontLetterSpacing); format.clearProperty(QTextFormat::FontLetterSpacingType);
        setCurrentCharFormat(format);
    }
}
void MarkdownDocumentView::revealSyntax(const MarkdownSpelling::Region &region, int caret) {
    m_revealed = region; m_markupRefresh = true; setMarkdownSource(m_path, m_source); m_markupRefresh = false;
    restoreSourceCursor(caret, caret);
}
void MarkdownDocumentView::mouseDoubleClickEvent(QMouseEvent *event) {
    const int source = sourcePositionAt(event->position().toPoint());
    for (const auto &region : MarkdownSpelling::regions(m_source)) if (region.kind == MarkdownSpelling::Kind::Image && source >= region.start && source <= region.end) {
        clearSourceSelection(); m_selecting = false; revealSyntax(region, region.start); event->accept(); return;
    }
    QTextBrowser::mouseDoubleClickEvent(event);
}
void MarkdownDocumentView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && event->modifiers().testFlag(Qt::ControlModifier)) {
        auto target = anchorAt(event->position().toPoint());
        if (target.isEmpty()) {
            const int position = sourcePositionAt(event->position().toPoint());
            for (const auto &region : MarkdownSpelling::regions(m_source)) if (region.kind == MarkdownSpelling::Kind::Link
                && position >= region.start && position <= region.end) { target = MarkdownSpelling::target(m_source, region); break; }
        }
        if (!target.isEmpty()) { emit anchorClicked(QUrl(target)); event->accept(); return; }
    }
    if (event->button() == Qt::LeftButton) {
        const auto cursor = cursorForPosition(event->position().toPoint()); const auto block = cursor.block();
        const int left = QTextBrowser::cursorRect(QTextCursor(block)).left();
        if (block.textList() && event->position().x() < left && event->position().x() >= left - 28) {
            const int caret = m_sourceMap.sourcePosition(block.position());
            for (const auto &marker : MarkdownBlockSyntax::markers(m_source)) if (marker.prefix && caret >= marker.start && caret <= marker.end) {
                const auto match = QRegularExpression("\\[([ xX])\\]").match(marker.text);
                if (!match.hasMatch()) continue;
                MarkdownEdit edit{m_source, caret, caret}; edit.source[marker.start + match.capturedStart(1)] = match.captured(1) == " " ? 'x' : ' ';
                edit.handled = edit.changed = true; emit sourceEditStarted(inlineCursorSourcePosition()); applySourceEdit(edit); emit sourceEditFinished(); event->accept(); return;
            }
        }
    }
    clearSourceSelection();
    const auto marker = m_inlineDecorations->hitTest(event->position().toPoint());
    m_preparing = true; QTextBrowser::mousePressEvent(event); m_preparing = false;
    if (event->button() == Qt::LeftButton && marker.valid() && !event->modifiers().testFlag(Qt::ShiftModifier)) {
        m_sourceMap.setBoundary(marker.displayPosition, marker.sourcePosition);
        QTextCursor cursor(document()); cursor.setPosition(marker.displayPosition); setTextCursor(cursor);
        refreshInlineCursorFormat();
    } else {
        refreshInlineCursorFormat();
        if (event->button() == Qt::LeftButton && !textCursor().hasSelection()) {
            const auto code = m_sourceMap.inlineFormatAt(textCursor().position());
            if (code.valid() && textCursor().position() == code.displayEnd) setInlineBoundary(code, true, event->position().x() >= QTextBrowser::cursorRect().left());
            else if (code.valid() && textCursor().position() == code.displayStart) setInlineBoundary(code, false, event->position().x() < QTextBrowser::cursorRect().left());
        }
    }
    if (event->button() == Qt::LeftButton) {
        m_selecting = true;
        m_selectionAnchor = m_sourceMap.sourcePosition(textCursor().anchor()); m_selectionCaret = inlineCursorSourcePosition();
        m_selectionDisplayAnchor = textCursor().anchor(); m_selectionDisplayCaret = textCursor().position();
        m_inlineDecorations->setSourceSelection(m_selectionAnchor, m_selectionCaret, true);
    }
}
int MarkdownDocumentView::sourcePositionAt(const QPoint &point) const {
    const auto hit = m_inlineDecorations->hitTest(point); if (hit.valid()) return hit.sourcePosition;
    const auto cursor = cursorForPosition(point);
    const auto boundary = m_sourceMap.markerBoundaryAt(cursor.position());
    if (boundary.valid()) return point.x() < QTextBrowser::cursorRect(cursor).left() ? boundary.sourceStart : boundary.sourceEnd;
    return m_sourceMap.sourcePosition(cursor.position());
}
void MarkdownDocumentView::mouseMoveEvent(QMouseEvent *event) {
    if (!m_selecting || !event->buttons().testFlag(Qt::LeftButton)) { QTextBrowser::mouseMoveEvent(event); return; }
    const int source = sourcePositionAt(event->position().toPoint());
    // Let Qt keep its drag scrolling, then replace its glyph-based range with
    // the exact source endpoints (two delimiters may share a display position).
    m_preparing = true; QTextBrowser::mouseMoveEvent(event);
    QTextCursor cursor(document()); cursor.setPosition(m_sourceMap.displayPosition(m_selectionAnchor));
    cursor.setPosition(m_sourceMap.displayPosition(source), QTextCursor::KeepAnchor); setTextCursor(cursor); m_preparing = false;
    m_selectionCaret = source; m_selectionDisplayAnchor = cursor.anchor(); m_selectionDisplayCaret = cursor.position();
    m_sourceMap.setBoundary(cursor.position(), source);
    m_inlineDecorations->setSourceSelection(m_selectionAnchor, source, true); refreshInlineCursorFormat();
}
void MarkdownDocumentView::mouseReleaseEvent(QMouseEvent *event) {
    m_preparing = true; QTextBrowser::mouseReleaseEvent(event); m_preparing = false;
    if (event->button() != Qt::LeftButton) return;
    m_selecting = false;
    if (!hasSourceSelection()) clearSourceSelection();
    else m_inlineDecorations->setSourceSelection(m_selectionAnchor, m_selectionCaret, false);
    refreshInlineCursorFormat();
    event->accept();
}
bool MarkdownDocumentView::hasSourceSelection() const { return m_selectionAnchor >= 0 && m_selectionCaret >= 0 && m_selectionAnchor != m_selectionCaret; }
void MarkdownDocumentView::clearSourceSelection() {
    m_selecting = false; m_selectionAnchor = m_selectionCaret = m_selectionDisplayAnchor = m_selectionDisplayCaret = -1;
    m_inlineDecorations->setSourceSelection(-1, -1, false);
}
QMimeData *MarkdownDocumentView::createMimeDataFromSelection() const {
    if (!hasSourceSelection()) return QTextBrowser::createMimeDataFromSelection();
    auto *data = new QMimeData;
    data->setText(m_source.mid(qMin(m_selectionAnchor, m_selectionCaret), qAbs(m_selectionCaret - m_selectionAnchor))); return data;
}
void MarkdownDocumentView::copySourceSelection() { QApplication::clipboard()->setMimeData(createMimeDataFromSelection()); }
void MarkdownDocumentView::replaceSourceSelection(const QString &text) {
    const int first = qMin(m_selectionAnchor, m_selectionCaret), length = qAbs(m_selectionCaret - m_selectionAnchor);
    MarkdownEdit edit; edit.source = m_source;
    const auto ranges = text.isEmpty() ? MarkdownSpelling::protectDeletions(m_source, {{first, first + length}}) : QVector<MarkdownSpelling::Range>{{first, first + length}};
    for (auto it = ranges.crbegin(); it != ranges.crend(); ++it) edit.source.remove(it->start, it->end - it->start);
    edit.source.insert(first, text);
    edit.anchor = edit.caret = first + text.size(); edit.changed = edit.handled = true; clearSourceSelection(); applySourceEdit(edit);
}
void MarkdownDocumentView::applySourceInsertion(const QString &text) {
    const int start = inlineCursorSourcePosition(), caret = start + text.size();
    QString source = m_source; source.insert(start, text); setMarkdownSource(m_path, source);
    QTextCursor cursor(document()); cursor.setPosition(m_sourceMap.displayPosition(caret)); setTextCursor(cursor);
    m_sourceMap.setBoundary(cursor.position(), caret); refreshInlineCursorFormat(); emit sourceEdited(m_source, caret); ensureCursorVisible();
}
void MarkdownDocumentView::applySourceEdit(const MarkdownEdit &inputEdit) {
    auto edit = inputEdit;
    const auto metadata = MarkdownCodeEditing::removeEmptyMetadata(m_source, edit.source, edit.caret);
    if (metadata.handled) edit = metadata;
    if (!edit.handled) return;
    if (edit.changed) setMarkdownSource(m_path, edit.source);
    restoreSourceCursor(edit.anchor, edit.caret);
    if (edit.changed) emit sourceEdited(m_source, edit.caret);
}
void MarkdownDocumentView::restoreSourceCursor(int anchor, int caret) {
    clearSourceSelection();
    anchor = qBound(0, anchor, int(m_source.size())); caret = qBound(0, caret, int(m_source.size()));
    QTextCursor cursor(document()); cursor.setPosition(m_sourceMap.displayPosition(anchor));
    cursor.setPosition(m_sourceMap.displayPosition(caret), QTextCursor::KeepAnchor); setTextCursor(cursor);
    if (anchor == caret) m_sourceMap.setBoundary(cursor.position(), caret);
    else {
        m_selectionAnchor = anchor; m_selectionCaret = caret;
        m_selectionDisplayAnchor = cursor.anchor(); m_selectionDisplayCaret = cursor.position();
        m_inlineDecorations->setSourceSelection(anchor, caret, false);
    }
    refreshInlineCursorFormat(); ensureCursorVisible();
}
QVariant MarkdownDocumentView::inputMethodQuery(Qt::InputMethodQuery query) const {
    if (query == Qt::ImCursorRectangle) return cursorRect().translated(viewport()->pos());
    return QTextBrowser::inputMethodQuery(query);
}
void MarkdownDocumentView::inputMethodEvent(QInputMethodEvent *event) {
    emit sourceEditStarted(hasSourceSelection() ? m_selectionCaret : inlineCursorSourcePosition());
    const auto finished = qScopeGuard([this] { emit sourceEditFinished(); });
    if (hasSourceSelection() && !event->commitString().isEmpty() && event->replacementStart() == 0 && event->replacementLength() == 0) {
        replaceSourceSelection(event->commitString()); event->accept(); return;
    }
    if (!textCursor().hasSelection() && m_sourceMap.isInsideBlockMarker(inlineCursorSourcePosition())
        && !event->commitString().isEmpty() && event->replacementStart() == 0 && event->replacementLength() == 0) {
        applySourceInsertion(event->commitString()); event->accept(); return;
    }
    QTextBrowser::inputMethodEvent(event);
}
QStringList MarkdownDocumentView::visibleBlockMarkers() const { return m_inlineDecorations->visibleBlockMarkers(); }
QStringList MarkdownDocumentView::visibleInlineMarkers() const { return m_inlineDecorations->visibleMarkers(); }
QRect MarkdownDocumentView::cursorRect() const { return cursorRect(textCursor()); }
QRect MarkdownDocumentView::cursorRect(const QTextCursor &cursor) const {
    const auto native = QTextBrowser::cursorRect(cursor);
    return cursor.position() == textCursor().position() && !textCursor().hasSelection() ? m_inlineDecorations->caretRect(native) : native;
}
void MarkdownDocumentView::keyPressEvent(QKeyEvent *event) {
    emit sourceEditStarted(hasSourceSelection() ? m_selectionCaret : inlineCursorSourcePosition());
    const auto finished = qScopeGuard([this] { emit sourceEditFinished(); });
    if (event->modifiers() == Qt::NoModifier && !textCursor().hasSelection()
        && (event->key() == Qt::Key_Down || event->key() == Qt::Key_Up)) {
        const auto *layout = textCursor().block().layout(); const bool down = event->key() == Qt::Key_Down;
        const int visual = layout ? layout->lineForTextPosition(textCursor().positionInBlock()).lineNumber() : 0;
        if (!layout || (down ? visual + 1 >= layout->lineCount() : visual <= 0)) {
            const int caret = inlineCursorSourcePosition(), start = caret ? m_source.lastIndexOf('\n', caret - 1) + 1 : 0;
            const int newline = m_source.indexOf('\n', start);
            const int next = down ? (newline < 0 ? -1 : newline + 1) : start > 0 ? m_source.lastIndexOf('\n', start - 2) + 1 : -1;
            for (const auto &region : MarkdownSpelling::regions(m_source)) if (region.start == next
                && (region.kind == MarkdownSpelling::Kind::Definition || region.kind == MarkdownSpelling::Kind::Rule || region.kind == MarkdownSpelling::Kind::Underline)) {
                revealSyntax(region, region.end); event->accept(); return;
            }
        }
    }
    if (event->key() == Qt::Key_Backspace && event->modifiers() == Qt::NoModifier && !textCursor().hasSelection() && !hasSourceSelection()
        && textCursor().position() > 0) {
        auto previous = textCursor(); previous.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
        if (previous.charFormat().isImageFormat()) { setTextCursor(previous); event->accept(); return; }
    }
    if (auto *table = textCursor().currentTable()) {
        const auto cell = table->cellAt(textCursor());
        const int index = table->format().intProperty(tableSourceIndex) - 1;
        using Action = MarkdownTableEditing::Action;
        const bool shift = event->modifiers() == Qt::ShiftModifier;
        const bool plain = event->modifiers() == Qt::NoModifier;
        const bool enter = event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter;
        const bool tab = event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab;
        if ((plain || shift) && ((enter && shift) || tab)) {
            const auto action = enter ? Action::AddRow : shift || event->key() == Qt::Key_Backtab ? Action::AddColumn : Action::NextCell;
            applySourceEdit(MarkdownTableEditing::operate(m_source, index, cell.row(), cell.column(), action));
            event->accept(); return;
        }
        if (plain && !textCursor().hasSelection() && (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete)) {
            const bool back = event->key() == Qt::Key_Backspace;
            const int boundary = back ? cell.firstCursorPosition().position() : cell.lastCursorPosition().position();
            if (textCursor().position() == boundary) {
                if (back && table->rows() == 1 && table->columns() == 1 && cell.firstCursorPosition().block().text().isEmpty())
                    applySourceEdit(MarkdownTableEditing::operate(m_source, index, 0, 0, Action::DeleteRow));
                event->accept(); return;
            }
        }
        if (plain && !textCursor().hasSelection() && event->key() >= Qt::Key_Left && event->key() <= Qt::Key_Down) {
            const auto operation = event->key() == Qt::Key_Left ? QTextCursor::Left : event->key() == Qt::Key_Right ? QTextCursor::Right
                : event->key() == Qt::Key_Up ? QTextCursor::Up : QTextCursor::Down;
            auto cursor = textCursor(); cursor.movePosition(operation);
            const bool sameCell = cursor.currentTable() == table && table->cellAt(cursor) == cell;
            if (sameCell) { QTextBrowser::keyPressEvent(event); return; }
            if (event->key() == Qt::Key_Down && cell.row() == table->rows() - 1) {
                applySourceEdit(MarkdownTableEditing::operate(m_source, index, cell.row(), cell.column(), Action::Leave));
            } else { setTextCursor(cursor); ensureCursorVisible(); }
            event->accept(); return;
        }
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && event->modifiers() == Qt::ShiftModifier) {
        QKeyEvent normal(QEvent::KeyPress, event->key(), Qt::NoModifier, event->text(), event->isAutoRepeat(), event->count());
        keyPressEvent(&normal); event->accept(); return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && event->modifiers() == Qt::NoModifier) {
        if (auto *table = textCursor().currentTable()) {
            const int anchor = hasSourceSelection() ? m_selectionAnchor : m_sourceMap.sourcePosition(textCursor().anchor());
            const int caret = hasSourceSelection() ? m_selectionCaret : inlineCursorSourcePosition();
            const auto cell = table->cellAt(textCursor());
            const int index = table->format().intProperty(tableSourceIndex) - 1;
            auto edit = index >= 0 ? MarkdownTableEditing::newline(m_source, anchor, caret, index, cell.row(), cell.column())
                                  : MarkdownTableEditing::newline(m_source, anchor, caret);
            if (edit.handled) applySourceEdit(edit);
            event->accept(); return;
        }
    }
    if (hasSourceSelection()) {
        if (event->matches(QKeySequence::Copy)) { copySourceSelection(); event->accept(); return; }
        if (event->matches(QKeySequence::Cut)) { copySourceSelection(); replaceSourceSelection({}); event->accept(); return; }
        if (event->matches(QKeySequence::Paste)) { insertFromMimeData(QApplication::clipboard()->mimeData()); event->accept(); return; }
        if (event->modifiers() == Qt::NoModifier && (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete)) {
            replaceSourceSelection({}); event->accept(); return;
        }
        if ((event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::ShiftModifier) && !event->text().isEmpty()
            && event->key() != Qt::Key_Tab && event->key() != Qt::Key_Backtab) {
            replaceSourceSelection(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ? QString("\n") : event->text()); event->accept(); return;
        }
        if (event->modifiers() == Qt::ShiftModifier && (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)) {
            const bool right = event->key() == Qt::Key_Right;
            QTextBoundaryFinder graphemes(QTextBoundaryFinder::Grapheme, m_source); graphemes.setPosition(m_selectionCaret);
            const int next = right ? graphemes.toNextBoundary() : graphemes.toPreviousBoundary();
            if (next >= 0) {
                m_selectionCaret = next; m_preparing = true;
                QTextCursor cursor(document()); cursor.setPosition(m_sourceMap.displayPosition(m_selectionAnchor));
                cursor.setPosition(m_sourceMap.displayPosition(next), QTextCursor::KeepAnchor); setTextCursor(cursor); m_preparing = false;
                m_selectionDisplayAnchor = cursor.anchor(); m_selectionDisplayCaret = cursor.position(); m_sourceMap.setBoundary(cursor.position(), next);
                m_inlineDecorations->setSourceSelection(m_selectionAnchor, next, false); refreshInlineCursorFormat(); ensureCursorVisible();
            }
            event->accept(); return;
        }
        if (event->modifiers() == Qt::NoModifier && (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)) {
            const int caret = event->key() == Qt::Key_Left ? qMin(m_selectionAnchor, m_selectionCaret) : qMax(m_selectionAnchor, m_selectionCaret);
            clearSourceSelection(); QTextCursor cursor(document()); cursor.setPosition(m_sourceMap.displayPosition(caret));
            m_sourceMap.setBoundary(cursor.position(), caret); setTextCursor(cursor); refreshInlineCursorFormat(); event->accept(); return;
        }
    }
    if (event->key() == Qt::Key_Backspace && event->modifiers() == Qt::NoModifier && !textCursor().hasSelection()) {
        auto edit = MarkdownListEditing::backspaceIndent(m_source, inlineCursorSourcePosition());
        if (!edit.handled) edit = MarkdownBlockSyntax::backspaceEmptyPrefix(m_source, inlineCursorSourcePosition());
        if (!edit.handled) edit = MarkdownCodeEditing::backspace(m_source, inlineCursorSourcePosition());
        if (!edit.handled) edit = MarkdownParagraphEditing::backspace(m_source, inlineCursorSourcePosition());
        const QString body = textCursor().block().text();
        if (!edit.handled && (body.isEmpty() || !body.trimmed().isEmpty())) edit = MarkdownListEditing::backspace(m_source, inlineCursorSourcePosition());
        if (edit.handled) { applySourceEdit(edit); event->accept(); return; }
    }
    // At the beginning of a heading, Backspace first restores the heading marker.
    if (event->key() == Qt::Key_Backspace && event->modifiers() == Qt::NoModifier && !textCursor().hasSelection()
        && textCursor().positionInBlock() == 0 && textCursor().blockFormat().headingLevel() > 0) {
        const int sourceCaret = m_sourceMap.sourcePosition(textCursor().position());
        for (const auto &heading : MarkdownParser::headingMarkers(m_source)) {
            if (!heading.separated || sourceCaret != heading.contentStart) continue;
            const int caret = heading.markerStart + heading.level;
            QString source = m_source; source.remove(caret, heading.contentStart - caret);
            setMarkdownSource(m_path, source);
            QTextCursor cursor(document()); cursor.setPosition(m_sourceMap.displayPosition(heading.markerStart) + heading.level);
            m_sourceMap.setBoundary(cursor.position(), caret); setTextCursor(cursor);
            emit sourceEdited(m_source, caret); ensureCursorVisible(); event->accept(); return;
        }
    }
    if (event->modifiers() == Qt::NoModifier && !textCursor().hasSelection()
        && (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete)) {
        const bool right = event->key() == Qt::Key_Delete;
        const int next = m_sourceMap.stepAcrossMarker(textCursor().position(), inlineCursorSourcePosition(), right);
        const int removed = next - (right ? 1 : 0);
        if (next >= 0 && removed >= 0) {
            QString source = m_source; source.remove(removed, 1); setMarkdownSource(m_path, source);
            QTextCursor cursor(document()); cursor.setPosition(m_sourceMap.displayPosition(removed)); setTextCursor(cursor);
            m_sourceMap.setBoundary(cursor.position(), removed); refreshInlineCursorFormat();
            emit sourceEdited(m_source, removed); ensureCursorVisible(); event->accept(); return;
        }
    }
    if (!textCursor().hasSelection() && m_sourceMap.isInsideBlockMarker(inlineCursorSourcePosition())
        && (event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::ShiftModifier)) {
        const QString inserted = event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ? QString("\n") : event->text();
        if (!inserted.isEmpty() && event->key() != Qt::Key_Backspace && event->key() != Qt::Key_Delete && event->key() != Qt::Key_Tab && event->key() != Qt::Key_Backtab
            && event->key() != Qt::Key_Return && event->key() != Qt::Key_Enter) {
            applySourceInsertion(inserted); event->accept(); return;
        }
    }
    const bool indent = (event->key() == Qt::Key_Tab && (event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::ShiftModifier))
        || (event->key() == Qt::Key_Backtab && (event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::ShiftModifier));
    const bool newline = (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && event->modifiers() == Qt::NoModifier;
    if (indent || newline) {
        const int anchor = hasSourceSelection() ? m_selectionAnchor : m_sourceMap.sourcePosition(textCursor().anchor());
        const int caret = hasSourceSelection() ? m_selectionCaret : m_sourceMap.sourcePosition(textCursor().position());
        auto edit = indent ? MarkdownCodeEditing::indent(m_source, anchor, caret, event->key() == Qt::Key_Backtab || event->modifiers() == Qt::ShiftModifier)
                                 : MarkdownCodeEditing::newline(m_source, anchor, caret);
        if (!edit.handled) edit = indent ? MarkdownListEditing::indent(m_source, anchor, caret, event->key() == Qt::Key_Backtab || event->modifiers() == Qt::ShiftModifier)
                                       : MarkdownListEditing::newline(m_source, anchor, caret);
        if (!edit.handled && newline) {
            const auto format = m_sourceMap.inlineFormatAt(textCursor().position());
            const int end = !textCursor().hasSelection() && textCursor().atBlockEnd() && format.valid() && format.displayEnd == textCursor().position()
                ? format.sourceEnd : caret;
            edit = MarkdownParagraphEditing::newline(m_source, anchor == caret ? end : anchor, end);
        }
        if (edit.handled) {
            applySourceEdit(edit);
            event->accept(); return;
        }
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && event->modifiers() == Qt::NoModifier
        && !textCursor().hasSelection() && textCursor().block().text().trimmed().isEmpty()
        && (textCursor().block().textList() || textCursor().blockFormat().intProperty(QTextFormat::BlockQuoteLevel) > 0)
        && !textCursor().blockFormat().hasProperty(QTextFormat::BlockCodeFence)) {
        const auto edit = MarkdownSyntax::leaveEmptyContainer(m_source, m_sourceMap.sourcePosition(textCursor().position()));
        if (edit.changed) {
            m_preparing = true; m_inlineDecorations->clear();
            auto cursor = textCursor(); if (auto *list = cursor.block().textList()) list->remove(cursor.block());
            cursor.setBlockFormat(QTextBlockFormat()); QTextCharFormat character; character.setFont(MarkdownTypography::bodyFont(m_zoomPercent));
            character.setForeground(MarkdownSettingsStore::current().color(MarkdownSettings::TextColor)); cursor.setBlockCharFormat(character);
            setTextCursor(cursor); setCurrentCharFormat(character); m_source = edit.source; m_display = displayedText();
            m_sourceMap.rebuild(m_source, m_display); m_sourceMap.setBoundary(cursor.position(), edit.caret); m_preparing = false;
            refreshInlineCursorFormat(); emit sourceEdited(m_source, edit.caret); m_markupTimer->start(); ensureCursorVisible(); event->accept(); return;
        }
    }
    if (event->modifiers() == Qt::NoModifier && (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)) {
        const bool right = event->key() == Qt::Key_Right, selection = textCursor().hasSelection();
        const int before = textCursor().position();
        const int next = selection ? -1 : m_sourceMap.stepAcrossMarker(before, inlineCursorSourcePosition(), right);
        if (next >= 0) {
            m_sourceMap.setBoundary(before, next); refreshInlineCursorFormat(); ensureCursorVisible(); event->accept(); return;
        }
        m_preparing = true; QTextBrowser::keyPressEvent(event); m_preparing = false;
        const auto boundary = m_sourceMap.markerBoundaryAt(textCursor().position());
        if (boundary.valid() && (before != textCursor().position() || selection))
            m_sourceMap.setBoundary(boundary.displayPosition, (right != selection) ? boundary.sourceStart : boundary.sourceEnd);
        refreshInlineCursorFormat(); return;
    }
    if (event->key() == Qt::Key_Down && event->modifiers() == Qt::NoModifier && !textCursor().hasSelection()
        && textCursor().blockFormat().hasProperty(QTextFormat::BlockCodeFence)
        && !CodeBlockFormat::samePanel(textCursor().block(), textCursor().block().next())
        && textCursor().block().layout()->lineForTextPosition(textCursor().positionInBlock()).lineNumber()
            + 1 >= textCursor().block().layout()->lineCount()) {
        auto next = textCursor().block().next();
        while (next.isValid() && next.blockFormat().hasProperty(QTextFormat::BlockCodeFence)) next = next.next();
        if (next.isValid()) { setTextCursor(QTextCursor(next)); ensureCursorVisible(); event->accept(); return; }
        const auto fences = MarkdownParser::codeFences(m_source);
        if (!fences.isEmpty() && fences.back().closed()) {
            QString source = m_source; if (!source.endsWith('\n')) source += '\n';
            setMarkdownSource(m_path, source); moveCursor(QTextCursor::End);
            emit sourceEdited(m_source, m_source.size()); ensureCursorVisible(); event->accept(); return;
        }
    }
    if (event->matches(QKeySequence::Undo)) { emit undoRequested(); event->accept(); return; }
    if (event->matches(QKeySequence::Redo)) { emit redoRequested(); event->accept(); return; }
    if (newline) {
        if (!textCursor().hasSelection() && textCursor().atBlockEnd()) {
            const auto format = m_sourceMap.inlineFormatAt(textCursor().position());
            if (format.valid() && format.displayEnd == textCursor().position()) m_sourceMap.setBoundary(textCursor().position(), format.sourceEnd);
        }
        if (!textCursor().hasSelection() && textCursor().blockFormat().headingLevel() > 0 && textCursor().atBlockEnd()) {
            const int caret = inlineCursorSourcePosition(); int end = m_source.indexOf('\n', caret);
            if (end < 0) end = m_source.size();
            if (end < m_source.size()) {
                int underlineEnd = m_source.indexOf('\n', end + 1); if (underlineEnd < 0) underlineEnd = m_source.size();
                static const QRegularExpression underline("^ {0,3}(?:=+|-+)[ \\t]*$");
                if (underline.match(m_source.mid(end + 1, underlineEnd - end - 1)).hasMatch()) end = underlineEnd;
            }
            // A Setext underline or trailing ATX hashes are part of the
            // heading, even though they have no displayed glyphs.
            if (!textCursor().block().textList()) m_paragraphInsertion = end;
        }
        // Decoration margins belong to the old block, and must not be cloned
        // into the paragraph Qt creates on Enter.
        m_preparing = true; m_inlineDecorations->clear(); m_preparing = false;
        auto character = currentCharFormat(); character.clearProperty(QTextFormat::FontLetterSpacing);
        character.clearProperty(QTextFormat::FontLetterSpacingType); setCurrentCharFormat(character);
    }
    m_headingExit = newline && !textCursor().block().textList() && textCursor().blockFormat().headingLevel() > 0;
    QTextBrowser::keyPressEvent(event);
    const bool headingExit = m_headingExit; m_headingExit = false;
    m_paragraphInsertion = -1;
    if (event->key() == Qt::Key_Space && event->modifiers() == Qt::NoModifier
        && MarkdownSettingsStore::current().enabled(MarkdownSettings::AutoRender)) {
        const int caret = inlineCursorSourcePosition();
        const auto created = MarkdownTableEditing::create(m_source, caret);
        if (created.handled) { applySourceEdit(created); event->accept(); return; }
        for (const auto &marker : MarkdownBlockSyntax::markers(m_source)) if (marker.prefix && marker.end == caret) {
            m_markupRefresh = true; setMarkdownSource(m_path, m_source); m_markupRefresh = false; break;
        }
    }
    if (event->modifiers() == Qt::NoModifier && !event->text().isEmpty() && QString("-=*_").contains(event->text())
        && MarkdownSettingsStore::current().enabled(MarkdownSettings::AutoRender)) {
        const int caret = inlineCursorSourcePosition();
        for (const auto &region : MarkdownSpelling::regions(m_source)) if (region.end == caret
            && (region.kind == MarkdownSpelling::Kind::Rule || region.kind == MarkdownSpelling::Kind::Underline)) {
            m_markupRefresh = true; setMarkdownSource(m_path, m_source); m_markupRefresh = false; restoreSourceCursor(caret, caret); break;
        }
    }
    if (newline) {
        auto cursor = textCursor(); auto format = cursor.blockFormat();
        if (headingExit) { format.setProperty(QTextFormat::BlockQuoteLevel, 0); format.setHeadingLevel(0); format.clearProperty(QTextFormat::BackgroundBrush); }
        if (!cursor.block().textList() && !format.hasProperty(QTextFormat::BlockCodeFence)
            && !format.nonBreakableLines() && format.intProperty(QTextFormat::BlockQuoteLevel) == 0 && format.headingLevel() == 0) {
            m_preparing = true; m_inlineDecorations->clear();
            format.setLeftMargin(0); format.setRightMargin(0); format.setTextIndent(0); format.setIndent(0);
            const auto &settings = MarkdownSettingsStore::current();
            format.setTopMargin(settings.number(MarkdownSettings::ParagraphMargin)); format.setBottomMargin(format.topMargin());
            format.setLineHeight(settings.number(MarkdownSettings::LineHeight), QTextBlockFormat::ProportionalHeight);
            cursor.setBlockFormat(format);
            QTextCharFormat character; character.setFont(MarkdownTypography::bodyFont(m_zoomPercent));
            character.setForeground(settings.color(MarkdownSettings::TextColor)); cursor.setBlockCharFormat(character);
            setTextCursor(cursor); setCurrentCharFormat(character); m_preparing = false;
        }
        refreshInlineCursorFormat();
    }
}
void MarkdownDocumentView::dragEnterEvent(QDragEnterEvent *event) {
    if (ImageInput::canInsert(event->mimeData(), m_hostedLinks)) { event->setDropAction(Qt::CopyAction); event->accept(); }
    else QTextBrowser::dragEnterEvent(event);
}
void MarkdownDocumentView::dragMoveEvent(QDragMoveEvent *event) {
    if (ImageInput::canInsert(event->mimeData(), m_hostedLinks)) { event->setDropAction(Qt::CopyAction); event->accept(); }
    else QTextBrowser::dragMoveEvent(event);
}
void MarkdownDocumentView::dropEvent(QDropEvent *event) {
    emit sourceEditStarted(hasSourceSelection() ? m_selectionCaret : inlineCursorSourcePosition());
    const auto finished = qScopeGuard([this] { emit sourceEditFinished(); });
    if (!ImageInput::canInsert(event->mimeData(), m_hostedLinks)) { QTextBrowser::dropEvent(event); return; }
    setTextCursor(cursorForPosition(event->position().toPoint())); insertImages(event->mimeData());
    event->setDropAction(Qt::CopyAction); event->accept();
}
void MarkdownDocumentView::contextMenuEvent(QContextMenuEvent *event) {
    QMenu popup(this); auto *menu = &popup;
    auto hit = cursorForPosition(event->pos());
    if (auto *table = hit.currentTable()) {
        const auto cell = table->cellAt(hit); const int index = table->format().intProperty(tableSourceIndex) - 1;
        auto remove = [this, index, row = cell.row(), column = cell.column()](MarkdownTableEditing::Action action) {
            emit sourceEditStarted(inlineCursorSourcePosition());
            applySourceEdit(MarkdownTableEditing::operate(m_source, index, row, column, action)); emit sourceEditFinished();
        };
        menu->addAction(QStringLiteral("删除行"), this, [remove] { remove(MarkdownTableEditing::Action::DeleteRow); });
        menu->addAction(QStringLiteral("删除列"), this, [remove] { remove(MarkdownTableEditing::Action::DeleteColumn); });
        menu->addSeparator();
    }
    auto *undo = menu->addAction(QStringLiteral("撤销\tCtrl+Z")); undo->setEnabled(m_canUndo);
    auto *redo = menu->addAction(QStringLiteral("重做\tCtrl+Y")); redo->setEnabled(m_canRedo);
    connect(undo, &QAction::triggered, this, &MarkdownDocumentView::undoRequested); connect(redo, &QAction::triggered, this, &MarkdownDocumentView::redoRequested);
    menu->addSeparator(); auto *cut = menu->addAction(QStringLiteral("剪切\tCtrl+X"), this, [this] {
        emit sourceEditStarted(hasSourceSelection() ? m_selectionCaret : inlineCursorSourcePosition());
        const auto finished = qScopeGuard([this] { emit sourceEditFinished(); });
        if (hasSourceSelection()) { copySourceSelection(); replaceSourceSelection({}); } else QTextBrowser::cut();
    }); cut->setEnabled(textCursor().hasSelection() || hasSourceSelection());
    auto *copy = menu->addAction(QStringLiteral("复制\tCtrl+C"), this, [this] { if (hasSourceSelection()) copySourceSelection(); else QTextBrowser::copy(); }); copy->setEnabled(textCursor().hasSelection() || hasSourceSelection());
    auto *paste = menu->addAction(QStringLiteral("粘贴\tCtrl+V"), this, &MarkdownDocumentView::pasteImages); paste->setEnabled(canInsertFromMimeData(QApplication::clipboard()->mimeData()));
    menu->addSeparator(); menu->addAction(QStringLiteral("全选\tCtrl+A"), this, &QTextEdit::selectAll);
    menu->exec(event->globalPos());
}
