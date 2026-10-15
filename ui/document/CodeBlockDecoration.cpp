#include "ui/document/CodeBlockDecoration.h"
#include "ui/document/CodeBlockFormat.h"
#include "ui/document/MarkdownTypography.h"
#include "core/markdown/LanguageCatalog.h"
#include "core/settings/MarkdownSettings.h"
#include <QAbstractTextDocumentLayout>
#include <QFontMetricsF>
#include <QPainter>
#include <QPointer>
#include <QScrollBar>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>

namespace {
QIcon copyIcon(const QColor &color, bool copied = false) {
    QPixmap pixmap(24, 24); pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap); painter.setRenderHint(QPainter::Antialiasing); painter.setPen(QPen(color, 1.6));
    if (copied) painter.drawPolyline(QPolygonF{QPointF(5, 12), QPointF(10, 17), QPointF(20, 7)});
    else { painter.drawRoundedRect(QRectF(5, 4, 11, 13), 2, 2); painter.setBrush(MarkdownTypography::codeBackground()); painter.drawRoundedRect(QRectF(9, 8, 11, 13), 2, 2); }
    return QIcon(pixmap);
}
QColor headerColor() { return MarkdownTypography::codeBackground().lightness() < 128 ? QColor("#c0ccd2") : QColor("#485764"); }
}
CodeBlockDecoration::CodeBlockDecoration(QTextEdit *edit) : QObject(edit), m_edit(edit), m_font(edit->font()) {}
qreal CodeBlockDecoration::headerHeight() const { return qMax(28.0, QFontMetricsF(m_font).height() + 12); }
void CodeBlockDecoration::sync() {
    m_panels.clear();
    const int h = m_edit->horizontalScrollBar()->value(), v = m_edit->verticalScrollBar()->value();
    QTextBlock first, last; QRectF body;
    const auto flush = [&] {
        if (body.isNull()) return;
        const qreal left = m_edit->document()->documentMargin() - 10 - h;
        const qreal right = qMax(left + 60, qMin(m_edit->document()->size().width() - m_edit->document()->documentMargin() + 10 - h,
            qreal(m_edit->viewport()->width() - 14)));
        const auto format = first.blockFormat();
        const qreal height = format.intProperty(CodeBlockFormat::Id) > 0 ? headerHeight() : 0;
        const QRectF header(left, body.top() - height - 6, right - left, height);
        m_panels.append({QRectF(left, header.top() - 2, right - left, body.bottom() - header.top() + 10), header,
            LanguageCatalog::displayName(format.stringProperty(QTextFormat::BlockCodeLanguage)), format.intProperty(CodeBlockFormat::Id) - 1, first.position()});
        body = {}; first = {}; last = {};
    };
    for (auto block = m_edit->document()->begin(); block.isValid(); block = block.next()) {
        if (!CodeBlockFormat::isCode(block)) { flush(); continue; }
        if (last.isValid() && !CodeBlockFormat::samePanel(last, block)) flush();
        if (!first.isValid()) first = block;
        auto rect = m_edit->document()->documentLayout()->blockBoundingRect(block).translated(-h, -v);
        // Qt reports 0x0 for a document consisting of one empty code row.
        // Give that real editing position a line-sized panel and its controls.
        if (rect.height() <= 0) rect.setHeight(qMax(1.0, QFontMetricsF(m_edit->font()).height()));
        body = body.isNull() ? rect : body.united(rect); last = block;
    }
    flush();
    QVector<Panel> headers; for (const auto &panel : m_panels) if (panel.codeIndex >= 0) headers.append(panel);
    while (m_buttons.size() > headers.size()) delete m_buttons.takeLast();
    while (m_buttons.size() < headers.size()) {
        auto *button = new QToolButton(m_edit->viewport()); button->setObjectName("copyCodeButton");
        button->setFocusPolicy(Qt::NoFocus); button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet("QToolButton { background: transparent; border: none; border-radius: 4px; padding: 2px; } QToolButton:hover { background: rgba(150,160,170,45); } QToolButton:pressed { background: rgba(150,160,170,75); }");
        connect(button, &QToolButton::clicked, this, [this, button] {
            emit copyRequested(button->property("codeIndex").toInt(), button->property("displayPosition").toInt());
            button->setIcon(copyIcon(headerColor(), true)); button->setToolTip(QStringLiteral("已复制"));
            button->setProperty("copied", true);
            const int generation = button->property("copyGeneration").toInt() + 1; button->setProperty("copyGeneration", generation);
            QPointer<QToolButton> guarded(button);
            QTimer::singleShot(1500, this, [guarded, generation] {
                if (!guarded || guarded->property("copyGeneration").toInt() != generation) return;
                guarded->setProperty("copied", false); guarded->setIcon(copyIcon(headerColor())); guarded->setToolTip(QStringLiteral("复制代码"));
            });
        });
        m_buttons.append(button);
    }
    for (int i = 0; i < headers.size(); ++i) {
        const auto &panel = headers[i]; auto *button = m_buttons[i];
        const bool changed = button->property("displayPosition").toInt() != panel.position || button->property("codeIndex").toInt() != panel.codeIndex;
        button->setProperty("codeIndex", panel.codeIndex); button->setProperty("displayPosition", panel.position);
        button->setAccessibleName(QStringLiteral("复制 %1 代码").arg(panel.label));
        if (changed) {
            button->setProperty("copied", false); button->setProperty("copyGeneration", button->property("copyGeneration").toInt() + 1);
        }
        const QColor color = headerColor();
        if (changed || button->icon().isNull() || button->property("iconColor").toString() != color.name()) {
            const bool copied = button->property("copied").toBool(); button->setIcon(copyIcon(color, copied));
            button->setToolTip(copied ? QStringLiteral("已复制") : QStringLiteral("复制代码")); button->setProperty("iconColor", color.name());
        }
        const int size = qRound(headerHeight() - 4); button->setIconSize(QSize(qMin(22, size - 4), qMin(22, size - 4)));
        button->setGeometry(QRect(qRound(panel.header.right() - size - 6), qRound(panel.header.top() + 2), size, size));
        button->setVisible(panel.header.top() >= 0 && panel.header.top() < m_edit->viewport()->height());
    }
}
void CodeBlockDecoration::paintBackground(QPainter &painter) {
    sync(); painter.save(); painter.setRenderHint(QPainter::Antialiasing);
    const auto &settings = MarkdownSettingsStore::current();
    painter.setPen(settings.enabled(MarkdownSettings::CodeBorder) ? QPen(headerColor().darker(160), 1) : QPen(Qt::NoPen));
    painter.setBrush(MarkdownTypography::codeBackground());
    for (const auto &panel : m_panels) if (panel.area.intersects(m_edit->viewport()->rect())) painter.drawRoundedRect(panel.area, 6, 6);
    painter.restore();
}
void CodeBlockDecoration::paintHeaders(QPainter &painter) {
    painter.save(); painter.setFont(m_font); const QFontMetricsF metrics(m_font); const QColor text = headerColor();
    for (const auto &panel : m_panels) {
        if (panel.codeIndex < 0) continue;
        if (!panel.header.intersects(m_edit->viewport()->rect())) continue;
        const qreal available = qMax(0.0, panel.header.width() - headerHeight() - 32);
        const QString label = metrics.elidedText(panel.label, Qt::ElideRight, available);
        QRectF badge(panel.header.left() + 10, panel.header.top() + 4, qMin(available, metrics.horizontalAdvance(label) + 14), panel.header.height() - 8);
        QColor fill = text; fill.setAlpha(22); painter.setPen(Qt::NoPen); painter.setBrush(fill); painter.drawRoundedRect(badge, 4, 4);
        painter.setPen(text); painter.drawText(badge.adjusted(7, 0, -7, 0), Qt::AlignVCenter | Qt::AlignLeft, label);
    }
    painter.restore();
}
