#include "platform/common/StartupController.h"
#include <QTemporaryDir>
#include <QFile>
#include <QtTest>
using namespace daymark;
class StartupTest:public QObject {
    Q_OBJECT
private slots:
    void explicitRegistrationAndDisablePreserveOtherEntries() {
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("startup.ini"));
        QSettings settings(path,QSettings::IniFormat);settings.setValue(QStringLiteral("OtherApp"),QStringLiteral("other"));settings.sync();
        StartupController startup(path);QVERIFY(startup.supported());QVERIFY(!startup.enabled());
        QVERIFY(startup.setEnabled(true));QVERIFY(startup.enabled());
        settings.sync();const auto command=settings.value(QStringLiteral("Daymark")).toString();
        QVERIFY(command.startsWith(QLatin1Char('"')) && command.endsWith(QLatin1Char('"')));
        QVERIFY(startup.setEnabled(false));QVERIFY(!startup.enabled());
        settings.sync();QCOMPARE(settings.value(QStringLiteral("OtherApp")).toString(),QStringLiteral("other"));
    }
    void failedWriteHasActionableError() {
        QTemporaryDir dir;QFile blocker(dir.filePath(QStringLiteral("file")));QVERIFY(blocker.open(QIODevice::WriteOnly));blocker.close();
        StartupController startup(blocker.fileName()+QStringLiteral("/startup.ini"));
        QVERIFY(!startup.setEnabled(true));QVERIFY(!startup.errorText().isEmpty());
    }
};
QTEST_GUILESS_MAIN(StartupTest)
#include "StartupTest.moc"
