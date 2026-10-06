#include "MarkdownDocumentView.h"
#include "ImageInput.h"
#include "MarkdownTypography.h"
#include "MarkdownFenceCompleter.h"
#include "core/MarkdownSettings.h"
#include "core/ImageLoader.h"
#include "core/MarkdownParser.h"
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
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QFontDatabase>
#include <QTimer>
#include <QFileInfo>
#include <QMimeData>
#include <QWheelEvent>

namespace {
class CodeColors : public QSyntaxHighlighter {
public:
    explicit CodeColors(QTextDocument *document) : QSyntaxHighlighter(document) {}
protected:
    void highlightBlock(const QString &text) override {
        if (!MarkdownSettingsStore::current().enabled(MarkdownSettings::SyntaxColors)) return;
        if (!currentBlock().blockFormat().nonBreakableLines() && !currentBlock().blockFormat().hasProperty(QTextFormat::BlockCodeFence)) return;
        const QList<QPair<QString, QColor>> rules = {
            {"\\b(?:class|struct|public|private|protected|return|if|else|for|while|void|int|bool|const|auto|static|include|def|import|from|as|function|let|var|true|false|null|None|True|False|SELECT|FROM|WHERE|JOIN|INSERT|UPDATE|DELETE|CREATE|TABLE|AND|OR|ORDER|BY|GROUP|LIMIT)\\b", QColor("#b8a6e0")},
            {"\\b[0-9]+(?:\\.[0-9]+)?\\b", QColor("#e5bb87")},
            {"(?:\"(?:\\\\.|[^\"\\\\])*\"|'(?:\\\\.|[^'\\\\])*')", QColor("#a7cca1")},
            {"(?://.*$|^\\s*#.*$|-- .*$)", QColor("#899d96")}
        };
        for (const auto &rule : rules) {
            auto matches = QRegularExpression(rule.first).globalMatch(text);
            while (matches.hasNext()) { const auto match = matches.next(); setFormat(match.capturedStart(), match.capturedLength(), rule.second); }
        }
    }
};
}
MarkdownDocumentView::MarkdownDocumentView(QWidget *parent) : QTextBrowser(parent), m_markupTimer(new QTimer(this)) {
    setObjectName("markdownDocument"); setReadOnly(false); setAcceptRichText(false); setUndoRedoEnabled(false);
    setAcceptDrops(true); viewport()->setAcceptDrops(true); setOpenLinks(false); setOpenExternalLinks(false);
    setTextInteractionFlags(Qt::TextEditorInteraction | Qt::LinksAccessibleByMouse);
    setFont(MarkdownTypography::bodyFont()); setTabStopDistance(36); document()->setDocumentMargin(32);
    setPlaceholderText(QStringLiteral("直接编辑文档，或切换到原文编辑 Markdown。可粘贴、拖入图片。"));
    new CodeColors(document());
    m_fenceCompleter = new MarkdownFenceCompleter(this,
        [this] {
            const auto format = textCursor().blockFormat();
            const bool code = format.nonBreakableLines() || format.hasProperty(QTextFormat::BlockCodeFence);
            return MarkdownFenceCompleter::Context(m_source, code ? -1 : m_sourceMap.sourcePosition(textCursor().position()));
        },
        [this](int start, int length, const QString &language) {
            auto cursor = textCursor(); cursor.setPosition(m_sourceMap.displayPosition(start));
            cursor.setPosition(m_sourceMap.displayPosition(start + length), QTextCursor::KeepAnchor); cursor.insertText(language);
            setTextCursor(cursor);
        });
    connect(document(), &QTextDocument::contentsChange, this, &MarkdownDocumentView::documentEdited);
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
    QMap<QString, QImage> cached;
    if (sameDocument) for (auto it = m_loaded.cbegin(); it != m_loaded.cend(); ++it) cached.insert(m_sources.value(it.key()), it.value());
    m_markupTimer->stop(); ++m_generation;
    for (auto *loader : m_pending) { loader->cancel(); loader->deleteLater(); }
    m_pending.clear(); m_sources.clear(); m_images.clear(); m_loaded.clear(); m_errors.clear(); m_requestedSizes.clear();
    m_path = path; m_source = source; m_references = MarkdownImages::parse(source);
    QMap<QString, QString> replacements; int count = 0;
    for (const auto &image : m_references) if (!replacements.contains(image.source)) {
        const QUrl url(QString("tree-md-image:/%1").arg(++count)); m_sources.insert(url, image.source);
        if (cached.contains(image.source) && !force) { m_loaded.insert(url, cached.value(image.source)); m_images.insert(url, cached.value(image.source)); }
        else m_images.insert(url, placeholder(QStringLiteral("正在加载图片…")));
        replacements.insert(image.source, url.toString());
    }
    m_preparing = true;
    document()->setBaseUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath() + '/'));
    document()->setMarkdown(MarkdownImages::replace(source, m_references, replacements), QTextDocument::MarkdownDialectGitHub);
    applyMarkdownStyles(); document()->setModified(false); m_preparing = false; fitImages();
    m_display = displayedText(); m_sourceMap.rebuild(m_source, m_display);
    if (sameDocument) {
        QTextCursor cursor(document()); cursor.setPosition(qBound(0, m_sourceMap.displayPosition(anchor), document()->characterCount() - 1));
        cursor.setPosition(qBound(0, m_sourceMap.displayPosition(caret), document()->characterCount() - 1), QTextCursor::KeepAnchor); setTextCursor(cursor);
    } else moveCursor(QTextCursor::Start);
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
    const QColor base = palette().color(QPalette::Base), codeBackground = MarkdownTypography::codeBackground(base);
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
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment(); if (!fragment.isValid() || fragment.charFormat().isImageFormat()) continue;
            auto character = fragment.charFormat();
            if (code || character.fontFixedPitch()) {
                character.setFontFamilies(fixed.families()); character.setFontPointSize(!code && heading ? MarkdownTypography::headingPointSize(heading, m_zoomPercent) * 0.8 : fixed.pointSizeF()); character.setForeground(settings.color(MarkdownSettings::TextColor));
                if (!code) character.setBackground(MarkdownTypography::codeBackground(base, true));
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
        background.setBrush(MarkdownTypography::codeBackground(palette().color(QPalette::Base)));
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
void MarkdownDocumentView::keyPressEvent(QKeyEvent *event) {
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
