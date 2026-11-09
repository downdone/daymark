#include "platform/common/WindowChrome.h"
#include <QCloseEvent>
#include <QDir>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>
#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif
using namespace daymark;
#ifdef Q_OS_WIN
static QString clientAreaMismatch(QQuickWindow &window) {
    RECT client{};
    const auto handle = reinterpret_cast<HWND>(window.winId());
    if (!GetClientRect(handle, &client))
        return QStringLiteral("GetClientRect failed");
    const auto dpr = window.devicePixelRatio();
    const QSize nativeSize(client.right - client.left, client.bottom - client.top);
    const QSize renderedSize = window.size() * dpr;
    if (qAbs(nativeSize.width() - renderedSize.width()) <= 1 &&
        qAbs(nativeSize.height() - renderedSize.height()) <= 1)
        return {};
    return QStringLiteral("Win32 client %1x%2 px, Qt window %3x%4 px at DPR %5")
        .arg(nativeSize.width()).arg(nativeSize.height())
        .arg(renderedSize.width()).arg(renderedSize.height()).arg(dpr);
}
static QRect screenClientRect(HWND hwnd) {
    POINT origin{};
    RECT client{};
    if (!ClientToScreen(hwnd, &origin) || !GetClientRect(hwnd, &client))
        return {};
    return {origin.x, origin.y, client.right, client.bottom};
}
#endif
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
        const auto checkClient = [&] {
            const auto mismatch = clientAreaMismatch(window);
            QVERIFY2(mismatch.isEmpty(), qPrintable(mismatch));
        };
        checkClient();
        qInfo("CHROME native window exposed");
        auto nativeRect = [&] {
            RECT r{};
            GetWindowRect(reinterpret_cast<HWND>(window.winId()), &r);
            return QRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
        };
        const auto original = nativeRect();
        const auto handle = window.winId();
        const auto hwnd = reinterpret_cast<HWND>(handle);
        const auto nativeHit = [hwnd](int x, int y) {
            return SendMessageW(hwnd, WM_NCHITTEST, 0, MAKELPARAM(x, y));
        };
        QCOMPARE(nativeHit(original.left() + 2, original.top() + 2),
                 LRESULT(HTTOPLEFT));
        QCOMPARE(nativeHit(original.right() - 2, original.bottom() - 2),
                 LRESULT(HTBOTTOMRIGHT));
        QCOMPARE(nativeHit(original.left() + 2, original.center().y()),
                 LRESULT(HTLEFT));
        QCOMPARE(nativeHit(original.right() - 2, original.center().y()),
                 LRESULT(HTRIGHT));
        QCOMPARE(nativeHit(original.center().x(), original.top() + 2),
                 LRESULT(HTTOP));
        QCOMPARE(nativeHit(original.center().x(), original.bottom() - 2),
                 LRESULT(HTBOTTOM));
        const auto checkOutline = [](HWND target) {
            RECT bounds{};
            QVERIFY(SUCCEEDED(DwmGetWindowAttribute(target, DWMWA_EXTENDED_FRAME_BOUNDS,
                                                    &bounds, sizeof(bounds))));
            const QRect frame(bounds.left, bounds.top, bounds.right - bounds.left,
                              bounds.bottom - bounds.top);
            const auto client = screenClientRect(target);
            QVERIFY(frame.contains(client));
            QVERIFY(frame.width() > client.width());
            QVERIFY(frame.height() > client.height());
            qInfo() << "CHROME visible frame and client" << frame << client;
        };
        checkOutline(hwnd);
        QCOMPARE(binding.hitTest({200, 20}), HTCAPTION);
        QCOMPARE(binding.hitTest({780, 20}), HTCLIENT);
        QCOMPARE(binding.hitTest({830, 20}), HTMAXBUTTON);
        QCOMPARE(binding.hitTest({200, 200}), HTCLIENT);
        auto *interactive = new QQuickItem(window.contentItem());
        interactive->setX(400); interactive->setWidth(70); interactive->setHeight(40);
        binding.setInteractiveItems({QVariant::fromValue(interactive)});
        QCOMPARE(binding.hitTest({410,20}), HTCLIENT);
        interactive->setEnabled(false);
        QCOMPARE(binding.hitTest({410,20}), HTCLIENT);
        delete interactive;
        QVERIFY(binding.interactiveItems().isEmpty());
        QCOMPARE(binding.hitTest({410,20}), HTCAPTION);
        QCOMPARE(binding.hitTest({-2, 20}), HTNOWHERE);
        qInfo("CHROME native hit policy passed");
        {
            QQuickWindow settingsWindow;
            settingsWindow.setTransientParent(&window);
            settingsWindow.setFlags(Qt::Dialog | Qt::WindowTitleHint |
                                    Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);
            settingsWindow.setGeometry(220, 220, 600, 400);
            WindowChrome settingsBinding;
            settingsBinding.setWindow(&settingsWindow);
            settingsBinding.setController(&settings);
            settingsBinding.componentComplete();
            settingsWindow.show();
            QVERIFY(QTest::qWaitForWindowExposed(&settingsWindow));
            const auto settingsHwnd = reinterpret_cast<HWND>(settingsWindow.winId());
            QVERIFY(screenClientRect(hwnd).intersects(screenClientRect(settingsHwnd)));
            const auto mismatch = clientAreaMismatch(settingsWindow);
            QVERIFY2(mismatch.isEmpty(), qPrintable(mismatch));
            checkOutline(settingsHwnd);
            checkClient();
        }
        QVERIFY(SetWindowPos(hwnd, nullptr, 0, 0, original.width() + 70,
                             original.height() + 35,
                             SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
        checkClient();
        QVERIFY(SetWindowPos(hwnd, nullptr, 0, 0, original.width(), original.height(),
                             SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
        checkClient();
        for (int i = 0; i < 8; ++i) {
            QVERIFY(settings.requestMode(0));
            QCOMPARE(nativeRect(), original);
            QVERIFY(!binding.customEnabled());
            QVERIFY(!window.flags().testFlag(Qt::FramelessWindowHint));
            checkClient();
            QVERIFY(settings.requestMode(1));
            QCOMPARE(nativeRect(), original);
            QCOMPARE(window.winId(), handle);
            checkClient();
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
        QVERIFY(IsZoomed(hwnd));
        checkClient();
        const auto checkMaximizedWorkArea = [hwnd] {
            MONITORINFO monitor{};
            monitor.cbSize = sizeof(monitor);
            QVERIFY(GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST),
                                    &monitor));
            const auto &work = monitor.rcWork;
            const QRect workArea(work.left, work.top, work.right - work.left,
                                 work.bottom - work.top);
            const auto client = screenClientRect(hwnd);
            qInfo() << "CHROME maximized work area and client" << workArea << client;
            QVERIFY(!client.isEmpty());
            // A live mode switch can retain one physical pixel of the previous
            // native frame while preserving the maximized HWND placement.
            QVERIFY(workArea.adjusted(-1, -1, 1, 1).contains(client));
        };
        checkMaximizedWorkArea();
        QVERIFY(settings.requestMode(0));
        QCoreApplication::processEvents();
        QCOMPARE(window.windowState(), Qt::WindowMaximized);
        QVERIFY(IsZoomed(reinterpret_cast<HWND>(handle)));
        checkClient();
        QVERIFY(settings.requestMode(1));
        QCoreApplication::processEvents();
        QCOMPARE(window.windowState(), Qt::WindowMaximized);
        QVERIFY(IsZoomed(reinterpret_cast<HWND>(handle)));
        checkClient();
        checkMaximizedWorkArea();
        binding.toggleMaximize();
        QTRY_COMPARE(window.windowState(), Qt::WindowNoState);
        QCOMPARE(nativeRect(), original);
        checkClient();
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
        POINT caption{LONG(200 * window.devicePixelRatio()),
                      LONG(20 * window.devicePixelRatio())};
        QVERIFY(ClientToScreen(hwnd, &caption));
        QCOMPARE(nativeHit(caption.x, caption.y), LRESULT(HTCAPTION));
        SendMessageW(hwnd, WM_NCLBUTTONDBLCLK, HTCAPTION,
                     MAKELPARAM(caption.x, caption.y));
        QTRY_VERIFY(IsZoomed(hwnd));
        checkClient();
        checkMaximizedWorkArea();
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
