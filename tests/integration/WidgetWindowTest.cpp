#include "models/WidgetViewModel.h"
#include "platform/common/QtWindowPlatform.h"
#include "platform/common/NativeWindowLayer.h"
#include "platform/common/DesktopTray.h"
#include <QAction>
#include <QDir>

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
    bool rejectDesktop=false;
    WindowResult prepare(QWindow* w) override { return qt.prepare(w); }
    WindowResult resize(QWindow* w, const QSize& size) override
    { return rejectResize ? WindowResult{WindowError::OperationRejected} : qt.resize(w, size); }
    WindowResult setAlwaysOnTop(QWindow* w, bool top) override
    { return rejectTop ? WindowResult{WindowError::OperationRejected} : qt.setAlwaysOnTop(w, top); }
    WindowResult setDesktopPinned(QWindow* w,bool enabled)override
    {return rejectDesktop?WindowResult{WindowError::OperationRejected}:qt.setDesktopPinned(w,enabled);}
    WindowResult setVisible(QWindow* w, bool visible) override { return qt.setVisible(w, visible); }
    WindowResult startMove(QWindow*) override { return {WindowError::MoveUnsupported}; }
};

// Command routing fixture, not evidence of a native shell tray.
class TrayCommandWindow final : public QWindow {
    Q_OBJECT
public:
    QVariant newTaskScope,openedTool;
    Q_INVOKABLE void newTask(const QVariant& scope) {newTaskScope=scope;}
    Q_INVOKABLE void openTool(const QVariant& tool) {openedTool=tool;}
};

