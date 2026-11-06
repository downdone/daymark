#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <cstdio>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
class UpdateFlowTest final : public QObject {
    Q_OBJECT
  private slots:
    void firstLaunchReportsActualPermissionError() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto executable = temporary.filePath("daymark.exe");
        QVERIFY(QFile::copy(QCoreApplication::applicationDirPath() + "/test_update_bootstrap.exe",
                            executable));
        QVERIFY(QDir().mkpath(temporary.filePath("update-state/locks")));
        const auto lock = temporary.filePath("update-state/locks/launch");
        QFile file(lock);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QVERIFY(SetFileAttributesW(reinterpret_cast<const wchar_t *>(lock.utf16()),
                                   FILE_ATTRIBUTE_READONLY));
        struct Attributes {
            QString path;
            ~Attributes() { SetFileAttributesW(reinterpret_cast<const wchar_t *>(path.utf16()),
                                               FILE_ATTRIBUTE_NORMAL); }
        } restore{lock};
        QProcess process;
        process.start(executable, {});
        QVERIFY(process.waitForStarted());
        HWND dialog = nullptr;
        struct Search { DWORD pid; HWND *window; } search{DWORD(process.processId()), &dialog};
        auto locate = [&] {
            EnumWindows([](HWND window, LPARAM value) -> BOOL {
                auto &search = *reinterpret_cast<Search *>(value);
                DWORD pid = 0;
                GetWindowThreadProcessId(window, &pid);
                wchar_t className[128]{};
                GetClassNameW(window, className, 128);
                if (pid == search.pid && std::wstring_view(className) == L"#32770") {
                    *search.window = window;
                    return FALSE;
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&search));
            return dialog != nullptr;
        };
        QTRY_VERIFY_WITH_TIMEOUT(locate(), 5000);
        QString message;
        EnumChildWindows(dialog, [](HWND control, LPARAM value) -> BOOL {
            wchar_t text[2048]{};
            GetWindowTextW(control, text, 2048);
            *reinterpret_cast<QString *>(value) += QString::fromWCharArray(text) + '\n';
            return TRUE;
        }, reinterpret_cast<LPARAM>(&message));
        // Close our own native dialog before assertions so failures never leave it running.
        PostMessageW(dialog, WM_CLOSE, 0, 0);
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitCode(), 31);
        QVERIFY2(message.contains("update.lock_access_denied"), qPrintable(message));
        QVERIFY2(message.contains(QStringLiteral("Windows 错误：5")), qPrintable(message));
        QVERIFY(!message.contains("instance_busy"));
        QVERIFY(!message.contains("reinstall"));
        const auto logs = QDir(temporary.filePath("update-state/logs")).entryList({"*.jsonl"}, QDir::Files);
        QVERIFY(!logs.isEmpty());
        QFile log(temporary.filePath("update-state/logs/" + logs.first()));
        QVERIFY(log.open(QIODevice::ReadOnly));
        const auto records = log.readAll();
        QVERIFY(records.contains("\"platform_error\":5"));
        QVERIFY(records.contains("\"resource\":\"launch\""));
        QVERIFY(!records.contains(temporary.path().toUtf8()));
    }
    void nativeObserverWindowUsesSystemControlsAndCloses() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto executable = temporary.filePath("daymark-updater.exe");
        QVERIFY(QFile::copy(QCoreApplication::applicationDirPath() + "/daymark-updater.exe",
                            executable));
        QProcess process;
        process.start(executable, {});
        QVERIFY(process.waitForStarted());
        struct WindowSearch {
            DWORD pid;
            HWND window = nullptr;
        };
        WindowSearch search{static_cast<DWORD>(process.processId())};
        auto locate = [&] {
            EnumWindows(
                [](HWND window, LPARAM value) -> BOOL {
                    auto *search = reinterpret_cast<WindowSearch *>(value);
                    DWORD pid = 0;
                    GetWindowThreadProcessId(window, &pid);
                    wchar_t name[64]{};
                    GetClassNameW(window, name, 64);
                    if (pid == search->pid && std::wstring_view(name) == L"DaymarkUpdater") {
                        search->window = window;
                        return FALSE;
                    }
                    return TRUE;
                },
                reinterpret_cast<LPARAM>(&search));
            return search.window != nullptr;
        };
        QTRY_VERIFY_WITH_TIMEOUT(locate(), 5000);
        // EnumWindows can observe CreateWindow before the host's ShowWindow.
        // Wait for this same window to be presented; never reopen the observer.
        QTRY_VERIFY_WITH_TIMEOUT(IsWindowVisible(search.window), 5000);
        auto *primary = GetDlgItem(search.window, 1);
        auto *secondary = GetDlgItem(search.window, 2);
        QVERIFY(primary && secondary);
        wchar_t text[128]{};
        GetWindowTextW(primary, text, 128);
        QCOMPARE(QString::fromWCharArray(text), QStringLiteral("检查更新"));
        RECT client{};
        QVERIFY(GetClientRect(search.window, &client));
        for (HWND button : {primary, secondary}) {
            RECT rectangle{};
            QVERIFY(GetWindowRect(button, &rectangle));
            MapWindowPoints(nullptr, search.window, reinterpret_cast<POINT *>(&rectangle), 2);
            QVERIFY(rectangle.left >= 0 && rectangle.top >= 0 && rectangle.right <= client.right &&
                    rectangle.bottom <= client.bottom);
        }
        PostMessageW(search.window, WM_COMMAND, MAKEWPARAM(2, BN_CLICKED),
                     reinterpret_cast<LPARAM>(secondary));
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitCode(), 0);
        QVERIFY(!QFile::exists(temporary.filePath("current.json")));
        qInfo() << "PASS: actual Win32 updater window, native buttons, reachable layout, Later "
                   "closes observer without installation";
    }
    void signedWindowsTransaction() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        QStringList args{QStringLiteral(DAYMARK_UPDATE_FLOW_SCRIPT),
                         "--work",
                         temporary.path(),
                         "--bin",
                         QCoreApplication::applicationDirPath(),
                         "--repo",
                         QStringLiteral(DAYMARK_TEST_REPO)};
        const bool publicGitHub = qEnvironmentVariable("DAYMARK_UPDATE_PUBLIC_GITHUB") == "1";
        if (publicGitHub) {
            QVERIFY(qEnvironmentVariableIsSet("DAYMARK_UPDATE_PUBLIC_REPORT_DIR"));
            args = {QStringLiteral(DAYMARK_TEST_REPO) + "/tests/update/github_flow.py",
                    "--work", temporary.filePath("public-github"),
                    "--wait-for-target-seconds", "3600",
                    "--export-artifacts", qEnvironmentVariable("DAYMARK_UPDATE_PUBLIC_REPORT_DIR")};
        } else if (qEnvironmentVariableIsSet("DAYMARK_UPDATE_REAL_OLD"))
            args << "--old-payload" << qEnvironmentVariable("DAYMARK_UPDATE_REAL_OLD")
                 << "--new-payload" << qEnvironmentVariable("DAYMARK_UPDATE_REAL_NEW");
        QByteArray output;
        if (publicGitHub) {
            connect(&process, &QProcess::readyReadStandardOutput, &process, [&] {
                const auto chunk = process.readAllStandardOutput();
                output += chunk;
                std::fwrite(chunk.constData(), 1, static_cast<size_t>(chunk.size()), stderr);
                std::fflush(stderr);
            });
        }
        process.start(QStringLiteral(DAYMARK_TEST_PYTHON), args);
        QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
        // The opt-in public acceptance includes two real Internet downloads and
        // publication coordination. The ordinary local fixture keeps its budget.
        const bool finished = process.waitForFinished(publicGitHub ? 90 * 60 * 1000 : 180000);
        output += process.readAll();
        QFile evidence(QCoreApplication::applicationDirPath() +
                       QStringLiteral("/update-flow-child.log"));
        if (evidence.open(QIODevice::WriteOnly | QIODevice::Truncate))
            evidence.write(output);
        if (!finished) {
            process.kill();
            process.waitForFinished();
        }
        QVERIFY2(finished, output.constData());
        QVERIFY2(process.exitCode() == 0, output.constData());
        QVERIFY2(output.contains(publicGitHub ? "PASS: public GitHub Qt update flow" :
                                               "PASS: signed Windows flow"), output.constData());
        qInfo().noquote() << output;
    }
};
QTEST_GUILESS_MAIN(UpdateFlowTest)
#include "UpdateFlowTest.moc"
