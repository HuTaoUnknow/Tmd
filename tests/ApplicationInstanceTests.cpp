#include "core/platform/ApplicationInstance.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

namespace {
bool write(const QString &path, const QJsonObject &value) {
    QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(value).toJson()) > 0;
}
QJsonObject read(const QString &path) {
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
QStringList probeArguments(const QString &identity, const QString &runtime, const QJsonObject &request, const QString &report, bool hold = false) {
    return {"--instance-probe", identity, runtime, QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact)), report, hold ? "hold" : "exit"};
}
struct Child : QProcess {
    ~Child() override { if (state() != NotRunning) { kill(); waitForFinished(2000); } }
};
}
class ApplicationInstanceTests : public QObject {
    Q_OBJECT
private slots:
    void repeatedLaunchForwardsUnicodeFilesAndActivation() {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        const QString identity=temp.filePath("Debug/library");
        ApplicationInstance owner(identity,temp.path()); QString error;
        QCOMPARE(owner.start({},&error),ApplicationInstance::Result::Primary);
        QSignalSpy received(&owner,&ApplicationInstance::requestsAvailable);
        const QJsonObject request{{"files",QJsonArray{QStringLiteral("D:/含空格 & 目录/文档.MD"),QStringLiteral("D:/另一份.md")}}, {"document","nested/a.md"}, {"source-view",true}};
        Child child; child.start(QCoreApplication::applicationFilePath(),probeArguments(identity,temp.path(),request,temp.filePath("report.json")));
        QTRY_COMPARE_WITH_TIMEOUT(received.count(),1,5000);
        QTRY_VERIFY_WITH_TIMEOUT(child.state()==QProcess::NotRunning,5000);
        QCOMPARE(child.exitCode(),0); QCOMPARE(read(temp.filePath("report.json")).value("role").toString(),QString("forwarded"));
        auto requests=owner.takeRequests(); QCOMPARE(requests.size(),1); QCOMPARE(requests.front(),request); QVERIFY(owner.takeRequests().isEmpty());
        Child activate; activate.start(QCoreApplication::applicationFilePath(),probeArguments(identity,temp.path(),{},temp.filePath("activate.json")));
        QTRY_COMPARE(received.count(),2); QTRY_VERIFY(activate.state()==QProcess::NotRunning);
        QCOMPARE(read(temp.filePath("activate.json")).value("role").toString(),QString("forwarded"));
        QCOMPARE(owner.takeRequests(),QVector<QJsonObject>({QJsonObject{}}));
    }
    void simultaneousLaunchesElectOneOwner() {
        QTemporaryDir temp; QVERIFY(temp.isValid()); const QString identity=temp.filePath("concurrent");
        Child children[4];
        for(int i=0;i<4;++i) children[i].start(QCoreApplication::applicationFilePath(),probeArguments(identity,temp.path(),QJsonObject{{"launch",i}},temp.filePath(QString::number(i)+".json"),true));
        for(int i=0;i<4;++i) QTRY_VERIFY_WITH_TIMEOUT(!read(temp.filePath(QString::number(i)+".json")).isEmpty(),5000);
        int primary=0,forwarded=0;
        for(int i=0;i<4;++i) {
            const auto role=read(temp.filePath(QString::number(i)+".json")).value("role").toString();
            primary+=role=="primary"; forwarded+=role=="forwarded";
        }
        QCOMPARE(primary,1); QCOMPARE(forwarded,3);
    }
    void crashedOwnerCanBeReplaced() {
        QTemporaryDir temp; QVERIFY(temp.isValid()); const QString identity=temp.filePath("crash");
        Child first; first.start(QCoreApplication::applicationFilePath(),probeArguments(identity,temp.path(),{},temp.filePath("owner.json"),true));
        QTRY_COMPARE_WITH_TIMEOUT(read(temp.filePath("owner.json")).value("role").toString(),QString("primary"),5000);
        first.kill(); QVERIFY(first.waitForFinished(3000));
        ApplicationInstance replacement(identity,temp.path()); QString error;
        QCOMPARE(replacement.start({},&error),ApplicationInstance::Result::Primary);
    }
    void differentExecutableAndLibraryScopesStayIndependent() {
        QTemporaryDir temp; QVERIFY(temp.isValid());
        const auto a=ApplicationInstance::identity(temp.filePath("debug/tree_md.exe"),temp.filePath("one"));
        const auto b=ApplicationInstance::identity(temp.filePath("release/tree_md.exe"),temp.filePath("one"));
        const auto c=ApplicationInstance::identity(temp.filePath("debug/tree_md.exe"),temp.filePath("two"));
        QVERIFY(a!=b); QVERIFY(a!=c);
        ApplicationInstance first(a,temp.path()),second(b,temp.path()),third(c,temp.path());
        QCOMPARE(first.start({}),ApplicationInstance::Result::Primary); QCOMPARE(second.start({}),ApplicationInstance::Result::Primary); QCOMPARE(third.start({}),ApplicationInstance::Result::Primary);
#ifdef Q_OS_WIN
        QCOMPARE(ApplicationInstance::identity(temp.filePath("Debug/TREE_MD.exe"),temp.filePath("ONE")),a);
#endif
    }
    void busyOwnerNeverCreatesAnotherWindow() {
        QTemporaryDir temp; QVERIFY(temp.isValid()); ApplicationInstance first("busy/"+temp.path(),temp.path()),second("busy/"+temp.path(),temp.path());
        QCOMPARE(first.start({}),ApplicationInstance::Result::Primary); QString error;
        // With this thread deliberately not servicing events, forwarding must
        // time out instead of opening a competing GUI owner.
        QCOMPARE(second.start({},&error,120),ApplicationInstance::Result::Failed); QVERIFY(!error.isEmpty());
    }
};
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv); const auto arguments=app.arguments();
    if(arguments.value(1)=="--instance-probe") {
        ApplicationInstance instance(arguments.value(2),arguments.value(3)); QString error;
        const auto request=QJsonDocument::fromJson(arguments.value(4).toUtf8()).object();
        const auto result=instance.start(request,&error);
        const QString role=result==ApplicationInstance::Result::Primary ? "primary" : result==ApplicationInstance::Result::Forwarded ? "forwarded" : "failed";
        if(!write(arguments.value(5),{{"role",role},{"error",error}})) return 3;
        if(result==ApplicationInstance::Result::Primary && arguments.value(6)=="hold") { QTimer::singleShot(15000,&app,&QCoreApplication::quit); return app.exec(); }
        return result==ApplicationInstance::Result::Failed ? 2 : 0;
    }
    ApplicationInstanceTests tests; return QTest::qExec(&tests,argc,argv);
}
#include "ApplicationInstanceTests.moc"
