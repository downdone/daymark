#include "models/WidgetViewModel.h"
#include "platform/common/QtWindowPlatform.h"

#include <QSignalSpy>
#include <QWindow>
#include <QtTest>
#include <memory>
#include <QTemporaryDir>

using namespace daymark;

// Failure injection verifies the command contract, not any OS window behavior.
class RejectingPlatform final : public IWindowPlatform {
public:
    QtWindowPlatform qt;
    bool rejectResize = false;
    bool rejectTop = false;
    WindowResult prepare(QWindow* w) override { return qt.prepare(w); }
    WindowResult resize(QWindow* w, const QSize& size) override
    { return rejectResize ? WindowResult{WindowError::OperationRejected} : qt.resize(w, size); }
    WindowResult setAlwaysOnTop(QWindow* w, bool top) override
    { return rejectTop ? WindowResult{WindowError::OperationRejected} : qt.setAlwaysOnTop(w, top); }
    WindowResult setVisible(QWindow* w, bool visible) override { return qt.setVisible(w, visible); }
    WindowResult startMove(QWindow*) override { return {WindowError::MoveUnsupported}; }
};

class WidgetWindowTest : public QObject {
    Q_OBJECT
private slots:
    void passthroughRequiresRecoveryAndSettingsStayIsolated()
    {
        QTemporaryDir dir;
        QtWindowPlatform platform;WidgetViewModel model(platform);QWindow window;
        window.resize(340,360);model.configureSettings(dir.filePath(QStringLiteral("ui.ini")));
        QVERIFY(model.attachWindow(&window));
        QVERIFY(!model.setClickThrough(true));QVERIFY(!model.clickThrough());
        model.setTrayAvailable(true);QVERIFY(model.setClickThrough(true));QVERIFY(model.clickThrough());
        model.setTrayAvailable(false);QVERIFY(!model.clickThrough());
        model.setLocked(true);QVERIFY(!model.startMove());
        model.setBackgroundOpacity(0.1);QCOMPARE(model.backgroundOpacity(),0.6);
        window.setPosition(-100000,-100000);model.normalizeGeometry();
        QVERIFY(QGuiApplication::primaryScreen()->availableGeometry().intersects(window.geometry()));
        model.saveSettings();
        WidgetViewModel restored(platform);QWindow second;second.resize(340,360);
        restored.configureSettings(dir.filePath(QStringLiteral("ui.ini")));QVERIFY(restored.attachWindow(&second));
        QVERIFY(restored.locked());QCOMPARE(restored.backgroundOpacity(),0.6);QVERIFY(!restored.clickThrough());
    }
    void hiddenTransitionsPreserveGeometryAndVisibility()
    {
        QtWindowPlatform platform;
        WidgetViewModel model(platform);
        QWindow owner;
        QWindow window;
        window.setGeometry(80, 100, 400, 220);
        window.setTransientParent(&owner);
        QVERIFY(model.attachWindow(&window));
        QVERIFY(!window.transientParent());
        QVERIFY(!model.visible());
        QVERIFY(model.alwaysOnTop());
        QVERIFY(model.setMini(true));
        QCOMPARE(window.size(), QSize(260, 48));
        QCOMPARE(window.position(), QPoint(80, 100));
        QVERIFY(!model.visible());
        QVERIFY(model.setAlwaysOnTop(false));
        QVERIFY(!model.alwaysOnTop());
        QVERIFY(!model.visible());
        QVERIFY(window.flags().testFlag(Qt::FramelessWindowHint));
        QCOMPARE(window.type(), Qt::Tool);
        QVERIFY(model.setMini(false));
        QCOMPARE(window.geometry(), QRect(80, 100, 400, 220));
        QVERIFY(!model.visible());
    }

    void shownTransitionsAndCloseStayInSync()
    {
        QtWindowPlatform platform;
        WidgetViewModel model(platform);
        QWindow window;
        window.setGeometry(80, 100, 340, 190);
        QVERIFY(model.attachWindow(&window));
        QVERIFY(model.setVisible(true));
        QSignalSpy changes(&model, &WidgetViewModel::stateChanged);
        QVERIFY(model.setAlwaysOnTop(false));
        QVERIFY(model.visible());
        QCOMPARE(window.geometry(), QRect(80, 100, 340, 190));
        QVERIFY(model.setMini(true));
        QVERIFY(model.visible());
        QVERIFY(model.setAlwaysOnTop(true));
        QCOMPARE(window.size(), QSize(260, 48));
        QVERIFY(model.setMini(false));
        QCOMPARE(window.size(), QSize(340, 190));
        QVERIFY(window.close());
        QVERIFY(!model.visible());
        QVERIFY(!changes.isEmpty());
    }

    void failedCommandsDoNotCommitState()
    {
        RejectingPlatform platform;
        WidgetViewModel model(platform);
        QWindow window;
        window.setGeometry(80, 100, 340, 190);
        QVERIFY(model.attachWindow(&window));
        platform.rejectResize = true;
        QVERIFY(!model.setMini(true));
        QVERIFY(!model.mini());
        QCOMPARE(window.size(), QSize(340, 190));
        QVERIFY(!model.errorText().isEmpty());
        platform.rejectTop = true;
        QSignalSpy changes(&model, &WidgetViewModel::stateChanged);
        QVERIFY(!model.setAlwaysOnTop(false));
        QVERIFY(model.alwaysOnTop());
        QVERIFY(!changes.isEmpty());
        platform.rejectResize = false;
        QVERIFY(model.setMini(true));
        QVERIFY(model.errorText().isEmpty());
        platform.rejectResize = true;
        QVERIFY(!model.setMini(false));
        QVERIFY(model.mini());
        QCOMPARE(window.size(), QSize(260, 48));
        platform.rejectResize = false;
        QVERIFY(model.setMini(false));
        QCOMPARE(window.size(), QSize(340, 190));
        QVERIFY(!model.startMove());
        QCOMPARE(model.errorCode(), static_cast<int>(WindowError::MoveUnsupported));
        QVERIFY(model.setMini(false)); // Successful no-op clears a stale error.
        QVERIFY(model.errorText().isEmpty());
        QVERIFY(!model.startMove());
        model.clearError();
        QVERIFY(model.errorText().isEmpty());
    }

    void destroyedWindowIsSafe()
    {
        QtWindowPlatform platform;
        WidgetViewModel model(platform);
        QVERIFY(!model.setMini(true));
        QVERIFY(!model.attachWindow(nullptr));
        auto window = std::make_unique<QWindow>();
        window->resize(340, 190);
        QVERIFY(model.attachWindow(window.get()));
        window.reset();
        QVERIFY(!model.ready());
        QVERIFY(!model.setAlwaysOnTop(false));
        QVERIFY(!model.setVisible(true));
        QVERIFY(!model.startMove());
        QCOMPARE(model.errorCode(), static_cast<int>(WindowError::WindowUnavailable));
    }

    void headlessDragCannotPassAsNativeDrag()
    {
        if (QGuiApplication::platformName() != QStringLiteral("offscreen"))
            QSKIP("This guard specifically tests the headless backend.");
        QtWindowPlatform platform;
        QWindow window;
        window.show();
        QCOMPARE(platform.startMove(&window).error, WindowError::MoveUnsupported);
    }
};

QTEST_MAIN(WidgetWindowTest)
#include "WidgetWindowTest.moc"
