#include "diagnostics/RuntimeLog.h"
#include "sync/NetworkDiagnostics.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>
#include <atomic>
#include <thread>
#include <vector>

using namespace daymark;
namespace {
std::atomic<int> forwarded{0};
void observer(QtMsgType, const QMessageLogContext &, const QString &) { ++forwarded; }
QByteArray readLogs(const QString &directory) {
    QByteArray result;
    for (const auto &name : QDir(directory).entryList({"daymark-*.jsonl"}, QDir::Files)) {
        QFile file(QDir(directory).filePath(name));
        if (file.open(QIODevice::ReadOnly))
            result += file.readAll();
    }
    return result;
}
} // namespace
class RuntimeLogTest final : public QObject {
    Q_OBJECT
  private slots:
    void captureIsBoundedPrivateAndChainsOriginalHandler() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto original = qInstallMessageHandler(observer);
        forwarded = 0;
        const auto restore = qScopeGuard([&] {
            diagnostics::stop();
            qInstallMessageHandler(original);
        });
        QVERIFY(diagnostics::start(temporary.path(), {4096, 3, 7}));
        diagnostics::event(
            "network", "synthetic",
            {{"password", "must-not-appear"},
             {"title", "private task"},
             {"nested", QJsonObject{{"refresh_token", "secret-token"}}},
             {"message", "GET https://example.invalid/private?q=secret password=hidden"}});
        qWarning("synthetic warning https://example.invalid/secret");
        const auto installed = qInstallMessageHandler(original);
        installed(QtFatalMsg, QMessageLogContext("source.cpp", 12, "fixture", "qt.fixture"),
                  QStringLiteral("synthetic fatal flush"));
        qInstallMessageHandler(installed);
        const auto beforeRotation = readLogs(temporary.path());
        QVERIFY(beforeRotation.contains("synthetic fatal flush"));
        QVERIFY(beforeRotation.contains("synthetic warning"));
        QVERIFY(beforeRotation.contains("<endpoint>"));
        for (const auto &secret : {"must-not-appear", "private task", "secret-token",
                                   "example.invalid", "password=hidden"})
            QVERIFY(!beforeRotation.contains(secret));
        for (int i = 0; i < 30; ++i)
            diagnostics::event("fixture", "rotation", {{"message", QString(300, 'x')}});
        diagnostics::stop();
        qInstallMessageHandler(original);
        QVERIFY(forwarded >= 2);
        const auto content = readLogs(temporary.path());
        QVERIFY(content.contains("rotation"));
        QVERIFY(!content.contains("must-not-appear"));
        QVERIFY(!content.contains("private task"));
        QVERIFY(!content.contains("secret-token"));
        QVERIFY(!content.contains("example.invalid"));
        const auto files = QDir(temporary.path()).entryList({"daymark-*.jsonl"}, QDir::Files);
        QVERIFY(files.size() <= 2);
        for (const auto &file : files)
            QVERIFY(QFileInfo(QDir(temporary.path()).filePath(file)).size() < 8192);
        for (const auto &line : content.split('\n'))
            if (!line.isEmpty())
                QVERIFY(QJsonDocument::fromJson(line).isObject());
    }
    void concurrentEventsAndUnknownFilesArePreserved() {
        const auto restore = qScopeGuard([] { diagnostics::stop(); });
        QTemporaryDir temporary;
        QFile unknown(temporary.filePath("keep.txt"));
        QVERIFY(unknown.open(QIODevice::WriteOnly));
        unknown.write("user-owned");
        unknown.close();
        QVERIFY(diagnostics::start(temporary.path()));
        std::vector<std::thread> writers;
        for (int i = 0; i < 4; ++i)
            writers.emplace_back([i] {
                for (int j = 0; j < 20; ++j)
                    diagnostics::event("thread", "event", {{"worker", i}, {"sequence", j}});
            });
        for (auto &writer : writers)
            writer.join();
        diagnostics::stop();
        int count = 0;
        for (const auto &line : readLogs(temporary.path()).split('\n'))
            if (!line.isEmpty()) {
                const auto object = QJsonDocument::fromJson(line).object();
                QVERIFY(!object.isEmpty());
                if (object.value("component") == "thread")
                    ++count;
            }
        QCOMPARE(count, 80);
        QVERIFY(unknown.open(QIODevice::ReadOnly));
        QCOMPARE(unknown.readAll(), QByteArray("user-owned"));
        QFile blocker(temporary.filePath("file"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        QVERIFY(!diagnostics::start(blocker.fileName() + "/logs"));
        QVERIFY(!diagnostics::error().isEmpty());
        diagnostics::event("fixture", "unavailable-is-safe");
        diagnostics::stop();
    }
    void closedSessionsArePrunedWithoutRemovingUnknownFiles() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto restore = qScopeGuard([] { diagnostics::stop(); });
        const auto unknown = temporary.filePath("daymark-20000101T000000Z-0-00000000.jsonl");
        QFile file(unknown);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("user-owned");
        file.close();
        for (int i = 0; i < 6; ++i) {
            QVERIFY(diagnostics::start(temporary.path(), {4096, 2, 7}));
            diagnostics::stop();
        }
        const auto files = QDir(temporary.path()).entryList({"daymark-*.jsonl"}, QDir::Files);
        QVERIFY(files.size() <= 3);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("user-owned"));
    }
    void transportEvidencePreservesRealEnumsAndRedactsEndpoint() {
        auto value = sync::networkEvidence(
            QNetworkReply::SslHandshakeFailedError,
            "TLS failed for https://private.invalid/account password=abc12345",
            {QSslError(QSslError::CertificateExpired)}, "https://private.invalid", 0, 15000,
            {"abc12345"});
        QCOMPARE(value.value("qt_network_error").toInt(), 6);
        QCOMPARE(value.value("qt_network_error_name").toString(),
                 QString("SslHandshakeFailedError"));
        QCOMPARE(value.value("tls_errors").toArray().first().toObject().value("name").toString(),
                 QString("CertificateExpired"));
        const auto bytes = QJsonDocument(value).toJson();
        QVERIFY(!bytes.contains("private.invalid"));
        QVERIFY(!bytes.contains("abc12345"));
        QVERIFY(!value.value("native_error_available").toBool());
        auto http = sync::networkEvidence(QNetworkReply::ContentAccessDenied,
                                          "server echoed private task and password", {},
                                          "https://private.invalid", 403, 15000);
        QVERIFY(!QJsonDocument(http).toJson().contains("private task"));
    }
};
int main(int argc,char** argv)
{
    QCoreApplication::setApplicationName("DaymarkLogTest");
    QTemporaryDir early;
    if(!early.isValid() || !diagnostics::start(early.path()))return 2;
    qWarning("synthetic warning before QCoreApplication");
    QCoreApplication application(argc,argv);
    const auto captured=readLogs(early.path());
    diagnostics::stop();
    if(!captured.contains("synthetic warning before QCoreApplication"))return 3;
    RuntimeLogTest test;return QTest::qExec(&test,argc,argv);
}
#include "RuntimeLogTest.moc"
