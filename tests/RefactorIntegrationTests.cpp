#include "core/library/MarkdownManager.h"
#include "core/library/MarkdownFileIO.h"
#include "core/settings/MarkdownSettings.h"
#include "ui/knowledge/RelationDialog.h"
#include "ui/knowledge/KnowledgeTreeCanvas.h"
#include "ui/knowledge/KnowledgeNodeWidget.h"
#include <QGraphicsScene>
#include <QPointer>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>
class RefactorIntegrationTests : public QObject {
    Q_OBJECT
private slots:
    void bodyAndTitleEditsPreserveCards() {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
        MarkdownSettingsStore::setStoragePath(temp.filePath("settings.txt"));
        MarkdownManager manager(temp.filePath("md_data")); QString error;
        QVERIFY(manager.scan(&error)); QVERIFY(manager.createDocument("a.md", "# A\n\nbody\n", &error));
        QVERIFY(manager.createDocument("b.md", "# B\n", &error));
        QVERIFY(manager.setRelation("a.md", "b.md", NodeRelationType::Next, &error));
        RelationDialog dialog(&manager, "a.md");
        auto *canvas = dialog.findChild<KnowledgeTreeCanvas *>(); QVERIFY(canvas);
        QPointer<KnowledgeNodeWidget> card;
        for (auto *item : canvas->scene()->items()) if (auto *node = dynamic_cast<KnowledgeNodeWidget *>(item); node && node->path() == "a.md") card = node;
        QVERIFY(card); const QRectF rect = card->sceneBoundingRect(); const auto revision = manager.graphRevision();
        manager.updateContent("a.md", "# A\n\nbody changed\n");
        QVERIFY(card); QCOMPARE(manager.graphRevision(), revision); QCOMPARE(card->sceneBoundingRect(), rect);
        manager.updateContent("a.md", "# Renamed title\n\nbody changed\n");
        QVERIFY(card); QVERIFY(card->toolTip().startsWith("Renamed title\n")); QCOMPARE(card->sceneBoundingRect(), rect);
        QVERIFY(manager.saveDocument("a.md", &error)); QVERIFY(manager.scan(&error));
        QVERIFY(card); QCOMPARE(manager.graphRevision(), revision);
        canvas->setGraph(&manager, "a.md"); QVERIFY(card);
        QVERIFY(manager.createDocument("c.md", "# C\n", &error)); QVERIFY(card.isNull());
    }
};
QTEST_MAIN(RefactorIntegrationTests)
#include "RefactorIntegrationTests.moc"
