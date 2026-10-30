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
    WindowResult setClickThrough(QWindow* w,bool enabled) override {return qt.setClickThrough(w,enabled);}
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
    void quickOpenPreservesDockingIntentAndCancelsOnExplicitSelection()
    {
        for(int compact:{1,2}){
            WidgetPresentation state;state.setEnabled(7);QVERIFY(state.select(compact));const auto history=state.history;
            QVERIFY(state.openPrimary());QCOMPARE(int(state.selected),compact);QCOMPARE(int(state.effective),0);QVERIFY(state.temporaryOpen);QCOMPARE(state.history,history);
            QVERIFY(!state.preview(2));QVERIFY(state.returnToSelected());QCOMPARE(int(state.effective),compact);QVERIFY(!state.temporaryOpen);
            QVERIFY(state.openPrimary());QVERIFY(state.select(0));QVERIFY(!state.temporaryOpen);QCOMPARE(int(state.selected),0);
            QVERIFY(state.select(compact));QVERIFY(state.openPrimary());state.setEnabled(1<<compact);QCOMPARE(int(state.effective),compact);QVERIFY(!state.temporaryOpen);
        }
        WidgetPresentation pair;pair.setEnabled(6);QVERIFY(pair.select(2));QVERIFY(pair.openPrimary());QCOMPARE(int(pair.effective),1);QVERIFY(pair.temporaryOpen);QVERIFY(pair.returnToSelected());QCOMPARE(int(pair.effective),2);
    }
    void clickedPanelReturnsToBallWithoutMiniOrAutoCompact()
    {
        QtWindowPlatform platform;WidgetViewModel model(platform);QWindow window;window.setGeometry(80,80,340,240);QVERIFY(model.attachWindow(&window));
        QVERIFY(model.setFormEnabled(2,true));QVERIFY(model.setFormEnabled(1,false));QVERIFY(model.selectForm(2));QVERIFY(!model.miniAutoCollapse());QVERIFY(model.setVisible(true));
        const auto ball=window.geometry();model.setInteraction(true,false,false,false);model.activatePrimary();
        QCOMPARE(model.selectedForm(),2);QCOMPARE(model.effectiveForm(),0);QVERIFY(!model.mini());QTest::qWait(1100);QCOMPARE(model.effectiveForm(),0);
        model.setInteraction(false,false,false,false);QTRY_COMPARE_WITH_TIMEOUT(model.effectiveForm(),2,1600);QCOMPARE(window.geometry(),ball);
        model.activatePrimary();QCOMPARE(model.effectiveForm(),0);QVERIFY(model.selectForm(0));model.setInteraction(false,false,false,false);QTest::qWait(1100);QCOMPARE(model.effectiveForm(),0);
    }
    void temporaryPanelReturnTimerIsCancelledByReentryAndInteraction()
    {
        QtWindowPlatform platform;WidgetViewModel model(platform);QWindow window;window.resize(340,240);QVERIFY(model.attachWindow(&window));QVERIFY(model.setFormEnabled(2,true));QVERIFY(model.selectForm(2));QVERIFY(model.setVisible(true));
        model.activatePrimary();model.setInteraction(false,false,false,false);QTest::qWait(500);model.setInteraction(true,false,false,false);QTest::qWait(700);QCOMPARE(model.effectiveForm(),0);
        for(int hold=0;hold<3;++hold){model.setInteraction(false,hold==0,hold==1,hold==2);QTest::qWait(1100);QCOMPARE(model.effectiveForm(),0);}
        model.setInteraction(false,false,false,false);QTRY_COMPARE_WITH_TIMEOUT(model.effectiveForm(),2,1600);
    }
    void temporaryPanelRestartAndDisableNeverPersistBorrowedMode()
    {
        QTemporaryDir dir;QtWindowPlatform platform;WidgetViewModel model(platform);QWindow window;window.resize(340,240);model.configureSettings(dir.filePath("dock.ini"));QVERIFY(model.attachWindow(&window));
        QVERIFY(model.setFormEnabled(2,true));QVERIFY(model.selectForm(2));model.activatePrimary();QCOMPARE(model.effectiveForm(),0);model.saveSettings();
        WidgetViewModel restored(platform);QWindow second;second.resize(340,240);restored.configureSettings(dir.filePath("dock.ini"));QVERIFY(restored.attachWindow(&second));QCOMPARE(restored.selectedForm(),2);QCOMPARE(restored.effectiveForm(),2);
        QVERIFY(model.setFormEnabled(2,false));QVERIFY(model.formEnabled(model.selectedForm()));QCOMPARE(model.effectiveForm(),model.selectedForm());
    }
    void everyEnabledCombinationHasLegalDirectTransitions()
    {
        for(int mask=0;mask<8;++mask){
            WidgetPresentation state;state.setEnabled(mask);
            QCOMPARE(state.count(),int(bool(mask&1))+int(bool(mask&2))+int(bool(mask&4)));
            for(int source=0;source<3;++source)for(int target=0;target<3;++target){
                auto copy=state;
                if(copy.allows(source))QVERIFY(copy.select(source));
                QCOMPARE(copy.select(target),bool(mask&(1<<target)));
                if(copy.allows(target)){QCOMPARE(int(copy.selected),target);QCOMPARE(int(copy.effective),target);}
            }
        }
    }
    void disablingCurrentOrPreviewUsesLegalHistory()
    {
        WidgetPresentation state;state.setEnabled(7);QVERIFY(state.select(2));QVERIFY(state.select(1));
        QVERIFY(state.preview(2));QCOMPARE(int(state.selected),1);
        state.setEnabled(3);QCOMPARE(int(state.effective),1);
        state.setEnabled(5);QCOMPARE(int(state.selected),2);
        state.setEnabled(0);QCOMPARE(state.count(),0);QVERIFY(!state.select(0));
        state.setEnabled(1);QCOMPARE(int(state.selected),0);
        QVERIFY(state.select(0));QVERIFY(!state.preview(1));
        state.setEnabled(4);QCOMPARE(state.primaryTarget(),-1);
        state.setEnabled(6);QCOMPARE(state.primaryTarget(),1);
    }
    void placementUsesAllCornersAndNeverWritesBackClamping()
    {
        const QList<WidgetScreen> screens{{"left",QRect(-1600,0,1600,900)},{"main",QRect(0,0,1920,1040)}};
        for(const auto& screen:screens)for(bool right:{false,true})for(bool bottom:{false,true}){
            const QPoint at(screen.available.x()+(right?screen.available.width()-40:4),screen.available.y()+(bottom?screen.available.height()-40:4));
            const auto anchor=WidgetPlacementPolicy::capture(QRect(at,QSize(36,36)),screen);
            for(int i=0;i<50;++i){
                const auto expanded=WidgetPlacementPolicy::resolve(anchor,QSize(420,360),screens);
                QVERIFY(screen.available.contains(expanded));
                QCOMPARE(WidgetPlacementPolicy::resolve(anchor,QSize(36,36),screens).topLeft(),at);
            }
        }
        QCOMPARE(WidgetPlacementPolicy::screenFor(QRect(-20,10,600,400),screens),1);
        const auto missing=WidgetPlacementPolicy::capture(QRect(1800,900,36,36),screens[1]);
        QVERIFY(screens[0].available.contains(WidgetPlacementPolicy::resolve(missing,QSize(340,360),{screens[0]})));
        QCOMPARE(WidgetPlacementPolicy::resolve(missing,QSize(36,36),screens).topLeft(),QPoint(1800,900));
    }
    void formEnablementRollbackAndEmptyTrayRecovery()
    {
        RejectingPlatform platform;WidgetViewModel model(platform);QWindow window;TrayCommandWindow main;
        window.setGeometry(80,80,420,240);QVERIFY(model.attachWindow(&window));QVERIFY(model.setVisible(true));
        QVERIFY(model.setFormEnabled(2,true));QVERIFY(model.selectForm(2));
        platform.rejectResize=true;QVERIFY(!model.setFormEnabled(2,false));QVERIFY(model.formEnabled(2));QCOMPARE(model.selectedForm(),2);QVERIFY(model.miniCollapsed());
        platform.rejectResize=false;
        for(int form=0;form<3;++form)QVERIFY(model.setFormEnabled(form,false));
        QCOMPARE(model.formCount(),0);QVERIFY(!model.visible());
        DesktopTray tray(model,&main);auto* show=tray.contextMenu()->findChild<QAction*>("trayToggleWidget");QVERIFY(show);QSignalSpy refresh(&model,&WidgetViewModel::stateChanged);show->trigger();QVERIFY(!refresh.isEmpty());
        QCOMPARE(main.openedTool.toString(),QString("settings"));QVERIFY(!model.visible());QCOMPARE(model.formCount(),0);
        QVERIFY(model.setFormEnabled(1,true));QVERIFY(model.visible());QCOMPARE(model.selectedForm(),1);
        QSignalSpy open(&model,&WidgetViewModel::openMainRequested);model.activatePrimary();QCOMPARE(open.count(),1);
    }
    void independentSizesExplicitIntentAndAnchorSurviveRestart()
    {
        QTemporaryDir dir;QtWindowPlatform platform;WidgetViewModel model(platform);QWindow window;window.setGeometry(80,100,340,360);
        model.configureSettings(dir.filePath("forms.ini"));QVERIFY(model.attachWindow(&window));
        window.resize(600,400);QVERIFY(model.selectForm(1));QCOMPARE(window.width(),340);
        QVERIFY(model.setFormEnabled(2,true));QVERIFY(model.setAutoCompact(true));QVERIFY(model.setMiniCollapsed(true));
        QCOMPARE(model.selectedForm(),1);model.saveSettings();
        QSettings saved(dir.filePath("forms.ini"),QSettings::IniFormat);QCOMPARE(saved.value("widget/v2/panelSize").toSize(),QSize(600,400));
        WidgetViewModel restored(platform);QWindow second;second.resize(340,360);restored.configureSettings(dir.filePath("forms.ini"));QVERIFY(restored.attachWindow(&second));
        QCOMPARE(restored.selectedForm(),1);QVERIFY(!restored.miniCollapsed());QCOMPARE(second.width(),340);
        QVERIFY(restored.selectForm(0));QCOMPARE(second.size(),QSize(600,400));
    }
    void automaticPreviewHonorsDwellPressMenuKeyboardAndDisabledTargets()
    {
        QtWindowPlatform platform;WidgetViewModel model(platform);QWindow window;window.setGeometry(80,80,340,240);QVERIFY(model.attachWindow(&window));
        QVERIFY(model.setFormEnabled(2,true));QVERIFY(model.setAutoCompact(true));QVERIFY(model.selectForm(2));QVERIFY(model.setVisible(true));
        model.setInteraction(true,false,false,false);QTest::qWait(60);QVERIFY(model.miniCollapsed());
        model.setInteraction(false,false,false,false);QTest::qWait(180);QVERIFY(model.miniCollapsed());
        model.setInteraction(true,true,false,false);QTest::qWait(180);QVERIFY(model.miniCollapsed());
        model.setInteraction(true,false,false,false);QTRY_COMPARE_WITH_TIMEOUT(model.effectiveForm(),1,500);QCOMPARE(model.selectedForm(),2);
        model.setInteraction(false,false,true,false);QTest::qWait(700);QCOMPARE(model.effectiveForm(),1);
        model.setInteraction(false,false,false,true);QTest::qWait(700);QCOMPARE(model.effectiveForm(),1);
        model.setInteraction(false,false,false,false);QTRY_COMPARE_WITH_TIMEOUT(model.effectiveForm(),2,1000);
        QVERIFY(model.setFormEnabled(1,false));model.setInteraction(true,false,false,false);QTest::qWait(180);QCOMPARE(model.effectiveForm(),2);
    }
    void obsoleteAnimationCannotCommitAndPassthroughFreezesForm()
    {
        QtWindowPlatform platform;WidgetViewModel model(platform);QWindow window;window.setGeometry(80,80,340,240);QVERIFY(model.attachWindow(&window));QVERIFY(model.setFormEnabled(2,true));QVERIFY(model.setVisible(true));
        model.enableVisualTransitions();QVERIFY(model.selectForm(1));QVERIFY(model.selectForm(2));const int stale=model.transitionId();
        QVERIFY(!model.miniCollapsed());QVERIFY(model.selectForm(0));const auto rect=window.geometry();model.finishTransition(stale);QCOMPARE(model.selectedForm(),0);QCOMPARE(window.geometry(),rect);
        QVERIFY(model.selectForm(2));model.setTrayAvailable(true);QVERIFY(model.setClickThrough(true));QVERIFY(model.miniCollapsed());QCOMPARE(model.selectedForm(),2);
        QVERIFY(model.setClickThrough(false));
    }
    void animationResizeFailureRestoresStableUserIntent()
    {
        RejectingPlatform platform;WidgetViewModel model(platform);QWindow window;window.setGeometry(80,80,340,240);QVERIFY(model.attachWindow(&window));QVERIFY(model.setFormEnabled(2,true));QVERIFY(model.setVisible(true));
        model.enableVisualTransitions();QVERIFY(model.selectForm(1));const auto rect=window.geometry();QVERIFY(model.selectForm(2));platform.rejectResize=true;
        model.finishTransition(model.transitionId());QCOMPARE(model.selectedForm(),1);QCOMPARE(model.effectiveForm(),1);QCOMPARE(window.geometry(),rect);QVERIFY(!model.errorText().isEmpty());
    }
    void panelMotionDefersShrinkAndRejectsStaleReverseCompletion()
    {
        QtWindowPlatform platform;WidgetViewModel model(platform);QWindow window;window.setGeometry(80,80,340,240);
        QVERIFY(model.attachWindow(&window));QVERIFY(model.setFormEnabled(2,true));QVERIFY(model.selectForm(2));QVERIFY(model.setVisible(true));
        const auto ball=window.geometry();model.enableVisualTransitions();model.activatePrimary();
        QVERIFY(model.panelMotion());QVERIFY(!model.mini());QCOMPARE(model.selectedForm(),2);QCOMPARE(model.effectiveForm(),0);
        const auto panel=window.geometry();QCOMPARE(panel.size(),QSize(340,240));model.finishTransition(model.transitionId());QVERIFY(!model.panelMotion());
        model.returnToCompact();const auto closing=model.transitionId();QVERIFY(model.panelMotion());QCOMPARE(window.geometry(),panel);
        model.activatePrimary();const auto reopening=model.transitionId();QVERIFY(reopening!=closing);QCOMPARE(window.geometry(),panel);
        model.finishTransition(closing);QVERIFY(model.panelMotion());QCOMPARE(model.effectiveForm(),0);QCOMPARE(window.geometry(),panel);
        model.finishTransition(reopening);QVERIFY(!model.panelMotion());model.returnToCompact();model.finishTransition(model.transitionId());
        QVERIFY(model.miniCollapsed());QCOMPARE(window.geometry(),ball);
    }
    void panelMotionFailureAndConfigurationHaveStableRecovery()
    {
        RejectingPlatform platform;WidgetViewModel model(platform);QWindow window;window.setGeometry(80,80,340,240);
        QVERIFY(model.attachWindow(&window));QVERIFY(model.setFormEnabled(2,true));QVERIFY(model.selectForm(2));QVERIFY(model.setVisible(true));model.enableVisualTransitions();
        model.activatePrimary();model.finishTransition(model.transitionId());const auto panel=window.geometry();
        model.returnToCompact();platform.rejectResize=true;model.finishTransition(model.transitionId());
        QVERIFY(!model.panelMotion());QCOMPARE(model.selectedForm(),2);QCOMPARE(model.effectiveForm(),0);QCOMPARE(window.geometry(),panel);QVERIFY(!model.errorText().isEmpty());
        platform.rejectResize=false;model.clearError();model.returnToCompact();const auto stale=model.transitionId();
        QVERIFY(model.setFormEnabled(2,false));QVERIFY(!model.panelMotion());QCOMPARE(model.effectiveForm(),0);model.finishTransition(stale);QCOMPARE(window.geometry(),panel);
        QVERIFY(model.setFormEnabled(2,true));QVERIFY(model.selectForm(2));QVERIFY(model.panelMotion());
        QVERIFY(model.setVisible(false));QVERIFY(!model.panelMotion());QVERIFY(model.setVisible(true));QVERIFY(model.miniCollapsed());
    }
    void miniPreviewOnlyAnimatesIntoPanelThatContainsItsSurface()
    {
        for(const int width:{260,420}){
            QtWindowPlatform platform;WidgetViewModel model(platform);QWindow window;window.setGeometry(80,80,width,240);
            QVERIFY(model.attachWindow(&window));QVERIFY(model.setFormEnabled(2,true));QVERIFY(model.selectForm(2));QVERIFY(model.setAutoCompact(true));QVERIFY(model.setVisible(true));
            QVERIFY(model.setMiniCollapsed(false));QCOMPARE(window.width(),340);QCOMPARE(model.selectedForm(),2);
            model.enableVisualTransitions();model.activatePrimary();
            QCOMPARE(model.panelMotion(),width>=340);QCOMPARE(window.size(),QSize(width,240));QCOMPARE(model.selectedForm(),2);QCOMPARE(model.effectiveForm(),0);
            model.finishTransition(model.transitionId());QVERIFY(!model.panelMotion());
        }
    }
    void migrationPreservesOldKeysAndRestoresRecentCompactChoice()
    {
        QTemporaryDir dir;const auto path=dir.filePath("migration.ini");
        {QSettings settings(path,QSettings::IniFormat);settings.setValue("widget/mini",true);settings.setValue("widget/miniAutoCollapse",true);settings.setValue("widget/floatingSize",QSize(400,260));settings.sync();}
        QtWindowPlatform platform;WidgetViewModel model(platform);QWindow window;window.resize(340,360);model.configureSettings(path);QVERIFY(model.attachWindow(&window));
        QCOMPARE(model.enabledForms(),7);QCOMPARE(model.selectedForm(),1);QCOMPARE(window.width(),400);QVERIFY(model.miniAutoCollapse());
        QVERIFY(model.selectForm(2));QVERIFY(model.selectForm(0));model.saveSettings();
        QSettings old(path,QSettings::IniFormat);QVERIFY(old.value("widget/mini").toBool());QVERIFY(old.value("widget/miniAutoCollapse").toBool());
        WidgetViewModel restored(platform);QWindow second;second.resize(340,360);restored.configureSettings(path);QVERIFY(restored.attachWindow(&second));
        QCOMPARE(restored.selectedForm(),0);restored.returnToCompact();QCOMPARE(restored.selectedForm(),2);
    }
    void miniBallKeepsNormalGeometryAcrossSaveAndRestart()
    {
        QTemporaryDir dir;QtWindowPlatform platform;WidgetViewModel model(platform);QWindow window;
        window.setGeometry(80,100,400,240);model.configureSettings(dir.filePath("mini.ini"));QVERIFY(model.attachWindow(&window));
        window.setGeometry(80,100,400,240);
        QVERIFY(!model.setMiniCollapsed(true));QVERIFY(model.setMiniAutoCollapse(true));QVERIFY(model.setMini(true));
        QVERIFY(model.setMiniLayout(72)); // Font metrics determine the height, not a transient window frame.
        QVERIFY(model.setMiniCollapsed(true));QCOMPARE(window.size(),QSize(36,36));QVERIFY(model.miniCollapsed());
        model.saveSettings();
        WidgetViewModel restored(platform);QWindow second;second.resize(340,360);restored.configureSettings(dir.filePath("mini.ini"));QVERIFY(restored.attachWindow(&second));
        QVERIFY(restored.miniAutoCollapse());QVERIFY(restored.mini());QVERIFY(!restored.miniCollapsed());QCOMPARE(second.width(),340);
        QVERIFY(restored.setMini(false));QCOMPARE(second.size(),QSize(400,240));
        QVERIFY(model.setMiniCollapsed(false));QCOMPARE(window.size(),QSize(340,72));
        QVERIFY(model.setMiniCollapsed(true));QVERIFY(model.setMiniAutoCollapse(false));QCOMPARE(window.size(),QSize(340,72));QVERIFY(!model.miniCollapsed());
        QVERIFY(!model.setMiniCollapsed(true));QVERIFY(model.setMini(false));QCOMPARE(window.size(),QSize(400,240));
    }
    void miniUsesCanonicalHeightAndAnchorAcrossRepeatedTransitions()
    {
        QtWindowPlatform platform;WidgetViewModel model(platform);QWindow window;
        window.setGeometry(80,100,400,240);QVERIFY(model.attachWindow(&window));QVERIFY(model.setMiniLayout(56));
        connect(&model,&WidgetViewModel::stateChanged,&window,[&]{window.setMinimumSize(model.mini() ? QSize(36,36) : QSize(260,190));});
        QVERIFY(model.setMiniAutoCollapse(true));QVERIFY(model.setMini(true));QCOMPARE(window.size(),QSize(340,56));
        for(int i=0;i<30;++i) {
            window.resize(480+i,190); // Stale native frames must not redefine either target dimension.
            QVERIFY(model.setMiniCollapsed(true));QCOMPARE(window.geometry(),QRect(80,100,36,36));
            QVERIFY(model.setMiniCollapsed(false));QCOMPARE(window.geometry(),QRect(80,100,340,56));QVERIFY(!model.geometryChanging());
        }
        QVERIFY(model.setMiniLayout(72));QCOMPARE(window.size(),QSize(340,72));
        QVERIFY(model.setMiniCollapsed(true));QVERIFY(model.setMini(false));QCOMPARE(window.geometry(),QRect(80,100,400,240));
    }
    void miniAtScreenEdgeKeepsItsExpandedAnchor()
    {
        QtWindowPlatform platform;WidgetViewModel model(platform);QWindow window;
        const auto available=QGuiApplication::primaryScreen()->availableGeometry();
        const QPoint anchor(available.right()-400+1,available.bottom()-240+1);
        window.setGeometry(QRect(anchor,QSize(400,240)));QVERIFY(model.attachWindow(&window));
        QVERIFY(model.setMiniLayout(56));QVERIFY(model.setMiniAutoCollapse(true));QVERIFY(model.setMini(true));
        const auto bar=window.geometry();const auto corner=QPoint(available.x()+available.width(),available.y()+available.height());
        for(int i=0;i<10;++i){QVERIFY(model.setMiniCollapsed(true));QCOMPARE(window.position()+QPoint(36,36),corner);QVERIFY(model.setMiniCollapsed(false));QCOMPARE(window.geometry(),bar);}
    }
    void miniBallFailureDoesNotCommitModeOrPreference()
    {
        RejectingPlatform platform;WidgetViewModel model(platform);QWindow window;window.setGeometry(80,80,400,240);QVERIFY(model.attachWindow(&window));
        QVERIFY(model.setMiniAutoCollapse(true));QVERIFY(model.setMini(true));platform.rejectResize=true;
        QVERIFY(!model.setMiniCollapsed(true));QVERIFY(!model.miniCollapsed());QCOMPARE(window.size(),QSize(340,48));
        QVERIFY(!model.setMiniLayout(96));QCOMPARE(window.size(),QSize(340,48));
        platform.rejectResize=false;QVERIFY(model.setMiniCollapsed(true));platform.rejectResize=true;
        QVERIFY(!model.setMiniAutoCollapse(false));QVERIFY(model.miniAutoCollapse());QVERIFY(model.miniCollapsed());QCOMPARE(window.size(),QSize(36,36));
        QVERIFY(!model.setMini(false));QVERIFY(model.mini());QVERIFY(model.miniCollapsed());
        platform.rejectResize=false;model.setTrayAvailable(true);QVERIFY(model.setClickThrough(true));QVERIFY(model.miniCollapsed());QCOMPARE(window.size(),QSize(36,36));
        QVERIFY(!model.setMiniCollapsed(true));QVERIFY(model.setClickThrough(false));QVERIFY(model.setMiniCollapsed(true));
        window.setPosition(-100000,-100000);QVERIFY(model.setMiniCollapsed(false));QVERIFY(QGuiApplication::primaryScreen()->availableGeometry().contains(window.geometry()));
        QVERIFY(model.setMiniCollapsed(true));QVERIFY(model.setMini(false));QCOMPARE(window.size(),QSize(400,240));
    }
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
    void visiblePopupRemainsAboveRestoredHost()
    {
        if(QGuiApplication::platformName()=="offscreen" || QGuiApplication::platformName()=="minimal")QSKIP("Native stacking requires a window system.");
        QtWindowPlatform platform;QWindow host;host.setGeometry(80,80,340,240);QVERIFY(platform.prepare(&host).succeeded());
        host.show();QCoreApplication::processEvents();host.hide();host.show();QCoreApplication::processEvents();
        for(int layer=0;layer<3;++layer){
            QVERIFY((layer==2?platform.setDesktopPinned(&host,true):platform.setAlwaysOnTop(&host,layer==1)).succeeded());QCoreApplication::processEvents();
            // Open after the host's flag change, as a real menu command does;
            // don't reuse a Popup still queued for dismissal by that change.
            QWindow popup;popup.setFlags(Qt::Popup|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);popup.setTransientParent(&host);popup.setGeometry(100,100,180,160);
            popup.show();QTRY_VERIFY(popup.isVisible());QTRY_VERIFY(nativeWindowLayerMatches(&popup,WindowLayer::Topmost));
            QVERIFY((layer==2?platform.setDesktopPinned(&host,true):platform.setAlwaysOnTop(&host,layer==1)).succeeded());QCoreApplication::processEvents();
            QTRY_VERIFY(nativeWindowAbove(&popup,&host));QVERIFY(popup.isVisible());
        }
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
        QCOMPARE(window.size(), QSize(340, 48));
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
        window.resize(420,190); // User resizing is available in the floating presentation.
        QVERIFY(model.setMini(true));
        QVERIFY(model.visible());
        QVERIFY(model.setAlwaysOnTop(true));
        QCOMPARE(window.size(), QSize(340, 48));
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
