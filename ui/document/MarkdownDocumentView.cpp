#include "ui/document/MarkdownDocumentView.h"
#include "ui/document/ImageInput.h"
#include "ui/document/MarkdownTypography.h"
#include "ui/document/MarkdownFenceCompleter.h"
#include "core/settings/MarkdownSettings.h"
#include "core/images/ImageLoader.h"
#include "core/markdown/MarkdownParser.h"
#include <QAbstractTextDocumentLayout>
#include <QClipboard>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextTable>
#include <QTextList>
#include "ui/document/CodeSyntaxHighlighter.h"
#include <QRegularExpression>
#include <QFontDatabase>
#include <QTimer>
#include <QFileInfo>
#include <QMimeData>
#include <QMouseEvent>
#include <QWheelEvent>

MarkdownDocumentView::MarkdownDocumentView(QWidget *parent) : QTextBrowser(parent), m_markupTimer(new QTimer(this)) {
    setObjectName("markdownDocument"); setReadOnly(false); setAcceptRichText(false); setUndoRedoEnabled(false);
    setAcceptDrops(true); viewport()->setAcceptDrops(true); setOpenLinks(false); setOpenExternalLinks(false);
    setTextInteractionFlags(Qt::TextEditorInteraction | Qt::LinksAccessibleByMouse);
    setFont(MarkdownTypography::bodyFont()); setTabStopDistance(36); document()->setDocumentMargin(32);
    setPlaceholderText(QStringLiteral("直接编辑文档，或切换到原文编辑 Markdown。可粘贴、拖入图片。"));
    new CodeSyntaxHighlighter(document());
    m_fenceCompleter = new MarkdownFenceCompleter(this,
        [this] {
            const auto format = textCursor().blockFormat();
            const bool code = format.nonBreakableLines() || format.hasProperty(QTextFormat::BlockCodeFence);
            return MarkdownFenceCompleter::Context(m_source, code ? -1 : m_sourceMap.sourcePosition(textCursor().position()));
        },
        [this](int start, int length, const QString &replacement, int caret) {
            QString source = m_source; source.replace(start, length, replacement);
            setMarkdownSource(m_path, source);
            QTextCursor cursor(document()); cursor.setPosition(m_sourceMap.displayPosition(caret)); setTextCursor(cursor);
            emit sourceEdited(m_source, m_sourceMap.sourcePosition(cursor.position())); ensureCursorVisible();
        });
    connect(document(), &QTextDocument::contentsChange, this, &MarkdownDocumentView::documentEdited);
    connect(this, &QTextEdit::cursorPositionChanged, this, &MarkdownDocumentView::refreshInlineCursorFormat);
    m_markupTimer->setSingleShot(true); m_markupTimer->setInterval(450);
    connect(m_markupTimer, &QTimer::timeout, this, [this] {
        // An empty paragraph is an editing position, even though Markdown
        // discards empty paragraphs. Keep it until the user has entered text.
        if (!MarkdownSettingsStore::current().enabled(MarkdownSettings::AutoRender)
            || textCursor().block().text().isEmpty() || m_fenceCompleter->isEditingFence()) return;
        m_markupRefresh = true; setMarkdownSource(m_path, m_source); m_markupRefresh = false;
    });
}
QString MarkdownDocumentView::displayedText() const {
    QString result = document()->toRawText(); result.replace(QChar::ParagraphSeparator, '\n'); return result;
}
QImage MarkdownDocumentView::placeholder(const QString &message) const {
    QImage image(420, 68, QImage::Format_ARGB32_Premultiplied); image.fill(QColor("#2b312e"));
    QPainter painter(&image); painter.setPen(QColor("#c0c9c3")); painter.setFont(QFont("Segoe UI", 10));
    painter.drawText(image.rect().adjusted(12, 8, -12, -8), Qt::AlignVCenter | Qt::TextWordWrap, message); return image;
}
void MarkdownDocumentView::setMarkdownSource(const QString &path, const QString &source, bool force) {
    if (!force && !m_markupRefresh && path == m_path && source == m_source) return;
    m_fenceCompleter->reset();
    const bool sameDocument = path == m_path; const int scroll = verticalScrollBar()->value();
    const int caret = m_sourceMap.sourcePosition(textCursor().position()), anchor = m_sourceMap.sourcePosition(textCursor().anchor());
    const auto oldInline = m_sourceMap.inlineCodeAt(textCursor().position());
    const bool outsideInline = oldInline.valid() && (caret == oldInline.sourceStart || caret == oldInline.sourceEnd);
    QMap<QString, QImage> cached;
    if (sameDocument) for (auto it = m_loaded.cbegin(); it != m_loaded.cend(); ++it) cached.insert(m_sources.value(it.key()), it.value());
    m_markupTimer->stop(); ++m_generation;
    for (auto *loader : m_pending) { loader->cancel(); loader->deleteLater(); }
    m_pending.clear(); m_sources.clear(); m_images.clear(); m_loaded.clear(); m_errors.clear(); m_requestedSizes.clear();
    const auto analysis = MarkdownAnalysis::scan(source);
    m_path = path; m_source = source; m_references = MarkdownImages::parse(source, analysis);
    QMap<QString, QString> replacements; int count = 0;
    for (const auto &image : m_references) if (!replacements.contains(image.source)) {
        const QUrl url(QString("tree-md-image:/%1").arg(++count)); m_sources.insert(url, image.source);
        if (cached.contains(image.source) && !force) { m_loaded.insert(url, cached.value(image.source)); m_images.insert(url, cached.value(image.source)); }
        else m_images.insert(url, placeholder(QStringLiteral("正在加载图片…")));
        replacements.insert(image.source, url.toString());
    }
    m_preparing = true;
    document()->setBaseUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath() + '/'));
    const auto headings = MarkdownParser::headingMarkers(source, analysis);
    QString renderSource = MarkdownImages::replace(source, m_references, replacements);
    const auto renderHeadings = MarkdownParser::headingMarkers(renderSource);
    QString emptyHeadingPrefix = QChar(0xe000) + QStringLiteral("tmd-heading:");
    while (renderSource.contains(emptyHeadingPrefix)) emptyHeadingPrefix += ':';
    auto emptyHeadingToken = [&](int index) { return emptyHeadingPrefix + QString::number(index) + QChar(0xe001); };
    for (int i = renderHeadings.size() - 1; i >= 0; --i) {
        const auto &heading = renderHeadings[i];
        if (!heading.separated) renderSource.insert(heading.markerStart, '\\');
        else if (heading.contentStart == heading.end) renderSource.insert(heading.contentStart, emptyHeadingToken(i));
    }
    document()->setMarkdown(renderSource, QTextDocument::MarkdownDialectGitHub);
    // Qt can omit consecutive empty headings. Give each an import-only token,
    // then remove it while retaining the editable heading block and source caret.
    QList<QPair<QTextCursor, int>> emptyHeadingCursors;
    for (int i = 0; i < headings.size(); ++i) if (headings[i].separated && headings[i].contentStart == headings[i].end) {
        auto cursor = document()->find(emptyHeadingToken(i));
        if (!cursor.isNull()) { cursor.removeSelectedText(); emptyHeadingCursors.append({cursor, headings[i].contentStart}); }
    }
    const auto &fences = analysis.fences;
    // Blank lines outside a fence must not steal an empty code line's source
    // position. Match the code lines Qt retained before applying our styles.
    QList<QTextBlock> codeBlocks; QList<QPair<int, int>> codeBoundaries;
    for (auto block = document()->begin(); block.isValid(); block = block.next())
        if (block.blockFormat().hasProperty(QTextFormat::BlockCodeFence)) codeBlocks.append(block);
    int nextCode = 0;
    for (const auto &fence : fences) {
        QList<QPair<QString, int>> lines;
        const int bodyEnd = fence.closed() ? fence.closeStart : fence.end;
        for (const auto &line : analysis.lines) {
            if (line.start < fence.bodyStart || line.start >= bodyEnd) continue;
            lines.append({source.mid(line.contentStart, line.end - line.contentStart), line.contentStart});
        }
        if (lines.isEmpty()) lines.append({QString(), fence.bodyStart});
        // Qt omits leading empty code lines, and keeps one for an empty fence.
        while (lines.size() > 1 && lines.front().first.isEmpty()) lines.removeFirst();
        const QString language = source.mid(fence.start + fence.indent.size() + fence.delimiter.size(),
            fence.headerEnd - fence.start - fence.indent.size() - fence.delimiter.size()).trimmed().section(QRegularExpression("\\s+"), 0, 0);
        for (int candidate = nextCode; candidate + lines.size() <= codeBlocks.size(); ++candidate) {
            bool matches = true;
            for (int line = 0; line < lines.size(); ++line) {
                const auto block = codeBlocks[candidate + line]; const auto format = block.blockFormat();
                if (block.text() != lines[line].first || format.stringProperty(QTextFormat::BlockCodeLanguage) != language
                    || format.stringProperty(QTextFormat::BlockCodeFence) != QString(fence.delimiter[0])) { matches = false; break; }
            }
            if (!matches) continue;
            for (int line = 0; line < lines.size(); ++line) codeBoundaries.append({codeBlocks[candidate + line].position(), lines[line].second});
            nextCode = candidate + lines.size(); break;
        }
    }
    // Qt drops trailing blank paragraphs. Keep one outside a completed code
    // block so the user can move on to normal Markdown without editing its fence.
    bool trailingCodeParagraph = false;
    const auto lastFormat = document()->lastBlock().blockFormat();
    if (source.endsWith("\n\n") && (lastFormat.nonBreakableLines() || lastFormat.hasProperty(QTextFormat::BlockCodeFence))) {
        if (!fences.isEmpty() && fences.back().closed() && source.mid(fences.back().end).trimmed().isEmpty()) {
            QTextCursor end(document()); end.movePosition(QTextCursor::End); end.insertBlock(QTextBlockFormat(), QTextCharFormat());
            trailingCodeParagraph = true;
        }
    }
    applyMarkdownStyles(); document()->setModified(false); m_preparing = false; fitImages();
    m_display = displayedText(); m_sourceMap.rebuild(m_source, m_display);
    for (const auto &heading : emptyHeadingCursors) m_sourceMap.setBoundary(heading.first.position(), heading.second);
    for (const auto &boundary : codeBoundaries) m_sourceMap.setBoundary(boundary.first, boundary.second);
    if (trailingCodeParagraph) m_sourceMap.setBoundary(document()->lastBlock().position(), source.size());
    if (sameDocument) {
        QTextCursor cursor(document()); cursor.setPosition(qBound(0, m_sourceMap.displayPosition(anchor), document()->characterCount() - 1));
        cursor.setPosition(qBound(0, m_sourceMap.displayPosition(caret), document()->characterCount() - 1), QTextCursor::KeepAnchor); setTextCursor(cursor);
    } else moveCursor(QTextCursor::Start);
    if (sameDocument && outsideInline) {
        const auto code = m_sourceMap.inlineCodeAt(textCursor().position());
        if (code.valid() && (caret == code.sourceStart || caret == code.sourceEnd)) setInlineBoundary(code, caret == code.sourceEnd, true);
    }
    refreshInlineCursorFormat();
    verticalScrollBar()->setValue(sameDocument ? scroll : 0);
    for (auto it = m_sources.cbegin(); it != m_sources.cend(); ++it) if (!m_loaded.contains(it.key())) {
        const QUrl url = it.key(); const int generation = m_generation;
        QTimer::singleShot(0, this, [this, url, generation] { loadImage(url, generation); });
    }
    if (m_loaded.size() == m_sources.size()) emit imagesSettled();
}
void MarkdownDocumentView::applyMarkdownStyles() {
    const auto &settings = MarkdownSettingsStore::current();
    document()->setIndentWidth(settings.number(MarkdownSettings::ListIndent));
    auto root = document()->rootFrame()->frameFormat(); root.setBottomMargin(24); document()->rootFrame()->setFrameFormat(root);
    const QFont body = MarkdownTypography::bodyFont(m_zoomPercent), fixed = MarkdownTypography::codeFont(m_zoomPercent);
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
            const auto isCode = [](const QTextBlock &other) { return other.isValid() && (other.blockFormat().nonBreakableLines() || other.blockFormat().hasProperty(QTextFormat::BlockCodeFence)); };
            format.setTopMargin(isCode(block.previous()) ? 0 : settings.number(MarkdownSettings::CodeMargin)); format.setBottomMargin(isCode(block.next()) ? 0 : settings.number(MarkdownSettings::CodeMargin));
        } else if (format.intProperty(QTextFormat::BlockQuoteLevel) > 0) {
            format.setLeftMargin(settings.number(MarkdownSettings::QuoteIndent) * format.intProperty(QTextFormat::BlockQuoteLevel)); format.setBackground(settings.color(MarkdownSettings::QuoteBackground));
        }
        cursor.setBlockFormat(format);
        if ((code || heading) && block.text().isEmpty()) {
            QTextCharFormat character; character.setFont(code ? fixed : body);
            character.setForeground(settings.color(code ? MarkdownSettings::TextColor : MarkdownSettings::HeadingColor));
            if (heading) { character.setFontPointSize(MarkdownTypography::headingPointSize(heading, m_zoomPercent)); character.setFontWeight(settings.enabled(MarkdownSettings::BoldHeadings) ? QFont::Bold : QFont::Normal); }
            cursor.setBlockCharFormat(character);
        }
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment(); if (!fragment.isValid() || fragment.charFormat().isImageFormat()) continue;
            auto character = fragment.charFormat();
            if (code || character.fontFixedPitch()) {
                character.setFontFamilies(fixed.families()); character.setFontPointSize(!code && heading ? MarkdownTypography::headingPointSize(heading, m_zoomPercent) * 0.8 : fixed.pointSizeF()); character.setForeground(settings.color(MarkdownSettings::TextColor));
                if (!code) character.setBackground(MarkdownTypography::codeBackground(true));
            } else {
                character.setFontFamilies(body.families()); character.setFontPointSize(heading ? MarkdownTypography::headingPointSize(heading, m_zoomPercent) : body.pointSizeF());
                character.setForeground(settings.color(MarkdownSettings::TextColor));
                if (heading) { character.setFontWeight(settings.enabled(MarkdownSettings::BoldHeadings) ? QFont::Bold : QFont::Normal); character.setForeground(settings.color(MarkdownSettings::HeadingColor)); }
            }
            if (character.isAnchor()) { character.setForeground(settings.color(MarkdownSettings::LinkColor)); character.setFontUnderline(settings.enabled(MarkdownSettings::UnderlineLinks)); }
            cursor.setPosition(fragment.position()); cursor.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor); cursor.setCharFormat(character);
        }
    }
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
    m_fenceCompleter->reset(); viewport()->update();
}
void MarkdownDocumentView::setZoomPercent(int percent) {
    percent = qBound(50, percent, 200); if (percent == m_zoomPercent) return;
    const QTextCursor anchor = cursorForPosition(QPoint(32, viewport()->height() / 3)); const int y = cursorRect(anchor).top();
    const bool modified = document()->isModified(); m_preparing = true; m_zoomPercent = percent;
    setFont(MarkdownTypography::bodyFont(percent)); applyMarkdownStyles(); m_preparing = false; fitImages();
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
        const int quoteLevel = block.blockFormat().intProperty(QTextFormat::BlockQuoteLevel);
        const QString quote = QString("> ").repeated(quoteLevel);
        inserted.replace("\n", quoteLevel ? "\n" + quote.trimmed() + "\n" + quote : "\n\n");
    }
    inserted.replace(QChar::LineSeparator, block.blockFormat().nonBreakableLines() || block.blockFormat().hasProperty(QTextFormat::BlockCodeFence) ? "\n" : "  \n");
    int sourceCursor = 0; m_source = m_sourceMap.applyEdit(prefix, oldEnd - prefix, inserted, display.mid(prefix, newEnd - prefix), &sourceCursor);
    m_display = display;
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
    // QTextDocument colors each code line separately. Paint the complete panel
    // behind it as well, including line spacing and blank code lines.
    {
        QPainter background(viewport()); background.setRenderHint(QPainter::Antialiasing);
        background.setPen(settings.enabled(MarkdownSettings::CodeBorder) ? QPen(palette().color(QPalette::Base).lighter(160), 1) : QPen(Qt::NoPen));
        background.setBrush(MarkdownTypography::codeBackground());
        QRectF panel;
        auto flush = [&] {
            if (!panel.isNull() && panel.bottom() >= 0 && panel.top() <= viewport()->height()) {
                panel.setLeft(document()->documentMargin() - 10 - horizontalScrollBar()->value());
                panel.setRight(document()->size().width() - document()->documentMargin() + 10 - horizontalScrollBar()->value());
                background.drawRoundedRect(panel.adjusted(0, -8, 0, 8), 3, 3);
            }
            panel = {};
        };
        for (auto block = document()->begin(); block.isValid(); block = block.next()) {
            const auto format = block.blockFormat();
            if (format.nonBreakableLines() || format.hasProperty(QTextFormat::BlockCodeFence)) {
                const QRectF rect = document()->documentLayout()->blockBoundingRect(block).translated(-horizontalScrollBar()->value(), -offset);
                panel = panel.isNull() ? rect : panel.united(rect);
            } else flush();
        }
        flush();
    }
    QTextBrowser::paintEvent(event); QPainter painter(viewport()); painter.setPen(QPen(QColor("#83a88f"), 3));
    for (auto block = document()->begin(); block.isValid(); block = block.next()) if (block.blockFormat().intProperty(QTextFormat::BlockQuoteLevel) > 0) {
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
    if (ImageInput::canInsert(data, m_hostedLinks)) insertImages(data);
    else if (data->hasText()) textCursor().insertText(data->text());
}
void MarkdownDocumentView::pasteImages() { insertFromMimeData(QApplication::clipboard()->mimeData()); }
void MarkdownDocumentView::setInlineBoundary(const MarkdownSourceMap::InlineCode &code, bool atEnd, bool outside) {
    m_sourceMap.setBoundary(textCursor().position(), atEnd ? (outside ? code.sourceEnd : code.contentEnd) : (outside ? code.sourceStart : code.contentStart));
    refreshInlineCursorFormat();
}
void MarkdownDocumentView::refreshInlineCursorFormat() {
    if (m_preparing || textCursor().hasSelection()) return;
    const auto code = m_sourceMap.inlineCodeAt(textCursor().position()); if (!code.valid()) return;
    const int source = m_sourceMap.sourcePosition(textCursor().position());
    if (source == code.sourceStart || source == code.sourceEnd) {
        QTextCharFormat format; const auto &settings = MarkdownSettingsStore::current(); format.setFont(MarkdownTypography::bodyFont(m_zoomPercent));
        format.setFontFixedPitch(false); format.setForeground(settings.color(MarkdownSettings::TextColor));
        const int heading = textCursor().blockFormat().headingLevel();
        if (heading) { format.setFontPointSize(MarkdownTypography::headingPointSize(heading, m_zoomPercent)); format.setFontWeight(settings.enabled(MarkdownSettings::BoldHeadings) ? QFont::Bold : QFont::Normal); format.setForeground(settings.color(MarkdownSettings::HeadingColor)); }
        setCurrentCharFormat(format);
    } else {
        QTextCursor sample(document()); sample.setPosition(code.displayStart); sample.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        setCurrentCharFormat(sample.charFormat());
    }
}
void MarkdownDocumentView::mousePressEvent(QMouseEvent *event) {
    QTextBrowser::mousePressEvent(event);
    if (event->button() != Qt::LeftButton || textCursor().hasSelection()) return;
    const auto code = m_sourceMap.inlineCodeAt(textCursor().position()); if (!code.valid()) return;
    if (textCursor().position() == code.displayEnd) setInlineBoundary(code, true, event->position().x() >= cursorRect().left());
    else if (textCursor().position() == code.displayStart) setInlineBoundary(code, false, event->position().x() < cursorRect().left());
    else refreshInlineCursorFormat();
}
void MarkdownDocumentView::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Backspace && event->modifiers() == Qt::NoModifier && !textCursor().hasSelection()
        && textCursor().positionInBlock() == 0 && textCursor().blockFormat().headingLevel() > 0) {
        const int sourceCaret = m_sourceMap.sourcePosition(textCursor().position());
        for (const auto &heading : MarkdownParser::headingMarkers(m_source)) {
            if (!heading.separated || sourceCaret < heading.markerStart || sourceCaret > heading.end) continue;
            const int caret = heading.markerStart + heading.level;
            QString source = m_source; source.remove(caret, heading.contentStart - caret);
            setMarkdownSource(m_path, source);
            QTextCursor cursor(document()); cursor.setPosition(m_sourceMap.displayPosition(heading.markerStart) + heading.level);
            m_sourceMap.setBoundary(cursor.position(), caret); setTextCursor(cursor);
            emit sourceEdited(m_source, caret); ensureCursorVisible(); event->accept(); return;
        }
    }
    if (event->modifiers() == Qt::NoModifier && !textCursor().hasSelection() && (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)) {
        const auto code = m_sourceMap.inlineCodeAt(textCursor().position());
        if (code.valid()) {
            const int caret = textCursor().position(), source = m_sourceMap.sourcePosition(caret);
            const bool right = event->key() == Qt::Key_Right;
            if (caret == code.displayEnd && ((right && source != code.sourceEnd) || (!right && source == code.sourceEnd))) {
                setInlineBoundary(code, true, right); event->accept(); return;
            }
            if (caret == code.displayStart && ((!right && source != code.sourceStart) || (right && source == code.sourceStart))) {
                setInlineBoundary(code, false, !right); event->accept(); return;
            }
        }
    }
    if (event->key() == Qt::Key_Down && event->modifiers() == Qt::NoModifier && !textCursor().hasSelection()
        && textCursor().blockFormat().hasProperty(QTextFormat::BlockCodeFence)) {
        auto next = textCursor().block().next();
        while (next.isValid() && next.blockFormat().hasProperty(QTextFormat::BlockCodeFence)) next = next.next();
        if (next.isValid()) { setTextCursor(QTextCursor(next)); ensureCursorVisible(); event->accept(); return; }
        const auto fences = MarkdownParser::codeFences(m_source);
        if (!fences.isEmpty() && fences.back().closed()) {
            QString source = m_source; if (!source.endsWith('\n')) source += '\n'; if (!source.endsWith("\n\n")) source += '\n';
            setMarkdownSource(m_path, source); moveCursor(QTextCursor::End);
            emit sourceEdited(m_source, m_source.size()); ensureCursorVisible(); event->accept(); return;
        }
    }
    if (event->matches(QKeySequence::Undo)) { emit undoRequested(); event->accept(); return; }
    if (event->matches(QKeySequence::Redo)) { emit redoRequested(); event->accept(); return; }
    QTextBrowser::keyPressEvent(event);
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
    if (!ImageInput::canInsert(event->mimeData(), m_hostedLinks)) { QTextBrowser::dropEvent(event); return; }
    setTextCursor(cursorForPosition(event->position().toPoint())); insertImages(event->mimeData());
    event->setDropAction(Qt::CopyAction); event->accept();
}
void MarkdownDocumentView::contextMenuEvent(QContextMenuEvent *event) {
    QMenu popup(this); auto *menu = &popup;
    auto *undo = menu->addAction(QStringLiteral("撤销\tCtrl+Z")); undo->setEnabled(m_canUndo);
    auto *redo = menu->addAction(QStringLiteral("重做\tCtrl+Y")); redo->setEnabled(m_canRedo);
    connect(undo, &QAction::triggered, this, &MarkdownDocumentView::undoRequested); connect(redo, &QAction::triggered, this, &MarkdownDocumentView::redoRequested);
    menu->addSeparator(); auto *cut = menu->addAction(QStringLiteral("剪切\tCtrl+X"), this, &QTextEdit::cut); cut->setEnabled(textCursor().hasSelection());
    auto *copy = menu->addAction(QStringLiteral("复制\tCtrl+C"), this, &QTextEdit::copy); copy->setEnabled(textCursor().hasSelection());
    auto *paste = menu->addAction(QStringLiteral("粘贴\tCtrl+V"), this, &MarkdownDocumentView::pasteImages); paste->setEnabled(canInsertFromMimeData(QApplication::clipboard()->mimeData()));
    menu->addSeparator(); menu->addAction(QStringLiteral("全选\tCtrl+A"), this, &QTextEdit::selectAll);
    menu->exec(event->globalPos());
}
