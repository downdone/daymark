#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
class UpdateFlowTest final : public QObject {
    Q_OBJECT
  private slots:
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
        QVERIFY(IsWindowVisible(search.window));
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
        if (qEnvironmentVariableIsSet("DAYMARK_UPDATE_REAL_OLD"))
            args << "--old-payload" << qEnvironmentVariable("DAYMARK_UPDATE_REAL_OLD")
                 << "--new-payload" << qEnvironmentVariable("DAYMARK_UPDATE_REAL_NEW");
        process.start(QStringLiteral(DAYMARK_TEST_PYTHON), args);
        QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
        const bool finished = process.waitForFinished(180000);
        const auto output = process.readAll();
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
        QVERIFY2(output.contains("PASS: signed Windows flow"), output.constData());
        qInfo().noquote() << output;
    }
};
QTEST_GUILESS_MAIN(UpdateFlowTest)
#include "UpdateFlowTest.moc"
