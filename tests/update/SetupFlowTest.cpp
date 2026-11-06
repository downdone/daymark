#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
class SetupFlowTest final : public QObject {
  Q_OBJECT
private slots:
  void signedTransactions() {
    QTemporaryDir work;
    QVERIFY(work.isValid());
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(QStringLiteral(DAYMARK_TEST_PYTHON),
                  {QStringLiteral(DAYMARK_SETUP_FLOW_SCRIPT), "--bin",
                   QCoreApplication::applicationDirPath(), "--work",
                   work.path()});
    QVERIFY(process.waitForStarted());
    QVERIFY(process.waitForFinished(90000));
    const auto output = process.readAll();
    qInfo().noquote() << output;
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QVERIFY2(process.exitCode() == 0, output.constData());
  }
};
QTEST_GUILESS_MAIN(SetupFlowTest)
#include "SetupFlowTest.moc"
