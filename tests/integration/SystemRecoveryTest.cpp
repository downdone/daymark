#include "platform/common/SystemEventFilter.h"
#include "platform/common/WindowStateStore.h"
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
using namespace daymark;
class SystemRecoveryTest:public QObject {
    Q_OBJECT
private slots:
    void coalescesBurstAndCancelsOnDestruction() {
        int calls=0;
        {
            SystemEventFilter filter([&]{++calls;});
            for(int i=0;i<1000;i++)filter.requestRefresh();
            QCOMPARE(calls,0);QTRY_COMPARE(calls,1);
            filter.requestRefresh();QTRY_COMPARE(calls,2);
            filter.requestRefresh();
        }
        QTest::qWait(100);QCOMPARE(calls,2);
    }
    void nativeMessagesOnlyDispatchRelevantEvents() {
#ifdef Q_OS_WIN
        int calls=0;SystemEventFilter filter([&]{++calls;});qintptr result=123;
        MSG message{};message.message=WM_TIMECHANGE;
        QVERIFY(!filter.nativeEventFilter("unrelated",&message,&result));
        message.message=WM_POWERBROADCAST;message.wParam=PBT_APMSUSPEND;
        QVERIFY(!filter.nativeEventFilter("windows_generic_MSG",&message,&result));
        QTest::qWait(75);QCOMPARE(calls,0);
        for(const auto type:{WM_TIMECHANGE,WM_SETTINGCHANGE,WM_POWERBROADCAST}) {
            message.message=type;message.wParam=PBT_APMRESUMEAUTOMATIC;
            QVERIFY(!filter.nativeEventFilter("windows_generic_MSG",&message,&result));
        }
        QCOMPARE(calls,0);QTRY_COMPARE(calls,1);QCOMPARE(result,qintptr(123));
        QVERIFY(!filter.nativeEventFilter("windows_dispatcher_MSG",nullptr,&result));
#else
        QSKIP("Windows MSG dispatch is not available on this platform.");
#endif
    }
    void normalizationPreservesHiddenAndMinimizedStates() {
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("ui.ini"));
        QWindow window;window.setGeometry(-100000,-100000,1000,700);
        WindowStateStore state(&window,path);QVERIFY(!window.isVisible());
        const auto available=QGuiApplication::primaryScreen()->availableGeometry();
        QVERIFY(available.intersects(window.geometry()));
        window.setPosition(-100000,-100000);state.normalize();state.save();
        QSettings settings(path,QSettings::IniFormat);QCOMPARE(settings.value(QStringLiteral("main/geometry")).toRect(),window.geometry());
        const auto normal=window.geometry();window.setWindowState(Qt::WindowMinimized);state.normalize();state.save();
        QCOMPARE(window.windowState(),Qt::WindowMinimized);QVERIFY(!window.isVisible());
        settings.sync();QCOMPARE(settings.value(QStringLiteral("main/geometry")).toRect(),normal);
    }
    void destroyedWindowIsSafe() {
        QTemporaryDir dir;auto* window=new QWindow;WindowStateStore state(window,dir.filePath(QStringLiteral("ui.ini")));
        delete window;state.normalize();state.save();QTest::qWait(550);
    }
};
QTEST_MAIN(SystemRecoveryTest)
#include "SystemRecoveryTest.moc"
