#include "Ux38SmokeProbe.h"
#include "models/TaskWorkspace.h"
#include "models/WidgetViewModel.h"
#include "platform/common/NativeWindowLayer.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QColor>
#include <QSet>
#include <QDebug>
#include <QImage>
#include <QtMath>
#include <QLineF>
#include <QTimer>
#include <QScreen>
#include <QDateTime>
#include <QCursor>
namespace daymark {
Ux38SmokeProbe::Ux38SmokeProbe(TaskWorkspace& workspace,WidgetViewModel& controller,QQuickWindow* main,QQuickWindow* widget,QString screenshot)
    :m_workspace(workspace),m_controller(controller),m_main(main),m_widget(widget),m_screenshot(std::move(screenshot)){
        // Focused rendering still runs the real registration assertions in the
        // isolated smoke workspace. The normal CTest executes the entire suite.
        if(qgetenv("DAYMARK_SMOKE_UI_SLICE")=="registration")m_stage=98;
        if(qgetenv("DAYMARK_SMOKE_UI_SLICE")=="compact")m_stage=116;
        if(qgetenv("DAYMARK_SMOKE_UI_SLICE")=="mini-menu")m_stage=137;
        if(qgetenv("DAYMARK_SMOKE_UI_SLICE")=="forms")m_stage=150;
        if(qgetenv("DAYMARK_SMOKE_UI_SLICE")=="dock")m_stage=160;
        if(qgetenv("DAYMARK_SMOKE_UI_SLICE")=="panel-motion")m_stage=180;
        if(qgetenv("DAYMARK_SMOKE_UI_SLICE")=="popup-layer")m_stage=190;
        if(qgetenv("DAYMARK_SMOKE_UI_SLICE")=="sidebar-footer")m_stage=200;
        m_wait.start();
    }
QObject* Ux38SmokeProbe::find(QObject* root,const QString& name) const {
    QList<QObject*> pending{root};QSet<QObject*> seen;
    while(!pending.isEmpty()) {
        auto* object=pending.takeLast();if(!object || seen.contains(object))continue;seen.insert(object);
        if(object->objectName()==name)return object;
        pending.append(object->children());
        if(auto* item=qobject_cast<QQuickItem*>(object))for(auto* child:item->childItems())pending.append(child);
        if(auto* window=qobject_cast<QQuickWindow*>(object))pending.append(window->contentItem());
    }
    return nullptr;
}
void Ux38SmokeProbe::click(const QString& name,QObject* scope) {
    auto* item=qobject_cast<QQuickItem*>(find(scope?scope:m_widget.data(),name));if(!item)return;
    auto* window=item->window();
    const auto at=item->mapToScene(QPointF(item->width()/2,item->height()/2));
    for(const auto type:{QEvent::MouseMove,QEvent::MouseButtonPress,QEvent::MouseButtonRelease}) {
        QMouseEvent event(type,at,window->mapToGlobal(at),type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,type==QEvent::MouseButtonPress?Qt::LeftButton:Qt::NoButton,Qt::NoModifier);
        QCoreApplication::sendEvent(window,&event);
    }
}
void Ux38SmokeProbe::hover(QObject* scope,const QString& name) {
    auto* item=qobject_cast<QQuickItem*>(find(scope,name));if(!item)return;
    auto* window=item->window();for(auto* other:{m_main.data(),m_widget.data()})if(other!=window){QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(other,&leave);}const auto at=item->mapToScene(QPointF(item->width()/2,item->height()/2));
    QMouseEvent event(QEvent::MouseMove,at,window->mapToGlobal(at),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QCoreApplication::sendEvent(window,&event);
}
void Ux38SmokeProbe::expandWithKey(int key) {
    auto* item=qobject_cast<QQuickItem*>(find(m_widget,"widgetMiniSummary"));if(!item)return;
    item->forceActiveFocus();
    for(const auto type:{QEvent::KeyPress,QEvent::KeyRelease}){QKeyEvent event(type,key,Qt::NoModifier);QCoreApplication::sendEvent(item,&event);}
}
bool Ux38SmokeProbe::capture(QQuickWindow* window,const QString& name) const {
    if(m_screenshot.isEmpty())return true;
    if((name=="menu" || name=="menu-200") && window==m_widget){auto* menu=find(m_widget,"widgetMenu");auto* content=menu?qobject_cast<QQuickItem*>(menu->property("contentItem").value<QObject*>()):nullptr;if(content && content->window())window=content->window();}
    return window && window->grabWindow().save(m_screenshot+".desktop-"+name+".png");
}
void Ux38SmokeProbe::capturePanelFrame(const QString& name,QQuickWindow* window) {
    if(m_screenshot.isEmpty())return;
    const auto result=(window ? window : m_widget.data())->contentItem()->grabToImage();
    if(!result){m_panelCaptureFailed=true;return;}
    ++m_panelCaptures;
    QObject::connect(result.data(),&QQuickItemGrabResult::ready,m_widget,[this,result,name]{
        const auto image=result->image();
        if(name.startsWith("opaque-menu-") && (image.isNull() || image.pixelColor(image.width()/2,image.height()/2).alpha()!=255))m_panelCaptureFailed=true;
        if(!result->saveToFile(m_screenshot+".desktop-"+name+".png"))m_panelCaptureFailed=true;
        --m_panelCaptures;
    },Qt::SingleShotConnection);
}
bool Ux38SmokeProbe::layout() const {
    QVariant result;QMetaObject::invokeMethod(m_widget,"verifyLayout",Q_RETURN_ARG(QVariant,result));
    if(result!="PASS")qCritical()<<"Desktop layout"<<result;
    return result=="PASS";
}
QString Ux38SmokeProbe::advance() {
    auto hint=[](QObject* target){QVariant result;if(target)QMetaObject::invokeMethod(target,"hintForVerification",Q_RETURN_ARG(QVariant,result));return result.toMap();};
    auto checkSidebar=[&]{
        if(m_main->property("menuBar").value<QObject*>())return false;
        auto* brand=qobject_cast<QQuickItem*>(find(m_main,"sidebarWordmark"));auto* footer=qobject_cast<QQuickItem*>(find(m_main,"sidebarFooter"));
        if(!brand || !brand->isVisible() || !qIsFinite(brand->height()) || brand->width()<88 || qAbs(brand->height()*164/36-brand->width())>1 || !footer || footer->mapToScene(QPointF(0,footer->height())).y()>m_main->height())return false;
        const auto frame=m_main->grabWindow();const double sx=double(frame.width())/m_main->width(),sy=double(frame.height())/m_main->height();const auto position=brand->mapToScene(QPointF());const QColor ink=brand->property("ink").value<QColor>();int painted=0;
        for(int y=qMax(0,int(position.y()*sy));y<qMin(frame.height(),int((position.y()+brand->height())*sy));++y)for(int x=qMax(0,int(position.x()*sx));x<qMin(frame.width(),int((position.x()+brand->width())*sx));++x){const auto pixel=frame.pixelColor(x,y);if(qAbs(pixel.red()-ink.red())+qAbs(pixel.green()-ink.green())+qAbs(pixel.blue()-ink.blue())<60)++painted;}
        if(painted<20){if(m_wait.elapsed()>=1800){qCritical()<<"Unpainted wordmark"<<brand->size()<<position<<brand->isVisible()<<ink<<painted;capture(m_main,"wordmark-failure");}return false;}
        for(const QString code:{"inbox","today","scheduled","all","important","completed"}){auto* tile=find(m_main,"navigation-"+code);auto* title=tile?find(tile,"smartNavigationTitle"):nullptr;if(!title || title->property("truncated").toBool())return false;}
        return true;
    };
    auto revealSettings=[&](const QString& name){auto* settings=find(m_main,"settingsDialog");auto* scroll=find(settings,"settingsScroll");auto* flick=scroll?scroll->property("contentItem").value<QObject*>():nullptr;auto* target=qobject_cast<QQuickItem*>(find(settings,name));auto* content=flick?qobject_cast<QQuickItem*>(flick->property("contentItem").value<QObject*>()):nullptr;if(!target || !content)return false;const auto y=target->mapToItem(content,QPointF()).y();flick->setProperty("contentY",qBound(0.,y-20,qMax(0.,flick->property("contentHeight").toDouble()-flick->property("height").toDouble())));return true;};
    auto glyphPainted=[&](const QString& name){auto* glyph=qobject_cast<QQuickItem*>(find(m_main,"galleryGlyph-"+name));if(!glyph || !glyph->isVisible() || glyph->width()<12 || glyph->property("pathData").toString().isEmpty())return false;const auto image=m_main->grabWindow();const double sx=double(image.width())/m_main->width(),sy=double(image.height())/m_main->height();const auto at=glyph->mapToScene(QPointF());const auto ink=glyph->property("ink").value<QColor>();int pixels=0;for(int y=qMax(0,int(at.y()*sy));y<qMin(image.height(),int((at.y()+glyph->height())*sy));++y)for(int x=qMax(0,int(at.x()*sx));x<qMin(image.width(),int((at.x()+glyph->width())*sx));++x){const auto p=image.pixelColor(x,y);if(qAbs(p.red()-ink.red())+qAbs(p.green()-ink.green())+qAbs(p.blue()-ink.blue())<100)++pixels;}qInfo()<<"Glyph pixels"<<name<<pixels<<at<<glyph->size();return pixels>=3;};
    auto settingsKey=[&](int code){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));for(const auto type:{QEvent::KeyPress,QEvent::KeyRelease}){QKeyEvent event(type,code,Qt::NoModifier);QCoreApplication::sendEvent(settings,&event);}};
    auto fail=[&](const QString& text){capture(m_widget,"failure");return QStringLiteral("UX38 stage %1: %2").arg(m_stage).arg(text);};
    auto leaveWidget=[&]{
        // Leave alone preserves Quick's last in-window pointer position. A
        // pending hover recalculation can otherwise re-enter after layout.
        const QPointF outside(-12,-12);
        QMouseEvent move(QEvent::MouseMove,outside,m_widget->mapToGlobal(outside),Qt::NoButton,Qt::NoButton,Qt::NoModifier);
        QCoreApplication::sendEvent(m_widget,&move);
        QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(m_widget,&leave);
    };
    auto* list=find(m_widget,"widgetTasks");
    auto* calendar=qobject_cast<QQuickWindow*>(find(m_main,"desktopCalendarWindow"));
    if(!list || !calendar)return fail("desktop surfaces missing");
    if(m_stage<=74){
    if(m_stage==0){m_workspace.setPreference("followSystemAccessibility",false);m_workspace.setPreference("reduceMotion",false);m_workspace.setThemeMode(1);m_workspace.setTextScale(1);m_workspace.setView("today");m_workspace.setPreference("desktopCalendarVisible",true);m_controller.setMini(false);m_controller.setVisible(true);m_widget->resize(340,360);}
    if(m_stage==1){if(!calendar->isVisible() || !m_widget->isVisible() || !layout() || list->property("interactive").toBool() || !capture(m_widget,"empty"))return fail("empty state and coexistence");m_workspace.createForTime(QStringLiteral("讨论方案与下一步安排"),m_workspace.dateOffset(0),540);}
    if(m_stage==2){if(m_workspace.todayTasks()->rowCount()!=1)return fail("today fixture");m_task=m_workspace.todayTasks()->items().first().id;m_workspace.createSubtask(QStringLiteral("准备评审材料"),m_task);}
    if(m_stage==3){auto* row=find(m_widget,"widgetTask-"+m_task);if(!row || row->property("summary").toString().isEmpty() || row->property("subtaskTotal").toInt()!=1 || !layout() || !capture(m_widget,"today-light") || !capture(calendar,"calendar-light"))return fail("shared schedule and relationship");click("widgetPin");}
    if(m_stage==4){if(m_controller.alwaysOnTop())return fail("unpin");m_widgetModeGeometry=m_widget->geometry();m_widgetModeCollapse=qobject_cast<QQuickItem*>(find(m_widget,"widgetCollapse"))->mapToScene(QPointF());m_widgetModeMore=qobject_cast<QQuickItem*>(find(m_widget,"widgetMore"))->mapToScene(QPointF());click("widgetCollapse");}
    if(m_stage==5){if(!m_controller.mini() || !layout() || !capture(m_widget,"mini-light"))return fail("mini summary");const auto more=qobject_cast<QQuickItem*>(find(m_widget,"miniMore"))->mapToScene(QPointF());if(m_widget->width()!=m_controller.miniWidth() || m_widget->position()!=m_widgetModeGeometry.topLeft() || QLineF(more,m_widgetModeMore).length()>1 || !find(m_widget,"widgetMiniGrip"))return fail("Mini independent width and dedicated drag slot");click("widgetMiniSummary");}
    if(m_stage==6){if(m_controller.mini() || m_widget->size()!=QSize(340,360))return fail("single click restores floating geometry");m_controller.setLocked(true);m_controller.setMini(true);}
    if(m_stage==7){expandWithKey(Qt::Key_Space);}
    if(m_stage==8){if(m_controller.mini() || !m_controller.errorText().isEmpty())return fail("locked mini keyboard expansion");m_controller.setLocked(false);m_workspace.setTextScale(2);m_widget->resize(260,190);auto* row=find(m_widget,"widgetTask-"+m_task);row->setProperty("rowVersion",row->property("rowVersion").toLongLong()-1);QMetaObject::invokeMethod(row,"complete");}
    if(m_stage==9){auto* row=find(m_widget,"widgetTask-"+m_task);auto* error=find(m_widget,"widgetError");const bool laidOut=layout();
        // The floating widget was resized to 260x190 in the previous stage while the
        // error row appears; Qt clamps the window to its new minimumHeight asynchronously,
        // so re-check layout for a bounded window instead of sampling the first frame.
        if(!laidOut && m_wait.elapsed()<1500)return {};
        if(!laidOut || !row || row->property("completing").toBool() || m_workspace.errorText().isEmpty() || !error->property("visible").toBool()){qCritical()<<"Stale completion state"<<laidOut<<bool(row)<<(row?row->property("completing"):QVariant())<<m_workspace.errorText()<<(error?error->property("visible"):QVariant())<<m_widget->property("errorMessage")<<m_widget->size()<<m_widget->property("minimumHeight");return fail("stale completion preserves row and error");}m_controller.setMini(true);}
    if(m_stage==10){if(m_widget->property("errorMessage").toString().isEmpty() || !capture(m_widget,"mini-error"))return fail("mini error retained");expandWithKey(Qt::Key_Return);}
    if(m_stage==11){if(m_controller.mini())return fail("Return expansion");m_workspace.clearError();find(m_widget,"widgetTask-"+m_task)->setProperty("rowVersion",m_workspace.task(m_task).value("rowVersion"));}
    if(m_stage==12){m_completedRow=qobject_cast<QQuickItem*>(find(m_widget,"widgetTask-"+m_task));m_count=m_workspace.todayTasks()->rowCount();if(!capture(m_widget,"completion-before"))return fail("completion source capture");click("widgetComplete-"+m_task);}
    if(m_stage==13){m_widget->grabWindow();if(m_workspace.todayTasks()->rowCount()>=m_count || (m_completedRow && !m_completedRow->property("removing").toBool()) || !capture(m_widget,"completion-feedback"))return fail("successful completion and feedback");}
    if(m_stage==14){if(m_wait.elapsed()<250)return {};if(m_completedRow)return fail("completion releases visual row");m_workspace.setPreference("reduceMotion",true);}
    if(m_stage>=15 && m_stage<=25){m_workspace.createTask(QStringLiteral("今日行动 %1：桌面任务验收").arg(m_stage-14));}
    if(m_stage==26){if(!list->property("interactive").toBool() || !capture(m_widget,"scrollable"))return fail("one overflowing scroll region");m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_widget->resize(340,440);}
    if(m_stage==27){if(!layout() || !capture(m_widget,"dark-200"))return fail("floating large text");m_controller.setMini(true);}
    if(m_stage==28){if(!layout() || !capture(m_widget,"mini-dark-200"))return fail("mini readable at 200 percent");expandWithKey(Qt::Key_Return);calendar->resize(420,580);}
    if(m_stage==29){auto* panel=find(calendar,"desktopCalendarPanel");auto* toolbar=qobject_cast<QQuickItem*>(find(panel,"calendarToolbar"));if(!toolbar || toolbar->property("columns").toInt()!=1 || !capture(calendar,"calendar-dark-200")){qCritical()<<"Calendar geometry"<<calendar->size()<<(panel?panel->property("width"):QVariant())<<(toolbar?toolbar->property("width"):QVariant())<<(toolbar?toolbar->property("columns"):QVariant());capture(calendar,"calendar-layout-failure");return fail("calendar adaptive toolbar");}auto* more=find(calendar,"calendarMore-"+m_workspace.dateOffset(0));if(!more || !more->property("visible").toBool() || more->property("remainingCount").toInt()<1 || (more->property("compactOverflow").toBool() && more->property("text")!=QStringLiteral("…")))return fail("narrow calendar overflow remains readable");m_workspace.setPreference("solidWidget",true);m_workspace.setPreference("desktopCalendarOpacity",50);}
    if(m_stage==30){for(auto* window:{m_widget.data(),calendar}){auto* background=find(window,window==calendar?"desktopCalendarBackground":"widgetBackground");if(!background || background->property("color").value<QColor>().alpha()!=255)return fail("solid background on both desktop surfaces");}m_workspace.setPreference("highContrast",true);m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_widget->resize(260,360);}
    if(m_stage==31){
        // Stage 30 resizes to 260x360 and turns on high contrast; on platforms whose
        // CJK font is wider/taller the error row and header can leave the list with no
        // height. Grow to the window's own minimumHeight (which already budgets for the
        // error row) before asserting, instead of accepting a collapsed list.
        // QML's Window.minimumHeight is authoritative; QWindow::minimumHeight() can
        // lag behind it (it reported 0 here on Linux, leaving the window at 223px and
        // pushing the footer 58px past the surface).
        int minHeight=m_widget->property("minimumHeight").toInt();
        if(minHeight>0 && m_widget->height()<minHeight){m_widget->resize(m_widget->width(),minHeight);if(m_wait.elapsed()<800)return {};}
        // A RowLayout can wrap at 236px and push the footer past the surface even though
        // the declared minimum was honoured; grow by the measured overflow and re-check
        // instead of asserting an inconsistent geometry.
        if(!layout()){
            QVariant overflow;QMetaObject::invokeMethod(m_widget,"verificationOverflow",Q_RETURN_ARG(QVariant,overflow));
            const int deficit=overflow.toInt();
            if(deficit>0 && m_wait.elapsed()<1500){m_widget->resize(m_widget->width(),m_widget->height()+deficit);return {};}
        }
        if(!layout() || !capture(m_widget,"narrow-high-contrast"))return fail(QStringLiteral("260 DIP high contrast: min=%1 h=%2").arg(minHeight).arg(m_widget->height()));
        // A popup needs an active window to become visible; X11/Wayland runners do not
        // focus a window that was never shown as active, so the menu stayed closed.
        if(!m_widget->isActive()){m_widget->show();m_widget->requestActivate();if(m_wait.elapsed()<800)return {};}
        QMetaObject::invokeMethod(m_widget,"openVerificationMenu");}
    if(m_stage==32){QVariant result;QMetaObject::invokeMethod(m_widget,"verifyMenu",Q_RETURN_ARG(QVariant,result));
        // The menu is created and shown asynchronously after openVerificationMenu();
        // sampling the first frame reports "closed" on slower/first-run platforms.
        if(result!="PASS" && m_wait.elapsed()<3000)return {};
        if(result!="PASS"){
            auto* menu=find(m_widget,"widgetMenu");
            qCritical()<<"Menu state"<<result<<(menu?menu->property("visible"):QVariant())<<(menu?menu->property("count"):QVariant())<<m_widget->property("width")<<m_widget->property("height")<<QGuiApplication::platformName();
            return fail("menu width: "+result.toString());
        }
        if(!capture(m_widget,"menu"))return fail("menu width: capture");QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");m_task=m_workspace.todayTasks()->items().first().id;m_completedRow=qobject_cast<QQuickItem*>(find(m_widget,"widgetTask-"+m_task));m_count=m_workspace.todayTasks()->rowCount();}
    if(m_stage==33){auto* item=qobject_cast<QQuickItem*>(list);item->setProperty("currentIndex",0);item->forceActiveFocus();for(const auto type:{QEvent::KeyPress,QEvent::KeyRelease}){QKeyEvent event(type,Qt::Key_Space,Qt::NoModifier);QCoreApplication::sendEvent(item,&event);}}
    if(m_stage==34){if(m_workspace.todayTasks()->rowCount()!=m_count-1 || (m_completedRow && m_completedRow->property("removing").toBool()) || m_workspace.task(m_task).value("taskStatus")!="COMPLETED")return fail("reduced motion keyboard completion without delayed removal");m_workspace.setTextScale(2);m_widget->resize(800,600);QMetaObject::invokeMethod(m_widget,"openVerificationMenu");}
    if(m_stage==35){if(!find(m_widget,"widgetMenu")->property("visible").toBool() && m_wait.elapsed()<500){QMetaObject::invokeMethod(m_widget,"openVerificationMenu");return {};}QVariant result;QMetaObject::invokeMethod(m_widget,"verifyMenu",Q_RETURN_ARG(QVariant,result));if(result!="PASS" || !capture(m_widget,"menu-200"))return fail("menu resolved font at 200 percent: "+result.toString());QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");m_workspace.setTextScale(1);m_controller.setMini(true);m_widget->resize(800,m_widget->height());}
    if(m_stage==36){if(find(m_widget,"widgetMiniTitle")->property("truncated").toBool())return fail("short mini tooltip fixture");hover(m_widget,"widgetMiniSummary");}
    if(m_stage==37){if(m_wait.elapsed()<750)return {};if(hint(m_widget).value("visible").toBool())return fail("complete mini title repeats tooltip");hover(m_widget,"miniMore");}
    if(m_stage==38){if(m_wait.elapsed()<750)return {};if(hint(find(m_widget,"miniMore")).value("visible").toBool())return fail("obvious expand arrow tooltip");expandWithKey(Qt::Key_Return);}
    if(m_stage==39){hover(m_widget,"widgetPin");}
    if(m_stage==40){if(m_wait.elapsed()<750)return {};auto* pin=qobject_cast<QQuickItem*>(find(m_widget,"widgetPin"));const auto help=hint(pin);if(!help.value("visible").toBool())return fail("necessary pin action hint absent");if(QRectF(0,0,pin->width(),pin->height()).intersects(QRectF(help.value("x").toDouble(),help.value("y").toDouble(),help.value("width").toDouble(),help.value("height").toDouble())))return fail("tooltip overlaps source operation");for(const auto& name:{"widgetMore","miniMore"})if(find(m_widget,name)->property("hintEnabled").toBool())return fail("standard action tooltip admitted");if(find(m_main,"pageOptionsAction")->property("hintEnabled").toBool())return fail("options menu tooltip admitted");QMetaObject::invokeMethod(m_widget,"openVerificationMenu");}
    if(m_stage==41){if(hint(find(m_widget,"widgetPin")).value("visible").toBool())return fail("menu did not dismiss current tooltip");QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");m_workspace.saveCatalog({{"kind","project"},{"title","123"}});}
    if(m_stage==42){m_workspace.saveCatalog({{"kind","project"},{"title",QStringLiteral("这是需要截断显示并且应该恢复完整名称的分类验收")}});}
    if(m_stage==43){for(const auto& value:m_workspace.categoryTree()){const auto row=value.toMap();if(row.value("title")=="123")m_shortCategory=row.value("id").toString();else if(row.value("title").toString().startsWith(QStringLiteral("这是需要截断")))m_longCategory=row.value("id").toString();}if(m_shortCategory.isEmpty() || m_longCategory.isEmpty())return fail("catalog hint fixtures");m_main->requestActivate();m_main->grabWindow();capture(m_main,"category-hints");hover(find(m_main,"categoryNav-"+m_shortCategory),"navigationLabel");}
    if(m_stage==44){if(m_wait.elapsed()<750)return {};if(hint(find(m_main,"categoryNav-"+m_shortCategory)).value("visible").toBool())return fail("short category repeats visible name");m_main->grabWindow();auto* row=find(m_main,"categoryNav-"+m_longCategory);auto* label=find(row,"navigationLabel");if(!label->property("truncated").toBool()){if(m_wait.elapsed()<1800)return {};qCritical()<<"Long hint geometry"<<label->property("width")<<label->property("text")<<label->property("visible")<<label->property("contentWidth")<<m_main->size();capture(m_main,"category-failure");return fail("long category fixture not truncated");}hover(row,"navigationLabel");}
    if(m_stage==45){if(m_wait.elapsed()<750)return {};const auto info=hint(find(m_main,"categoryNav-"+m_longCategory));if(!info.value("visible").toBool() || !info.value("text").toString().startsWith(QStringLiteral("这是需要截断")))return fail("truncated category did not reveal full text");QMetaObject::invokeMethod(m_widget,"openVerificationMenu");}
    if(m_stage==46){if(hint(find(m_main,"categoryNav-"+m_longCategory)).value("visible").toBool())return fail("cross-window menu tooltip suppression");QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");auto* row=qobject_cast<QQuickItem*>(find(m_main,"categoryNav-"+m_shortCategory));hover(m_main,"contentViewSwitch");row->forceActiveFocus();}
    if(m_stage==47){if(m_wait.elapsed()<750)return {};if(hint(find(m_main,"categoryNav-"+m_shortCategory)).value("visible").toBool())return fail("keyboard focus opened tooltip");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_workspace.setPreference("highContrast",false);m_main->resize(1120,720);m_main->requestActivate();m_workspace.setView("inbox");}
    if(m_stage>=48 && m_stage<=53){m_main->grabWindow();const QStringList views={"inbox","today","scheduled","all","important","completed"};if(!checkSidebar() || m_workspace.scope()!=views[m_stage-48]){if(m_wait.elapsed()<1800)return {};return fail("sidebar layout or six-scope routing");}if(m_stage==48){QVariant rhythm;QMetaObject::invokeMethod(m_main,"verifyPageRhythm",Q_RETURN_ARG(QVariant,rhythm));if(rhythm!="PASS" || !capture(m_main,"sidebar-light"))return fail("page typography rhythm: "+rhythm.toString());}click("navigation-"+(m_stage<53?views[m_stage-47]:QString("inbox")),m_main);}
    if(m_stage==54){if(m_workspace.scope()!="inbox")return fail("scope return");click("sidebarTasksHeader",m_main);}
    if(m_stage==55){if(m_workspace.preferences().value("sidebarTasksExpanded").toBool())return fail("tasks group collapse");click("sidebarTasksHeader",m_main);}
    if(m_stage==56){if(!m_workspace.preferences().value("sidebarTasksExpanded").toBool())return fail("tasks group expand");m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_main->resize(880,580);}
    if(m_stage==57){m_main->grabWindow();if(!checkSidebar() || find(m_main,"smartNavigationGrid")->property("columns").toInt()!=1){if(m_wait.elapsed()<1800)return {};return fail("large sidebar text/brand/footer");}QVariant rhythm;QMetaObject::invokeMethod(m_main,"verifyPageRhythm",Q_RETURN_ARG(QVariant,rhythm));if(rhythm!="PASS" || !capture(m_main,"sidebar-dark-200"))return fail("large page typography rhythm: "+rhythm.toString());click("toggleSidebar",m_main);}
    if(m_stage==58){if(m_main->property("sidebarWidth").toInt()!=44 || find(m_main,"sidebarWordmark")->property("visible").toBool())return fail("collapsed sidebar recovery");click("toggleSidebar",m_main);m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_main->resize(1120,720);}
    if(m_stage==59){if(!checkSidebar()){if(m_wait.elapsed()<1800)return {};return fail("restored sidebar");}click("settingsAndTools",m_main);}
    if(m_stage==60){auto* settings=find(m_main,"settingsDialog");if(!settings->property("visible").toBool())return fail("settings entry");QMetaObject::invokeMethod(settings,"setVerificationCategory",Q_ARG(QVariant,QVariant(2)));}
    if(m_stage==61){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));settings->grabWindow();if(!revealSettings("settingsHelp"))return fail("settings support scroll");}
    if(m_stage==62){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(!capture(settings,"settings-support"))return fail("support capture");click("settingsHelp",settings);}
    if(m_stage==63){if(find(m_main,"settingsDialog")->property("visible").toBool() || !find(m_main,"helpPanel")->property("visible").toBool())return fail("settings help routing");QMetaObject::invokeMethod(find(m_main,"helpPanel"),"close");click("settingsAndTools",m_main);}
    if(m_stage==64){if(!find(m_main,"settingsDialog")->property("visible").toBool() || !revealSettings("settingsData"))return fail("settings reopened");}
    if(m_stage==65){click("settingsData",find(m_main,"settingsDialog"));}
    if(m_stage==66){if(find(m_main,"settingsDialog")->property("visible").toBool() || !find(m_main,"dataToolsDialog")->property("visible").toBool())return fail("settings data routing");QMetaObject::invokeMethod(find(m_main,"dataToolsDialog"),"close");auto* settings=find(m_main,"settingsDialog");QMetaObject::invokeMethod(settings,"open");QMetaObject::invokeMethod(settings,"showOverview");}
    if(m_stage==67){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));QVariant result;QMetaObject::invokeMethod(settings,"verifyCategory",Q_RETURN_ARG(QVariant,result));if(result!="PASS" || find(settings,"settingsBack") || !capture(settings,"settings-two-column"))return fail("settings keeps sidebar beside active content: "+result.toString());click("settingsCategory-0",settings);}
    if(m_stage==68){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));QVariant check;QMetaObject::invokeMethod(settings,"verifyCategory",Q_RETURN_ARG(QVariant,check));if(settings->property("categoryIndex").toInt()!=0 || check!="PASS" || !capture(settings,"settings-appearance"))return fail("settings sidebar selects appearance");click("themeModePicker-segment-2",settings);}
    if(m_stage==69){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(m_workspace.themeMode()!=2 || !capture(settings,"settings-appearance-dark"))return fail("settings theme applies");click("themeModePicker-segment-1",settings);qobject_cast<QQuickItem*>(find(settings,"settingsCategory-0"))->forceActiveFocus();}
    if(m_stage==70){auto* settings=find(m_main,"settingsDialog");auto* row=qobject_cast<QQuickItem*>(find(settings,"settingsCategory-0"));if(settings->property("categoryIndex").toInt()!=0 || !row->hasActiveFocus())return fail("settings sidebar retains selection and focus");settingsKey(Qt::Key_Down);}
    if(m_stage==71){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(settings->property("categoryIndex").toInt()!=1 || !capture(settings,"settings-widgets"))return fail("settings keyboard category entry");if(find(settings,"widgetOpacitySetting")->property("enabled").toBool())return fail("opaque background disables irrelevant opacity");click("widgetSolidBackgroundSetting",settings);qobject_cast<QQuickItem*>(find(settings,"calendarDesktopPinnedSetting"))->forceActiveFocus();}
    if(m_stage==72){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));auto* target=qobject_cast<QQuickItem*>(find(settings,"calendarDesktopPinnedSetting"));auto* scroll=qobject_cast<QQuickItem*>(find(settings,"settingsScroll"));const auto at=target->mapToItem(scroll,QPointF());if(!find(settings,"widgetOpacitySetting")->property("enabled").toBool() || at.y()<0 || at.y()+target->height()>scroll->height()+1 || !capture(settings,"settings-calendar-options"))return fail("settings focus reveals lower calendar group");m_settingsScroll=scroll->property("contentItem").value<QObject*>()->property("contentY").toDouble();}
    if(m_stage==73){auto* settings=find(m_main,"settingsDialog");click("settingsCategory-2",settings);}
    if(m_stage==74){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(settings->property("categoryIndex").toInt()!=2 || !revealSettings("backupDaysPicker"))return fail("settings general and backup reachability");qobject_cast<QQuickItem*>(find(settings,"backupDaysPicker"))->forceActiveFocus();settingsKey(Qt::Key_Space);settingsKey(Qt::Key_End);settingsKey(Qt::Key_Return);}
    }
    else if(m_stage>=75){
    if(m_stage==75){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(m_workspace.preferences().value("backupDays").toInt()!=30 || !capture(settings,"settings-general"))return fail("settings grouped option persists");click("settingsCategory-1",settings);}
    if(m_stage==76){auto* settings=find(m_main,"settingsDialog");auto* scroll=find(settings,"settingsScroll")->property("contentItem").value<QObject*>();
        if(settings->property("categoryIndex").toInt()==1 && qAbs(scroll->property("contentY").toDouble()-m_settingsScroll)>1 && m_wait.elapsed()<1500)return {};
        if(settings->property("categoryIndex").toInt()!=1 || qAbs(scroll->property("contentY").toDouble()-m_settingsScroll)>1){qInfo()<<"Settings scroll restore"<<scroll->property("contentY")<<m_settingsScroll<<settings->property("categoryScroll")<<settings->property("activeFocusItem");return fail("category change restores its own scroll");}click("settingsCategory-3",settings);}
    if(m_stage==77){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(settings->property("categoryIndex").toInt()!=3 || !capture(settings,"settings-about"))return fail("settings about entry");m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_main->resize(880,580);}
    if(m_stage==78){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));QVariant check;QMetaObject::invokeMethod(settings,"verifyCategory",Q_RETURN_ARG(QVariant,check));if(check!="PASS" || !capture(settings,"settings-about-dark-200"))return fail("settings large about");settingsKey(Qt::Key_Escape);}
    if(m_stage==79){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(settings->isVisible())return fail("Escape closes split settings");QMetaObject::invokeMethod(settings,"open");if(settings->property("categoryIndex").toInt()!=3)return fail("reopen preserves selected category");}
    if(m_stage==80){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(!m_settingsHomeSent){qobject_cast<QQuickItem*>(find(settings,"settingsCategory-3"))->forceActiveFocus();settingsKey(Qt::Key_Home);m_settingsHomeSent=true;return {};}QVariant check;QMetaObject::invokeMethod(settings,"verifyCategory",Q_RETURN_ARG(QVariant,check));if(settings->property("categoryIndex").toInt()!=0 || check!="PASS" || !capture(settings,"settings-appearance-dark-200"))return fail("settings large preference rows and two columns: "+check.toString());qobject_cast<QQuickItem*>(find(settings,"settingsCategory-0"))->forceActiveFocus();settingsKey(Qt::Key_End);}
    if(m_stage==81){auto* settings=find(m_main,"settingsDialog");if(settings->property("categoryIndex").toInt()!=3)return fail("End selects final sidebar category");qobject_cast<QQuickItem*>(find(settings,"settingsCategory-3"))->forceActiveFocus();settingsKey(Qt::Key_Up);settingsKey(Qt::Key_Up);}
    if(m_stage==86){if(find(m_main,"settingsDialog")->property("visible").toBool())return fail("settings close action preserves main window");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_main->resize(1120,720);QMetaObject::invokeMethod(m_main,"showVerificationPage",Q_ARG(QVariant,QVariant("style")),Q_ARG(QVariant,QVariant("")));}
    if(m_stage==82){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));QVariant check;QMetaObject::invokeMethod(settings,"verifyCategory",Q_RETURN_ARG(QVariant,check));if(settings->property("categoryIndex").toInt()!=1 || check!="PASS" || !capture(settings,"settings-widgets-dark-200"))return fail("settings large widget page");qobject_cast<QQuickItem*>(find(settings,"calendarDesktopPinnedSetting"))->forceActiveFocus();}
    if(m_stage==83){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));auto* target=qobject_cast<QQuickItem*>(find(settings,"calendarDesktopPinnedSetting"));auto* scroll=qobject_cast<QQuickItem*>(find(settings,"settingsScroll"));const auto at=target->mapToItem(scroll,QPointF());if(at.y()<0 || at.y()+target->height()>scroll->height()+1 || !capture(settings,"settings-calendar-dark-200"))return fail("large lower settings stay reachable");m_workspace.setPreference("highContrast",true);}
    if(m_stage==84){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(find(settings,"widgetOpacitySetting")->property("enabled").toBool() || !capture(settings,"settings-contrast-200"))return fail("high contrast keeps setting boundaries and disables opacity");m_workspace.setPreference("highContrast",false);settingsKey(Qt::Key_Escape);}
    if(m_stage==85){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(settings->isVisible())return fail("Escape closes large settings");QMetaObject::invokeMethod(settings,"open");if(settings->property("categoryIndex").toInt()!=1)return fail("reopen keeps widget category");click("settingsClose",settings);}
    if(m_stage==87){auto* gallery=find(m_main,"styleGallery");if(!gallery->property("visible").toBool())return fail("style matrix entry");QMetaObject::invokeMethod(gallery,"showVisualAudit",Q_ARG(QVariant,QVariant("icons")));}
    if(m_stage==88){capture(m_main,"glyph-debug");if(!glyphPainted("calendar") || !glyphPainted("check") || !glyphPainted("grip") || !glyphPainted("settings") || !capture(m_main,"glyph-matrix-light"))return fail("vector symbols paint distinct small shapes");m_workspace.setThemeMode(2);m_workspace.setTextScale(2);m_main->resize(880,580);QMetaObject::invokeMethod(find(m_main,"styleGallery"),"showVisualAudit",Q_ARG(QVariant,QVariant("icons")));}
    if(m_stage==89){if(!glyphPainted("calendar") || !capture(m_main,"glyph-matrix-dark-200"))return fail("vector symbol follows theme and text scale");QMetaObject::invokeMethod(find(m_main,"styleGallery"),"showVisualAudit",Q_ARG(QVariant,QVariant("clip")));}
    if(m_stage==91){QVariant result;auto* gallery=find(m_main,"styleGallery");QMetaObject::invokeMethod(gallery,"verifyVisualSystem",Q_RETURN_ARG(QVariant,result));if(result!="PASS" || !capture(m_main,"type-scale-dark-200"))return fail("shared type and radius hierarchy: "+result.toString());QMetaObject::invokeMethod(gallery,"close");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_workspace.setPreference("highContrast",false);m_workspace.setPreference("solidWidget",false);m_controller.setBackgroundOpacity(0.6);m_controller.setMini(false);m_widget->setPosition(160,90);m_widget->resize(700,m_widget->height());m_controller.setMini(true);}
    if(m_stage==92){
        auto* more=find(m_widget,"miniMore");
        if(!more || !more->property("visible").toBool())return fail(QStringLiteral("mini more not reachable: mini=%1 size=%2x%3").arg(m_controller.mini()).arg(m_widget->width()).arg(m_widget->height()));
        // This stage verifies the Mini menu's anchor and surface, not the click
        // plumbing (covered by other stages). Invoke the same entry point the button
        // uses: a synthetic click on the 56px-tall mini bar does not open the popup on
        // the offscreen Linux backend, while the menu itself works there in the
        // floating layout (stages 32/35 pass).
        QMetaObject::invokeMethod(m_widget,"openVerificationMenu");}
    if(m_stage==93){QVariant result;QMetaObject::invokeMethod(m_widget,"verifyMenu",Q_RETURN_ARG(QVariant,result));
        // Same asynchronous popup as stage 32: on runners that do not focus the widget
        // the menu becomes visible a frame or more after the click.
        if(result!="PASS" && m_wait.elapsed()<3000)return {};
        if(result!="PASS"){
            auto* menu=find(m_widget,"widgetMenu");
            qCritical()<<"Mini menu state"<<result<<(menu?menu->property("visible"):QVariant())<<(menu?menu->property("count"):QVariant())<<(menu?menu->property("opacity"):QVariant())<<m_widget->isActive()<<"mini"<<m_controller.mini()<<"size"<<m_widget->size()<<"moreVisible"<<(find(m_widget,"miniMore")?find(m_widget,"miniMore")->property("visible"):QVariant())<<QGuiApplication::platformName();
            return fail("Mini menu anchor and opaque surface: "+result.toString());
        }auto* content=qobject_cast<QQuickItem*>(find(m_widget,"widgetMenu")->property("contentItem").value<QObject*>());if(!content)return fail("Mini menu content");const auto image=content->window()->grabWindow();int ink=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(image.pixelColor(x,y).lightness()<100)++ink;if(ink<30){if(m_wait.elapsed()<1500)return {};return fail("Mini opaque window must paint menu text");}if(!capture(content->window(),"mini-solid-menu"))return fail("Mini menu window capture");m_controller.setMini(false);}
    if(m_stage==94){if(find(m_widget,"widgetMenu")->property("visible").toBool())return fail("mode change left stale menu open");click("widgetMore");}
    if(m_stage==95){QVariant result;QMetaObject::invokeMethod(m_widget,"verifyMenu",Q_RETURN_ARG(QVariant,result));
        if(result!="PASS" && m_wait.elapsed()<1500)return {};
        if(result!="PASS")return fail("expanded menu reacquires current anchor: "+result.toString());auto* content=qobject_cast<QQuickItem*>(find(m_widget,"widgetMenu")->property("contentItem").value<QObject*>());if(!capture(content->window(),"floating-solid-menu"))return fail("floating menu capture");m_widget->setX(m_widget->x()+20);}
    if(m_stage==96){if(find(m_widget,"widgetMenu")->property("visible").toBool())return fail("moving host left detached menu");click("desktopCalendarOptions",calendar);}
    if(m_stage==97){auto* button=find(calendar,"desktopCalendarOptions");auto* menu=find(button,"desktopCalendarOptionsMenu");auto* content=menu?qobject_cast<QQuickItem*>(menu->property("contentItem").value<QObject*>()):nullptr;if(!menu || !menu->property("visible").toBool() || menu->property("anchorItem").value<QObject*>()!=button || !content || content->window()->color().alpha()!=255 || !capture(content->window(),"calendar-solid-menu"))return fail("desktop calendar explicit opaque menu anchor");QMetaObject::invokeMethod(menu,"close");m_stage=115;}
    if(m_stage==98){
        if(!m_workspace.syncState().contains("mode") || m_workspace.syncState().value("busy").toBool()){
            if(m_wait.elapsed()<5000)return {};
            return fail("registration waits for the local account profile to load");
        }
        m_workspace.setPreference("desktopCalendarVisible",true);m_workspace.setThemeMode(1);m_workspace.setTextScale(1);
        auto* settings=find(m_main,"accountWindow");QMetaObject::invokeMethod(m_main,"openTool",Q_ARG(QVariant,QVariant("account")));
        if(find(find(m_main,"settingsDialog"),"syncPassword"))return fail("settings must not retain an account form");
        settings->setProperty("syncRegisterMode",false);
        if(!find(m_main,"accountEntry") || !find(m_main,"accountCollapsedEntry"))return fail("account entry in both sidebar modes");
        if(!capture(m_main,"account-entry"))return fail("account entry capture");
    }
    if(m_stage==99){
        auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        if(!capture(settings,"account-login-light"))return fail("dedicated login capture");
        auto* password=find(settings,"syncPassword");auto* confirm=find(settings,"syncConfirmPassword");
        find(settings,"syncServerOrigin")->setProperty("text","https://example.invalid");find(settings,"syncLogin")->setProperty("text","synthetic-user");
        password->setProperty("text","discard-on-mode-switch");
        QMetaObject::invokeMethod(find(settings,"accountShowPassword"),"clicked");
        if(!settings->property("showPassword").toBool() || password->property("echoMode").toInt()!=0)return fail("show password action changes the input echo mode");
        QMetaObject::invokeMethod(find(settings,"syncRegisterMode"),"clicked");
        if(!settings->property("syncRegisterMode").toBool() || settings->property("showPassword").toBool() || !password->property("text").toString().isEmpty() || !confirm->property("visible").toBool())return fail("registration switch clears password and reveals confirmation");
        password->setProperty("text","synthetic-long-password");confirm->setProperty("text","different");
        if(find(settings,"syncSubmit")->property("enabled").toBool())return fail("mismatched registration password cannot submit");
        QMetaObject::invokeMethod(settings,"reveal",Q_ARG(QVariant,QVariant::fromValue(find(settings,"syncConfirmPassword"))));
        QMetaObject::invokeMethod(confirm,"forceActiveFocus");
    }
    if(m_stage==100){
        auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        if(settings->activeFocusItem()!=find(settings,"syncConfirmPassword"))return fail("confirmation field keyboard focus");
        for(const auto type:{QEvent::KeyPress,QEvent::KeyRelease}){QKeyEvent event(type,Qt::Key_Return,Qt::NoModifier);QCoreApplication::sendEvent(settings,&event);}
        if(m_workspace.syncState().value("busy").toBool() || find(settings,"syncPassword")->property("text").toString().isEmpty())return fail("Enter on mismatched confirmation must keep input and avoid request");
        if(!capture(settings,"registration-light-mismatch"))return fail("registration light capture");
        find(settings,"syncConfirmPassword")->setProperty("text","synthetic-long-password");
        if(!find(settings,"syncSubmit")->property("enabled").toBool()){
            qCritical()<<"Registration readiness"<<find(settings,"syncServerOrigin")->property("text").toString().length()<<find(settings,"syncLogin")->property("text").toString().length()<<find(settings,"syncPassword")->property("text").toString().length()<<find(settings,"syncConfirmPassword")->property("text").toString().length()<<m_workspace.syncState().value("busy");
            return fail("matching registration password enables explicit submit");
        }
        settings->setProperty("syncRegisterMode",false);
        if(!find(settings,"syncPassword")->property("text").toString().isEmpty() || !find(settings,"syncConfirmPassword")->property("text").toString().isEmpty())return fail("leaving registration clears both passwords");
        settings->setProperty("syncRegisterMode",true);m_workspace.setTextScale(2);m_workspace.setThemeMode(2);
    }
    if(m_stage==101){
        auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        if(!QMetaObject::invokeMethod(settings,"reveal",Q_ARG(QVariant,QVariant::fromValue(find(settings,"syncConfirmPassword")))))return fail("large confirmation field scroll");
        settings->grabWindow();
    }
    if(m_stage==102){
        auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        auto* field=qobject_cast<QQuickItem*>(find(settings,"syncConfirmPassword"));auto* view=qobject_cast<QQuickItem*>(find(settings,"accountScroll"));
        const auto at=field->mapToItem(view,QPointF());
        if(at.y()<0 || at.y()+field->height()>view->height()+1 || !capture(settings,"registration-dark-200")){
            qCritical()<<"Account scroll"<<at<<field->size()<<view->size()<<find(settings,"accountScroll")->property("contentHeight");capture(settings,"registration-geometry-failure");
            return fail("registration confirmation remains reachable at 200 percent");
        }
        if(!QMetaObject::invokeMethod(settings,"reveal",Q_ARG(QVariant,QVariant::fromValue(find(settings,"syncSubmit")))))return fail("large registration action scroll");
    }
    if(m_stage==103){
        auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        auto* button=qobject_cast<QQuickItem*>(find(settings,"syncSubmit"));auto* view=qobject_cast<QQuickItem*>(find(settings,"accountScroll"));const auto at=button->mapToItem(view,QPointF());
        if(at.y()<0 || at.y()+button->height()>view->height()+1 || !capture(settings,"registration-actions-dark-200"))return fail("registration action remains reachable at 200 percent");
        QMetaObject::invokeMethod(settings,"close");
        if(settings->property("syncRegisterMode").toBool() || !find(settings,"syncPassword")->property("text").toString().isEmpty() || !find(settings,"syncConfirmPassword")->property("text").toString().isEmpty())return fail("closing registration clears secrets and resets mode");
        m_workspace.setTextScale(1);m_workspace.setThemeMode(1);
    }
    if(m_stage==104){
        // Synthetic presentation states do not authenticate the coordinator or
        // touch a server. Real auth/sync remains covered by the Qt/Go TLS test.
        m_originalSyncState=m_workspace.syncState();auto state=m_originalSyncState;
        state.insert("safe_error_code","invalid_credentials");state.insert("status",QStringLiteral("账号或密码不正确。"));
        state.insert("authenticated",false);state.insert("busy",false);m_accountDisplayState=state;m_workspace.setSyncState(state);
        QMetaObject::invokeMethod(m_main,"openTool",Q_ARG(QVariant,QVariant("account")));
    }
    if(m_stage==105){
        m_workspace.setSyncState(m_accountDisplayState);
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        if(!find(window,"accountError")->property("visible").toBool() || !find(window,"syncPassword")->property("text").toString().isEmpty())return fail("dedicated login error and empty password");
        if(find(window,"syncServerOrigin")->property("text")!="https://example.invalid" || find(window,"syncLogin")->property("text")!="synthetic-user")return fail("closing and reopening preserves server/account input, without retaining password");
        if(!capture(window,"account-login-error"))return fail("account error capture");
        auto state=m_workspace.syncState();state.insert("busy",true);state.insert("safe_error_code",QString());state.insert("status",QStringLiteral("正在登录…"));m_accountDisplayState=state;m_workspace.setSyncState(state);
    }
    if(m_stage==106){
        m_workspace.setSyncState(m_accountDisplayState);
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        if(find(window,"syncSubmit")->property("enabled").toBool() || find(window,"syncLogin")->property("enabled").toBool() || find(window,"syncRegisterMode")->property("enabled").toBool())return fail("connecting disables duplicate auth and mode changes");
        if(!capture(window,"account-login-busy"))return fail("connecting capture");
        QMetaObject::invokeMethod(window,"close");
        auto state=m_originalSyncState;state.insert("mode","ACCOUNT");state.insert("authenticated",true);state.insert("busy",false);state.insert("login_label","synthetic-user");state.insert("server_origin","https://example.invalid");
        state.insert("status",QStringLiteral("已连接，修改将自动同步"));state.insert("pending_count",2);state.insert("phase","ACTIVE");
        state.insert("profiles",QVariantList{QVariantMap{{"id","one"},{"label","synthetic-user"}},QVariantMap{{"id","two"},{"label","other-user"}}});
        state.insert("conflicts",QVariantList{QVariantMap{{"id","fixture-conflict"},{"related_count",2},{"local_title",QStringLiteral("合成修改")},{"local_summary",QStringLiteral("此设备的内容")},{"remote_summary",QStringLiteral("另一设备的内容")}}});
        m_accountDisplayState=state;m_workspace.setSyncState(state);QMetaObject::invokeMethod(window,"open");
    }
    if(m_stage==107){
        m_workspace.setSyncState(m_accountDisplayState);
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        if(find(window,"syncLogin")->property("visible").toBool() || !find(window,"syncNow")->property("visible").toBool() || find(window,"syncProfiles")->property("count").toInt()!=2 || !find(window,"accountConflictLocal"))return fail("connected account management replaces auth form and keeps conflicts/profiles");
        if(!capture(window,"account-management-light"))return fail("account management capture");
        auto state=m_workspace.syncState();state.insert("busy",true);state.insert("phase","RECONCILE_REQUIRED");m_accountDisplayState=state;m_workspace.setSyncState(state);
    }
    if(m_stage==108){
        m_workspace.setSyncState(m_accountDisplayState);
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        if(!find(window,"syncRebuild")->property("visible").toBool() || find(window,"syncRebuild")->property("enabled").toBool() || find(window,"accountConflictLocal")->property("enabled").toBool())return fail("busy account keeps recovery visible and prevents repeated decisions");
        m_workspace.setTextScale(2);m_workspace.setThemeMode(2);window->resize(420,440);
    }
    if(m_stage==109){
        m_workspace.setSyncState(m_accountDisplayState);
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        QMetaObject::invokeMethod(window,"reveal",Q_ARG(QVariant,QVariant::fromValue(find(window,"syncProfiles"))));
    }
    if(m_stage==110){
        m_workspace.setSyncState(m_accountDisplayState);
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));auto* footer=qobject_cast<QQuickItem*>(find(window,"accountContinueLocal"));
        const auto at=footer->mapToScene(QPointF());
        if(at.x()<0 || at.y()<0 || at.x()+footer->width()>window->width()+1 || at.y()+footer->height()>window->height()+1 || !capture(window,"account-management-dark-200-narrow"))return fail("account footer remains reachable in narrow large text window");
        QMetaObject::invokeMethod(window,"close");m_workspace.setSyncState(m_originalSyncState);m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_workspace.setPreference("sidebarCollapsed",true);
    }
    if(m_stage==111){
        auto* entry=qobject_cast<QQuickItem*>(find(m_main,"accountCollapsedEntry"));
        if(!entry || !entry->isVisible() || entry->mapToScene(QPointF()).x()<0 || !capture(m_main,"account-entry-collapsed"))return fail("collapsed account entry remains available");
        click("accountCollapsedEntry",m_main);
    }
    if(m_stage==112){
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        if(!window->isVisible())return fail("collapsed account button opens dedicated account window");
        for(const auto type:{QEvent::KeyPress,QEvent::KeyRelease}){QKeyEvent event(type,Qt::Key_Escape,Qt::NoModifier);QCoreApplication::sendEvent(window,&event);}
        if(window->isVisible())return fail("Escape closes account window");
        m_workspace.setPreference("sidebarCollapsed",false);
    }
    if(m_stage==113){
        if(m_workspace.busy()){if(m_wait.elapsed()<5000)return {};return fail("account auth routing waits for local operations");}
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        window->setProperty("verificationAuthIntents",0);
        QObject::connect(&m_workspace,&TaskWorkspace::syncActionRequested,window,[window](const QString& action,const QVariantMap& values){
            if(action!="login" && action!="register")return;
            window->setProperty("verificationAuthIntents",window->property("verificationAuthIntents").toInt()+1);
            window->setProperty("verificationAuthAction",action);
            window->setProperty("verificationAuthValues",values.value("origin")=="https://127.0.0.1:1" && values.value("login")=="synthetic-user" && values.value("password")=="synthetic-long-password");
        });
        QMetaObject::invokeMethod(m_main,"openTool",Q_ARG(QVariant,QVariant("account")));
        find(window,"syncServerOrigin")->setProperty("text","https://127.0.0.1:1");find(window,"syncLogin")->setProperty("text","synthetic-user");find(window,"syncPassword")->setProperty("text","synthetic-long-password");
        QMetaObject::invokeMethod(find(window,"syncSubmit"),"clicked");
    }
    if(m_stage==114){
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        if(window->property("verificationAuthIntents").toInt()!=1 || window->property("verificationAuthAction")!="login" || !window->property("verificationAuthValues").toBool() || !find(window,"syncPassword")->property("text").toString().isEmpty())return fail("dedicated login routes exact transient credentials once and clears them");
        if(m_workspace.syncState().value("busy").toBool()){if(m_wait.elapsed()<5000)return {};return fail("loopback connection failure bounded");}
        if(m_workspace.syncState().value("authenticated").toBool() || !find(window,"accountError")->property("visible").toBool())return fail("real coordinator connection failure stays local and actionable");
        QMetaObject::invokeMethod(find(window,"syncRegisterMode"),"clicked");find(window,"syncPassword")->setProperty("text","synthetic-long-password");find(window,"syncConfirmPassword")->setProperty("text","synthetic-long-password");
        QMetaObject::invokeMethod(find(window,"syncSubmit"),"clicked");
    }
    if(m_stage==115){
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        const int intents=window->property("verificationAuthIntents").toInt();
        // Stage 97 jumps straight here on the default path, which never runs the
        // registration stages (98-114) that issue the intent. Only the registration
        // slice (account_access) reaches this point with an intent to verify; on the
        // default path there is nothing to assert, so continue to the next stage.
        if(intents==0 && qgetenv("DAYMARK_SMOKE_UI_SLICE")!="registration"){++m_stage;m_wait.restart();return {};}
        const bool cleared=find(window,"syncPassword")->property("text").toString().isEmpty() && find(window,"syncConfirmPassword")->property("text").toString().isEmpty();
        if((intents!=2 || !cleared) && m_wait.elapsed()<3000)return {};
        if(intents!=2 || window->property("verificationAuthAction")!="register" || !window->property("verificationAuthValues").toBool() || !cleared){qCritical()<<"Registration state"<<intents<<window->property("verificationAuthAction")<<window->property("verificationAuthValues")<<find(window,"syncPassword")->property("text")<<find(window,"syncConfirmPassword")->property("text");return fail("dedicated registration routes credentials once and clears both fields");}
        if(m_workspace.syncState().value("busy").toBool()){if(m_wait.elapsed()<5000)return {};return fail("registration loopback failure bounded");}
        if(m_workspace.syncState().value("authenticated").toBool())return fail("failed connection must not authenticate");
        QMetaObject::invokeMethod(window,"close");return "DONE";
    }
    if(m_stage==116){
        m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_workspace.setPreference("followSystemAccessibility",false);m_workspace.setPreference("reduceMotion",true);
        m_controller.clearError();m_controller.setVisible(true);m_controller.setMini(false);m_widget->resize(340,360);m_controller.setMini(true);m_controller.setFormEnabled(2,true);
        QMetaObject::invokeMethod(m_main,"openTool",Q_ARG(QVariant,QVariant("settings")));find(m_main,"settingsDialog")->setProperty("categoryIndex",1);
        find(m_main,"settingsDialog")->setProperty("restoringScroll",false);qobject_cast<QQuickItem*>(find(m_main,"miniAutoCollapseSetting"))->forceActiveFocus(Qt::TabFocusReason);
    }
    if(m_stage==117){
        auto* setting=find(m_main,"miniAutoCollapseSetting");if(!setting || !setting->property("enabled").toBool())return fail("mini ball setting available");
        if(!m_controller.miniAutoCollapse() && !m_miniSettingClicked){
            // Category/scale changes polish asynchronously. Reveal and hit-test the
            // real control before sending exactly one click, including offscreen CI.
            auto* settings=find(m_main,"settingsDialog");
            auto* target=qobject_cast<QQuickItem*>(setting);
            auto* scroll=qobject_cast<QQuickItem*>(find(settings,"settingsScroll"));
            QMetaObject::invokeMethod(settings,"reveal",Q_ARG(QVariant,QVariant::fromValue(setting)));
            m_main->grabWindow();
            const auto at=target->mapToItem(scroll,QPointF());
            if(!target->isVisible() || target->width()<=0 || at.y()<0 || at.y()+target->height()>scroll->height()+1){
                if(m_wait.elapsed()<1500)return {};
                return fail("mini ball setting revealed before click");
            }
            click("miniAutoCollapseSetting",m_main);m_miniSettingClicked=true;
        }
        if(!m_controller.miniAutoCollapse()){
            if(m_wait.elapsed()<1500)return {};
            return fail("mini ball setting wired");
        }
        QMetaObject::invokeMethod(find(m_main,"settingsDialog"),"close");QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(m_widget,&leave);
    }
    if(m_stage==118){
        if(!m_controller.miniCollapsed()){if(m_wait.elapsed()<1400)return {};return fail("idle mini becomes ball");}
        if(!layout() || m_widget->size()!=QSize(36,36) || !capture(m_widget,"mini-ball-light"))return fail("compact ball geometry");
        hover(m_widget,"widgetMiniBall");
    }
    if(m_stage==119){
        if((m_controller.miniCollapsed() || m_widget->property("miniTransitionRunning").toBool() || !find(m_widget,"widgetMiniSummary")->property("enabled").toBool() || m_widget->property("miniReveal").toDouble()<0.999) && m_wait.elapsed()<1000)return {};
        if(m_controller.miniCollapsed() || m_widget->width()!=340 || !layout() || !capture(m_widget,"mini-hover-light"))return fail("pointer expands original mini width");
        click("miniMore");
    }
    if(m_stage==120){
        QMetaObject::invokeMethod(m_widget,"heightChanged",Q_ARG(int,m_widget->height()));
        if(!find(m_widget,"widgetMenu")->property("visible").toBool())return fail("mini menu opens");
        QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(m_widget,&leave);
    }
    if(m_stage==121){
        if(m_wait.elapsed()<600)return {};
        if(m_controller.miniCollapsed())return fail("menu prevents idle collapse");
        QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");
        m_workspace.setTextScale(2);m_workspace.setThemeMode(2);
    }
    if(m_stage==122){
        QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(m_widget,&leave);
        if(!m_controller.miniCollapsed()){if(m_wait.elapsed()<1400)return {};return fail("large text ball collapses");}
        if(!layout() || !capture(m_widget,"mini-ball-dark-200"))return fail("large text ball geometry");
        hover(m_widget,"widgetMiniBall");
    }
    if(m_stage==123){
        if((m_controller.miniCollapsed() || m_widget->property("miniTransitionRunning").toBool() || !find(m_widget,"widgetMiniSummary")->property("enabled").toBool() || m_widget->property("miniReveal").toDouble()<0.999) && m_wait.elapsed()<1000)return {};
        if(m_controller.miniCollapsed() || m_widget->width()!=340 || !layout() || !capture(m_widget,"mini-hover-dark-200"))return fail("large text mini expansion");
        expandWithKey(Qt::Key_Space);
    }
    if(m_stage==124){
        if(m_controller.mini() || m_widget->width()!=340){qCritical()<<"Keyboard quick view state"<<m_controller.selectedForm()<<m_controller.effectiveForm()<<m_widget->geometry()<<m_widget->property("miniReveal");return fail("keyboard opens full widget");}
        m_workspace.setTextScale(1);m_workspace.setThemeMode(1);calendar->resize(560,540);m_workspace.setPreference("desktopCalendarVisible",true);
        auto* panel=find(calendar,"desktopCalendarPanel");panel->setProperty("independentScale","month");panel->setProperty("agendaVisible",false);panel->setProperty("anchorDate",m_workspace.dateOffset(0));panel->setProperty("selectedDate",m_workspace.dateOffset(0));
    }
    if(m_stage==125){
        auto* panel=find(calendar,"desktopCalendarPanel");auto* metrics=panel->property("metrics").value<QObject*>();auto* mainPanel=find(m_main,"calendarPanel");auto* normal=mainPanel->property("metrics").value<QObject*>();
        if(!metrics || !normal || metrics->property("bodySize").toInt()!=12 || normal->property("bodySize").toInt()!=13 || !capture(calendar,"compact-calendar-light"))return fail("independent typography does not change main calendar");
        calendar->resize(360,380);m_workspace.setTextScale(2);m_workspace.setThemeMode(2);
    }
    if(m_stage==126){
        auto* panel=find(calendar,"desktopCalendarPanel");auto* picker=qobject_cast<QQuickItem*>(find(panel,"calendarScalePicker"));auto* grid=qobject_cast<QQuickItem*>(find(panel,"calendarMonthGrid"));
        if(!picker || !grid || picker->mapToScene(QPointF()).x()<0 || picker->mapToScene(QPointF(picker->width(),picker->height())).x()>calendar->width() || grid->height()<50 || !capture(calendar,"compact-calendar-dark-200"))return fail("small calendar large text controls reachable");
        auto* day=find(panel,"calendarDay-"+m_workspace.dateOffset(0));
        if(!grid->property("layoutReady").toBool() || !day || day->property("availableWidth").toDouble()<34){
            if(m_wait.elapsed()<1500)return {};
            return fail("large date numerals have full horizontal space after viewport placement");
        }
        m_controller.setMini(true);if(!m_controller.setMiniAutoCollapse(false))return fail("disable mini ball");
    }
    if(m_stage==127){
        if(m_controller.miniAutoCollapse() || m_controller.miniCollapsed() || m_widget->width()!=340)return fail("disable ball restores persistent bar");
        m_controller.setMiniAutoCollapse(true);QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(m_widget,&leave);
    }
    if(m_stage==128){
        if(!m_controller.miniCollapsed()){if(m_wait.elapsed()<1400)return {};return fail("ball ready for keyboard focus");}
        m_widget->requestActivate();
    }
    if(m_stage==129){qobject_cast<QQuickItem*>(find(m_widget,"widgetMiniBall"))->forceActiveFocus(Qt::TabFocusReason);}
    if(m_stage==130){
        if(!m_controller.miniCollapsed() || !find(m_widget,"widgetMiniBall")->property("activeFocus").toBool())return fail("Tab focuses ball without changing selected shape");
        QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(m_widget,&leave);
    }
    if(m_stage==131){
        if(m_wait.elapsed()<700)return {};
        if(!m_controller.miniCollapsed())return fail("keyboard focus preserves ball");
        auto* ball=qobject_cast<QQuickItem*>(find(m_widget,"widgetMiniBall"));
        for(const auto type:{QEvent::KeyPress,QEvent::KeyRelease}){QKeyEvent event(type,Qt::Key_Return,Qt::NoModifier);QCoreApplication::sendEvent(ball,&event);}
    }
    if(m_stage==132){
        if(m_controller.mini())return fail("keyboard opens widget after idle");
        m_controller.setMiniAutoCollapse(false);m_workspace.setTextScale(1);
    }
    if(m_stage==133){
        m_workspace.setThemeMode(1);m_widgetModeGeometry=m_widget->geometry();
        m_workspace.setPreference("reduceMotion",false);m_widget->setProperty("miniKeyboardInteraction",false);
        m_controller.setMiniAutoCollapse(true);m_controller.setMini(true);
        QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(m_widget,&leave);
    }
    if(m_stage==134){
        if(!m_controller.miniCollapsed()){if(m_wait.elapsed()<1500)return {};return fail("animated mini idle collapse");}
        if(m_widget->position()!=m_widgetModeGeometry.topLeft())return fail("repeated transitions keep the original anchor");
        m_miniMotionGeometry=QRect(m_widgetModeGeometry.topLeft(),QSize(m_controller.miniWidth(),m_widget->property("miniBarHeight").toInt()));
        m_miniMotionSampled=false;m_miniMotionValid=false;
        hover(m_widget,"widgetMiniBall");
        // The pointer must dwell for 150ms before the reveal begins.
        QTimer::singleShot(210,m_widget,[this]{
            const double progress=m_widget->property("miniReveal").toDouble();
            m_miniMotionValid=m_widget->geometry()==m_miniMotionGeometry;
            m_miniIntermediateSeen=m_miniIntermediateSeen || (progress>0 && progress<1);
            qInfo()<<"Mini animation sample"<<m_miniMotionCycle<<progress<<m_widget->geometry();
            m_miniMotionValid=m_miniMotionValid && capture(m_widget,QStringLiteral("mini-opening-mid-%1").arg(m_miniMotionCycle));
            m_miniMotionSampled=true;
        });
    }
    if(m_stage==135){
        if(!m_miniMotionSampled || m_widget->property("miniTransitionRunning").toBool()){if(m_wait.elapsed()<1500)return {};return fail("mini animation completes in bounded time");}
        if(!m_miniMotionValid || m_widget->geometry()!=m_miniMotionGeometry || !layout())return fail("animation keeps canonical native size and anchor");
        if(++m_miniMotionCycle<4){
            if(m_miniMotionCycle==2){m_workspace.setTextScale(2);m_workspace.setThemeMode(2);}
            m_widget->setProperty("miniKeyboardInteraction",false);QEvent leave(QEvent::Leave);QCoreApplication::sendEvent(m_widget,&leave);m_stage=133;
        } else {
            if(!m_miniIntermediateSeen)return fail("observe actual intermediate reveal frame");
            m_controller.selectForm(2);
            QTimer::singleShot(50,m_widget,[this]{m_controller.selectForm(1);});
        }
    }
    if(m_stage==136){
        if(m_widget->property("miniTransitionRunning").toBool()){if(m_wait.elapsed()<1500)return {};return fail("rapid reversal settles");}
        if(m_controller.miniCollapsed() || m_widget->geometry()!=m_miniMotionGeometry || !layout() || !capture(m_widget,"mini-animation-settled"))return fail("rapid reversal preserves final geometry");
        m_workspace.setPreference("reduceMotion",true);
        m_controller.selectForm(2);
        QMetaObject::invokeMethod(m_widget,"setMiniAnimation");
        if(!m_controller.miniCollapsed() || m_widget->property("miniTransitionRunning").toBool())return fail("reduced motion collapse is immediate");
        m_controller.selectForm(1);
        QMetaObject::invokeMethod(m_widget,"setMiniAnimation");
        if(m_controller.miniCollapsed() || m_widget->geometry()!=m_miniMotionGeometry || m_widget->property("miniTransitionRunning").toBool())return fail("reduced motion expansion keeps geometry");
        m_controller.setMini(false);m_controller.setMiniAutoCollapse(false);m_workspace.setTextScale(1);m_stage=149;
    }
    if(m_stage==150){
        m_workspace.setPreference("desktopCalendarVisible",true);
        m_workspace.setPreference("reduceMotion",true);m_widget->setProperty("miniKeyboardInteraction",false);
        for(int form=0;form<3;++form)m_controller.setFormEnabled(form,true);
        m_controller.selectForm(2);
    }
    if(m_stage==151){
        if(!m_controller.miniCollapsed() || !layout() || !capture(m_widget,"forms-ball"))return fail("explicit ball renders independently");
        if(QGuiApplication::platformName()=="windows"){
            const auto mask=m_widget->mask();if(mask.isEmpty() || mask.contains(QPoint(0,0)) || !mask.contains(QPoint(m_widget->width()/2,m_widget->height()/2)))return fail("native ball input region follows round surface");
        }
        QMetaObject::invokeMethod(m_widget,"openVerificationMenu");
    }
    if(m_stage==152){
        if(!find(m_widget,"widgetMenu")->property("visible").toBool())return fail("ball menu reachable");
        QMetaObject::invokeMethod(find(m_widget,"widgetFormMini"),"triggered");QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");
    }
    if(m_stage==153){
        if(m_controller.selectedForm()!=1 || !layout() || !capture(m_widget,"forms-mini"))return fail("ball directly selects Mini through menu");
        QMetaObject::invokeMethod(m_widget,"openVerificationMenu");
    }
    if(m_stage==154){QMetaObject::invokeMethod(find(m_widget,"widgetFormPanel"),"triggered");QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");}
    if(m_stage==155){
        if(m_controller.selectedForm()!=0 || !layout() || !capture(m_widget,"forms-panel"))return fail("Mini directly selects panel through menu");
        QMetaObject::invokeMethod(m_main,"openTool",Q_ARG(QVariant,QVariant("settings")));find(m_main,"settingsDialog")->setProperty("categoryIndex",1);
    }
    if(m_stage==156){
        // Each real click toggles and commits before the next control's binding refresh.
        for(const auto name:{"widgetPanelEnabled","widgetMiniEnabled","widgetBallEnabled"}){
            auto* toggle=find(m_main,name);if(!QMetaObject::invokeMethod(toggle,"click"))return fail("form setting click is invokable");
        }
    }
    if(m_stage==157){
        if(m_controller.formCount()!=0 || m_controller.visible())return fail("all settings disabled hide and stop the component");
        auto* toggle=find(m_main,"widgetPanelEnabled");if(!QMetaObject::invokeMethod(toggle,"click"))return fail("panel setting click is invokable");
    }
    if(m_stage==158){
        if(m_controller.formCount()!=1 || m_controller.selectedForm()!=0 || !m_controller.visible())return fail("one form resumes without entering disabled forms");
        QMetaObject::invokeMethod(find(m_main,"settingsDialog"),"close");m_controller.setFormEnabled(1,true);m_stage=159;
    }
    if(m_stage==160){
        m_workspace.setPreference("desktopCalendarVisible",true);m_workspace.setPreference("followSystemAccessibility",false);m_workspace.setPreference("reduceMotion",true);
        m_widget->setProperty("miniKeyboardInteraction",false);m_controller.clearError();for(int form=0;form<3;++form)m_controller.setFormEnabled(form,true);
        m_controller.setAutoCompact(false);m_controller.selectForm(2);m_controller.setVisible(true);
        const auto available=QGuiApplication::primaryScreen()->availableGeometry();
        // Keep the synthetic panel away from the real desktop pointer. Otherwise
        // a native Enter generated by resize correctly cancels our synthetic Leave.
        const int y=QCursor::pos().y()<=available.center().y() ? available.y()+available.height()-60 : available.y()+24;
        m_widget->setPosition(QPoint(m_dockCycle==0 ? available.x()+24 : available.x()+available.width()-60,y));m_controller.selectForm(2);
    }
    if(m_stage==161){
        auto* ball=qobject_cast<QQuickItem*>(find(m_widget,"widgetMiniBall"));
        if(!ball->isVisible() || m_widget->property("miniReveal").toDouble()>0.01){if(m_wait.elapsed()<800)return {};return fail("docking ball finishes its presentation before pointer click");}
        m_dockPoint=ball->mapToGlobal(QPointF(ball->width()/2,ball->height()/2));click("widgetMiniBall");
    }
    if(m_stage==162){
        if(m_controller.selectedForm()!=2 || m_controller.effectiveForm()!=0 || !capture(m_widget,QStringLiteral("dock-panel-%1").arg(m_dockCycle))){qCritical()<<"Dock click state"<<m_controller.selectedForm()<<m_controller.effectiveForm()<<m_widget->geometry()<<m_widget->property("miniReveal");return fail("ball click borrows panel without changing docking intent");}
        m_widget->setProperty("miniKeyboardInteraction",false);leaveWidget();
    }
    if(m_stage==163){
        if(!m_controller.miniCollapsed()){if(m_wait.elapsed()<1900)return {};qCritical()<<"Dock idle holds"<<m_widget->property("miniKeyboardFocus")<<m_widget->property("interactionPressed")<<m_widget->property("errorMessage")<<m_widget->isActive()<<QCursor::pos()<<m_widget->geometry();return fail("idle borrowed panel returns to ball without auto compact");}
        m_widget->grabWindow();
        auto* ball=qobject_cast<QQuickItem*>(find(m_widget,"widgetMiniBall"));const auto center=ball->mapToGlobal(QPointF(ball->width()/2,ball->height()/2));
        if(QLineF(center,m_dockPoint).length()>1){qCritical()<<"Dock return location"<<m_dockCycle<<center<<m_dockPoint<<m_widget->geometry();return fail("return preserves the original ball location");}
        if(++m_dockCycle<2){m_stage=159;}else {m_controller.setAutoCompact(true);hover(m_widget,"widgetMiniBall");}
    }
    if(m_stage==164){
        if(m_controller.miniCollapsed()){if(m_wait.elapsed()<800)return {};return fail("ball hover previews Mini after dwell");}
        // Click the old ball's global point, including on the right-hand screen edge.
        m_widget->grabWindow();
        const auto at=m_widget->mapFromGlobal(m_dockPoint.toPoint());
        auto* grip=qobject_cast<QQuickItem*>(find(m_widget,"widgetMiniGrip"));
        const auto gripPoint=grip->mapFromScene(at);
        if(!grip->isVisible() || !grip->isEnabled() || !grip->contains(gripPoint)){
            if(m_wait.elapsed()<800)return {};
            qCritical()<<"Dock hover hit area"<<at<<grip->mapToScene(QPointF())<<grip->size()<<m_widget->geometry()<<m_widget->property("miniReveal");
            return fail("hover preserves ball landing area in Mini grip");
        }
        for(const auto type:{QEvent::MouseMove,QEvent::MouseButtonPress,QEvent::MouseButtonRelease}){
            QMouseEvent event(type,at,m_dockPoint,type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,type==QEvent::MouseButtonPress?Qt::LeftButton:Qt::NoButton,Qt::NoModifier);event.setTimestamp(QDateTime::currentMSecsSinceEpoch());QCoreApplication::sendEvent(m_widget,&event);
        }
    }
    if(m_stage==165){
        if(m_controller.selectedForm()!=2 || m_controller.effectiveForm()!=0){qCritical()<<"Hover click result"<<m_controller.selectedForm()<<m_controller.effectiveForm()<<m_widget->geometry();return fail("original ball hit target survives hover expansion");}
        QMetaObject::invokeMethod(m_widget,"openVerificationMenu");leaveWidget();
    }
    if(m_stage==166){
        if(m_wait.elapsed()<1200)return {};
        if(m_controller.effectiveForm()!=0 || !find(m_widget,"widgetMenu")->property("visible").toBool())return fail("menu holds borrowed panel beyond idle timeout");
        QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");m_widget->setProperty("miniKeyboardInteraction",false);leaveWidget();
    }
    if(m_stage==167){
        if(!m_controller.miniCollapsed()){if(m_wait.elapsed()<1900)return {};return fail("menu dismissal restarts docking delay");}
        m_controller.setFormEnabled(1,false);m_controller.setAutoCompact(false);click("widgetMiniBall");
    }
    if(m_stage==168){
        if(m_controller.selectedForm()!=2 || m_controller.effectiveForm()!=0)return fail("ball-only compact choice opens panel directly");
        m_widget->setProperty("miniKeyboardInteraction",false);leaveWidget();
    }
    if(m_stage==169){
        if(!m_controller.miniCollapsed()){if(m_wait.elapsed()<1900)return {};return fail("panel returns without Mini enabled");}
        m_controller.selectForm(0);leaveWidget();
    }
    if(m_stage==170){
        if(m_wait.elapsed()<1200)return {};
        if(m_controller.selectedForm()!=0 || m_controller.effectiveForm()!=0)return fail("explicit Panel selection stays resident");
        m_controller.setFormEnabled(1,true);m_stage=179;
    }
    if(m_stage==180){
        m_workspace.setPreference("desktopCalendarVisible",true);m_workspace.setPreference("followSystemAccessibility",false);m_workspace.setPreference("reduceMotion",true);
        m_workspace.setThemeMode(m_panelCycle ? 2 : 1);m_workspace.setTextScale(m_panelCycle ? 2 : 1);
        for(int form=0;form<3;++form)m_controller.setFormEnabled(form,true);
        m_controller.setAutoCompact(false);m_controller.selectForm(0);m_controller.setVisible(true);
        m_widget->resize(m_panelCycle ? QSize(600,600) : QSize(340,360));
        m_panelPositioned=false;
    }
    if(m_stage==181){
        if(!m_panelPositioned){
            // The preceding event-loop turn presents the full panel with the
            // selected fonts. Do not synchronously read back the GPU here.
            m_controller.selectForm(2);
            const auto available=QGuiApplication::primaryScreen()->availableGeometry();
            m_widget->setPosition(m_panelCycle ? available.bottomRight()-QPoint(60,60) : available.topLeft()+QPoint(24,24));m_controller.selectForm(2);
            m_widget->requestActivate();
            m_panelPositioned=true;return {};
        }
        if(!m_widget->isActive()){if(m_wait.elapsed()<1500)return {};return fail("native synthetic click fixture activates its own window");}
        if(!m_controller.miniCollapsed() || m_widget->property("panelTransitionRunning").toBool()){if(m_wait.elapsed()<800)return {};return fail("settle ball before panel animation");}
        m_dockPoint=m_widget->geometry().topLeft();
        auto* entry=qobject_cast<QQuickItem*>(find(m_widget,"widgetMiniBall"));
        qInfo()<<"Panel ball readiness"<<m_panelCycle<<m_widget->geometry()<<m_controller.selectedForm()<<m_controller.effectiveForm()<<m_controller.errorText()<<entry->isVisible()<<entry->isEnabled()<<entry->mapToScene(QPointF())<<entry->size()<<m_widget->mask().boundingRect();
        m_workspace.setPreference("reduceMotion",false);m_controller.setInteraction(true,false,false,false);m_widget->setProperty("miniKeyboardInteraction",true);
        m_panelSampled=false;m_panelValid=false;
        m_panelSampleConnection=QObject::connect(m_widget,&QQuickWindow::afterAnimating,m_widget,[this]{
            const double progress=m_widget->property("panelReveal").toDouble();
            if(progress<0.15 || progress>0.85 || m_controller.targetBall())return;
            QObject::disconnect(m_panelSampleConnection);
            auto* surface=qobject_cast<QQuickItem*>(find(m_widget,"widgetBackground"));auto* content=qobject_cast<QQuickItem*>(find(m_widget,"widgetPanelContent"));
            const double ball=m_widget->property("ballDiameter").toDouble();
            m_panelValid=surface && content && m_controller.panelMotion() && m_widget->property("panelTransitionRunning").toBool() &&
                surface->width()>ball+1 && surface->width()<m_widget->width()-1 && surface->height()>ball+1 && surface->height()<m_widget->height()-1 &&
                content->size()==QSizeF(m_widget->size()) && content->scale()==1 && content->opacity()>=0 && content->opacity()<1 && m_widget->geometry()==m_panelGeometry;
            if(QGuiApplication::platformName()=="windows")m_panelValid=m_panelValid && !m_widget->mask().isEmpty() && m_widget->mask().boundingRect().width()<m_widget->width();
            qInfo()<<"Panel opening intermediate"<<m_panelCycle<<(surface?surface->size():QSizeF())<<(content?content->opacity():-1)<<m_panelValid<<m_widget->geometry()<<"progress"<<progress;
            capturePanelFrame(QStringLiteral("panel-opening-%1").arg(m_panelCycle));m_panelSampled=true;m_wait.restart();
        });
        click("widgetMiniBall");m_panelGeometry=m_widget->geometry();
    }
    if(m_stage==182){
        if(!m_panelSampled || m_widget->property("panelTransitionRunning").toBool()){if(m_wait.elapsed()<900)return {};return fail("panel opening completes");}
        if(!m_panelValid || !layout())return fail("opening has intermediate background and fixed content dimensions");
        capturePanelFrame(QStringLiteral("panel-open-%1").arg(m_panelCycle));
        m_panelSampled=false;m_panelValid=false;
        m_panelSampleConnection=QObject::connect(m_widget,&QQuickWindow::afterAnimating,m_widget,[this]{
            const double progress=m_widget->property("panelReveal").toDouble();
            if(progress>0.6 || progress<0.15 || !m_controller.targetBall())return;
            QObject::disconnect(m_panelSampleConnection);
            auto* surface=qobject_cast<QQuickItem*>(find(m_widget,"widgetBackground"));auto* content=qobject_cast<QQuickItem*>(find(m_widget,"widgetPanelContent"));
            const double ball=m_widget->property("ballDiameter").toDouble();
            m_panelValid=surface && content && m_controller.panelMotion() && m_controller.targetBall() && m_widget->geometry()==m_panelGeometry &&
                surface->width()>ball+1 && surface->width()<m_widget->width()-1 && content->size()==QSizeF(m_widget->size()) && content->scale()==1 && content->opacity()<0.4;
            const auto before=surface?surface->size():QSizeF();const auto opacity=content?content->opacity():-1;const auto stale=m_controller.transitionId();
            m_controller.activatePrimary();QMetaObject::invokeMethod(m_widget,"setMiniAnimation");m_controller.finishTransition(stale);
            m_panelValid=m_panelValid && surface->size()==before && content->opacity()==opacity && m_controller.effectiveForm()==0 && m_widget->geometry()==m_panelGeometry;
            qInfo()<<"Panel closing/reverse intermediate"<<m_panelCycle<<before<<opacity<<m_panelValid;
            capturePanelFrame(QStringLiteral("panel-closing-%1").arg(m_panelCycle));m_panelSampled=true;m_wait.restart();
        });
        m_controller.returnToCompact();
    }
    if(m_stage==183){
        if(!m_panelSampled || m_widget->property("panelTransitionRunning").toBool()){if(m_wait.elapsed()<900)return {};return fail("reversed opening completes");}
        if(!m_panelValid || m_controller.effectiveForm()!=0 || !layout())return fail("reverse preserves size and rejects stale completion");
        m_controller.returnToCompact();m_panelSampled=false;
        QTimer::singleShot(40,m_widget,[this]{m_workspace.setPreference("reduceMotion",true);m_panelSampled=true;});
    }
    if(m_stage==184){
        if(!m_panelSampled || !m_controller.miniCollapsed()){if(m_wait.elapsed()<900)return {};return fail("reduced motion settles pending close");}
        if(m_widget->property("panelTransitionRunning").toBool() || m_widget->geometry().topLeft()!=m_dockPoint || !layout())return fail("reduced motion keeps original ball anchor");
        m_controller.activatePrimary();
    }
    if(m_stage==185){
        if(m_panelCaptures){if(m_wait.elapsed()<1500)return {};return fail("asynchronous animation screenshots finish");}
        if(m_panelCaptureFailed)return fail("animation screenshot write");
        if(m_controller.panelMotion() || m_widget->property("panelTransitionRunning").toBool() || !layout())return fail("reduced motion opens immediately");
        if(++m_panelCycle<2){m_stage=179;}else {m_widget->setProperty("miniKeyboardInteraction",false);m_stage=189;}
    }
    if(m_stage==190){
        m_workspace.setPreference("desktopCalendarVisible",true);m_workspace.setPreference("followSystemAccessibility",false);m_workspace.setPreference("reduceMotion",true);
        m_workspace.setThemeMode(m_menuLayerCycle>=2 ? 2 : 1);m_workspace.setTextScale(m_menuLayerCycle>=2 ? 2 : 1);
        QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");
        for(int form=0;form<3;++form)m_controller.setFormEnabled(form,true);
        m_controller.setAutoCompact(false);m_controller.setBackgroundOpacity(0.6);m_controller.selectForm(0);m_controller.setVisible(true);
        m_widget->resize(600,500);m_widget->setPosition(80,80);m_controller.selectForm(m_menuLayerCycle==4 ? 1 : 2);
        m_controller.setLayer(m_menuLayerCycle<3 ? m_menuLayerCycle : 1);
        if(m_menuLayerCycle==3)m_controller.activatePrimary();
        m_menuInvoked=false;m_menuCaptureRequested=false;
    }
    if(m_stage==191){
        m_workspace.setPreference("reduceMotion",false);
        if(m_menuLayerCycle<4){
            m_panelSampleConnection=QObject::connect(m_widget,&QQuickWindow::afterAnimating,m_widget,[this]{
                const double progress=m_widget->property("panelReveal").toDouble();
                if(progress<0.15 || progress>0.85 || !m_controller.panelMotion())return;
                QObject::disconnect(m_panelSampleConnection);QMetaObject::invokeMethod(m_widget,"openVerificationMenu");m_menuInvoked=true;
            });
            if(m_menuLayerCycle==3)m_controller.returnToCompact();else m_controller.activatePrimary();
        }else {QMetaObject::invokeMethod(m_widget,"openVerificationMenu");m_menuInvoked=true;}
    }
    if(m_stage==192){
        auto* menu=find(m_widget,"widgetMenu");auto* content=qobject_cast<QQuickItem*>(menu->property("contentItem").value<QObject*>());auto* popup=content?content->window():nullptr;
        if(!m_menuInvoked || !popup || !popup->isVisible()){if(m_wait.elapsed()<1500)return {};return fail("menu opens during motion and stays visible");}
        QVariant result;QMetaObject::invokeMethod(m_widget,"verifyMenu",Q_RETURN_ARG(QVariant,result));
        if(result!="PASS" || m_controller.panelMotion() || m_widget->property("panelTransitionRunning").toBool()){
            if(m_wait.elapsed()<1500)return {};
            qCritical()<<"Menu layer cycle"<<m_menuLayerCycle<<result<<"popup"<<menu->property("x")<<menu->property("width")<<popup->geometry();return fail("menu is opaque, enabled, independent and above a stable surface");
        }
        if(QGuiApplication::platformName()!="offscreen" && QGuiApplication::platformName()!="minimal" &&
           (!nativeWindowLayerMatches(popup,WindowLayer::Topmost) || !nativeWindowAbove(popup,m_widget)))return fail("native popup is above its host");
        QEvent activated(QEvent::WindowActivate);QCoreApplication::sendEvent(m_widget,&activated);
    }
    if(m_stage==193){
        auto* menu=find(m_widget,"widgetMenu");auto* content=qobject_cast<QQuickItem*>(menu->property("contentItem").value<QObject*>());auto* popup=content?content->window():nullptr;
        if(!menu->property("visible").toBool() || !popup)return fail("host layer restoration preserves open menu");
        if(QGuiApplication::platformName()!="offscreen" && QGuiApplication::platformName()!="minimal" && !nativeWindowAbove(popup,m_widget))return fail("host cannot cover an open menu after layer restoration");
        if(!m_menuCaptureRequested){capturePanelFrame(QStringLiteral("opaque-menu-%1").arg(m_menuLayerCycle),popup);m_menuCaptureRequested=true;return {};}
        if(m_panelCaptures){if(m_wait.elapsed()<1500)return {};return fail("visible menu screenshot completes before dismissal");}
        if(m_panelCaptureFailed)return fail("menu screenshot contains opaque rendered pixels");
        for(auto type:{QEvent::KeyPress,QEvent::KeyRelease}){QKeyEvent key(type,Qt::Key_Escape,Qt::NoModifier);QCoreApplication::sendEvent(popup,&key);}
    }
    if(m_stage==194){
        if(find(m_widget,"widgetMenu")->property("visible").toBool()){if(m_wait.elapsed()<800)return {};return fail("Escape dismisses independent menu");}
        if(m_panelCaptures){if(m_wait.elapsed()<1500)return {};return fail("menu screenshot completes");}
        if(m_panelCaptureFailed)return fail("menu screenshot write");
        if(++m_menuLayerCycle<6){m_stage=189;}else m_stage=199;
    }
    if(m_stage==200){
        m_workspace.setPreference("desktopCalendarVisible",true);m_controller.setVisible(true);
        // Windows may apply a hidden helper process's startup ShowWindow flag
        // to its first native window. Present Main before testing live resize.
        if(QGuiApplication::platformName()=="windows"){m_main->hide();m_main->show();}
        m_main->requestActivate();
        QMetaObject::invokeMethod(find(m_main,"accountWindow"),"close");QMetaObject::invokeMethod(find(m_main,"settingsDialog"),"close");
        m_footerSyncState=m_workspace.syncState();auto state=m_footerSyncState;state.insert("mode","LOCAL");state.insert("authenticated",false);state.insert("busy",false);state.insert("login_label","cached-account-must-not-appear");m_workspace.setSyncState(state);
        m_workspace.setPreference("sidebarCollapsed",false);m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_main->resize(1120,720);
    }
    if(m_stage==201){
        QVariant result;QMetaObject::invokeMethod(m_main,"verifySidebarFooter",Q_RETURN_ARG(QVariant,result));
        if(result!="PASS"){if(m_wait.elapsed()<1000)return {};return fail("single-row footer layout: "+result.toString());}
        if(find(m_main,"dataAndBackup") || find(m_main,"accountEntry")->property("text")!=QStringLiteral("登录"))return fail("backup removed from sidebar and local cached identity stays logged out");
        if(!capture(m_main,"footer-local-light"))return fail("local footer capture");
        click("navigation-trash",m_main);
    }
    if(m_stage==202){
        if(m_workspace.view()!="trash" || !find(m_main,"navigation-trash")->property("checked").toBool())return fail("footer trash opens deleted items and retains selected feedback");
        auto* entry=qobject_cast<QQuickItem*>(find(m_main,"accountEntry"));entry->forceActiveFocus(Qt::TabFocusReason);
        for(auto type:{QEvent::KeyPress,QEvent::KeyRelease}){QKeyEvent key(type,Qt::Key_Space,Qt::NoModifier);QCoreApplication::sendEvent(entry,&key);}
    }
    if(m_stage==203){
        if(!find(m_main,"accountWindow")->property("visible").toBool())return fail("footer login keyboard action opens dedicated account window");
        QMetaObject::invokeMethod(find(m_main,"accountWindow"),"close");
        // Presentation-only identity; no coordinator authentication or upload.
        auto state=m_footerSyncState;state.insert("mode","ACCOUNT");state.insert("authenticated",true);state.insert("busy",false);state.insert("login_label","sidebar.account.with.a.long.name.for.elision@example.invalid");m_accountDisplayState=state;m_workspace.setSyncState(state);
    }
    if(m_stage==204){
        m_workspace.setSyncState(m_accountDisplayState);
        if(find(m_main,"accountEntry")->property("text")!=m_accountDisplayState.value("login_label"))return fail("connected footer displays account name");
        if(!capture(m_main,"footer-account-light"))return fail("account footer capture");
        m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_main->resize(880,580);
    }
    if(m_stage==205){
        m_workspace.setSyncState(m_accountDisplayState);
        QVariant result;QMetaObject::invokeMethod(m_main,"verifySidebarFooter",Q_RETURN_ARG(QVariant,result));
        if(result!="PASS"){if(m_wait.elapsed()<1000)return {};return fail("large text footer: "+result.toString());}
        if(!find(m_main,"accountEntry")->property("textTruncated").toBool() || !capture(m_main,"footer-account-dark-200"))return fail("long account name elides within its own slot");
        m_workspace.setPreference("sidebarCollapsed",true);
    }
    if(m_stage==206){
        m_workspace.setSyncState(m_accountDisplayState);
        QVariant result;QMetaObject::invokeMethod(m_main,"verifySidebarFooter",Q_RETURN_ARG(QVariant,result));
        if(result!="PASS"){if(m_wait.elapsed()<1000)return {};return fail("collapsed footer: "+result.toString());}
        if(find(m_main,"accountCollapsedEntry")->property("description")!=m_workspace.syncState().value("login_label") || !capture(m_main,"footer-collapsed-dark-200"))return fail("collapsed account preserves full accessible identity");
        click("settingsCollapsedEntry",m_main);
    }
    if(m_stage==207){
        if(!find(m_main,"settingsDialog")->property("visible").toBool())return fail("collapsed settings stays accessible");
        click("settingsCategory-2",find(m_main,"settingsDialog"));
    }
    if(m_stage==208){if(!revealSettings("settingsData")){if(m_wait.elapsed()<1000)return {};return fail("backup management reachable in general settings");}}
    if(m_stage==209){click("settingsData",find(m_main,"settingsDialog"));}
    if(m_stage==210){
        if(find(m_main,"settingsDialog")->property("visible").toBool() || !find(m_main,"dataToolsDialog")->property("visible").toBool())return fail("settings backup entry opens existing data tools");
        QMetaObject::invokeMethod(find(m_main,"dataToolsDialog"),"close");
        // Logout keeps an ACCOUNT profile and its label, but ends authentication.
        m_accountDisplayState.insert("authenticated",false);m_workspace.setSyncState(m_accountDisplayState);
        m_workspace.setPreference("sidebarCollapsed",false);m_workspace.setTextScale(1);m_workspace.setThemeMode(1);
    }
    if(m_stage==211){
        m_workspace.setSyncState(m_accountDisplayState);
        if(find(m_main,"accountEntry")->property("text")!=QStringLiteral("登录"))return fail("signed-out account profile does not present its cached name as logged in");
        m_workspace.setSyncState(m_footerSyncState);
        QMetaObject::invokeMethod(m_main,"changeView",Q_ARG(QVariant,QVariant("inbox")));return "DONE";
    }
    if(m_stage==137){
        m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_workspace.setPreference("followSystemAccessibility",false);m_workspace.setPreference("reduceMotion",true);
        m_controller.setVisible(true);m_controller.setMini(false);m_widget->resize(700,360);m_widget->setPosition(160,90);m_controller.setMini(true);m_stage=91;
    }
    if(m_stage==90){auto* glyph=qobject_cast<QQuickItem*>(find(m_main,"galleryGlyph-calendar"));const auto at=glyph->mapToScene(QPointF());if(at.y()<0 || at.y()+glyph->height()>m_main->height() || glyphPainted("calendar") || !capture(m_main,"glyph-scroll-clip-dark-200"))return fail("off-viewport vector cannot leak over dialog actions");QMetaObject::invokeMethod(find(m_main,"styleGallery"),"showVisualAudit",Q_ARG(QVariant,QVariant("type")));}
    }
    qInfo()<<"UX38 desktop scenario"<<m_stage<<"complete";++m_stage;m_wait.restart();return {};
}
}
