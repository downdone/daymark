#include "Ux38SmokeProbe.h"
#include "models/TaskWorkspace.h"
#include "models/WidgetViewModel.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QColor>
#include <QSet>
#include <QDebug>
#include <QImage>
#include <QtMath>
#include <QLineF>
namespace daymark {
Ux38SmokeProbe::Ux38SmokeProbe(TaskWorkspace& workspace,WidgetViewModel& controller,QQuickWindow* main,QQuickWindow* widget,QString screenshot)
    :m_workspace(workspace),m_controller(controller),m_main(main),m_widget(widget),m_screenshot(std::move(screenshot)){
        // Focused rendering still runs the real registration assertions in the
        // isolated smoke workspace. The normal CTest executes the entire suite.
        if(qgetenv("DAYMARK_SMOKE_UI_SLICE")=="registration")m_stage=98;
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
        QMouseEvent event(type,at,window->mapToGlobal(at),type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,Qt::NoModifier);
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
    auto* list=find(m_widget,"widgetTasks");
    auto* calendar=qobject_cast<QQuickWindow*>(find(m_main,"desktopCalendarWindow"));
    if(!list || !calendar)return fail("desktop surfaces missing");
    if(m_stage==0){m_workspace.setPreference("followSystemAccessibility",false);m_workspace.setPreference("reduceMotion",false);m_workspace.setThemeMode(1);m_workspace.setTextScale(1);m_workspace.setView("today");m_workspace.setPreference("desktopCalendarVisible",true);m_controller.setMini(false);m_controller.setVisible(true);m_widget->resize(340,360);}
    else if(m_stage==1){if(!calendar->isVisible() || !m_widget->isVisible() || !layout() || list->property("interactive").toBool() || !capture(m_widget,"empty"))return fail("empty state and coexistence");m_workspace.createForTime(QStringLiteral("讨论方案与下一步安排"),m_workspace.dateOffset(0),540);}
    else if(m_stage==2){if(m_workspace.todayTasks()->rowCount()!=1)return fail("today fixture");m_task=m_workspace.todayTasks()->items().first().id;m_workspace.createSubtask(QStringLiteral("准备评审材料"),m_task);}
    else if(m_stage==3){auto* row=find(m_widget,"widgetTask-"+m_task);if(!row || row->property("summary").toString().isEmpty() || row->property("subtaskTotal").toInt()!=1 || !layout() || !capture(m_widget,"today-light") || !capture(calendar,"calendar-light"))return fail("shared schedule and relationship");click("widgetPin");}
    else if(m_stage==4){if(m_controller.alwaysOnTop())return fail("unpin");m_widgetModeGeometry=m_widget->geometry();m_widgetModeCollapse=qobject_cast<QQuickItem*>(find(m_widget,"widgetCollapse"))->mapToScene(QPointF());m_widgetModeMore=qobject_cast<QQuickItem*>(find(m_widget,"widgetMore"))->mapToScene(QPointF());click("widgetCollapse");}
    else if(m_stage==5){if(!m_controller.mini() || !layout() || !capture(m_widget,"mini-light"))return fail("mini summary");const auto expand=qobject_cast<QQuickItem*>(find(m_widget,"widgetExpand"))->mapToScene(QPointF()),more=qobject_cast<QQuickItem*>(find(m_widget,"miniMore"))->mapToScene(QPointF());if(m_widget->width()!=m_widgetModeGeometry.width() || m_widget->position()!=m_widgetModeGeometry.topLeft() || QLineF(expand,m_widgetModeCollapse).length()>1 || QLineF(more,m_widgetModeMore).length()>1)return fail("Mini preserves width, position and action slots");click("widgetMiniSummary");}
    else if(m_stage==6){if(m_controller.mini() || m_widget->size()!=QSize(340,360))return fail("single click restores floating geometry");m_controller.setLocked(true);m_controller.setMini(true);}
    else if(m_stage==7){expandWithKey(Qt::Key_Space);}
    else if(m_stage==8){if(m_controller.mini() || !m_controller.errorText().isEmpty())return fail("locked mini keyboard expansion");m_controller.setLocked(false);m_workspace.setTextScale(2);m_widget->resize(260,190);auto* row=find(m_widget,"widgetTask-"+m_task);row->setProperty("rowVersion",row->property("rowVersion").toLongLong()-1);QMetaObject::invokeMethod(row,"complete");}
    else if(m_stage==9){auto* row=find(m_widget,"widgetTask-"+m_task);auto* error=find(m_widget,"widgetError");const bool laidOut=layout();
        // The floating widget was resized to 260x190 in the previous stage while the
        // error row appears; Qt clamps the window to its new minimumHeight asynchronously,
        // so re-check layout for a bounded window instead of sampling the first frame.
        if(!laidOut && m_wait.elapsed()<1500)return {};
        if(!laidOut || !row || row->property("completing").toBool() || m_workspace.errorText().isEmpty() || !error->property("visible").toBool()){qCritical()<<"Stale completion state"<<laidOut<<bool(row)<<(row?row->property("completing"):QVariant())<<m_workspace.errorText()<<(error?error->property("visible"):QVariant())<<m_widget->property("errorMessage")<<m_widget->size()<<m_widget->property("minimumHeight");return fail("stale completion preserves row and error");}m_controller.setMini(true);}
    else if(m_stage==10){if(m_widget->property("errorMessage").toString().isEmpty() || !capture(m_widget,"mini-error"))return fail("mini error retained");expandWithKey(Qt::Key_Return);}
    else if(m_stage==11){if(m_controller.mini())return fail("Return expansion");m_workspace.clearError();find(m_widget,"widgetTask-"+m_task)->setProperty("rowVersion",m_workspace.task(m_task).value("rowVersion"));}
    else if(m_stage==12){m_completedRow=qobject_cast<QQuickItem*>(find(m_widget,"widgetTask-"+m_task));m_count=m_workspace.todayTasks()->rowCount();if(!capture(m_widget,"completion-before"))return fail("completion source capture");click("widgetComplete-"+m_task);}
    else if(m_stage==13){m_widget->grabWindow();if(m_workspace.todayTasks()->rowCount()>=m_count || (m_completedRow && !m_completedRow->property("removing").toBool()) || !capture(m_widget,"completion-feedback"))return fail("successful completion and feedback");}
    else if(m_stage==14){if(m_wait.elapsed()<250)return {};if(m_completedRow)return fail("completion releases visual row");m_workspace.setPreference("reduceMotion",true);}
    else if(m_stage>=15 && m_stage<=25){m_workspace.createTask(QStringLiteral("今日行动 %1：桌面任务验收").arg(m_stage-14));}
    else if(m_stage==26){if(!list->property("interactive").toBool() || !capture(m_widget,"scrollable"))return fail("one overflowing scroll region");m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_widget->resize(340,440);}
    else if(m_stage==27){if(!layout() || !capture(m_widget,"dark-200"))return fail("floating large text");m_controller.setMini(true);}
    else if(m_stage==28){if(!layout() || !capture(m_widget,"mini-dark-200"))return fail("mini readable at 200 percent");expandWithKey(Qt::Key_Return);calendar->resize(420,580);}
    else if(m_stage==29){auto* panel=find(calendar,"desktopCalendarPanel");auto* toolbar=qobject_cast<QQuickItem*>(find(panel,"calendarToolbar"));if(!toolbar || toolbar->property("columns").toInt()!=1 || !capture(calendar,"calendar-dark-200")){qCritical()<<"Calendar geometry"<<calendar->size()<<(panel?panel->property("width"):QVariant())<<(toolbar?toolbar->property("width"):QVariant())<<(toolbar?toolbar->property("columns"):QVariant());capture(calendar,"calendar-layout-failure");return fail("calendar adaptive toolbar");}auto* more=find(calendar,"calendarMore-"+m_workspace.dateOffset(0));if(!more || !more->property("visible").toBool() || more->property("remainingCount").toInt()<1 || (more->property("compactOverflow").toBool() && more->property("text")!=QStringLiteral("…")))return fail("narrow calendar overflow remains readable");m_workspace.setPreference("solidWidget",true);m_workspace.setPreference("desktopCalendarOpacity",50);}
    else if(m_stage==30){for(auto* window:{m_widget.data(),calendar}){auto* background=find(window,window==calendar?"desktopCalendarBackground":"widgetBackground");if(!background || background->property("color").value<QColor>().alpha()!=255)return fail("solid background on both desktop surfaces");}m_workspace.setPreference("highContrast",true);m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_widget->resize(260,360);}
    else if(m_stage==31){
        // Stage 30 resizes to 260x360 and turns on high contrast; on platforms whose
        // CJK font is wider/taller the error row and header can leave the list with no
        // height. Grow to the window's own minimumHeight (which already budgets for the
        // error row) before asserting, instead of accepting a collapsed list.
        const int minHeight=m_widget->minimumHeight();
        if(m_widget->height()<minHeight){m_widget->resize(m_widget->width(),minHeight);if(m_wait.elapsed()<800)return {};}
        if(!layout() || !capture(m_widget,"narrow-high-contrast"))return fail(QStringLiteral("260 DIP high contrast: min=%1 h=%2").arg(minHeight).arg(m_widget->height()));
        // A popup needs an active window to become visible; X11/Wayland runners do not
        // focus a window that was never shown as active, so the menu stayed closed.
        if(!m_widget->isActive()){m_widget->show();m_widget->requestActivate();if(m_wait.elapsed()<800)return {};}
        QMetaObject::invokeMethod(m_widget,"openVerificationMenu");}
    else if(m_stage==32){QVariant result;QMetaObject::invokeMethod(m_widget,"verifyMenu",Q_RETURN_ARG(QVariant,result));
        // The menu is created and shown asynchronously after openVerificationMenu();
        // sampling the first frame reports "closed" on slower/first-run platforms.
        if(result!="PASS" && m_wait.elapsed()<1500)return {};
        if(result!="PASS"){
            auto* menu=find(m_widget,"widgetMenu");
            qCritical()<<"Menu state"<<result<<(menu?menu->property("visible"):QVariant())<<(menu?menu->property("count"):QVariant())<<m_widget->property("width")<<m_widget->property("height")<<QGuiApplication::platformName();
            return fail("menu width: "+result.toString());
        }
        if(!capture(m_widget,"menu"))return fail("menu width: capture");QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");m_task=m_workspace.todayTasks()->items().first().id;m_completedRow=qobject_cast<QQuickItem*>(find(m_widget,"widgetTask-"+m_task));m_count=m_workspace.todayTasks()->rowCount();}
    else if(m_stage==33){auto* item=qobject_cast<QQuickItem*>(list);item->setProperty("currentIndex",0);item->forceActiveFocus();for(const auto type:{QEvent::KeyPress,QEvent::KeyRelease}){QKeyEvent event(type,Qt::Key_Space,Qt::NoModifier);QCoreApplication::sendEvent(item,&event);}}
    else if(m_stage==34){if(m_workspace.todayTasks()->rowCount()!=m_count-1 || (m_completedRow && m_completedRow->property("removing").toBool()) || m_workspace.task(m_task).value("taskStatus")!="COMPLETED")return fail("reduced motion keyboard completion without delayed removal");m_workspace.setTextScale(2);m_widget->resize(800,600);QMetaObject::invokeMethod(m_widget,"openVerificationMenu");}
    else if(m_stage==35){if(!find(m_widget,"widgetMenu")->property("visible").toBool() && m_wait.elapsed()<500){QMetaObject::invokeMethod(m_widget,"openVerificationMenu");return {};}QVariant result;QMetaObject::invokeMethod(m_widget,"verifyMenu",Q_RETURN_ARG(QVariant,result));if(result!="PASS" || !capture(m_widget,"menu-200"))return fail("menu resolved font at 200 percent: "+result.toString());QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");m_workspace.setTextScale(1);m_controller.setMini(true);m_widget->resize(800,m_widget->height());}
    else if(m_stage==36){if(find(m_widget,"widgetMiniTitle")->property("truncated").toBool())return fail("short mini tooltip fixture");hover(m_widget,"widgetMiniSummary");}
    else if(m_stage==37){if(m_wait.elapsed()<750)return {};if(hint(m_widget).value("visible").toBool())return fail("complete mini title repeats tooltip");hover(m_widget,"widgetExpand");}
    else if(m_stage==38){if(m_wait.elapsed()<750)return {};if(hint(find(m_widget,"widgetExpand")).value("visible").toBool())return fail("obvious expand arrow tooltip");expandWithKey(Qt::Key_Return);}
    else if(m_stage==39){hover(m_widget,"widgetPin");}
    else if(m_stage==40){if(m_wait.elapsed()<750)return {};auto* pin=qobject_cast<QQuickItem*>(find(m_widget,"widgetPin"));const auto help=hint(pin);if(!help.value("visible").toBool())return fail("necessary pin action hint absent");if(QRectF(0,0,pin->width(),pin->height()).intersects(QRectF(help.value("x").toDouble(),help.value("y").toDouble(),help.value("width").toDouble(),help.value("height").toDouble())))return fail("tooltip overlaps source operation");for(const auto& name:{"widgetMore","widgetExpand"})if(find(m_widget,name)->property("hintEnabled").toBool())return fail("standard action tooltip admitted");if(find(m_main,"pageOptionsAction")->property("hintEnabled").toBool())return fail("options menu tooltip admitted");QMetaObject::invokeMethod(m_widget,"openVerificationMenu");}
    else if(m_stage==41){if(hint(find(m_widget,"widgetPin")).value("visible").toBool())return fail("menu did not dismiss current tooltip");QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");m_workspace.saveCatalog({{"kind","project"},{"title","123"}});}
    else if(m_stage==42){m_workspace.saveCatalog({{"kind","project"},{"title",QStringLiteral("这是需要截断显示并且应该恢复完整名称的分类验收")}});}
    else if(m_stage==43){for(const auto& value:m_workspace.categoryTree()){const auto row=value.toMap();if(row.value("title")=="123")m_shortCategory=row.value("id").toString();else if(row.value("title").toString().startsWith(QStringLiteral("这是需要截断")))m_longCategory=row.value("id").toString();}if(m_shortCategory.isEmpty() || m_longCategory.isEmpty())return fail("catalog hint fixtures");m_main->requestActivate();m_main->grabWindow();capture(m_main,"category-hints");hover(find(m_main,"categoryNav-"+m_shortCategory),"navigationLabel");}
    else if(m_stage==44){if(m_wait.elapsed()<750)return {};if(hint(find(m_main,"categoryNav-"+m_shortCategory)).value("visible").toBool())return fail("short category repeats visible name");m_main->grabWindow();auto* row=find(m_main,"categoryNav-"+m_longCategory);auto* label=find(row,"navigationLabel");if(!label->property("truncated").toBool()){if(m_wait.elapsed()<1800)return {};qCritical()<<"Long hint geometry"<<label->property("width")<<label->property("text")<<label->property("visible")<<label->property("contentWidth")<<m_main->size();capture(m_main,"category-failure");return fail("long category fixture not truncated");}hover(row,"navigationLabel");}
    else if(m_stage==45){if(m_wait.elapsed()<750)return {};const auto info=hint(find(m_main,"categoryNav-"+m_longCategory));if(!info.value("visible").toBool() || !info.value("text").toString().startsWith(QStringLiteral("这是需要截断")))return fail("truncated category did not reveal full text");QMetaObject::invokeMethod(m_widget,"openVerificationMenu");}
    else if(m_stage==46){if(hint(find(m_main,"categoryNav-"+m_longCategory)).value("visible").toBool())return fail("cross-window menu tooltip suppression");QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");auto* row=qobject_cast<QQuickItem*>(find(m_main,"categoryNav-"+m_shortCategory));hover(m_main,"contentViewSwitch");row->forceActiveFocus();}
    else if(m_stage==47){if(m_wait.elapsed()<750)return {};if(hint(find(m_main,"categoryNav-"+m_shortCategory)).value("visible").toBool())return fail("keyboard focus opened tooltip");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_workspace.setPreference("highContrast",false);m_main->resize(1120,720);m_main->requestActivate();m_workspace.setView("inbox");}
    else if(m_stage>=48 && m_stage<=53){m_main->grabWindow();const QStringList views={"inbox","today","scheduled","all","important","completed"};if(!checkSidebar() || m_workspace.scope()!=views[m_stage-48]){if(m_wait.elapsed()<1800)return {};return fail("sidebar layout or six-scope routing");}if(m_stage==48){QVariant rhythm;QMetaObject::invokeMethod(m_main,"verifyPageRhythm",Q_RETURN_ARG(QVariant,rhythm));if(rhythm!="PASS" || !capture(m_main,"sidebar-light"))return fail("page typography rhythm: "+rhythm.toString());}click("navigation-"+(m_stage<53?views[m_stage-47]:QString("inbox")),m_main);}
    else if(m_stage==54){if(m_workspace.scope()!="inbox")return fail("scope return");click("sidebarTasksHeader",m_main);}
    else if(m_stage==55){if(m_workspace.preferences().value("sidebarTasksExpanded").toBool())return fail("tasks group collapse");click("sidebarTasksHeader",m_main);}
    else if(m_stage==56){if(!m_workspace.preferences().value("sidebarTasksExpanded").toBool())return fail("tasks group expand");m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_main->resize(880,580);}
    else if(m_stage==57){m_main->grabWindow();if(!checkSidebar() || find(m_main,"smartNavigationGrid")->property("columns").toInt()!=1){if(m_wait.elapsed()<1800)return {};return fail("large sidebar text/brand/footer");}QVariant rhythm;QMetaObject::invokeMethod(m_main,"verifyPageRhythm",Q_RETURN_ARG(QVariant,rhythm));if(rhythm!="PASS" || !capture(m_main,"sidebar-dark-200"))return fail("large page typography rhythm: "+rhythm.toString());click("toggleSidebar",m_main);}
    else if(m_stage==58){if(m_main->property("sidebarWidth").toInt()!=44 || find(m_main,"sidebarWordmark")->property("visible").toBool())return fail("collapsed sidebar recovery");click("toggleSidebar",m_main);m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_main->resize(1120,720);}
    else if(m_stage==59){if(!checkSidebar()){if(m_wait.elapsed()<1800)return {};return fail("restored sidebar");}click("settingsAndTools",m_main);}
    else if(m_stage==60){auto* settings=find(m_main,"settingsDialog");if(!settings->property("visible").toBool())return fail("settings entry");QMetaObject::invokeMethod(settings,"setVerificationCategory",Q_ARG(QVariant,QVariant(2)));}
    else if(m_stage==61){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));settings->grabWindow();if(!revealSettings("settingsHelp"))return fail("settings support scroll");}
    else if(m_stage==62){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(!capture(settings,"settings-support"))return fail("support capture");click("settingsHelp",settings);}
    else if(m_stage==63){if(find(m_main,"settingsDialog")->property("visible").toBool() || !find(m_main,"helpPanel")->property("visible").toBool())return fail("settings help routing");QMetaObject::invokeMethod(find(m_main,"helpPanel"),"close");click("settingsAndTools",m_main);}
    else if(m_stage==64){if(!find(m_main,"settingsDialog")->property("visible").toBool() || !revealSettings("settingsData"))return fail("settings reopened");}
    else if(m_stage==65){click("settingsData",find(m_main,"settingsDialog"));}
    else if(m_stage==66){if(find(m_main,"settingsDialog")->property("visible").toBool() || !find(m_main,"dataToolsDialog")->property("visible").toBool())return fail("settings data routing");QMetaObject::invokeMethod(find(m_main,"dataToolsDialog"),"close");auto* settings=find(m_main,"settingsDialog");QMetaObject::invokeMethod(settings,"open");QMetaObject::invokeMethod(settings,"showOverview");}
    else if(m_stage==67){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));QVariant result;QMetaObject::invokeMethod(settings,"verifyCategory",Q_RETURN_ARG(QVariant,result));if(result!="PASS" || find(settings,"settingsBack") || !capture(settings,"settings-two-column"))return fail("settings keeps sidebar beside active content: "+result.toString());click("settingsCategory-0",settings);}
    else if(m_stage==68){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));QVariant check;QMetaObject::invokeMethod(settings,"verifyCategory",Q_RETURN_ARG(QVariant,check));if(settings->property("categoryIndex").toInt()!=0 || check!="PASS" || !capture(settings,"settings-appearance"))return fail("settings sidebar selects appearance");click("themeModePicker-segment-2",settings);}
    else if(m_stage==69){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(m_workspace.themeMode()!=2 || !capture(settings,"settings-appearance-dark"))return fail("settings theme applies");click("themeModePicker-segment-1",settings);qobject_cast<QQuickItem*>(find(settings,"settingsCategory-0"))->forceActiveFocus();}
    else if(m_stage==70){auto* settings=find(m_main,"settingsDialog");auto* row=qobject_cast<QQuickItem*>(find(settings,"settingsCategory-0"));if(settings->property("categoryIndex").toInt()!=0 || !row->hasActiveFocus())return fail("settings sidebar retains selection and focus");settingsKey(Qt::Key_Down);}
    else if(m_stage==71){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(settings->property("categoryIndex").toInt()!=1 || !capture(settings,"settings-widgets"))return fail("settings keyboard category entry");if(find(settings,"widgetOpacitySetting")->property("enabled").toBool())return fail("opaque background disables irrelevant opacity");click("widgetSolidBackgroundSetting",settings);qobject_cast<QQuickItem*>(find(settings,"calendarDesktopPinnedSetting"))->forceActiveFocus();}
    else if(m_stage==72){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));auto* target=qobject_cast<QQuickItem*>(find(settings,"calendarDesktopPinnedSetting"));auto* scroll=qobject_cast<QQuickItem*>(find(settings,"settingsScroll"));const auto at=target->mapToItem(scroll,QPointF());if(!find(settings,"widgetOpacitySetting")->property("enabled").toBool() || at.y()<0 || at.y()+target->height()>scroll->height()+1 || !capture(settings,"settings-calendar-options"))return fail("settings focus reveals lower calendar group");m_settingsScroll=scroll->property("contentItem").value<QObject*>()->property("contentY").toDouble();}
    else if(m_stage==73){auto* settings=find(m_main,"settingsDialog");click("settingsCategory-2",settings);}
    else if(m_stage==74){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(settings->property("categoryIndex").toInt()!=2 || !revealSettings("backupDaysPicker"))return fail("settings general and backup reachability");qobject_cast<QQuickItem*>(find(settings,"backupDaysPicker"))->forceActiveFocus();settingsKey(Qt::Key_Space);settingsKey(Qt::Key_End);settingsKey(Qt::Key_Return);}
    else if(m_stage==75){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(m_workspace.preferences().value("backupDays").toInt()!=30 || !capture(settings,"settings-general"))return fail("settings grouped option persists");click("settingsCategory-1",settings);}
    else if(m_stage==76){auto* settings=find(m_main,"settingsDialog");auto* scroll=find(settings,"settingsScroll")->property("contentItem").value<QObject*>();if(settings->property("categoryIndex").toInt()!=1 || qAbs(scroll->property("contentY").toDouble()-m_settingsScroll)>1){qInfo()<<"Settings scroll restore"<<scroll->property("contentY")<<m_settingsScroll<<settings->property("categoryScroll")<<settings->property("activeFocusItem");return fail("category change restores its own scroll");}click("settingsCategory-3",settings);}
    else if(m_stage==77){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(settings->property("categoryIndex").toInt()!=3 || !capture(settings,"settings-about"))return fail("settings about entry");m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_main->resize(880,580);}
    else if(m_stage==78){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));QVariant check;QMetaObject::invokeMethod(settings,"verifyCategory",Q_RETURN_ARG(QVariant,check));if(check!="PASS" || !capture(settings,"settings-about-dark-200"))return fail("settings large about");settingsKey(Qt::Key_Escape);}
    else if(m_stage==79){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(settings->isVisible())return fail("Escape closes split settings");QMetaObject::invokeMethod(settings,"open");if(settings->property("categoryIndex").toInt()!=3)return fail("reopen preserves selected category");}
    else if(m_stage==80){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(!m_settingsHomeSent){qobject_cast<QQuickItem*>(find(settings,"settingsCategory-3"))->forceActiveFocus();settingsKey(Qt::Key_Home);m_settingsHomeSent=true;return {};}QVariant check;QMetaObject::invokeMethod(settings,"verifyCategory",Q_RETURN_ARG(QVariant,check));if(settings->property("categoryIndex").toInt()!=0 || check!="PASS" || !capture(settings,"settings-appearance-dark-200"))return fail("settings large preference rows and two columns: "+check.toString());qobject_cast<QQuickItem*>(find(settings,"settingsCategory-0"))->forceActiveFocus();settingsKey(Qt::Key_End);}
    else if(m_stage==81){auto* settings=find(m_main,"settingsDialog");if(settings->property("categoryIndex").toInt()!=3)return fail("End selects final sidebar category");qobject_cast<QQuickItem*>(find(settings,"settingsCategory-3"))->forceActiveFocus();settingsKey(Qt::Key_Up);settingsKey(Qt::Key_Up);}
    else if(m_stage==86){if(find(m_main,"settingsDialog")->property("visible").toBool())return fail("settings close action preserves main window");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_main->resize(1120,720);QMetaObject::invokeMethod(m_main,"showVerificationPage",Q_ARG(QVariant,QVariant("style")),Q_ARG(QVariant,QVariant("")));}
    else if(m_stage==82){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));QVariant check;QMetaObject::invokeMethod(settings,"verifyCategory",Q_RETURN_ARG(QVariant,check));if(settings->property("categoryIndex").toInt()!=1 || check!="PASS" || !capture(settings,"settings-widgets-dark-200"))return fail("settings large widget page");qobject_cast<QQuickItem*>(find(settings,"calendarDesktopPinnedSetting"))->forceActiveFocus();}
    else if(m_stage==83){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));auto* target=qobject_cast<QQuickItem*>(find(settings,"calendarDesktopPinnedSetting"));auto* scroll=qobject_cast<QQuickItem*>(find(settings,"settingsScroll"));const auto at=target->mapToItem(scroll,QPointF());if(at.y()<0 || at.y()+target->height()>scroll->height()+1 || !capture(settings,"settings-calendar-dark-200"))return fail("large lower settings stay reachable");m_workspace.setPreference("highContrast",true);}
    else if(m_stage==84){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(find(settings,"widgetOpacitySetting")->property("enabled").toBool() || !capture(settings,"settings-contrast-200"))return fail("high contrast keeps setting boundaries and disables opacity");m_workspace.setPreference("highContrast",false);settingsKey(Qt::Key_Escape);}
    else if(m_stage==85){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(settings->isVisible())return fail("Escape closes large settings");QMetaObject::invokeMethod(settings,"open");if(settings->property("categoryIndex").toInt()!=1)return fail("reopen keeps widget category");click("settingsClose",settings);}
    else if(m_stage==87){auto* gallery=find(m_main,"styleGallery");if(!gallery->property("visible").toBool())return fail("style matrix entry");QMetaObject::invokeMethod(gallery,"showVisualAudit",Q_ARG(QVariant,QVariant("icons")));}
    else if(m_stage==88){capture(m_main,"glyph-debug");if(!glyphPainted("calendar") || !glyphPainted("check") || !glyphPainted("grip") || !glyphPainted("settings") || !capture(m_main,"glyph-matrix-light"))return fail("vector symbols paint distinct small shapes");m_workspace.setThemeMode(2);m_workspace.setTextScale(2);m_main->resize(880,580);QMetaObject::invokeMethod(find(m_main,"styleGallery"),"showVisualAudit",Q_ARG(QVariant,QVariant("icons")));}
    else if(m_stage==89){if(!glyphPainted("calendar") || !capture(m_main,"glyph-matrix-dark-200"))return fail("vector symbol follows theme and text scale");QMetaObject::invokeMethod(find(m_main,"styleGallery"),"showVisualAudit",Q_ARG(QVariant,QVariant("clip")));}
    else if(m_stage==91){QVariant result;auto* gallery=find(m_main,"styleGallery");QMetaObject::invokeMethod(gallery,"verifyVisualSystem",Q_RETURN_ARG(QVariant,result));if(result!="PASS" || !capture(m_main,"type-scale-dark-200"))return fail("shared type and radius hierarchy: "+result.toString());QMetaObject::invokeMethod(gallery,"close");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_workspace.setPreference("highContrast",false);m_workspace.setPreference("solidWidget",false);m_controller.setBackgroundOpacity(0.6);m_controller.setMini(true);m_widget->setPosition(160,90);m_widget->resize(700,m_widget->height());}
    else if(m_stage==92){click("miniMore");}
    else if(m_stage==93){QVariant result;QMetaObject::invokeMethod(m_widget,"verifyMenu",Q_RETURN_ARG(QVariant,result));if(result!="PASS")return fail("Mini menu anchor and opaque surface: "+result.toString());auto* content=qobject_cast<QQuickItem*>(find(m_widget,"widgetMenu")->property("contentItem").value<QObject*>());if(!content)return fail("Mini menu content");const auto image=content->window()->grabWindow();int ink=0;for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x)if(image.pixelColor(x,y).lightness()<100)++ink;if(ink<30){if(m_wait.elapsed()<1500)return {};return fail("Mini opaque window must paint menu text");}if(!capture(content->window(),"mini-solid-menu"))return fail("Mini menu window capture");m_controller.setMini(false);}
    else if(m_stage==94){if(find(m_widget,"widgetMenu")->property("visible").toBool())return fail("mode change left stale menu open");click("widgetMore");}
    else if(m_stage==95){QVariant result;QMetaObject::invokeMethod(m_widget,"verifyMenu",Q_RETURN_ARG(QVariant,result));if(result!="PASS")return fail("expanded menu reacquires current anchor: "+result.toString());auto* content=qobject_cast<QQuickItem*>(find(m_widget,"widgetMenu")->property("contentItem").value<QObject*>());if(!capture(content->window(),"floating-solid-menu"))return fail("floating menu capture");m_widget->setX(m_widget->x()+20);}
    else if(m_stage==96){if(find(m_widget,"widgetMenu")->property("visible").toBool())return fail("moving host left detached menu");click("desktopCalendarOptions",calendar);}
    else if(m_stage==97){auto* button=find(calendar,"desktopCalendarOptions");auto* menu=find(button,"desktopCalendarOptionsMenu");auto* content=menu?qobject_cast<QQuickItem*>(menu->property("contentItem").value<QObject*>()):nullptr;if(!menu || !menu->property("visible").toBool() || menu->property("anchorItem").value<QObject*>()!=button || !content || content->window()->color().alpha()!=255 || !capture(content->window(),"calendar-solid-menu"))return fail("desktop calendar explicit opaque menu anchor");QMetaObject::invokeMethod(menu,"close");return "DONE";}
    else if(m_stage==98){
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
    else if(m_stage==99){
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
    else if(m_stage==100){
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
    else if(m_stage==101){
        auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        if(!QMetaObject::invokeMethod(settings,"reveal",Q_ARG(QVariant,QVariant::fromValue(find(settings,"syncConfirmPassword")))))return fail("large confirmation field scroll");
        settings->grabWindow();
    }
    else if(m_stage==102){
        auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        auto* field=qobject_cast<QQuickItem*>(find(settings,"syncConfirmPassword"));auto* view=qobject_cast<QQuickItem*>(find(settings,"accountScroll"));
        const auto at=field->mapToItem(view,QPointF());
        if(at.y()<0 || at.y()+field->height()>view->height()+1 || !capture(settings,"registration-dark-200")){
            qCritical()<<"Account scroll"<<at<<field->size()<<view->size()<<find(settings,"accountScroll")->property("contentHeight");capture(settings,"registration-geometry-failure");
            return fail("registration confirmation remains reachable at 200 percent");
        }
        if(!QMetaObject::invokeMethod(settings,"reveal",Q_ARG(QVariant,QVariant::fromValue(find(settings,"syncSubmit")))))return fail("large registration action scroll");
    }
    else if(m_stage==103){
        auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        auto* button=qobject_cast<QQuickItem*>(find(settings,"syncSubmit"));auto* view=qobject_cast<QQuickItem*>(find(settings,"accountScroll"));const auto at=button->mapToItem(view,QPointF());
        if(at.y()<0 || at.y()+button->height()>view->height()+1 || !capture(settings,"registration-actions-dark-200"))return fail("registration action remains reachable at 200 percent");
        QMetaObject::invokeMethod(settings,"close");
        if(settings->property("syncRegisterMode").toBool() || !find(settings,"syncPassword")->property("text").toString().isEmpty() || !find(settings,"syncConfirmPassword")->property("text").toString().isEmpty())return fail("closing registration clears secrets and resets mode");
        m_workspace.setTextScale(1);m_workspace.setThemeMode(1);
    }
    else if(m_stage==104){
        // Synthetic presentation states do not authenticate the coordinator or
        // touch a server. Real auth/sync remains covered by the Qt/Go TLS test.
        m_originalSyncState=m_workspace.syncState();auto state=m_originalSyncState;
        state.insert("safe_error_code","invalid_credentials");state.insert("status",QStringLiteral("账号或密码不正确。"));
        state.insert("authenticated",false);state.insert("busy",false);m_accountDisplayState=state;m_workspace.setSyncState(state);
        QMetaObject::invokeMethod(m_main,"openTool",Q_ARG(QVariant,QVariant("account")));
    }
    else if(m_stage==105){
        m_workspace.setSyncState(m_accountDisplayState);
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        if(!find(window,"accountError")->property("visible").toBool() || !find(window,"syncPassword")->property("text").toString().isEmpty())return fail("dedicated login error and empty password");
        if(find(window,"syncServerOrigin")->property("text")!="https://example.invalid" || find(window,"syncLogin")->property("text")!="synthetic-user")return fail("closing and reopening preserves server/account input, without retaining password");
        if(!capture(window,"account-login-error"))return fail("account error capture");
        auto state=m_workspace.syncState();state.insert("busy",true);state.insert("safe_error_code",QString());state.insert("status",QStringLiteral("正在登录…"));m_accountDisplayState=state;m_workspace.setSyncState(state);
    }
    else if(m_stage==106){
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
    else if(m_stage==107){
        m_workspace.setSyncState(m_accountDisplayState);
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        if(find(window,"syncLogin")->property("visible").toBool() || !find(window,"syncNow")->property("visible").toBool() || find(window,"syncProfiles")->property("count").toInt()!=2 || !find(window,"accountConflictLocal"))return fail("connected account management replaces auth form and keeps conflicts/profiles");
        if(!capture(window,"account-management-light"))return fail("account management capture");
        auto state=m_workspace.syncState();state.insert("busy",true);state.insert("phase","RECONCILE_REQUIRED");m_accountDisplayState=state;m_workspace.setSyncState(state);
    }
    else if(m_stage==108){
        m_workspace.setSyncState(m_accountDisplayState);
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        if(!find(window,"syncRebuild")->property("visible").toBool() || find(window,"syncRebuild")->property("enabled").toBool() || find(window,"accountConflictLocal")->property("enabled").toBool())return fail("busy account keeps recovery visible and prevents repeated decisions");
        m_workspace.setTextScale(2);m_workspace.setThemeMode(2);window->resize(420,440);
    }
    else if(m_stage==109){
        m_workspace.setSyncState(m_accountDisplayState);
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        QMetaObject::invokeMethod(window,"reveal",Q_ARG(QVariant,QVariant::fromValue(find(window,"syncProfiles"))));
    }
    else if(m_stage==110){
        m_workspace.setSyncState(m_accountDisplayState);
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));auto* footer=qobject_cast<QQuickItem*>(find(window,"accountContinueLocal"));
        const auto at=footer->mapToScene(QPointF());
        if(at.x()<0 || at.y()<0 || at.x()+footer->width()>window->width()+1 || at.y()+footer->height()>window->height()+1 || !capture(window,"account-management-dark-200-narrow"))return fail("account footer remains reachable in narrow large text window");
        QMetaObject::invokeMethod(window,"close");m_workspace.setSyncState(m_originalSyncState);m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_workspace.setPreference("sidebarCollapsed",true);
    }
    else if(m_stage==111){
        auto* entry=qobject_cast<QQuickItem*>(find(m_main,"accountCollapsedEntry"));
        if(!entry || !entry->isVisible() || entry->mapToScene(QPointF()).x()<0 || !capture(m_main,"account-entry-collapsed"))return fail("collapsed account entry remains available");
        click("accountCollapsedEntry",m_main);
    }
    else if(m_stage==112){
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        if(!window->isVisible())return fail("collapsed account button opens dedicated account window");
        for(const auto type:{QEvent::KeyPress,QEvent::KeyRelease}){QKeyEvent event(type,Qt::Key_Escape,Qt::NoModifier);QCoreApplication::sendEvent(window,&event);}
        if(window->isVisible())return fail("Escape closes account window");
        m_workspace.setPreference("sidebarCollapsed",false);
    }
    else if(m_stage==113){
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
    else if(m_stage==114){
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        if(window->property("verificationAuthIntents").toInt()!=1 || window->property("verificationAuthAction")!="login" || !window->property("verificationAuthValues").toBool() || !find(window,"syncPassword")->property("text").toString().isEmpty())return fail("dedicated login routes exact transient credentials once and clears them");
        if(m_workspace.syncState().value("busy").toBool()){if(m_wait.elapsed()<5000)return {};return fail("loopback connection failure bounded");}
        if(m_workspace.syncState().value("authenticated").toBool() || !find(window,"accountError")->property("visible").toBool())return fail("real coordinator connection failure stays local and actionable");
        QMetaObject::invokeMethod(find(window,"syncRegisterMode"),"clicked");find(window,"syncPassword")->setProperty("text","synthetic-long-password");find(window,"syncConfirmPassword")->setProperty("text","synthetic-long-password");
        QMetaObject::invokeMethod(find(window,"syncSubmit"),"clicked");
    }
    else if(m_stage==115){
        auto* window=qobject_cast<QQuickWindow*>(find(m_main,"accountWindow"));
        if(window->property("verificationAuthIntents").toInt()!=2 || window->property("verificationAuthAction")!="register" || !window->property("verificationAuthValues").toBool() || !find(window,"syncPassword")->property("text").toString().isEmpty() || !find(window,"syncConfirmPassword")->property("text").toString().isEmpty())return fail("dedicated registration routes credentials once and clears both fields");
        if(m_workspace.syncState().value("busy").toBool()){if(m_wait.elapsed()<5000)return {};return fail("registration loopback failure bounded");}
        if(m_workspace.syncState().value("authenticated").toBool())return fail("failed connection must not authenticate");
        QMetaObject::invokeMethod(window,"close");return "DONE";
    }
    else if(m_stage==90){auto* glyph=qobject_cast<QQuickItem*>(find(m_main,"galleryGlyph-calendar"));const auto at=glyph->mapToScene(QPointF());if(at.y()<0 || at.y()+glyph->height()>m_main->height() || glyphPainted("calendar") || !capture(m_main,"glyph-scroll-clip-dark-200"))return fail("off-viewport vector cannot leak over dialog actions");QMetaObject::invokeMethod(find(m_main,"styleGallery"),"showVisualAudit",Q_ARG(QVariant,QVariant("type")));}
    qInfo()<<"UX38 desktop scenario"<<m_stage<<"complete";++m_stage;m_wait.restart();return {};
}
}
