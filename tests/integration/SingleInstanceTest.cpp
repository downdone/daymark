#include "platform/common/SingleInstance.h"
#include <QCoreApplication>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace daymark;
class SingleInstanceTest : public QObject {
    Q_OBJECT
private slots:
    void independentDataDirectoriesHaveIndependentInstances()
    {
        QTemporaryDir firstDirectory,secondDirectory;
        QVERIFY(firstDirectory.isValid());QVERIFY(secondDirectory.isValid());
        SingleInstance first,second;
        QCOMPARE(first.acquire(firstDirectory.path()),SingleInstance::Outcome::Primary);
        QCOMPARE(second.acquire(secondDirectory.path()),SingleInstance::Outcome::Primary);
        QVERIFY(!QFileInfo::exists(firstDirectory.filePath(QStringLiteral("daymark.sqlite"))));
        QVERIFY(!QFileInfo::exists(secondDirectory.filePath(QStringLiteral("daymark.sqlite"))));
    }
    void secondProcessActivatesWithoutBecomingWriter()
    {
        QTemporaryDir dir(QDir::tempPath()+QStringLiteral("/daymark instance-XXXXXX"));QVERIFY(dir.isValid());
        SingleInstance primary;QCOMPARE(primary.acquire(dir.path()),SingleInstance::Outcome::Primary);
        QSignalSpy activated(&primary,&SingleInstance::activationRequested);
        QProcess child;child.start(QCoreApplication::applicationFilePath(),{QStringLiteral("--instance-child"),dir.path()});
        QVERIFY(child.waitForStarted());
        QTRY_COMPARE_WITH_TIMEOUT(child.state(),QProcess::NotRunning,10000);
        QCOMPARE(child.exitCode(),0);QCOMPARE(activated.count(),1);
        QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral("daymark.sqlite"))));
    }
};
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);
    if(app.arguments().size()==3 && app.arguments()[1]==QStringLiteral("--instance-child")) {
        SingleInstance instance;
        return instance.acquire(app.arguments()[2])==SingleInstance::Outcome::ActivatedExisting?0:9;
    }
    SingleInstanceTest test;return QTest::qExec(&test,argc,argv);
}
#include "SingleInstanceTest.moc"
