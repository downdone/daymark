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
namespace daymark {
Ux38SmokeProbe::Ux38SmokeProbe(TaskWorkspace& workspace,WidgetViewModel& controller,QQuickWindow* main,QQuickWindow* widget,QString screenshot)
    :m_workspace(workspace),m_controller(controller),m_main(main),m_widget(widget),m_screenshot(std::move(screenshot)){m_wait.start();}
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
        const auto frame=m_main->grabWindow();const double dpr=frame.devicePixelRatio();const auto position=brand->mapToScene(QPointF());const QColor ink=brand->property("ink").value<QColor>();int painted=0;
        for(int y=qMax(0,int(position.y()*dpr));y<qMin(frame.height(),int((position.y()+brand->height())*dpr));++y)for(int x=qMax(0,int(position.x()*dpr));x<qMin(frame.width(),int((position.x()+brand->width())*dpr));++x){const auto pixel=frame.pixelColor(x,y);if(qAbs(pixel.red()-ink.red())+qAbs(pixel.green()-ink.green())+qAbs(pixel.blue()-ink.blue())<60)++painted;}
        if(painted<20){if(m_wait.elapsed()>=1800){qCritical()<<"Unpainted wordmark"<<brand->size()<<position<<brand->isVisible()<<ink<<painted;capture(m_main,"wordmark-failure");}return false;}
        for(const QString code:{"inbox","today","scheduled","all","important","completed"}){auto* tile=find(m_main,"navigation-"+code);auto* title=tile?find(tile,"smartNavigationTitle"):nullptr;if(!title || title->property("truncated").toBool())return false;}
        return true;
    };
    auto revealSettings=[&](const QString& name){auto* settings=find(m_main,"settingsDialog");auto* scroll=find(settings,"settingsScroll");auto* flick=scroll?scroll->property("contentItem").value<QObject*>():nullptr;auto* target=qobject_cast<QQuickItem*>(find(settings,name));auto* content=flick?qobject_cast<QQuickItem*>(flick->property("contentItem").value<QObject*>()):nullptr;if(!target || !content)return false;const auto y=target->mapToItem(content,QPointF()).y();flick->setProperty("contentY",qBound(0.,y-20,qMax(0.,flick->property("contentHeight").toDouble()-flick->property("height").toDouble())));return true;};
    auto fail=[&](const QString& text){capture(m_widget,"failure");return QStringLiteral("UX38 stage %1: %2").arg(m_stage).arg(text);};
    auto* list=find(m_widget,"widgetTasks");
    auto* calendar=qobject_cast<QQuickWindow*>(find(m_main,"desktopCalendarWindow"));
    if(!list || !calendar)return fail("desktop surfaces missing");
    if(m_stage==0){m_workspace.setPreference("followSystemAccessibility",false);m_workspace.setPreference("reduceMotion",false);m_workspace.setThemeMode(1);m_workspace.setTextScale(1);m_workspace.setView("today");m_workspace.setPreference("desktopCalendarVisible",true);m_controller.setMini(false);m_controller.setVisible(true);m_widget->resize(340,360);}
    else if(m_stage==1){if(!calendar->isVisible() || !m_widget->isVisible() || !layout() || list->property("interactive").toBool() || !capture(m_widget,"empty"))return fail("empty state and coexistence");m_workspace.createForTime(QStringLiteral("讨论方案与下一步安排"),m_workspace.dateOffset(0),540);}
    else if(m_stage==2){if(m_workspace.todayTasks()->rowCount()!=1)return fail("today fixture");m_task=m_workspace.todayTasks()->items().first().id;m_workspace.createSubtask(QStringLiteral("准备评审材料"),m_task);}
    else if(m_stage==3){auto* row=find(m_widget,"widgetTask-"+m_task);if(!row || row->property("summary").toString().isEmpty() || row->property("subtaskTotal").toInt()!=1 || !layout() || !capture(m_widget,"today-light") || !capture(calendar,"calendar-light"))return fail("shared schedule and relationship");click("widgetPin");}
    else if(m_stage==4){if(m_controller.alwaysOnTop())return fail("unpin");click("widgetCollapse");}
    else if(m_stage==5){if(!m_controller.mini() || !layout() || !capture(m_widget,"mini-light"))return fail("mini summary");click("widgetMiniSummary");}
    else if(m_stage==6){if(m_controller.mini() || m_widget->size()!=QSize(340,360))return fail("single click restores floating geometry");m_controller.setLocked(true);m_controller.setMini(true);}
    else if(m_stage==7){expandWithKey(Qt::Key_Space);}
    else if(m_stage==8){if(m_controller.mini() || !m_controller.errorText().isEmpty())return fail("locked mini keyboard expansion");m_controller.setLocked(false);m_workspace.setTextScale(2);m_widget->resize(260,190);auto* row=find(m_widget,"widgetTask-"+m_task);row->setProperty("rowVersion",row->property("rowVersion").toLongLong()-1);QMetaObject::invokeMethod(row,"complete");}
    else if(m_stage==9){auto* row=find(m_widget,"widgetTask-"+m_task);if(!layout() || !row || row->property("completing").toBool() || m_workspace.errorText().isEmpty() || !find(m_widget,"widgetError")->property("visible").toBool())return fail("stale completion preserves row and error");m_controller.setMini(true);}
    else if(m_stage==10){if(m_widget->property("errorMessage").toString().isEmpty() || !capture(m_widget,"mini-error"))return fail("mini error retained");expandWithKey(Qt::Key_Return);}
    else if(m_stage==11){if(m_controller.mini())return fail("Return expansion");m_workspace.clearError();find(m_widget,"widgetTask-"+m_task)->setProperty("rowVersion",m_workspace.task(m_task).value("rowVersion"));}
    else if(m_stage==12){m_completedRow=qobject_cast<QQuickItem*>(find(m_widget,"widgetTask-"+m_task));m_count=m_workspace.todayTasks()->rowCount();click("widgetComplete-"+m_task);}
    else if(m_stage==13){if(m_workspace.todayTasks()->rowCount()>=m_count || (m_completedRow && !m_completedRow->property("removing").toBool()) || !capture(m_widget,"completion-feedback"))return fail("successful completion and feedback");}
    else if(m_stage==14){if(m_wait.elapsed()<250)return {};if(m_completedRow)return fail("completion releases visual row");m_workspace.setPreference("reduceMotion",true);}
    else if(m_stage>=15 && m_stage<=25){m_workspace.createTask(QStringLiteral("今日行动 %1：桌面任务验收").arg(m_stage-14));}
    else if(m_stage==26){if(!list->property("interactive").toBool() || !capture(m_widget,"scrollable"))return fail("one overflowing scroll region");m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_widget->resize(340,440);}
    else if(m_stage==27){if(!layout() || !capture(m_widget,"dark-200"))return fail("floating large text");m_controller.setMini(true);}
    else if(m_stage==28){if(!layout() || !capture(m_widget,"mini-dark-200"))return fail("mini readable at 200 percent");expandWithKey(Qt::Key_Return);calendar->resize(420,580);}
    else if(m_stage==29){auto* panel=find(calendar,"desktopCalendarPanel");auto* toolbar=qobject_cast<QQuickItem*>(find(panel,"calendarToolbar"));if(!toolbar || toolbar->property("columns").toInt()!=1 || !capture(calendar,"calendar-dark-200")){qCritical()<<"Calendar geometry"<<calendar->size()<<(panel?panel->property("width"):QVariant())<<(toolbar?toolbar->property("width"):QVariant())<<(toolbar?toolbar->property("columns"):QVariant());capture(calendar,"calendar-layout-failure");return fail("calendar adaptive toolbar");}auto* more=find(calendar,"calendarMore-"+m_workspace.dateOffset(0));if(!more || !more->property("visible").toBool() || more->property("remainingCount").toInt()<1 || (more->property("compactOverflow").toBool() && more->property("text")!=QStringLiteral("…")))return fail("narrow calendar overflow remains readable");m_workspace.setPreference("solidWidget",true);m_workspace.setPreference("desktopCalendarOpacity",50);}
    else if(m_stage==30){for(auto* window:{m_widget.data(),calendar}){auto* background=find(window,window==calendar?"desktopCalendarBackground":"widgetBackground");if(!background || background->property("color").value<QColor>().alpha()!=255)return fail("solid background on both desktop surfaces");}m_workspace.setPreference("highContrast",true);m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_widget->resize(260,360);}
    else if(m_stage==31){if(!layout() || !capture(m_widget,"narrow-high-contrast"))return fail("260 DIP high contrast");QMetaObject::invokeMethod(m_widget,"openVerificationMenu");}
    else if(m_stage==32){QVariant result;QMetaObject::invokeMethod(m_widget,"verifyMenu",Q_RETURN_ARG(QVariant,result));if(result!="PASS" || !capture(m_widget,"menu"))return fail("menu width");QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");m_task=m_workspace.todayTasks()->items().first().id;m_completedRow=qobject_cast<QQuickItem*>(find(m_widget,"widgetTask-"+m_task));m_count=m_workspace.todayTasks()->rowCount();}
    else if(m_stage==33){auto* item=qobject_cast<QQuickItem*>(list);item->setProperty("currentIndex",0);item->forceActiveFocus();for(const auto type:{QEvent::KeyPress,QEvent::KeyRelease}){QKeyEvent event(type,Qt::Key_Space,Qt::NoModifier);QCoreApplication::sendEvent(item,&event);}}
    else if(m_stage==34){if(m_workspace.todayTasks()->rowCount()!=m_count-1 || (m_completedRow && m_completedRow->property("removing").toBool()) || m_workspace.task(m_task).value("taskStatus")!="COMPLETED")return fail("reduced motion keyboard completion without delayed removal");m_workspace.setTextScale(2);m_widget->resize(800,600);QMetaObject::invokeMethod(m_widget,"openVerificationMenu");}
    else if(m_stage==35){QVariant result;QMetaObject::invokeMethod(m_widget,"verifyMenu",Q_RETURN_ARG(QVariant,result));if(result!="PASS" || !capture(m_widget,"menu-200"))return fail("menu resolved font at 200 percent");QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");m_workspace.setTextScale(1);m_controller.setMini(true);m_widget->resize(800,m_widget->height());}
    else if(m_stage==36){if(find(m_widget,"widgetMiniTitle")->property("truncated").toBool())return fail("short mini tooltip fixture");hover(m_widget,"widgetMiniSummary");}
    else if(m_stage==37){if(m_wait.elapsed()<750)return {};if(hint(m_widget).value("visible").toBool())return fail("complete mini title repeats tooltip");hover(m_widget,"widgetExpand");}
    else if(m_stage==38){if(m_wait.elapsed()<750)return {};if(hint(find(m_widget,"widgetExpand")).value("visible").toBool())return fail("obvious expand arrow tooltip");expandWithKey(Qt::Key_Return);}
    else if(m_stage==39){hover(m_widget,"widgetPin");}
    else if(m_stage==40){if(m_wait.elapsed()<750)return {};if(!hint(find(m_widget,"widgetPin")).value("visible").toBool())return fail("necessary pin action hint absent");QMetaObject::invokeMethod(m_widget,"openVerificationMenu");}
    else if(m_stage==41){if(hint(find(m_widget,"widgetPin")).value("visible").toBool())return fail("menu did not dismiss current tooltip");QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");m_workspace.saveCatalog({{"kind","project"},{"title","123"}});}
    else if(m_stage==42){m_workspace.saveCatalog({{"kind","project"},{"title",QStringLiteral("这是需要截断显示并且应该恢复完整名称的分类验收")}});}
    else if(m_stage==43){for(const auto& value:m_workspace.categoryTree()){const auto row=value.toMap();if(row.value("title")=="123")m_shortCategory=row.value("id").toString();else if(row.value("title").toString().startsWith(QStringLiteral("这是需要截断")))m_longCategory=row.value("id").toString();}if(m_shortCategory.isEmpty() || m_longCategory.isEmpty())return fail("catalog hint fixtures");m_main->requestActivate();m_main->grabWindow();capture(m_main,"category-hints");hover(find(m_main,"categoryNav-"+m_shortCategory),"navigationLabel");}
    else if(m_stage==44){if(m_wait.elapsed()<750)return {};if(hint(find(m_main,"categoryNav-"+m_shortCategory)).value("visible").toBool())return fail("short category repeats visible name");m_main->grabWindow();auto* row=find(m_main,"categoryNav-"+m_longCategory);auto* label=find(row,"navigationLabel");if(!label->property("truncated").toBool()){if(m_wait.elapsed()<1800)return {};qCritical()<<"Long hint geometry"<<label->property("width")<<label->property("text")<<label->property("visible")<<label->property("contentWidth")<<m_main->size();capture(m_main,"category-failure");return fail("long category fixture not truncated");}hover(row,"navigationLabel");}
    else if(m_stage==45){if(m_wait.elapsed()<750)return {};const auto info=hint(find(m_main,"categoryNav-"+m_longCategory));if(!info.value("visible").toBool() || !info.value("text").toString().startsWith(QStringLiteral("这是需要截断")))return fail("truncated category did not reveal full text");QMetaObject::invokeMethod(m_widget,"openVerificationMenu");}
    else if(m_stage==46){if(hint(find(m_main,"categoryNav-"+m_longCategory)).value("visible").toBool())return fail("cross-window menu tooltip suppression");QMetaObject::invokeMethod(m_widget,"closeVerificationMenu");auto* row=qobject_cast<QQuickItem*>(find(m_main,"categoryNav-"+m_shortCategory));hover(m_main,"contentViewSwitch");row->forceActiveFocus();}
    else if(m_stage==47){if(m_wait.elapsed()<750)return {};if(hint(find(m_main,"categoryNav-"+m_shortCategory)).value("visible").toBool())return fail("keyboard focus opened tooltip");m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_workspace.setPreference("highContrast",false);m_main->resize(1120,720);m_main->requestActivate();m_workspace.setView("inbox");}
    else if(m_stage>=48 && m_stage<=53){m_main->grabWindow();const QStringList views={"inbox","today","scheduled","all","important","completed"};if(!checkSidebar() || m_workspace.scope()!=views[m_stage-48]){if(m_wait.elapsed()<1800)return {};return fail("sidebar layout or six-scope routing");}if(m_stage==48 && !capture(m_main,"sidebar-light"))return fail("sidebar capture");click("navigation-"+(m_stage<53?views[m_stage-47]:QString("inbox")),m_main);}
    else if(m_stage==54){if(m_workspace.scope()!="inbox")return fail("scope return");click("sidebarTasksHeader",m_main);}
    else if(m_stage==55){if(m_workspace.preferences().value("sidebarTasksExpanded").toBool())return fail("tasks group collapse");click("sidebarTasksHeader",m_main);}
    else if(m_stage==56){if(!m_workspace.preferences().value("sidebarTasksExpanded").toBool())return fail("tasks group expand");m_workspace.setTextScale(2);m_workspace.setThemeMode(2);m_main->resize(880,580);}
    else if(m_stage==57){m_main->grabWindow();if(!checkSidebar() || find(m_main,"smartNavigationGrid")->property("columns").toInt()!=1){if(m_wait.elapsed()<1800)return {};return fail("large sidebar text/brand/footer");}if(!capture(m_main,"sidebar-dark-200"))return fail("sidebar capture");click("toggleSidebar",m_main);}
    else if(m_stage==58){if(m_main->property("sidebarWidth").toInt()!=44 || find(m_main,"sidebarWordmark")->property("visible").toBool())return fail("collapsed sidebar recovery");click("toggleSidebar",m_main);m_workspace.setTextScale(1);m_workspace.setThemeMode(1);m_main->resize(1120,720);}
    else if(m_stage==59){if(!checkSidebar()){if(m_wait.elapsed()<1800)return {};return fail("restored sidebar");}click("settingsAndTools",m_main);}
    else if(m_stage==60){auto* settings=find(m_main,"settingsDialog");if(!settings->property("visible").toBool())return fail("settings entry");QMetaObject::invokeMethod(settings,"setVerificationCategory",Q_ARG(QVariant,QVariant(2)));}
    else if(m_stage==61){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));settings->grabWindow();if(!revealSettings("settingsHelp"))return fail("settings support scroll");}
    else if(m_stage==62){auto* settings=qobject_cast<QQuickWindow*>(find(m_main,"settingsDialog"));if(!capture(settings,"settings-support"))return fail("support capture");click("settingsHelp",settings);}
    else if(m_stage==63){if(find(m_main,"settingsDialog")->property("visible").toBool() || !find(m_main,"helpPanel")->property("visible").toBool())return fail("settings help routing");QMetaObject::invokeMethod(find(m_main,"helpPanel"),"close");click("settingsAndTools",m_main);}
    else if(m_stage==64){if(!find(m_main,"settingsDialog")->property("visible").toBool() || !revealSettings("settingsData"))return fail("settings reopened");}
    else if(m_stage==65){click("settingsData",find(m_main,"settingsDialog"));}
    else if(m_stage==66){if(find(m_main,"settingsDialog")->property("visible").toBool() || !find(m_main,"dataToolsDialog")->property("visible").toBool())return fail("settings data routing");QMetaObject::invokeMethod(find(m_main,"dataToolsDialog"),"close");return "DONE";}
    qInfo()<<"UX38 desktop scenario"<<m_stage<<"complete";++m_stage;m_wait.restart();return {};
}
}
