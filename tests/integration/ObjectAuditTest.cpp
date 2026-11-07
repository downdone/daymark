#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
class ObjectAuditTest final : public QObject {
  Q_OBJECT
private slots:
  void isolatedLifecycleAndReportProtection() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto report = directory.filePath("objects.json");
    const auto app = QCoreApplication::applicationDirPath() + "/daymark.exe";
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(app, {"--object-audit-report", report});
    QVERIFY(process.waitForStarted());
    const bool finished = process.waitForFinished(85000);
    const auto log = process.readAll();
    if (!finished) {
      process.kill();
      process.waitForFinished(5000);
    }
    QVERIFY2(finished, log.constData());
    QVERIFY2(process.exitCode() == 0, log.constData());
    QFile file(report);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto bytes = file.readAll();
    const auto root = QJsonDocument::fromJson(bytes).object();
    file.close();
    QVERIFY(root.value("passed").toBool());
    QVERIFY(!root.value("userDataAccessed").toBool(true));
    QVERIFY(root.value("optimizationApplied").toBool());
    const auto snapshots = root.value("snapshots").toArray();
    QCOMPARE(snapshots.size(), 34);
    QSet<QString> stages;
    for (const auto &entry : snapshots) {
      const auto shot = entry.toObject();
      stages.insert(shot.value("stage").toString());
      QVERIFY(shot.value("objects").toInt() > 0);
      QVERIFY(shot.value("quickItems").toInt() > 0);
      QCOMPARE(
          shot.value("classes").toObject().value("TaskContextMenu").toInt(), 1);
      for (const auto &entry : shot.value("windows").toArray()) {
        const auto surface = entry.toObject();
        const auto owner = surface.value("owner").toString();
        if (owner == QStringLiteral("mainWindow"))
          QVERIFY(!surface.value("persistentSceneGraph").toBool(true));
        if (owner == QStringLiteral("desktopCalendarWindow") &&
            !surface.value("visible").toBool())
          QVERIFY(surface.value("objects").toInt() < 100);
      }
      QVERIFY(shot.value("classes").toObject().value("CalendarPicker").toInt() >
              0);
      QCOMPARE(shot.value("models").toObject().value("today").toInt(), 20);
    }
    for (const auto *name :
         {"startup_twenty_tasks", "settings_open", "settings_closed",
          "account_closed", "calendar_closed", "repeat_closed_9",
          "main_hidden_widget_only", "ball", "panel_returned",
          "after_single_diagnostic_gc", "main_hidden_ball_only",
          "all_windows_hidden"})
      QVERIFY(stages.contains(QString::fromLatin1(name)));
    // Neither an existing report nor a user-selected data directory can be
    // used.
    process.start(app, {"--object-audit-report", report});
    QVERIFY(process.waitForFinished(10000));
    QCOMPARE(process.exitCode(), 2);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), bytes);
    file.close();
    process.start(app,
                  {"--object-audit-report", directory.filePath("other.json"),
                   "--data-dir", directory.filePath("must-not-create")});
    QVERIFY(process.waitForFinished(10000));
    QCOMPARE(process.exitCode(), 2);
    QVERIFY(!QFileInfo::exists(
        directory.filePath("must-not-create/daymark.sqlite")));
  }
};
QTEST_GUILESS_MAIN(ObjectAuditTest)
#include "ObjectAuditTest.moc"
