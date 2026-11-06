#include "update/qt/UpdateController.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QSaveFile>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <cstdio>

using daymark::UpdateController;
class LinkReceiver final : public QObject {
    Q_OBJECT
  public:
    QUrl last;
  public slots:
    void accept(const QUrl &value) { last = value; }
};

class UpdateControllerTest final : public QObject {
    Q_OBJECT
    QString root;
    QString path(const QString &name) const { return QDir(root).filePath("update-state/" + name); }
    bool write(const QString &name, const QByteArray &bytes) {
        QSaveFile file(path(name));
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() &&
               file.commit();
    }
    bool object(const QString &name, const QJsonObject &value) {
        return write(name, QJsonDocument(value).toJson(QJsonDocument::Compact));
    }
    bool prepared(const QString &version, const QString &transaction) {
        return object("prepared-update.json",
                      {{"format", 1}, {"version", version}, {"transaction_id", transaction}});
    }

  public:
    explicit UpdateControllerTest(QString directory) : root(std::move(directory)) {}
  private slots:
    void init() {
        for (const auto &name : {"status.json", "preferences.json", "prepared-update.json"})
            QFile::remove(path(name));
    }
    void readyNotificationsFollowCandidateAndRejectMalformedState() {
        QVERIFY(object("status.json", {{"state", "ready"}, {"version", "2.1.0"}}));
        QVERIFY(prepared("2.1.0", "candidate-one"));
        UpdateController controller(root + "/synthetic-data", false);
        QVERIFY(controller.supported());
        QSignalSpy notices(&controller, &UpdateController::updateReady);
        QTRY_COMPARE(notices.count(), 1); // Initial Ready reaches subscribers after construction.
        QVERIFY(controller.ready());
        QCOMPARE(controller.availableVersion(), QString("2.1.0"));
        QVERIFY(object("preferences.json", {{"auto_update", false}}));
        QTRY_VERIFY(!controller.automatic());
        QCOMPARE(notices.count(), 1);
        QVERIFY(write("status.json", "{broken"));
        QTRY_VERIFY(!controller.ready());
        QVERIFY(!controller.automatic());
        QVERIFY(controller.statusText().contains(QStringLiteral("无法读取")));
        QVERIFY(QFile::exists(path("prepared-update.json"))); // Display never mutates receipts.
        QFile::remove(path("prepared-update.json"));
        QVERIFY(object("status.json", {{"state", "idle"}}));
        QTRY_VERIFY(!controller.statusText().contains(QStringLiteral("无法读取")));
        QVERIFY(prepared("2.2.0", "candidate-two"));
        QTRY_COMPARE(notices.count(), 2);
        QCOMPARE(controller.availableVersion(), QString("2.2.0"));
        QFile::remove(path("prepared-update.json"));
        QVERIFY(object("status.json", {{"state", "ready"}}));
        QTRY_VERIFY(!controller.ready()); // Stale status alone cannot claim installed Ready.
    }
    void failedCommandsAreVisibleAndDoNotRetainProcesses() {
        UpdateController controller(root + "/synthetic-data", false);
        QVERIFY(controller.supported());
        controller.check(); // The owned fixture CLI is deliberately not an executable.
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QVERIFY(controller.statusText().contains(QStringLiteral("无法启动")));
        controller.cancel();
        QTRY_VERIFY(controller.findChildren<QProcess *>().isEmpty());
        QVERIFY(controller.statusText().contains(QStringLiteral("无法启动")));
        controller.showUpdater(); // The optional native observer is absent in this fixture.
        QVERIFY(controller.statusText().contains(QStringLiteral("无法启动")));
    }
    void releaseLinksStayOnProjectAndWorkForUnmanagedInstallations() {
        QVERIFY(object("status.json", {{"state", "available"}, {"version", "2.1.0"}}));
        UpdateController controller(root + "/synthetic-data", false);
        LinkReceiver receiver;
        QDesktopServices::setUrlHandler("https", &receiver, "accept");
        const auto restore = qScopeGuard([] { QDesktopServices::unsetUrlHandler("https"); });
        QVERIFY(controller.openReleaseNotes());
        QCOMPARE(receiver.last, QUrl("https://github.com/downdone/daymark/releases/tag/v2.1.0"));
        QVERIFY(
            object("status.json", {{"state", "available"}, {"version", "../../other.invalid"}}));
        QTRY_COMPARE(controller.availableVersion(), QString("../../other.invalid"));
        QVERIFY(controller.openReleaseNotes());
        QCOMPARE(receiver.last, QUrl("https://github.com/downdone/daymark/releases"));
        UpdateController unmanaged(root + "/other-synthetic-data", true);
        QVERIFY(!unmanaged.supported());
        QVERIFY(unmanaged.openReleaseNotes());
        QCOMPARE(receiver.last, QUrl("https://github.com/downdone/daymark/releases"));
    }
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc == 3 && QString::fromLocal8Bit(argv[1]) == "--owned-fixture") {
        UpdateControllerTest test(QString::fromLocal8Bit(argv[2]));
        int count = 1;
        return QTest::qExec(&test, count, argv);
    }
    // Exercise production installation discovery without introducing a root/trust
    // override into the application. Only this temporary child image is launched.
    QTemporaryDir temporary;
    if (!temporary.isValid())
        return 2;
    const auto entry = temporary.filePath("versions/payload/test_update_controller.exe");
    if (!QDir().mkpath(QFileInfo(entry).absolutePath()) ||
        !QDir().mkpath(temporary.filePath("update-state")) ||
        !QFile::copy(app.applicationFilePath(), entry))
        return 2;
    QFile cli(temporary.filePath("daymark-update-cli.exe"));
    if (!cli.open(QIODevice::WriteOnly) || cli.write("deliberately-not-an-executable") < 0)
        return 2;
    cli.close();
    QProcess child;
    child.start(entry, {"--owned-fixture", temporary.path()});
    if (!child.waitForStarted(5000) || !child.waitForFinished(15000)) {
        child.kill();
        child.waitForFinished(5000);
        return 3;
    }
    const auto output = child.readAllStandardOutput();
    std::fwrite(output.constData(), 1, output.size(), stdout);
    const auto errors = child.readAllStandardError();
    std::fwrite(errors.constData(), 1, errors.size(), stderr);
    return child.exitStatus() == QProcess::NormalExit ? child.exitCode() : 3;
}
#include "UpdateControllerTest.moc"
