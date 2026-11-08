#include "platform/common/WindowChrome.h"
#include <QCloseEvent>
#include <QDir>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
using namespace daymark;
class NativeFailureBinding : public WindowChrome {
  public:
    bool rejectNative = false;
    bool applyMode(bool custom) override {
        return !custom && rejectNative ? false
                                       : WindowChrome::applyMode(custom);
    }
};
class WindowChromeTest : public QObject {
    Q_OBJECT
  private slots:
    void partialNativeFailureRollsBackPreviouslyAppliedWindows() {
        QTemporaryDir directory;
        WindowChromeController settings(directory.filePath("ui.ini"));
        if (!settings.supported())
            QSKIP("Windows chrome preference is not supported");
        QQuickWindow firstWindow, secondWindow;
        WindowChrome first;
        NativeFailureBinding second;
        first.setWindow(&firstWindow);
        first.setController(&settings);
        first.componentComplete();
        second.setWindow(&secondWindow);
        second.setController(&settings);
        second.componentComplete();
        second.rejectNative = true;
        QVERIFY(!settings.requestMode(0));
        QCOMPARE(settings.mode(), 1);
        QVERIFY(first.customEnabled());
        QVERIFY(second.customEnabled());
        QCOMPARE(settings.errorKey(),
                 QStringLiteral("windowChromeNativeFailed"));
        QSettings saved(directory.filePath("ui.ini"), QSettings::IniFormat);
        QVERIFY(!saved.contains("window/titleBarMode"));
    }
    void preferenceValidationPersistenceAndRestrictedWriteGate() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("ui.ini");
        WindowChromeController settings(path);
        if (!settings.supported()) {
            QVERIFY(!settings.requestMode(1));
            return;
        }
        QCOMPARE(settings.mode(), 1);
        QVERIFY(!settings.requestMode(3));
        QCOMPARE(settings.mode(), 1);
        QCOMPARE(settings.errorKey(),
                 QStringLiteral("windowChromeUnavailable"));
        QVERIFY(settings.requestMode(0));
        QVERIFY(settings.errorText().isEmpty());
        WindowChromeController restored(path, false);
        QCOMPARE(restored.mode(), 0);
        QVERIFY(!restored.requestMode(1));
        QCOMPARE(restored.mode(), 0);
        restored.setWritable(true);
        QVERIFY(restored.requestMode(1));
        QSettings saved(path, QSettings::IniFormat);
        QCOMPARE(saved.value("window/titleBarMode").toInt(), 1);
        QVERIFY(saved.allKeys() == QStringList{"window/titleBarMode"});
        saved.setValue("window/titleBarMode", "invalid");
        saved.sync();
        WindowChromeController corrupted(path);
        QCOMPARE(corrupted.mode(), 1);
    }
    void settingsFailureKeepsAllWindowBindingsAndPreferenceUnchanged() {
        QTemporaryDir directory;
        const auto path = directory.filePath("not-an-ini");
        QVERIFY(QDir().mkpath(path));
        WindowChromeController settings(path);
        if (!settings.supported())
            QSKIP("Native Windows chrome is not supported by this platform");
        QQuickWindow a, b;
        WindowChrome first, second;
        for (auto *binding : {&first, &second})
            binding->setController(&settings);
        first.setWindow(&a);
        second.setWindow(&b);
        first.componentComplete();
        second.componentComplete();
        QVERIFY(first.customEnabled());
        QVERIFY(second.customEnabled());
        QVERIFY(!settings.requestMode(0));
        QCOMPARE(settings.mode(), 1);
        QVERIFY(first.customEnabled());
        QVERIFY(second.customEnabled());
        QCOMPARE(settings.errorKey(),
                 QStringLiteral("windowChromeSettingsFailed"));
        QVERIFY(settings.errorText().contains("S000-C902"));
    }
    void nativeFrameAndHitTargetsSurviveRoundTrips() {
#ifdef Q_OS_WIN
        QTemporaryDir directory;
        WindowChromeController settings(directory.filePath("ui.ini"));
        if (!settings.supported())
            QSKIP("A real Windows QPA backend is required");
        QQuickWindow window;
        window.setFlags(Qt::Window | Qt::WindowTitleHint |
                        Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint |
                        Qt::WindowCloseButtonHint);
        window.setGeometry(120, 130, 900, 650);
        WindowChrome binding;
        binding.setWindow(&window);
        binding.setController(&settings);
        binding.componentComplete();
        QQuickItem title(window.contentItem()), buttons(window.contentItem()),
            maximize(window.contentItem());
        title.setWidth(900);
        title.setHeight(40);
        buttons.setX(762);
        buttons.setWidth(138);
        buttons.setHeight(40);
        maximize.setX(808);
        maximize.setWidth(46);
        maximize.setHeight(40);
        QVERIFY(binding.setProperty("titleBar", QVariant::fromValue(&title)));
        QVERIFY(
            binding.setProperty("buttonStrip", QVariant::fromValue(&buttons)));
        QVERIFY(binding.setProperty("maximizeButton",
                                    QVariant::fromValue(&maximize)));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        qInfo("CHROME native window exposed");
        auto nativeRect = [&] {
            RECT r{};
            GetWindowRect(reinterpret_cast<HWND>(window.winId()), &r);
            return QRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
        };
        const auto original = nativeRect();
        QCOMPARE(binding.hitTest({2, 2}, 8), HTTOPLEFT);
        QCOMPARE(binding.hitTest({898, 648}, 8), HTBOTTOMRIGHT);
        QCOMPARE(binding.hitTest({200, 20}, 8), HTCAPTION);
        QCOMPARE(binding.hitTest({780, 20}, 8), HTCLIENT);
        QCOMPARE(binding.hitTest({830, 20}, 8), HTMAXBUTTON);
        QCOMPARE(binding.hitTest({200, 200}, 8), HTCLIENT);
        auto *interactive = new QQuickItem(window.contentItem());
        interactive->setX(400); interactive->setWidth(70); interactive->setHeight(40);
        binding.setInteractiveItems({QVariant::fromValue(interactive)});
        QCOMPARE(binding.hitTest({410,20},8), HTCLIENT);
        interactive->setEnabled(false);
        QCOMPARE(binding.hitTest({410,20},8), HTCLIENT);
        delete interactive;
        QVERIFY(binding.interactiveItems().isEmpty());
        QCOMPARE(binding.hitTest({410,20},8), HTCAPTION);
        QCOMPARE(binding.hitTest({-2, 20}, 8), HTNOWHERE);
        const auto handle = window.winId();
        qInfo("CHROME native hit policy passed");
        for (int i = 0; i < 8; ++i) {
            QVERIFY(settings.requestMode(0));
            QCOMPARE(nativeRect(), original);
            QVERIFY(!binding.customEnabled());
            QVERIFY(!window.flags().testFlag(Qt::FramelessWindowHint));
            QVERIFY(settings.requestMode(1));
            QCOMPARE(nativeRect(), original);
            QCOMPARE(window.winId(), handle);
        }
        POINT at{LONG(830 * window.devicePixelRatio()),
                 LONG(20 * window.devicePixelRatio())};
        qInfo("CHROME native round trips passed");
        ClientToScreen(reinterpret_cast<HWND>(handle), &at);
        QCOMPARE(SendMessageW(reinterpret_cast<HWND>(handle), WM_NCHITTEST, 0,
                              MAKELPARAM(at.x, at.y)),
                 LRESULT(HTMAXBUTTON));
        SendMessageW(reinterpret_cast<HWND>(handle), WM_NCLBUTTONDOWN,
                     HTMAXBUTTON, MAKELPARAM(at.x, at.y));
        qInfo("CHROME native maximize pressed");
        QVERIFY(binding.nativeMaximizePressed());
        SendMessageW(reinterpret_cast<HWND>(handle), WM_NCMOUSELEAVE, 0, 0);
        QVERIFY(binding.nativeMaximizePressed());
        const POINT client{LONG(830 * window.devicePixelRatio()),
                           LONG(20 * window.devicePixelRatio())};
        SendMessageW(reinterpret_cast<HWND>(handle), WM_LBUTTONUP, 0,
                     MAKELPARAM(client.x, client.y));
        QVERIFY(!binding.nativeMaximizePressed());
        QTRY_COMPARE(window.windowState(), Qt::WindowMaximized);
        QVERIFY(IsZoomed(reinterpret_cast<HWND>(handle)));
        QVERIFY(settings.requestMode(0));
        QCoreApplication::processEvents();
        QCOMPARE(window.windowState(), Qt::WindowMaximized);
        QVERIFY(IsZoomed(reinterpret_cast<HWND>(handle)));
        QVERIFY(settings.requestMode(1));
        QCoreApplication::processEvents();
        QCOMPARE(window.windowState(), Qt::WindowMaximized);
        QVERIFY(IsZoomed(reinterpret_cast<HWND>(handle)));
        binding.toggleMaximize();
        QTRY_COMPARE(window.windowState(), Qt::WindowNoState);
        QCOMPARE(nativeRect(), original);
        window.showMinimized();
        QTRY_COMPARE(window.windowState(), Qt::WindowMinimized);
        QVERIFY(settings.requestMode(0));
        QCoreApplication::processEvents();
        QCOMPARE(window.windowState(), Qt::WindowMinimized);
        QVERIFY(settings.requestMode(1));
        QCoreApplication::processEvents();
        QCOMPARE(window.windowState(), Qt::WindowMinimized);
        window.showNormal();
        QTRY_COMPARE(window.windowState(), Qt::WindowNoState);
        QCOMPARE(nativeRect(), original);
        window.hide();
        QVERIFY(settings.requestMode(0));
        QVERIFY(!window.isVisible());
        QVERIFY(settings.requestMode(1));
        QVERIFY(!window.isVisible());
#else
        QSKIP("Win32 integration is Windows only");
#endif
    }
};
QTEST_MAIN(WindowChromeTest)
#include "WindowChromeTest.moc"
