#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
class InstallerFlowTest final : public QObject {
  Q_OBJECT
private slots:
  void signedInstaller() {
    const auto artifact = qEnvironmentVariable("DAYMARK_INSTALLER_ARTIFACT");
    QVERIFY2(!artifact.isEmpty(),
             "Set DAYMARK_INSTALLER_ARTIFACT to a signed local Setup.exe");
    QTemporaryDir work;
    QVERIFY(work.isValid());
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(QStringLiteral(DAYMARK_TEST_PYTHON),
                  {QStringLiteral(DAYMARK_INSTALLER_FLOW_SCRIPT), "--artifact",
                   artifact, "--work", work.path(), "--repo",
                   QStringLiteral(DAYMARK_TEST_REPO)});
    QVERIFY(process.waitForStarted());
    const bool finished = process.waitForFinished(240000);
    const auto output = process.readAll();
    qInfo().noquote() << output;
    QVERIFY2(finished, output.constData());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QVERIFY2(process.exitCode() == 0, output.constData());
  }
};
QTEST_GUILESS_MAIN(InstallerFlowTest)
#include "InstallerFlowTest.moc"