class WidgetWindowTest : public QObject {
    Q_OBJECT
private slots:
    void trayCommandsAreGroupedAndRecoveryTracksState()
    {
        QTemporaryDir dir;QtWindowPlatform platform;WidgetViewModel model(platform);QWindow window;TrayCommandWindow main;
        window.resize(340,360);model.configureSettings(dir.filePath("tray.ini"));QVERIFY(model.attachWindow(&window));
        DesktopTray tray(model,&main);auto* menu=tray.contextMenu();QVERIFY(menu);
        QStringList order;bool previousRule=true;
        for(auto* action:menu->actions()){
            if(action->isSeparator()){QVERIFY(!previousRule);order.append("|");previousRule=true;}
            else {order.append(action->objectName());previousRule=false;}
        }
        QVERIFY(!previousRule);
        QCOMPARE(order,QStringList({"trayShowMain","trayNewTask","|","trayToggleWidget","|","trayClickThrough","trayRestoreInput","trayResetWidget","|","traySettings","|","trayQuit"}));
        auto action=[&](const char* id){return menu->findChild<QAction*>(QString::fromLatin1(id));};
        QVERIFY(!action("trayRestoreInput")->isEnabled());QVERIFY(!action("trayClickThrough")->isChecked());
        QCOMPARE(action("trayToggleWidget")->text(),QString::fromUtf8("显示任务组件"));
        action("trayToggleWidget")->trigger();QVERIFY(model.visible());QCOMPARE(action("trayToggleWidget")->text(),QString::fromUtf8("隐藏任务组件"));
        action("trayToggleWidget")->trigger();QVERIFY(!model.visible());
        // Exercise commands without claiming the shell icon exists on offscreen.
        model.setTrayAvailable(true);QVERIFY(model.setClickThrough(true));
        QVERIFY(action("trayClickThrough")->isChecked());QVERIFY(action("trayRestoreInput")->isEnabled());
        const auto artifacts=qEnvironmentVariable("DAYMARK_TEST_ARTIFACT_DIR");
        if(!artifacts.isEmpty()){
            QVERIFY(QDir().mkpath(artifacts));menu->popup(QPoint(20,20));QTest::qWait(30);
            QVERIFY(menu->grab().save(QDir(artifacts).filePath("tray-grouped-input-recovery.png")));menu->hide();
        }
        action("trayRestoreInput")->trigger();QVERIFY(!model.clickThrough());QVERIFY(!action("trayClickThrough")->isChecked());QVERIFY(!action("trayRestoreInput")->isEnabled());
        main.setWindowState(Qt::WindowMaximized);action("trayShowMain")->trigger();QVERIFY(main.isVisible());QCOMPARE(main.windowState(),Qt::WindowMaximized);
        action("trayNewTask")->trigger();QCOMPARE(main.newTaskScope.toString(),QStringLiteral("inbox"));
        action("traySettings")->trigger();QCOMPARE(main.openedTool.toString(),QStringLiteral("settings"));
        main.hide();model.setVisible(false);
    }
    void desktopLayerPersistsAndSurvivesWindowChanges()
    {
        QTemporaryDir dir;QtWindowPlatform platform;WidgetViewModel model(platform);QWindow window;
        window.setGeometry(80,100,360,240);model.configureSettings(dir.filePath("ui.ini"));QVERIFY(model.attachWindow(&window));
        const auto geometry=window.geometry();QVERIFY(model.setDesktopPinned(true));QVERIFY(model.desktopPinned());QVERIFY(!model.alwaysOnTop());QCOMPARE(window.geometry(),geometry);QVERIFY(!model.visible());
        QVERIFY(model.setMini(true));QVERIFY(model.desktopPinned());QCOMPARE(window.size(),QSize(geometry.width(),48));QVERIFY(model.setMini(false));QCOMPARE(window.geometry(),geometry);
        model.setTrayAvailable(true);QVERIFY(model.setClickThrough(true));QVERIFY(model.desktopPinned());QVERIFY(model.setClickThrough(false));
        QVERIFY(model.setVisible(true));QCoreApplication::processEvents();
        if(QGuiApplication::platformName()=="offscreen")QVERIFY(!nativeWindowLayerMatches(&window,WindowLayer::Desktop));
        else {QTRY_VERIFY(nativeWindowLayerMatches(&window,WindowLayer::Desktop));window.raise();window.requestActivate();QCoreApplication::processEvents();QVERIFY(nativeWindowLayerMatches(&window,WindowLayer::Desktop));}
        QVERIFY(model.setVisible(false));QVERIFY(model.setVisible(true));QCoreApplication::processEvents();QVERIFY(model.desktopPinned());
        model.saveSettings();
        WidgetViewModel restored(platform);QWindow second;second.resize(340,190);restored.configureSettings(dir.filePath("ui.ini"));QVERIFY(restored.attachWindow(&second));QVERIFY(restored.desktopPinned());QVERIFY(!restored.alwaysOnTop());
        QVERIFY(model.setAlwaysOnTop(true));QVERIFY(model.alwaysOnTop());QVERIFY(!model.desktopPinned());
        QVERIFY(model.setDesktopPinned(true));QVERIFY(model.setDesktopPinned(false));QVERIFY(!model.desktopPinned() && !model.alwaysOnTop());QCOMPARE(window.geometry(),geometry);
        if(QGuiApplication::platformName()!="offscreen")QVERIFY(nativeWindowLayerMatches(&window,WindowLayer::Normal));
    }
    void rejectedDesktopModeKeepsOriginalLayer()
    {
        RejectingPlatform platform;WidgetViewModel model(platform);QWindow window;window.resize(360,240);QVERIFY(model.attachWindow(&window));
        platform.rejectDesktop=true;QVERIFY(!model.setDesktopPinned(true));QVERIFY(!model.desktopPinned());QVERIFY(model.alwaysOnTop());QVERIFY(!model.errorText().isEmpty());
        platform.rejectDesktop=false;QVERIFY(model.setDesktopPinned(true));platform.rejectTop=true;QVERIFY(!model.setAlwaysOnTop(true));QVERIFY(model.desktopPinned());QVERIFY(!model.alwaysOnTop());
    }
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
        QCOMPARE(window.size(), QSize(400, 48));
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
        QCOMPARE(window.size(), QSize(340, 48));
        window.resize(420,48);
        QVERIFY(model.setMini(false));
        QCOMPARE(window.size(), QSize(420, 190));
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
        QCOMPARE(window.size(), QSize(340, 48));
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
