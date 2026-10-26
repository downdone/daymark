#include "TaskWorkspace.h"
#include "services/ReminderSchedule.h"
#include <QGuiApplication>
#include <QStyleHints>
#include <QAccessibilityHints>
#include "platform/common/AccessibilityPreferences.h"
#include "persistence/SqliteDatabase.h"
#include "persistence/TaskRepository.h"
#include "services/TaskService.h"
#include "services/CalendarService.h"
#include "services/EventService.h"
#include "services/RecurrenceService.h"
#include "persistence/DataTransfer.h"
#include <QUrl>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QSet>
#include <QTimeZone>
#include <algorithm>
#include <memory>

namespace daymark {

QHash<int,QByteArray> TaskListModel::roleNames() const
{ return {{Id,"taskId"},{Title,"title"},{Notes,"notes"},{Done,"isDone"},{Status,"taskStatus"},{StartOn,"startOn"},
          {DeadlineOn,"deadlineOn"},{Focus,"todayFocus"},{Version,"rowVersion"},{Overdue,"isOverdue"},
          {Evening,"evening"},{ProjectName,"projectName"},{TagSummary,"tagSummary"},{Section,"taskSection"},{HeadingTitle,"headingTitle"},{SubtaskPending,"subtaskPending"},{ContextGroup,"contextGroup"},{InToday,"inToday"},{ViewSection,"viewSection"},{ParentTask,"parentTaskId"},{ParentTitle,"parentTitle"},{ChildCount,"visibleChildCount"},{Depth,"taskDepth"},{Pinned,"pinned"},{Important,"important"},{Planning,"planningState"},{ParentDeleted,"parentDeleted"},{PlannedDate,"plannedDate"},{CompletionMonth,"completionMonth"},{DeletedAt,"deletedAt"},{ArrangementSummary,"arrangementSummary"},{SubtaskTotal,"subtaskTotal"}}; }
QVariant TaskListModel::data(const QModelIndex& index,int role) const
{
    if(!index.isValid() || index.row()<0 || index.row()>=m_tasks.size()) return {};
    const auto& t=m_tasks[index.row()];
    switch(role) {
    case Planning:return t.planningState;case ParentDeleted:return t.parentDeleted;case ParentTitle:return t.parentTitle;case ChildCount:return t.listChildCount;case ParentTask:return t.parentTaskId;case Depth:return t.listDepth;case Pinned:return t.pinned;case Important:return t.important;
    case PlannedDate:return QueryService::plannedDate(t).toString(Qt::ISODate);
    case CompletionMonth:return QDateTime::fromString(t.completedAtUtc,Qt::ISODateWithMs).toLocalTime().date().toString(QStringLiteral("yyyy年M月"));
    case DeletedAt:return t.deletedAtUtc;
    case ViewSection:return m_groupHeaders.value(t.id);
    case Id:return t.id; case Title:return t.title; case Notes:return t.notes; case Done:return t.status!=QStringLiteral("OPEN");
    case Status:return t.status; case StartOn:return t.startOn?t.startOn->toString(Qt::ISODate):QString();
    case DeadlineOn:return t.deadlineOn?t.deadlineOn->toString(Qt::ISODate):QString(); case Focus:return t.todayFocus;
    case Version:return t.rowVersion;
    case Evening:return t.evening;
    case ProjectName:return t.projectName.isEmpty()?t.areaName:t.projectName;
    case ContextGroup:return t.areaName.isEmpty() && t.projectName.isEmpty()?tr("收集箱"):t.areaName+(t.areaName.isEmpty() || t.projectName.isEmpty()?QString():QStringLiteral(" / "))+t.projectName;
    case InToday:return QueryService::inView(t,QStringLiteral("today"),m_today);
    case ArrangementSummary:return CalendarService::timeSummary(t,m_today,QTimeZone::systemTimeZone());
    case SubtaskTotal:return t.totalSubtasks;
    case TagSummary:{const auto shown=t.tagNames.mid(0,2).join(QStringLiteral(" · "));return t.tagNames.size()>2?shown+QStringLiteral(" +%1").arg(t.tagNames.size()-2):shown;}
    case Section:return QStringList{tr("全天"),tr("上午"),tr("下午"),tr("今晚")}.value(QueryService::todayPeriod(t,m_today));
    case HeadingTitle:return t.headingTitle.isEmpty()?tr("未分组"):t.headingTitle;
    case SubtaskPending:return t.pendingSubtasks;
    case Overdue:return QueryService::isOverdue(t,m_today);
    default:return {};
    }
}
QVariantMap TaskListModel::taskMap(const Task& stored)
{
    const auto t=QueryService::withUnifiedDates(stored);
    QVariantMap schedule,repeat;auto timeText=[](const QTime& value){return value.toString(value.second() || value.msec()?QStringLiteral("HH:mm:ss.zzz"):QStringLiteral("HH:mm"));};
    if(t.schedule){const auto& s=*t.schedule;const QTimeZone zone(s.timeZone.toUtf8());const auto start=s.startUtc.toTimeZone(zone),end=s.endUtc.toTimeZone(zone);schedule={{"allDay",s.allDay},{"startDate",s.allDay?s.startOn.toString(Qt::ISODate):start.date().toString(Qt::ISODate)},{"endDate",s.allDay?s.endOn.addDays(-1).toString(Qt::ISODate):end.date().toString(Qt::ISODate)},{"startTime",timeText(start.time())},{"endTime",timeText(end.time())},{"zone",s.timeZone},{"alertMinutes",s.alertMinutes}};}
    if(t.repeat){const auto& r=*t.repeat;repeat={{"active",r.active},{"frequency",r.frequency},{"interval",r.interval},{"weekdays",r.weekdays},{"monthDay",r.monthDay},{"yearMonth",r.yearMonth},{"end",r.end.toString(Qt::ISODate)},{"rowVersion",r.rowVersion}};}
    return {{"arrangementSummary",CalendarService::timeSummary(t,QDate::currentDate(),QTimeZone::systemTimeZone())},{"subtaskTotal",t.totalSubtasks},{"storedStartOn",stored.startOn?stored.startOn->toString(Qt::ISODate):QString()},{"storedEndOn",stored.deadlineOn?stored.deadlineOn->toString(Qt::ISODate):QString()},{"timeDatesDiffer",(stored.startOn && stored.startOn!=t.startOn) || (stored.deadlineOn && stored.deadlineOn!=t.deadlineOn)},{"schedule",schedule.isEmpty()?QVariant():QVariant(schedule)},{"repeat",repeat.isEmpty()?QVariant():QVariant(repeat)},{"parentTaskId",t.parentTaskId},{"parentTitle",t.parentTitle},{"parentDeleted",t.parentDeleted},{"pinned",t.pinned},{"important",t.important},{"taskDepth",t.listDepth},{"taskId",t.id},{"title",t.title},{"notes",t.notes},{"planningState",t.planningState},{"taskStatus",t.status},
            {"startOn",t.startOn?t.startOn->toString(Qt::ISODate):QString()},{"deadlineOn",t.deadlineOn?t.deadlineOn->toString(Qt::ISODate):QString()},
            {"todayFocus",t.todayFocus},{"rowVersion",t.rowVersion},
            {"projectId",t.projectId},{"areaId",t.areaId},{"headingId",t.headingId},{"tagIds",t.tagIds},
            {"tagNames",t.tagNames},{"evening",t.evening},{"remindAtUtc",t.remindAtUtc},
            {"reminderRepeat",t.reminderRepeat},{"reminderEvery",t.reminderEvery},{"reminderZone",t.reminderZone},{"createdAtUtc",t.createdAtUtc},{"updatedAtUtc",t.updatedAtUtc},{"completedAtUtc",t.completedAtUtc},{"recurrenceId",t.recurrenceId},
            {"links",[&]{QVariantList list;for(const auto& link:t.links)list.append(QVariantMap{{"taskId",link.taskId},{"role",link.role}});return list;}()},{"subtaskPending",t.pendingSubtasks}};
}
QVariantList TaskListModel::selectionEntries(int first,int last) const
{
    QVariantList result;
    first=std::max(0,first);last=std::min(last,static_cast<int>(m_tasks.size())-1);
    if(last<first)return result;
    result.reserve(last-first+1);
    for(int i=first;i<=last;++i)result.append(QVariantMap{{"taskId",m_tasks[i].id},{"rowVersion",m_tasks[i].rowVersion}});
    return result;
}
void TaskListModel::replace(const QList<Task>& tasks,const QDate& today,int groupRole)
{
    QHash<QString,QString> headers;QString current;
    for(const auto& task:tasks){
        if(task.listDepth==0){
            if(groupRole==Section)current=QStringList{tr("全天"),tr("上午"),tr("下午"),tr("今晚")}.value(QueryService::todayPeriod(task,today));
            else if(groupRole==PlannedDate){const auto date=QueryService::plannedDate(task);current=date<today?tr("此前安排"):date==today?tr("今天"):date==today.addDays(1)?tr("明天"):date<=today.addDays(6)?date.toString(QStringLiteral("M月d日 dddd")):date.toString(QStringLiteral("yyyy年M月"));}
            else if(groupRole==CompletionMonth)current=QDateTime::fromString(task.completedAtUtc,Qt::ISODateWithMs).toLocalTime().date().toString(QStringLiteral("yyyy年M月"));
            else if(groupRole==DeletedAt)current=QDateTime::fromString(task.deletedAtUtc,Qt::ISODateWithMs).toLocalTime().date().toString(QStringLiteral("yyyy年M月d日"));
            else if(groupRole==StartOn)current=task.startOn?task.startOn->toString(Qt::ISODate):QString();
            else if(groupRole==HeadingTitle)current=task.headingTitle.isEmpty()?tr("未分组"):task.headingTitle;
            else if(groupRole==ContextGroup)current=task.areaName.isEmpty() && task.projectName.isEmpty()?tr("收集箱"):task.areaName+(task.areaName.isEmpty() || task.projectName.isEmpty()?QString():QStringLiteral(" / "))+task.projectName;
            else current.clear();
        }
        headers.insert(task.id,current);
    }
    const bool groupingChanged=m_groupRole!=groupRole || headers!=m_groupHeaders;m_groupRole=groupRole;m_groupHeaders=headers;
    const auto oldToday=m_today; m_today=today;
    QSet<QString> incomingIds;
    for(const auto& task:tasks)incomingIds.insert(task.id);
    for(int i=m_tasks.size()-1;i>=0;--i) {
        const auto id=m_tasks[i].id;
        if(!incomingIds.contains(id)) {
            beginRemoveRows({},i,i);m_tasks.removeAt(i);endRemoveRows();
        }
    }
    for(int i=0;i<tasks.size();++i) {
        int found=i;
        while(found<m_tasks.size() && m_tasks[found].id!=tasks[i].id) ++found;
        if(found==m_tasks.size()) { beginInsertRows({},i,i);m_tasks.insert(i,tasks[i]);endInsertRows(); }
        else {
            if(found!=i) { beginMoveRows({},found,found,{},i);m_tasks.move(found,i);endMoveRows(); }
            if(m_tasks[i]!=tasks[i] || oldToday!=today || groupingChanged) { m_tasks[i]=tasks[i];emit dataChanged(index(i),index(i)); }
        }
    }
    emit countChanged();
}

// Created without a DB connection; connection creation and destruction occur on the worker thread.
class TaskWorker final : public QObject {
public:
    SqliteDatabase database;
    std::unique_ptr<TaskRepository> repository;
    std::unique_ptr<TaskService> service;
    std::unique_ptr<CatalogRepository> catalog;
    std::unique_ptr<CatalogService> catalogService;
    std::unique_ptr<RecurrenceRepository> recurrence;
    std::unique_ptr<RecurrenceService> recurrenceService;
    std::optional<ImportPlan> pendingImport;
    SystemClock clock;
    Result<QList<Task>> load(const QString& path)
    {
        recurrenceService.reset();recurrence.reset();service.reset();repository.reset();catalogService.reset();catalog.reset();
        const auto opened=database.open(path);
        if(!opened.ok()) return {{},opened.error};
        repository=std::make_unique<TaskRepository>(database.connection());
        catalog=std::make_unique<CatalogRepository>(database.connection());
        catalogService=std::make_unique<CatalogService>(*catalog,clock);
        service=std::make_unique<TaskService>(*repository,clock,catalogService.get());
        recurrence=std::make_unique<RecurrenceRepository>(database.connection());
        recurrenceService=std::make_unique<RecurrenceService>(*recurrence,*repository,clock);
        const auto generated=recurrenceService->generate();if(!generated.ok())return {{},generated.error};
        return repository->all();
    }
};

TaskWorkspace::TaskWorkspace(QObject* parent) : QObject(parent),m_today(m_clock.localToday())
{
    m_worker=new TaskWorker;
    m_worker->moveToThread(&m_thread);
    connect(&m_thread,&QThread::finished,m_worker,&QObject::deleteLater);
    m_thread.start();
    m_searchTimer.setSingleShot(true);m_searchTimer.setInterval(250);
    connect(&m_searchTimer,&QTimer::timeout,this,&TaskWorkspace::refresh);
    auto checkDate=[this] {
        if(m_clock.localToday()!=m_today) { m_today=m_clock.localToday();refresh();emit stateChanged(); }
        const auto now=QDateTime::currentDateTime();
        const auto next=QDateTime(now.date().addDays(1),QTime(0,0),now.timeZone());
        m_midnightTimer.start(qMax<qint64>(1000,now.msecsTo(next)));
    };
    m_midnightTimer.setSingleShot(true);
    connect(&m_midnightTimer,&QTimer::timeout,this,checkDate);
    connect(&m_dateTimer,&QTimer::timeout,this,checkDate);
    m_dateTimer.start(60000);checkDate();
    m_maintenanceTimer.setInterval(30000);
    m_pendingMaintenance.setSingleShot(true);
    connect(&m_pendingMaintenance,&QTimer::timeout,this,&TaskWorkspace::maintain);
    connect(&m_maintenanceTimer,&QTimer::timeout,this,&TaskWorkspace::maintain);m_maintenanceTimer.start();
    m_undoTimer.setSingleShot(true);m_undoTimer.setInterval(8000);
    m_eventUndoTimer.setSingleShot(true);m_eventUndoTimer.setInterval(8000);
    connect(&m_eventUndoTimer,&QTimer::timeout,this,[this]{m_eventUndoId.clear();emit stateChanged();});
    connect(this,&TaskWorkspace::operationFinished,this,[this](const QString& operation,bool success){
        if(m_refreshPending && m_ready)m_pendingMaintenance.start(0);
        if(operation==QStringLiteral("eventDelete") && success){m_eventUndoId=m_eventDeleteCandidate;m_eventUndoVersion=m_eventDeleteVersion+1;m_eventUndoTimer.start();emit stateChanged();}
        if(success && (operation==QStringLiteral("eventRestore") || operation==QStringLiteral("applyImport"))){m_eventUndoId.clear();m_eventUndoTimer.stop();emit stateChanged();}
    });
    connect(&m_undoTimer,&QTimer::timeout,this,[this]{m_lastDeleted.clear();emit stateChanged();});
}
TaskWorkspace::~TaskWorkspace()
{
    // Drain accepted commands before stopping; callbacks are removed by QObject destruction.
    if(m_thread.isRunning()) QMetaObject::invokeMethod(m_worker,[]{},Qt::BlockingQueuedConnection);
    m_thread.quit();m_thread.wait();
}
void TaskWorkspace::start(const QString& path) {
    m_path=path;
    QSettings settings(QFileInfo(path).dir().filePath(QStringLiteral("ui.ini")),QSettings::IniFormat);
    m_sundayFirst=settings.value(QStringLiteral("calendar/sundayFirst"),false).toBool();
    m_themeMode=qBound(0,settings.value(QStringLiteral("appearance/themeMode"),0).toInt(),2);
    m_textScale=qBound(1.0,settings.value(QStringLiteral("appearance/textScale"),1.0).toDouble(),2.0);
    for(auto it=m_preferences.begin();it!=m_preferences.end();++it) {
        const auto stored=settings.value(QStringLiteral("preferences/")+it.key(),it.value());
        if(it.key()==QStringLiteral("backupDays")){const int days=stored.toInt();if(days==1 || days==7 || days==30)it.value()=days;}
        else if(it.key()==QStringLiteral("desktopCalendarOpacity")){const int opacity=stored.toInt();if(opacity>=50 && opacity<=100)it.value()=opacity;}
        else if(it.key()==QStringLiteral("calendarScale") || it.key()==QStringLiteral("desktopCalendarScale")){if(QStringList{"day","week","month","year"}.contains(stored.toString()))it.value()=stored;}
        else if(stored.metaType().id()==QMetaType::Bool)it.value()=stored;
        else if(stored.toString()==QStringLiteral("true") || stored.toString()==QStringLiteral("false"))it.value()=stored.toBool();
    }
    m_calendarScale=m_preferences.value(QStringLiteral("calendarScale")).toString();
    reload();
}
bool TaskWorkspace::begin(bool clearFeedback)
{
    if(m_busy) return false;
    m_backgroundMaintenance=!clearFeedback;m_busy=true;if(clearFeedback){m_error={};m_info.clear();m_pendingInfo.clear();}emit stateChanged();return true;
}
void TaskWorkspace::reload()
{
    if(m_path.isEmpty() || !begin()) return;
    QPointer<TaskWorkspace> self(this);auto* worker=m_worker;const auto path=m_path;
    QMetaObject::invokeMethod(worker,[self,worker,path]{
        const auto result=worker->load(path);
        const auto catalogs=result.ok()?worker->catalog->all():Result<QList<CatalogItem>>{{},result.error};
        const auto rules=result.ok()?worker->recurrence->all():Result<QList<RecurrenceRule>>{{},result.error};
        const auto events=result.ok()?EventRepository(worker->database.connection()).all():Result<QList<CalendarEvent>>{{},result.error};
        if(self) QMetaObject::invokeMethod(self.data(),[self,result,catalogs,rules,events]{
            if(!self)return;
            self->m_busy=false;self->m_ready=result.ok() && catalogs.ok() && rules.ok();self->m_error=result.ok()?(catalogs.ok()?rules.error:catalogs.error):result.error;
            if(catalogs.ok())self->m_catalog=catalogs.value;
            if(result.ok()) {self->m_cache=result.value;self->refresh();}
            if(rules.ok())self->m_rules=rules.value;
            if(events.ok())self->m_events=events.value;else {self->m_error=events.error;self->m_ready=false;}
            emit self->stateChanged();emit self->operationFinished(QStringLiteral("load"),self->m_ready);
            if(self->m_ready)self->maintain();
        },Qt::QueuedConnection);
    },Qt::QueuedConnection);
}
void TaskWorkspace::refresh()
{
    QHash<QString,QString> paths;const auto tree=categoryTree();for(int i=0;i<tree.size();++i)paths.insert(tree[i].toMap().value("id").toString(),QStringLiteral("%1").arg(i+1,8,10,QLatin1Char('0')));for(auto& task:m_cache)task.categoryOrderPath=task.projectId.isEmpty() && task.areaId.isEmpty()?QStringLiteral("00000000"):paths.value(task.projectId.isEmpty()?task.areaId:task.projectId,QStringLiteral("99999999"));
    QSet<QString> deleted;for(const auto& task:m_cache)if(!task.deletedAtUtc.isEmpty())deleted.insert(task.id);for(auto& task:m_cache)task.parentDeleted=deleted.contains(task.parentTaskId);
    QHash<QString,int> total;for(const auto& task:m_cache)if(!task.parentTaskId.isEmpty() && task.deletedAtUtc.isEmpty())++total[task.parentTaskId];for(auto& task:m_cache)task.totalSubtasks=total.value(task.id);
    QHash<QString,int> pending;for(const auto& task:m_cache)if(!task.parentTaskId.isEmpty() && task.deletedAtUtc.isEmpty() && task.status==QStringLiteral("OPEN"))++pending[task.parentTaskId];for(auto& task:m_cache)task.pendingSubtasks=pending.value(task.id);
    m_events.clear();for(const auto& task:m_cache)if(task.schedule && task.deletedAtUtc.isEmpty())m_events.append(EventService::projection(task));

    m_viewCounts.clear();for(const auto& code:QStringList{QStringLiteral("today"),QStringLiteral("scheduled"),QStringLiteral("all"),QStringLiteral("important"),QStringLiteral("completed"),QStringLiteral("inbox"),QStringLiteral("trash")}){int count=0;for(const auto& task:m_cache)if(QueryService::inView(task,code,m_today))++count;m_viewCounts.insert(code,count);}
    m_taskIndex.clear();for(const auto& task:m_cache)m_taskIndex.insert(task.id,task);
    const int groupRole=m_view==QStringLiteral("today") ? TaskListModel::Section : m_view==QStringLiteral("upcoming") ? TaskListModel::StartOn : m_view.startsWith(QStringLiteral("project:")) ? TaskListModel::HeadingTitle : m_view==QStringLiteral("scheduled") ? TaskListModel::PlannedDate : m_view==QStringLiteral("completed") ? TaskListModel::CompletionMonth : m_view==QStringLiteral("trash") ? TaskListModel::DeletedAt : (m_view==QStringLiteral("anytime") || m_view==QStringLiteral("all")) ? TaskListModel::ContextGroup : 0;
    m_tasks.replace(QueryService::select(m_cache,m_view,m_today,m_search,m_includeCompleted,m_tagFilter,m_upcomingDays,m_historyDays,m_projectFilter,m_view==QStringLiteral("trash") ? QSet<QString>{} : m_collapsedTasks,(smartView() || !m_search.isEmpty() || m_preferences.value(QStringLiteral("listShowSubtasks")).toBool()),m_view==QStringLiteral("trash") || !smartView()),m_today,groupRole);
    m_todayTasks.replace(QueryService::select(m_cache,QStringLiteral("today"),m_today,{},false,{},0,0,{}, {},true,false),m_today);
    m_deadlineTasks.replace(QueryService::select(m_cache,QStringLiteral("deadlines"),m_today,{},false,{},m_upcomingDays),m_today);
}
void TaskWorkspace::setTaskChildrenExpanded(const QString& id,bool expanded)
{
    if(expanded)m_collapsedTasks.remove(id);else m_collapsedTasks.insert(id);
    refresh();emit stateChanged();
}
bool TaskWorkspace::smartView() const
{return QStringList{QStringLiteral("today"),QStringLiteral("scheduled"),QStringLiteral("important"),QStringLiteral("completed"),QStringLiteral("cancelled"),QStringLiteral("trash"),QStringLiteral("logbook"),QStringLiteral("upcoming"),QStringLiteral("repeating")}.contains(m_view);}
QVariantMap TaskWorkspace::viewCounts() const
{return m_viewCounts;}

void TaskWorkspace::setView(const QString& view)
{
    if(!view.startsWith(QStringLiteral("project:")) && !view.startsWith(QStringLiteral("area:")) && !view.startsWith(QStringLiteral("tag:"))
       && !QStringList{QStringLiteral("all"),QStringLiteral("scheduled"),QStringLiteral("completed"),QStringLiteral("cancelled"),QStringLiteral("trash"),QStringLiteral("inbox"),QStringLiteral("today"),QStringLiteral("upcoming"),QStringLiteral("anytime"),QStringLiteral("someday"),QStringLiteral("important"),QStringLiteral("repeating"),QStringLiteral("logbook"),QStringLiteral("month"),QStringLiteral("week"),QStringLiteral("day"),QStringLiteral("year")}.contains(view))return;
    if(QStringList{"day","week","month","year"}.contains(view) && m_calendarScale!=view){if(!setPreference(QStringLiteral("calendarScale"),view))return;}
    m_view=view;
    if(calendarMode())m_calendarScale=view;else m_scope=view;
    refresh();emit stateChanged();
}
void TaskWorkspace::setScope(const QString& scope)
{
    const bool calendar=calendarMode();
    setView(scope);
    if(calendar && scope!=QStringLiteral("trash") && scope!=QStringLiteral("logbook") && scope!=QStringLiteral("completed") && scope!=QStringLiteral("cancelled"))setView(m_calendarScale);
}
void TaskWorkspace::setSearch(const QString& search)
{
    const auto next=search.trimmed();
    if(next==m_search)return;
    m_search=next;
    m_searchTimer.start();
    // Filtering stays debounced; permissions must update before the next input.
    emit stateChanged();
}
void TaskWorkspace::refreshClock(){m_today=m_clock.localToday();refresh();emit stateChanged();m_refreshPending=true;m_pendingMaintenance.start(0);}
QVariantMap TaskWorkspace::task(const QString& id) const
{
    const auto item=m_taskIndex.constFind(id);
    if(item!=m_taskIndex.cend() && item->deletedAtUtc.isEmpty())return TaskListModel::taskMap(item.value());
    return {};
}
bool TaskWorkspace::runTask(const QString& operation,std::function<Result<Task>(TaskWorker&)> command)
{
    if(!m_ready || !begin()) return false;
    QPointer<TaskWorkspace> self(this);auto* worker=m_worker;
    QMetaObject::invokeMethod(worker,[self,worker,operation,command=std::move(command)]{
        const auto result=command(*worker);
        const auto snapshot=result.ok()?worker->repository->all():Result<QList<Task>>{};
        if(self) QMetaObject::invokeMethod(self.data(),[self,operation,result,snapshot]{if(!self)return;if(result.ok() && !snapshot.ok()){self->finishTask(operation,Result<Task>{{},snapshot.error});return;}if(result.ok())self->m_cache=snapshot.value;self->finishTask(operation,result);},Qt::QueuedConnection);
    },Qt::QueuedConnection);
    return true;
}
void TaskWorkspace::finishTask(const QString& operation,const Result<Task>& result)
{
    m_busy=false;m_error=result.error;
    if(result.ok()) {
        m_info=m_pendingInfo;
        auto it=std::find_if(m_cache.begin(),m_cache.end(),[&](const Task& t){return t.id==result.value.id;});
        if(it==m_cache.end())m_cache.append(result.value);else *it=result.value;
        if(operation==QStringLiteral("delete")) {m_lastDeleted=result.value.id;m_undoTimer.start();}
        if(operation==QStringLiteral("restore")) {m_lastDeleted.clear();m_undoTimer.stop();}
        refresh();
    }
    emit stateChanged();emit operationFinished(operation,result.ok());
    if(result.ok() && operation==QStringLiteral("calendarCreate"))emit calendarTaskCreated(result.value.id);
    if(result.ok() && operation==QStringLiteral("createSubtask"))emit subtaskCreated(result.value.id);
    if(result.ok() && !result.value.recurrenceId.isEmpty())maintain();
}
void TaskWorkspace::requestTaskEditHistory(const QString& id)
{
    if(!m_ready)return;m_historyTaskId=id;m_historyLoading=true;m_taskEditHistory.clear();emit historyChanged();
    QPointer<TaskWorkspace> self(this);auto* worker=m_worker;
    QMetaObject::invokeMethod(worker,[self,worker,id]{const auto history=worker->repository->editHistory(id);if(self)QMetaObject::invokeMethod(self.data(),[self,id,history]{if(!self || self->m_historyTaskId!=id)return;self->m_historyLoading=false;if(history.ok())self->m_taskEditHistory=history.value;else{self->m_error=history.error;emit self->stateChanged();}emit self->historyChanged();},Qt::QueuedConnection);},Qt::QueuedConnection);
}
bool TaskWorkspace::createSubtask(const QString& title,const QString& parentId)
{return runTask(QStringLiteral("createSubtask"),[title,parentId](TaskWorker& w){return w.service->createSubtask(title,parentId);});}
QVariantList TaskWorkspace::subtaskChoices(const QString& parentId) const
{
    QList<Task> children;for(const auto& task:m_cache)if(task.parentTaskId==parentId && task.deletedAtUtc.isEmpty())children.append(task);
    std::stable_sort(children.begin(),children.end(),[](const Task& a,const Task& b){if(a.pinned!=b.pinned)return a.pinned;return a.sortOrder==b.sortOrder?a.id<b.id:a.sortOrder<b.sortOrder;});
    QVariantList result;for(const auto& child:children)result.append(TaskListModel::taskMap(child));return result;
}
QVariantList TaskWorkspace::parentTaskChoices(const QString& taskId,const QString& search) const
{
    // A former parent, including deleted descendants, must be detached/organized
    // before it can become a child. Existing deep relations remain readable.
    if(std::any_of(m_cache.cbegin(),m_cache.cend(),[&](const Task& task){return task.parentTaskId==taskId;}))return {};
    QVariantList result;
    for(const auto& task:m_cache) {
        if(task.id==taskId || !task.parentTaskId.isEmpty() || !task.deletedAtUtc.isEmpty() || task.status!=QStringLiteral("OPEN") || (!search.isEmpty() && !task.title.contains(search,Qt::CaseInsensitive)))continue;
        result.append(QVariantMap{{"id",task.id},{"title",task.title}});
        if(result.size()>=100)break;
    }
    return result;
}
bool TaskWorkspace::createTask(const QString& title)
{
    const auto view=calendarMode()?m_scope:m_view;
    return runTask(QStringLiteral("create"),[title,view](TaskWorker& w){return w.service->create(title,view);});
}
bool TaskWorkspace::saveTask(const QString& id,qint64 version,const QString& title,const QString& notes,
                             const QString& planning,const QString& start,const QString& deadline,bool focus)
{ return saveTaskWithDetails(id,version,title,notes,planning,start,deadline,focus,{}); }
bool TaskWorkspace::saveTaskWithDetails(const QString& id,qint64 version,const QString& title,const QString& notes,
                                      const QString& planning,const QString& start,const QString& deadline,bool focus,const QVariantMap& details)
{
    TaskPatch patch;
    if(!prepareTaskPatch(id,title,notes,planning,start,deadline,focus,details,patch))return false;
    const bool movedToToday=focus && !task(id).value("schedule").isValid() && !start.isEmpty() && QDate::fromString(start,Qt::ISODate)>m_today && planning!=QStringLiteral("SOMEDAY");
    const bool accepted=runTask(QStringLiteral("save"),[id,version,patch](TaskWorker& w){
        return w.service->edit(id,version,patch);
    });
    if(accepted && movedToToday)m_pendingInfo=tr("已加入今天，计划开始日期已调整为今天。");
    return accepted;
}
bool TaskWorkspace::prepareTaskPatch(const QString& id,const QString& title,const QString& notes,const QString& planning,
                                    const QString& start,const QString& deadline,bool focus,const QVariantMap& details,TaskPatch& patch)
{
    patch.title=FieldPatch<QString>::set(title);patch.notes=FieldPatch<QString>::set(notes);
    patch.planningState=FieldPatch<QString>::set(planning);patch.todayFocus=FieldPatch<bool>::set(focus);
    auto parse=[](const QString& text){
        const auto normalized=text.trimmed();
        if(normalized.isEmpty())return FieldPatch<QDate>::clear();
        auto date=QDate::fromString(normalized,Qt::ISODate);
        if(date.toString(Qt::ISODate)!=normalized)date={};
        return FieldPatch<QDate>::set(date);
    };
    patch.startOn=parse(start);patch.deadlineOn=parse(deadline);
    if(!appendDetailsPatch(id,details,patch))return false;
    return true;
}

bool TaskWorkspace::createTaskWithDetails(const QString& title,const QString& notes,const QString& start,const QString& end,const QVariantMap& details)
{
    TaskPatch patch;
    if(!prepareTaskPatch({},title,notes,QStringLiteral("ACTIVE"),start,end,false,details,patch))return false;
    return runTask(QStringLiteral("calendarCreate"),[title,patch](TaskWorker& w){return w.service->createWithDetails(title,patch);});
}

bool TaskWorkspace::changeState(const QString& id,qint64 version,const QString& action)
{ return runTask(action,[id,version,action](TaskWorker& w){return w.service->changeState(id,version,action);}); }
bool TaskWorkspace::moveTaskToView(const QString& id,qint64 version,const QString& view,const QString& start)
{
    const bool accepted=runTask(QStringLiteral("move"),[id,version,view,start](TaskWorker& w){return w.service->moveToView(id,version,view,QDate::fromString(start,Qt::ISODate));});
    const QHash<QString,QString> names{{QStringLiteral("inbox"),tr("收集箱")},{QStringLiteral("today"),tr("今天")},{QStringLiteral("upcoming"),tr("计划")},{QStringLiteral("scheduled"),tr("计划")},{QStringLiteral("important"),tr("特别关注")},{QStringLiteral("completed"),tr("完成")},{QStringLiteral("cancelled"),tr("已取消")},{QStringLiteral("logbook"),tr("完成")},{QStringLiteral("anytime"),tr("全部")},{QStringLiteral("someday"),tr("暂停安排")}};
    if(accepted && names.contains(view))m_pendingInfo=tr("已移到%1。").arg(names.value(view));
    return accepted;
}
void TaskWorkspace::setHistoryDays(int days)
{if(days!=0 && days!=7 && days!=30)return;m_historyDays=days;refresh();emit stateChanged();}
bool TaskWorkspace::setPreference(const QString& key,const QVariant& value)
{
    if(!m_preferences.contains(key))return false;
    if(key==QStringLiteral("backupDays")){const int days=value.toInt();if(days!=1 && days!=7 && days!=30)return false;}
    else if(key==QStringLiteral("desktopCalendarOpacity")){bool ok=false;const int opacity=value.toInt(&ok);if(!ok || opacity<50 || opacity>100)return false;}
    else if(key==QStringLiteral("calendarScale") || key==QStringLiteral("desktopCalendarScale")){if(!QStringList{"day","week","month","year"}.contains(value.toString()))return false;}
    else if(value.metaType().id()!=QMetaType::Bool)return false;
    QSettings settings(QFileInfo(m_path).dir().filePath(QStringLiteral("ui.ini")),QSettings::IniFormat);
    settings.setValue(QStringLiteral("preferences/")+key,value);
    const bool manualAccessibility=key==QStringLiteral("reduceMotion") || key==QStringLiteral("highContrast");
    if(manualAccessibility)settings.setValue(QStringLiteral("preferences/followSystemAccessibility"),false);
    settings.sync();
    if(settings.status()!=QSettings::NoError){settingsFailed();return false;}
    m_preferences[key]=value;if(manualAccessibility)m_preferences[QStringLiteral("followSystemAccessibility")]=false;if(key==QStringLiteral("listShowSubtasks"))refresh();emit stateChanged();return true;
}
QVariantMap TaskWorkspace::systemAccessibility() const
{
    bool contrast=false;const bool available=qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    if(available)contrast=QGuiApplication::styleHints()->accessibility()->contrastPreference()==Qt::ContrastPreference::HighContrast;
    const auto motion=systemReducedMotion();return {{"highContrast",contrast},{"reduceMotion",motion.value_or(false)},{"contrastAvailable",available},{"motionAvailable",motion.has_value()}};
}
QVariantMap TaskWorkspace::backupInfo() const
{
    const QDir directory(QFileInfo(m_path).dir().filePath(QStringLiteral("backups")));const auto files=directory.entryInfoList({QStringLiteral("*.sqlite")},QDir::Files,QDir::Time);
    return {{"folderUrl",QUrl::fromLocalFile(directory.absolutePath()).toString()},{"lastSuccess",files.isEmpty()?QString():files.first().lastModified().toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))},{"count",files.size()}};
}
QVariantMap TaskWorkspace::catalogImpact(const QString& kind,const QString& id) const
{
    int tasks=0,children=0;for(const auto& task:m_cache)if(kind==QStringLiteral("tag")?task.tagIds.contains(id):(task.projectId==id || task.areaId==id))++tasks;
    for(const auto& item:m_catalog)if(item.parentId==id)++children;return {{"tasks",tasks},{"children",children}};
}
int TaskWorkspace::batchImpact(const QVariantList& selection,const QString& action) const
{
    QSet<QString> ids;for(const auto& value:selection)ids.insert(value.toMap().value("taskId").toString());
    if(action==QStringLiteral("reopen")){
        QHash<QString,QSet<QString>> groups;
        for(const auto& value:selection){
            const auto root=m_taskIndex.constFind(value.toMap().value("taskId").toString());
            if(root!=m_taskIndex.cend() && !root->completionGroupId.isEmpty())groups[root->completionGroupId].insert(root->id);
        }
        for(const auto& task:m_cache){
            const auto group=groups.constFind(task.completionGroupId);
            if(group==groups.cend() || !task.deletedAtUtc.isEmpty() || task.status!=QStringLiteral("COMPLETED"))continue;
            QSet<QString> visited;auto parent=task.parentTaskId;
            while(!parent.isEmpty() && !visited.contains(parent)){
                if(group->contains(parent)){ids.insert(task.id);break;}
                visited.insert(parent);parent=m_taskIndex.value(parent).parentTaskId;
            }
        }
    }
    if(action==QStringLiteral("complete") || action==QStringLiteral("project")){bool changed=true;while(changed){changed=false;for(const auto& task:m_cache)if(task.deletedAtUtc.isEmpty() && ids.contains(task.parentTaskId) && !ids.contains(task.id)){ids.insert(task.id);changed=true;}}}
    return ids.size();
}
bool TaskWorkspace::batchTasks(const QVariantList& selection,const QString& action,const QString& value)
{
    QList<QPair<QString,qint64>> items;
    for(const auto& entry:selection){const auto map=entry.toMap();items.append({map.value(QStringLiteral("taskId")).toString(),map.value(QStringLiteral("rowVersion")).toLongLong()});}
    TaskPatch patch;
    if(action==QStringLiteral("project"))patch.projectId=FieldPatch<QString>::set(value);
    else if(action==QStringLiteral("start")) {
        const auto date=QDate::fromString(value,Qt::ISODate);
        if(!value.isEmpty() && (!date.isValid() || date.toString(Qt::ISODate)!=value)){m_error={ErrorCode::ValidationError,QStringLiteral("dateInvalid")};emit stateChanged();return false;}
        patch.startOn=value.isEmpty()?FieldPatch<QDate>::clear():FieldPatch<QDate>::set(date);
        if(date>m_today)patch.todayFocus=FieldPatch<bool>::set(false);
    } else if(action==QStringLiteral("tags"))patch.tagIds=FieldPatch<QStringList>::set(value.isEmpty()?QStringList{}:value.split(QLatin1Char(','),Qt::SkipEmptyParts));
    else if(action!=QStringLiteral("complete") && action!=QStringLiteral("reopen")){m_error={ErrorCode::ValidationError,QStringLiteral("actionInvalid")};emit stateChanged();return false;}
    return runCatalog([items,action,patch](TaskWorker& w){return w.service->batch(items,(action==QStringLiteral("complete") || action==QStringLiteral("reopen"))?action:QStringLiteral("edit"),patch);},QStringLiteral("batch"));
}
bool TaskWorkspace::undoDelete()
{
    for(const auto& task:m_cache) if(task.id==m_lastDeleted)return changeState(task.id,task.rowVersion,QStringLiteral("restore"));
    return false;
}
bool TaskWorkspace::createBackup()
{
    if(!m_ready || !begin())return false;
    QPointer<TaskWorkspace> self(this);auto* worker=m_worker;
    const auto directory=QFileInfo(m_path).dir().filePath(QStringLiteral("backups"));
    QMetaObject::invokeMethod(worker,[self,worker,directory]{
        const auto result=worker->database.createSnapshot(directory);
        if(self)QMetaObject::invokeMethod(self.data(),[self,result]{
            if(!self)return;
            self->m_busy=false;self->m_error=result.error;
            if(result.ok())self->m_info=tr("备份已创建：%1").arg(QDir::toNativeSeparators(result.value));
            emit self->stateChanged();emit self->operationFinished(QStringLiteral("backup"),result.ok());
        },Qt::QueuedConnection);
    },Qt::QueuedConnection);
    return true;
}
bool TaskWorkspace::verifyStorage()
{return runCatalog([](TaskWorker& w){return w.database.checkIntegrity();},QStringLiteral("integrity"));}
void TaskWorkspace::clearError(){m_error={};m_info.clear();emit stateChanged();}
QString TaskWorkspace::errorText() const
{
    if(m_error.code==ErrorCode::None)return {};
    const auto& key=m_error.key;
    if(key==QStringLiteral("titleInvalid"))return tr("标题不能为空，且不能超过512个Unicode字符。");
    if(key==QStringLiteral("dateInvalid"))return tr("日期无效。请使用 yyyy-MM-dd，或留空清除日期。");
    if(key==QStringLiteral("eventRange"))return tr("事件结束必须晚于开始。请检查日期、时间及有效时区。");
    if(key==QStringLiteral("eventTime"))return tr("时间或时区无效，或处于夏令时跳变的缺失/重复时段。请更换明确的当地时间（yyyy-MM-dd HH:mm）。");
    if(key==QStringLiteral("eventTaskMissing"))return tr("关联任务已删除或不可用。请重新选择任务后保存，原事件未修改。");
    if(key==QStringLiteral("eventInvalid"))return tr("事件或关联无效。请检查标题、日期及重复关联后重试。");
    if(key==QStringLiteral("eventConflict"))return tr("事件已被修改或删除，当前输入已保留。请记录修改内容，再按“重新载入”检查最新事件。");
    if(key==QStringLiteral("dateOrder"))return tr("结束日期不能早于开始日期。请调整日期后重试。");
    if(key==QStringLiteral("conflict"))return tr("任务已被修改。请重新加载后打开任务，再应用修改；当前输入已保留。");
    if(key==QStringLiteral("notFound"))return tr("任务已不存在或已删除。请重新加载列表。");
    if(key==QStringLiteral("schemaTooNew"))return tr("数据库由较新版本创建。请使用新版Daymark，当前数据库未覆盖。");
    if(key==QStringLiteral("unknownSchema"))return tr("数据库格式无法识别，未修改原数据。请使用单独数据目录或恢复备份。");
    if(key==QStringLiteral("backupFailed"))return tr("备份失败。请检查磁盘空间和数据目录写入权限后重试。");
    if(key==QStringLiteral("planningInvalid"))return tr("规划状态无效。请选择收集箱、可执行或以后再说。");
    if(key==QStringLiteral("futureStartRequired"))return tr("请选择今天之后的计划开始日。任务未移动。");
    if(key==QStringLiteral("destinationUnavailable"))return tr("所属分类已归档。请先恢复分类，或将任务移出后再安排。");
    if(key==QStringLiteral("taskParentCycle"))return tr("父任务不能是自己或自己的子任务。请选择其他任务。");
    if(key==QStringLiteral("taskParentDepth"))return tr("任务只支持父任务和子任务两层。请选择没有父任务的任务。");
    if(key==QStringLiteral("taskHasChildren"))return tr("此任务已有下级，不能再设为子任务。请先将下级移出。");
    if(key==QStringLiteral("taskParentInvalid"))return tr("父任务不存在或已删除，请重新选择。");
    if(key==QStringLiteral("categoryCycle"))return tr("分类不能属于自己。请选择其他一级分类。");
    if(key==QStringLiteral("categoryDepth"))return tr("只支持两级分类。请选择一级分类作为上级。");
    if(key==QStringLiteral("categoryHasChildren"))return tr("此分类仍有二级分类。请先将它们移出，再调整此分类的上级。");
    if(key==QStringLiteral("ownershipInvalid"))return tr("归属无效。请确认分类仍存在，且分组属于所选分类。");
    if(key==QStringLiteral("tagConflict"))return tr("标签名称已存在（不区分大小写），或标签无法写入。请修改名称后重试。");
    if(key==QStringLiteral("eventSeriesInvalid"))return tr("请检查循环间隔、结束日期与提醒设置；循环事件时长不得超过366天。");
    if(key==QStringLiteral("reminderInvalid"))return tr("提醒必须是有效的将来时间。请重新选择时间，或清除提醒。");
    if(key==QStringLiteral("integrityFailed"))return tr("本地数据完整性检查失败。请停止编辑并保留当前数据，使用已验证的备份在独立目录恢复。");
    if(key==QStringLiteral("calendarAmbiguous"))return tr("同日标记包含开始和结束。请明确选择要移动的日期字段。");
    if(key==QStringLiteral("recurrenceInvalid"))return tr("重复规则无效。请检查日期、周期和星期选择。");
    if(key==QStringLiteral("recurrenceModeImmutable"))return tr("请停止旧规则后建立另一周期类型，以保留现有实例的时间语义。");
    if(key==QStringLiteral("recurrenceOpenInstance"))return tr("完成后重复规则已有一个未完成实例。请先处理该实例再恢复历史任务。");
    if(key==QStringLiteral("importInvalid"))return tr("导入文件无效或与现有数据冲突：请检查版本、重复ID、日期和外键。数据库未修改。");
    if(key==QStringLiteral("transferFailed"))return tr("文件读写失败。请检查文件路径、磁盘空间和权限，当前数据已保留。");
    if(key==QStringLiteral("settingsWrite"))return tr("窗口或外观设置无法保存。请检查数据目录权限和磁盘空间后重新设置。");
    return tr("本地数据库操作失败。输入已保留；请检查磁盘空间、目录权限后重试或重新加载。");
}

QVariantList TaskWorkspace::catalog() const
{
    QVariantList result;
    for(const auto& item:m_catalog)result.append(QVariantMap{{"id",item.id},{"kind",item.kind},{"title",item.title},
        {"parentId",item.parentId},{"planningState",item.planningState},{"status",item.status},{"rowVersion",item.rowVersion},
        {"description",item.description},{"colorKey",item.colorKey},{"startOn",item.startOn?item.startOn->toString(Qt::ISODate):QString()},
        {"deadlineOn",item.deadlineOn?item.deadlineOn->toString(Qt::ISODate):QString()},{"archived",item.archived},
        {"hidden",item.archived || std::any_of(m_catalog.begin(),m_catalog.end(),[&](const auto& area){return (area.kind==QStringLiteral("area") || area.kind==QStringLiteral("project")) && area.id==item.parentId && (area.archived || area.status!=QStringLiteral("OPEN"));})}});
    return result;
}
QVariantList TaskWorkspace::categoryTree() const
{
    const auto entries=catalog();QHash<QString,QVariantList> children;QVariantList roots,result;
    for(const auto& value:entries){auto item=value.toMap();const auto kind=item.value("kind").toString();if(kind!=QStringLiteral("project") && kind!=QStringLiteral("area"))continue;const bool archived=item.value("archived").toBool() || item.value("status")!=QStringLiteral("OPEN") || item.value("planningState")==QStringLiteral("SOMEDAY");item.insert("archived",archived);item.insert("hidden",item.value("hidden").toBool() || archived);
        const auto parent=item.value("parentId").toString();if(parent.isEmpty())roots.append(item);else children[parent].append(item);}
    for(const auto& value:roots){auto root=value.toMap();const auto nested=children.value(root.value("id").toString());root.insert("depth",0);root.insert("path",root.value("title"));root.insert("parentTitle",QString());root.insert("childCount",nested.size());result.append(root);
        for(const auto& child:nested){auto item=child.toMap();item.insert("depth",1);item.insert("hidden",item.value("hidden").toBool() || root.value("archived").toBool());item.insert("parentTitle",root.value("title"));item.insert("path",root.value("title").toString()+QStringLiteral(" / ")+item.value("title").toString());item.insert("childCount",0);result.append(item);}}
    return result;
}
QVariantList TaskWorkspace::navigationCatalog() const
{
    QVariantList result;
    for(const auto& value:categoryTree()){const auto item=value.toMap();if(!item.value("hidden").toBool() && item.value("status")==QStringLiteral("OPEN"))result.append(item);}
    return result;
}
QVariantList TaskWorkspace::catalogOf(const QString& kind,const QString& parent) const
{
    QVariantList output;
    for(const auto& value:catalog()){const auto item=value.toMap();if(item.value(QStringLiteral("kind")).toString()==kind
        && (parent.isNull() || parent.isEmpty() || item.value(QStringLiteral("parentId")).toString()==parent))output.append(value);}
    return output;
}
bool TaskWorkspace::runCatalog(std::function<Result<void>(TaskWorker&)> command,const QString& operation,std::function<void()> completed)
{
    if(!m_ready || !begin(operation!=QStringLiteral("maintenance") && operation!=QStringLiteral("calendarRange")))return false;
    QPointer<TaskWorkspace> self(this);auto* worker=m_worker;
    QMetaObject::invokeMethod(worker,[self,worker,operation,command=std::move(command),completed=std::move(completed)]{
        const auto result=command(*worker);
        const auto generated=result.ok() && operation!=QStringLiteral("batch") && operation!=QStringLiteral("integrity")?worker->recurrenceService->generate():Result<int>{{},result.error};
        const auto tasks=worker->repository->all();const auto catalog=worker->catalog->all();const auto rules=worker->recurrence->all();
        const auto events=EventRepository(worker->database.connection()).all();const auto history=worker->repository->reminderHistory();
        if(self)QMetaObject::invokeMethod(self.data(),[self,result,tasks,catalog,rules,events,history,generated,operation,completed]{
            if(!self)return;
            self->m_busy=false;if((operation!=QStringLiteral("maintenance") && operation!=QStringLiteral("calendarRange")) || !result.ok())self->m_error=result.error;
            if(tasks.ok())self->m_cache=tasks.value;else self->m_error=tasks.error;
            if(catalog.ok())self->m_catalog=catalog.value;else self->m_error=catalog.error;
            if(rules.ok())self->m_rules=rules.value;else self->m_error=rules.error;
            if(events.ok())self->m_events=events.value;else self->m_error=events.error;
            if(history.ok())self->m_taskNotifications=history.value;else self->m_error=history.error;
            if(!generated.ok())self->m_error=generated.error;
            self->refresh();emit self->stateChanged();const bool success=result.ok() && tasks.ok() && catalog.ok() && rules.ok() && events.ok() && history.ok() && generated.ok();if(success && completed)completed();emit self->operationFinished(operation,success);
            if(operation==QStringLiteral("batch") && result.ok())self->maintain();
        },Qt::QueuedConnection);
    },Qt::QueuedConnection);return true;
}
bool TaskWorkspace::canMoveCatalog(const QString& kind,const QString& id,int direction) const
{
    if(!m_ready || (direction!=-1 && direction!=1))return false;
    const auto current=std::find_if(m_catalog.cbegin(),m_catalog.cend(),[&](const auto& item){return item.kind==kind && item.id==id;});if(current==m_catalog.cend())return false;
    int count=0,index=-1;for(const auto& item:m_catalog)if(item.kind==kind && item.parentId==current->parentId){if(item.id==id)index=count;++count;}
    return index>=0 && index+direction>=0 && index+direction<count;
}
bool TaskWorkspace::dropCategory(const QString& id,qint64 version,const QString& targetId,qint64 targetVersion,const QString& placement)
{
    return runCatalog([id,version,targetId,targetVersion,placement](TaskWorker& worker){return worker.catalogService->drop(id,version,targetId,targetVersion,placement);},QStringLiteral("catalog"),[this,id]{emit categoryRelocated(id);});
}
bool TaskWorkspace::reparentCategory(const QString& id,qint64 version,const QString& parentId)
{
    return runCatalog([id,version,parentId](TaskWorker& worker){
        auto item=worker.catalog->find(QStringLiteral("project"),id);if(!item.ok())return Result<void>{item.error};
        if(item.value.rowVersion!=version)return Result<void>::failure(ErrorCode::Conflict,QStringLiteral("conflict"));
        item.value.parentId=parentId;return Result<void>{worker.catalogService->save(item.value).error};
    },QStringLiteral("catalog"),[this,id]{emit categoryRelocated(id);});
}
bool TaskWorkspace::saveCatalog(const QVariantMap& value)
{
    CatalogItem item;item.id=value.value(QStringLiteral("id")).toString();item.kind=value.value(QStringLiteral("kind")).toString();
    item.title=value.value(QStringLiteral("title")).toString();item.parentId=value.value(QStringLiteral("parentId")).toString();
    item.planningState=value.value(QStringLiteral("planningState"),QStringLiteral("ACTIVE")).toString();
    item.status=value.value(QStringLiteral("status"),QStringLiteral("OPEN")).toString();item.rowVersion=value.value(QStringLiteral("rowVersion"),1).toLongLong();
    item.description=value.value(QStringLiteral("description")).toString();item.colorKey=value.value(QStringLiteral("colorKey"),QStringLiteral("neutral")).toString();
    item.archived=value.value(QStringLiteral("archived"),false).toBool();
    if(!value.value(QStringLiteral("startOn")).toString().trimmed().isEmpty())item.startOn=QDate::fromString(value.value(QStringLiteral("startOn")).toString().trimmed(),Qt::ISODate);
    if(!value.value(QStringLiteral("deadlineOn")).toString().trimmed().isEmpty())item.deadlineOn=QDate::fromString(value.value(QStringLiteral("deadlineOn")).toString().trimmed(),Qt::ISODate);
    auto saved=std::make_shared<CatalogItem>();
    return runCatalog([item,saved](TaskWorker& w){const auto result=w.catalogService->save(item);if(result.ok())*saved=result.value;return Result<void>{result.error};},QStringLiteral("catalog"),[this,saved]{emit catalogSaved(saved->id,saved->kind);});
}
bool TaskWorkspace::deleteCatalog(const QString& kind,const QString& id,qint64 version)
{return runCatalog([kind,id,version](TaskWorker& w){return w.catalogService->remove(kind,id,version);});}
bool TaskWorkspace::moveCatalog(const QString& kind,const QString& id,qint64 version,int direction)
{return runCatalog([kind,id,version,direction](TaskWorker& w){return w.catalog->move(kind,id,version,direction,w.clock.nowUtc().toUTC().toString(Qt::ISODateWithMs));});}
bool TaskWorkspace::saveDetails(const QString& id,qint64 version,const QVariantMap& details)
{
    TaskPatch patch;
    if(!appendDetailsPatch(id,details,patch))return false;
    return runTask(QStringLiteral("details"),[id,version,patch](TaskWorker& w){return w.service->edit(id,version,patch);});
}
bool TaskWorkspace::appendDetailsPatch(const QString& id,const QVariantMap& details,TaskPatch& patch)
{
    if(details.contains(QStringLiteral("schedule"))){
        const auto json=details.value(QStringLiteral("schedule")).toMap();
        if(json.isEmpty())patch.schedule=FieldPatch<TaskSchedule>::clear();
        else {TaskSchedule schedule;schedule.allDay=json.value(QStringLiteral("allDay"),true).toBool();schedule.alertMinutes=json.value(QStringLiteral("alertMinutes"),-1).toInt();
            if(schedule.allDay){schedule.startOn=QDate::fromString(json.value(QStringLiteral("startDate")).toString(),Qt::ISODate);schedule.endOn=QDate::fromString(json.value(QStringLiteral("endDate")).toString(),Qt::ISODate).addDays(1);}
            else {schedule.timeZone=json.value(QStringLiteral("zone"),systemTimeZone()).toString();const auto start=EventService::localInstant(json.value(QStringLiteral("startDate")).toString()+QLatin1Char(' ')+json.value(QStringLiteral("startTime")).toString(),schedule.timeZone),end=EventService::localInstant(json.value(QStringLiteral("endDate")).toString()+QLatin1Char(' ')+json.value(QStringLiteral("endTime")).toString(),schedule.timeZone);if(!start.ok() || !end.ok()){m_error={ErrorCode::ValidationError,QStringLiteral("eventTime")};emit stateChanged();return false;}schedule.startUtc=start.value;schedule.endUtc=end.value;}
            patch.schedule=FieldPatch<TaskSchedule>::set(schedule);
        }
    }
    if(details.contains(QStringLiteral("repeatVersion")))patch.repeatVersion=FieldPatch<qint64>::set(details.value(QStringLiteral("repeatVersion")).toLongLong());
    if(details.contains(QStringLiteral("repeat"))){const auto json=details.value(QStringLiteral("repeat")).toMap();if(json.isEmpty())patch.repeat=FieldPatch<RecurrenceRule>::clear();else {RecurrenceRule rule;rule.frequency=json.value(QStringLiteral("frequency")).toString();rule.interval=json.value(QStringLiteral("interval"),1).toInt();rule.weekdays=json.value(QStringLiteral("weekdays"),0).toInt();rule.monthDay=json.value(QStringLiteral("monthDay"),1).toInt();rule.yearMonth=json.value(QStringLiteral("yearMonth"),1).toInt();rule.rowVersion=json.value(QStringLiteral("rowVersion"),1).toLongLong();rule.end=QDate::fromString(json.value(QStringLiteral("end")).toString(),Qt::ISODate);if(!json.value(QStringLiteral("end")).toString().isEmpty() && !rule.end.isValid()){m_error={ErrorCode::ValidationError,QStringLiteral("dateInvalid")};emit stateChanged();return false;}
        const auto original=task(id);const auto start=patch.startOn.kind==FieldPatch<QDate>::Kind::Set?patch.startOn.value:QDate::fromString(original.value(QStringLiteral("startOn")).toString(),Qt::ISODate);const auto anchor=start.isValid()?start:m_today;
        if(!original.value(QStringLiteral("repeat")).toMap().value(QStringLiteral("active")).toBool()){rule.monthDay=anchor.day();rule.yearMonth=anchor.month();if(!rule.weekdays)rule.weekdays=1<<(anchor.dayOfWeek()-1);}
        patch.repeat=FieldPatch<RecurrenceRule>::set(rule);
    }}
    auto stringField=[&](const char* key,FieldPatch<QString>& field){if(details.contains(QLatin1String(key)))field=FieldPatch<QString>::set(details.value(QLatin1String(key)).toString());};
    stringField("reminderRepeat",patch.reminderRepeat);stringField("reminderZone",patch.reminderZone);
    if(details.contains(QStringLiteral("reminderEvery")))patch.reminderEvery=FieldPatch<int>::set(details.value(QStringLiteral("reminderEvery")).toInt());
    stringField("parentTaskId",patch.parentTaskId);
    if(details.contains(QStringLiteral("pinned")))patch.pinned=FieldPatch<bool>::set(details.value(QStringLiteral("pinned")).toBool());
    if(details.contains(QStringLiteral("important")))patch.important=FieldPatch<bool>::set(details.value(QStringLiteral("important")).toBool());
    stringField("projectId",patch.projectId);stringField("areaId",patch.areaId);stringField("headingId",patch.headingId);stringField("remindAtUtc",patch.remindAtUtc);
    if(details.contains(QStringLiteral("remindAtLocal")) && details.value(QStringLiteral("remindAtLocal")).toString().trimmed()!=localReminder(task(id).value(QStringLiteral("remindAtUtc")).toString())) {
        const auto input=details.value(QStringLiteral("remindAtLocal")).toString().trimmed();
        if(input.isEmpty())patch.remindAtUtc=FieldPatch<QString>::clear();
        else {
            const auto date=QDate::fromString(input.left(10),Qt::ISODate);
            const auto time=QTime::fromString(input.mid(11),QStringLiteral("HH:mm"));
            const QDateTime local(date,time,QTimeZone::systemTimeZone(),QDateTime::TransitionResolution::Reject);
            if(!local.isValid() || local.toString(QStringLiteral("yyyy-MM-dd HH:mm"))!=input) {
                m_error={ErrorCode::ValidationError,QStringLiteral("reminderInvalid")};emit stateChanged();return false;
            }
            patch.remindAtUtc=FieldPatch<QString>::set(local.toUTC().toString(Qt::ISODateWithMs));
            patch.reminderZone=FieldPatch<QString>::set(QString::fromUtf8(QTimeZone::systemTimeZoneId()));
        }
    }
    if(details.contains(QStringLiteral("evening")))patch.evening=FieldPatch<bool>::set(details.value(QStringLiteral("evening")).toBool());
    if(details.contains(QStringLiteral("tagIds")))patch.tagIds=FieldPatch<QStringList>::set(details.value(QStringLiteral("tagIds")).toStringList());
    return true;
}

bool TaskWorkspace::moveTaskTime(const QString& id,qint64 version,const QString& date,int minute,const QString& edge)
{
    if(!m_taskIndex.contains(id))return false;const auto before=m_taskIndex.value(id);const auto target=QDate::fromString(date,Qt::ISODate);const auto zone=QTimeZone::systemTimeZone();
    return runTask(QStringLiteral("timeMove"),[before,version,target,minute,edge,zone](TaskWorker& w){return CalendarService::moveTime(*w.service,before,version,target,minute,edge,zone);});
}
bool TaskWorkspace::moveTaskDate(const QString& id,qint64 version,const QString& from,const QString& date,const QString& edge)
{
    if(!m_taskIndex.contains(id))return false;const auto before=m_taskIndex.value(id);const auto origin=QDate::fromString(from,Qt::ISODate),target=QDate::fromString(date,Qt::ISODate);const auto zone=QTimeZone::systemTimeZone();
    return runTask(QStringLiteral("calendarMove"),[before,version,origin,target,edge,zone](TaskWorker& w){return CalendarService::moveDate(*w.service,before,version,origin,target,edge,zone);});
}
bool TaskWorkspace::saveEvent(const QVariantMap& value) {
    return runCatalog([value](TaskWorker& w){
        CalendarEvent e;e.repeatFrequency=value.value(QStringLiteral("repeatFrequency"),QStringLiteral("NONE")).toString();e.repeatInterval=value.value(QStringLiteral("repeatInterval"),1).toInt();e.alertMinutes=value.value(QStringLiteral("alertMinutes"),-1).toInt();const auto until=value.value(QStringLiteral("repeatUntil")).toString();e.repeatUntil=QDate::fromString(until,Qt::ISODate);if(!until.isEmpty() && !e.repeatUntil.isValid())return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("eventSeriesInvalid"));e.id=value.value(QStringLiteral("eventId")).toString();e.rowVersion=value.value(QStringLiteral("rowVersion"),1).toLongLong();e.title=value.value(QStringLiteral("title")).toString();e.notes=value.value(QStringLiteral("notes")).toString();e.allDay=value.value(QStringLiteral("allDay"),true).toBool();
        if(e.allDay){e.startOn=QDate::fromString(value.value(QStringLiteral("startDate")).toString(),Qt::ISODate);e.endOn=QDate::fromString(value.value(QStringLiteral("endDate")).toString(),Qt::ISODate).addDays(1);}
        else {
            e.timeZone=value.value(QStringLiteral("zone")).toString();
            const auto start=EventService::localInstant(value.value(QStringLiteral("startDate")).toString()+QLatin1Char(' ')+value.value(QStringLiteral("startTime")).toString(),e.timeZone);
            const auto end=EventService::localInstant(value.value(QStringLiteral("endDate")).toString()+QLatin1Char(' ')+value.value(QStringLiteral("endTime")).toString(),e.timeZone);
            if(!start.ok() || !end.ok())return Result<void>{!start.ok()?start.error:end.error};
            e.startUtc=start.value;e.endUtc=end.value;
        }
        for(const auto& item:value.value(QStringLiteral("links")).toList()){const auto link=item.toMap();e.links.append({link.value(QStringLiteral("taskId")).toString(),link.value(QStringLiteral("role"),QStringLiteral("preparation")).toString()});}
        EventRepository repo(w.database.connection());return EventService(repo,w.clock).save(e);
    },QStringLiteral("eventSave"));
}
bool TaskWorkspace::deleteEvent(const QString& id,qint64 version) {
    if(!m_ready || m_busy)return false;
    m_eventDeleteCandidate=id;m_eventDeleteVersion=version;
    return runCatalog([id,version](TaskWorker& w){EventRepository repo(w.database.connection());return EventService(repo,w.clock).setDeleted(id,version,true);},QStringLiteral("eventDelete"));
}
bool TaskWorkspace::undoEventDelete() {
    if(m_eventUndoId.isEmpty())return false;
    const auto id=m_eventUndoId;const auto version=m_eventUndoVersion;
    return runCatalog([id,version](TaskWorker& w){EventRepository repo(w.database.connection());return EventService(repo,w.clock).setDeleted(id,version,false);},QStringLiteral("eventRestore"));
}
void TaskWorkspace::stepCalendar(int offset)
{
    m_calendarDate=QDate::fromString(shiftCalendarScale(calendarDate(),offset,m_view),Qt::ISODate);
    emit stateChanged();
}
void TaskWorkspace::setSundayFirst(bool enabled)
{
    if(m_sundayFirst==enabled)return;
    QSettings settings(QFileInfo(m_path).dir().filePath(QStringLiteral("ui.ini")),QSettings::IniFormat);
    settings.setValue(QStringLiteral("calendar/sundayFirst"),enabled);settings.sync();
    if(settings.status()!=QSettings::NoError){m_error={ErrorCode::PersistenceError,QStringLiteral("settingsWrite")};emit stateChanged();return;}
    m_sundayFirst=enabled;emit stateChanged();
}
bool TaskWorkspace::requestCalendarRange(const QString& anchor)
{
    const auto date=QDate::fromString(anchor,Qt::ISODate);if(!date.isValid() || !m_ready || m_busy)return false;
    const auto first=QDate(date.year(),date.month(),1).addDays(-7),last=QDate(date.year(),date.month(),1).addMonths(1).addDays(7);
    return runCatalog([first,last](TaskWorker& w){const auto generated=w.recurrenceService->generate(first,last);return Result<void>{generated.error};},QStringLiteral("calendarRange"));
}
bool TaskWorkspace::requestCalendarSpan(const QString& from,const QString& through)
{
    const auto first=QDate::fromString(from,Qt::ISODate),last=QDate::fromString(through,Qt::ISODate);
    if(!first.isValid() || !last.isValid() || last<first || first.daysTo(last)>70 || !m_ready || m_busy)return false;
    return runCatalog([first,last](TaskWorker& w){const auto generated=w.recurrenceService->generate(first.addDays(-7),last.addDays(7));return Result<void>{generated.error};},QStringLiteral("calendarRange"));
}
bool TaskWorkspace::createForTime(const QString& title,const QString& date,int minute,int duration,bool global)
{
    const auto target=QDate::fromString(date,Qt::ISODate);const auto zone=QTimeZone::systemTimeZone();const auto from=target.startOfDay(zone),until=target.addDays(1).startOfDay(zone);
    if(!target.isValid() || minute<0 || minute>=from.secsTo(until)/60 || duration<1 || duration>7*25*60){m_error={ErrorCode::ValidationError,QStringLiteral("dateInvalid")};emit stateChanged();return false;}
    TaskSchedule schedule;schedule.allDay=false;schedule.timeZone=QString::fromUtf8(zone.id());schedule.startUtc=from.addSecs(minute*60).toUTC();schedule.endUtc=schedule.startUtc.addSecs(duration*60);const auto scope=global?QStringLiteral("inbox"):m_scope;
    return runTask(QStringLiteral("calendarCreate"),[title,target,scope,schedule](TaskWorker& w){return w.service->create(title,scope,target,schedule);});
}
bool TaskWorkspace::createForDate(const QString& title,const QString& date,bool global)
{
    const auto scope=global?QStringLiteral("inbox"):m_scope;const auto target=QDate::fromString(date,Qt::ISODate);
    if(!target.isValid() || target.toString(Qt::ISODate)!=date){m_error={ErrorCode::ValidationError,QStringLiteral("dateInvalid")};emit stateChanged();return false;}
    return runTask(QStringLiteral("calendarCreate"),[title,target,scope](TaskWorker& w){return w.service->create(title,scope,target);});
}
bool TaskWorkspace::moveCalendar(const QString& id,qint64 version,const QString& kind,const QString& date)
{
    return runTask(QStringLiteral("calendarMove"),[id,version,kind,date](TaskWorker& w){return CalendarService::move(*w.service,id,version,kind,QDate::fromString(date,Qt::ISODate));});
}
bool TaskWorkspace::saveRecurrence(const QVariantMap& value)
{
    RecurrenceRule r;r.id=value.value(QStringLiteral("id")).toString();r.title=value.value(QStringLiteral("title")).toString();r.notes=value.value(QStringLiteral("notes")).toString();
    r.frequency=value.value(QStringLiteral("frequency"),QStringLiteral("DAILY")).toString();r.anchor=QDate::fromString(value.value(QStringLiteral("anchor")).toString(),Qt::ISODate);
    r.end=QDate::fromString(value.value(QStringLiteral("end")).toString(),Qt::ISODate);
    if(!value.value(QStringLiteral("end")).toString().isEmpty() && !r.end.isValid()) {m_error={ErrorCode::ValidationError,QStringLiteral("dateInvalid")};emit stateChanged();return false;}
    r.interval=value.value(QStringLiteral("interval"),1).toInt();r.weekdays=value.value(QStringLiteral("weekdays"),0).toInt();r.monthDay=value.value(QStringLiteral("monthDay"),1).toInt();r.yearMonth=value.value(QStringLiteral("yearMonth"),1).toInt();r.active=value.value(QStringLiteral("active"),true).toBool();r.rowVersion=value.value(QStringLiteral("rowVersion"),1).toLongLong();
    return runCatalog([r](TaskWorker& w){const auto result=w.recurrenceService->save(r);return Result<void>{result.error};},QStringLiteral("recurrence"));
}
bool TaskWorkspace::stopRecurrence(const QString& id,qint64 version,bool removeFuture)
{return runCatalog([id,version,removeFuture](TaskWorker& w){return w.recurrenceService->stop(id,version,removeFuture);},QStringLiteral("recurrence"));}
void TaskWorkspace::setInteractionActive(bool active)
{
    if(m_interactionActive==active)return;
    m_interactionActive=active;
    emit interactionActiveChanged();
    if(!active && m_refreshPending)m_pendingMaintenance.start(0);
}
void TaskWorkspace::maintain()
{
    if(m_interactionActive){m_refreshPending=true;return;}
    if(!m_ready || m_busy)return;
    m_refreshPending=false;
    QPointer<TaskWorkspace> self(this);
    runCatalog([self](TaskWorker& w){
        const auto due=w.repository->claimDueReminders(w.clock.nowUtc().toUTC().toString(Qt::ISODateWithMs));
        if(!due.ok())return Result<void>{due.error};
        if(!due.value.isEmpty() && self) {
            QStringList titles;for(int i=0;i<qMin(10,int(due.value.size()));++i)titles.append(due.value[i].title);
            const auto count=due.value.size();const auto message=titles.join(QLatin1Char('\n'));
            QMetaObject::invokeMethod(self.data(),[self,count,message]{if(self && self->m_preferences.value(QStringLiteral("systemReminders")).toBool())emit self->reminderReady(tr("%1 项任务到达提醒时间").arg(count),message);},Qt::QueuedConnection);
        }
        return Result<void>{};
    },QStringLiteral("maintenance"));
}
bool TaskWorkspace::runTransfer(const QString& operation,std::function<Result<QString>(TaskWorker&)> command)
{
    if(!m_ready || !begin())return false;
    QPointer<TaskWorkspace> self(this);auto* worker=m_worker;
    QMetaObject::invokeMethod(worker,[self,worker,operation,command=std::move(command)]{
        const auto result=command(*worker);
        const auto tasks=worker->repository->all();const auto catalog=worker->catalog->all();const auto rules=worker->recurrence->all();
        const auto events=EventRepository(worker->database.connection()).all();
        if(self)QMetaObject::invokeMethod(self.data(),[self,result,tasks,catalog,rules,events,operation]{
            if(!self)return;
            self->m_busy=false;self->m_error=result.error;
            if(tasks.ok())self->m_cache=tasks.value;else self->m_error=tasks.error;
            if(catalog.ok())self->m_catalog=catalog.value;else self->m_error=catalog.error;
            if(rules.ok())self->m_rules=rules.value;else self->m_error=rules.error;
            if(events.ok())self->m_events=events.value;else self->m_error=events.error;
            if(result.ok()) {
                self->m_info=result.value;
                if(operation==QStringLiteral("previewImport"))self->m_importSummary=result.value;
                if(operation==QStringLiteral("applyImport")){
                    self->m_importSummary.clear();self->m_lastDeleted.clear();
                    QSettings settings(QFileInfo(self->m_path).dir().filePath(QStringLiteral("ui.ini")),QSettings::IniFormat);
                    self->m_sundayFirst=settings.value(QStringLiteral("calendar/sundayFirst"),false).toBool();
                    self->m_themeMode=qBound(0,settings.value(QStringLiteral("appearance/themeMode"),0).toInt(),2);
                    self->m_textScale=qBound(1.0,settings.value(QStringLiteral("appearance/textScale"),1.0).toDouble(),2.0);
                }
            }
            self->refresh();emit self->stateChanged();emit self->operationFinished(operation,result.ok() && tasks.ok() && catalog.ok() && rules.ok() && events.ok());
        },Qt::QueuedConnection);
    },Qt::QueuedConnection);return true;
}
bool TaskWorkspace::exportData(const QString& input,bool csv)
{
    const QUrl url(input);const auto path=url.isLocalFile()?url.toLocalFile():input;
    return runTransfer(QStringLiteral("export"),[path,csv](TaskWorker& w){DataTransfer transfer(w.database);const auto result=csv?transfer.exportCsv(path):transfer.exportJson(path);return Result<QString>{result.ok()?tr("已导出：%1").arg(QDir::toNativeSeparators(path)):QString(),result.error};});
}
bool TaskWorkspace::previewImport(const QString& input,bool snapshot)
{
    const QUrl url(input);const auto path=url.isLocalFile()?url.toLocalFile():input;
    if(m_busy)return false;
    m_importSummary.clear();
    return runTransfer(QStringLiteral("previewImport"),[path,snapshot](TaskWorker& w){
        w.pendingImport.reset();const auto plan=DataTransfer::preview(path,snapshot);if(!plan.ok())return Result<QString>{{},plan.error};
        w.pendingImport=plan.value;return Result<QString>{tr("校验通过：%1 行数据。预览保留在内存，当前数据库尚未修改。").arg(plan.value.rows),{}};
    });
}
bool TaskWorkspace::applyImport(bool replace)
{
    return runTransfer(QStringLiteral("applyImport"),[replace](TaskWorker& w){
        if(!w.pendingImport)return Result<QString>::failure(ErrorCode::ValidationError,QStringLiteral("importInvalid"));
        DataTransfer transfer(w.database);const auto result=transfer.apply(*w.pendingImport,replace);if(!result.ok())return Result<QString>{{},result.error};
        w.pendingImport.reset();return Result<QString>{tr("已导入 %1 行，跳过 %2 行ID冲突。%3%4").arg(result.value.added).arg(result.value.skipped).arg(replace?tr("替换前已创建完整备份。"):QString()).arg(result.value.settingsApplied?QString():tr("数据已提交，但外观设置未能保存；请在设置页重试。")),{}};
    });
}
void TaskWorkspace::setThemeMode(int mode)
{
    if(mode<0 || mode>2)return;
    QSettings settings(QFileInfo(m_path).dir().filePath(QStringLiteral("ui.ini")),QSettings::IniFormat);
    settings.setValue(QStringLiteral("appearance/themeMode"),mode);settings.sync();
    if(settings.status()!=QSettings::NoError){m_error={ErrorCode::PersistenceError,QStringLiteral("settingsWrite")};emit stateChanged();return;}
    m_themeMode=mode;emit stateChanged();
}
bool TaskWorkspace::selectionHasStatus(const QVariantList& selection,const QString& status) const
{
    if(selection.isEmpty())return false;
    for(const auto& value:selection){const auto found=m_taskIndex.constFind(value.toMap().value("taskId").toString());if(found==m_taskIndex.cend() || found->status!=status || !found->deletedAtUtc.isEmpty())return false;}
    return true;
}
int TaskWorkspace::taskIndex(const QString& id) const
{const auto& tasks=m_tasks.items();for(int index=0;index<tasks.size();++index)if(tasks[index].id==id)return index;return -1;}
int TaskWorkspace::dragDistance() const {return qobject_cast<QGuiApplication*>(QCoreApplication::instance())?QGuiApplication::styleHints()->startDragDistance():10;}
bool TaskWorkspace::taskReorderingAllowed() const
{return m_ready && !m_busy && !m_includeCompleted && m_search.isEmpty() && m_tagFilter.isEmpty() && m_projectFilter.isEmpty() && (QStringList{"today","inbox","all","important","repeating"}.contains(m_view) || m_view.startsWith(QStringLiteral("project:")) || m_view.startsWith(QStringLiteral("area:")) || m_view.startsWith(QStringLiteral("tag:")));}
bool TaskWorkspace::canMoveTask(const QString& id,int direction) const
{
    if(!taskReorderingAllowed() || (direction!=-1 && direction!=1))return false;
    const auto& tasks=m_tasks.items();
    int index=-1;for(int i=0;i<tasks.size();++i)if(tasks[i].id==id){index=i;break;}
    if(index<0 || index+direction<0 || index+direction>=tasks.size())return false;
    if(m_view==QStringLiteral("today") && ((tasks[index].schedule && !tasks[index].schedule->allDay) || (tasks[index+direction].schedule && !tasks[index+direction].schedule->allDay)))return false;
    if(tasks[index].pinned!=tasks[index+direction].pinned || tasks[index].parentTaskId!=tasks[index+direction].parentTaskId)return false;
    if(m_view==QStringLiteral("all") && (tasks[index].projectId!=tasks[index+direction].projectId || tasks[index].areaId!=tasks[index+direction].areaId))return false;
    if(m_view==QStringLiteral("today") && QueryService::todayPeriod(tasks[index],m_today)!=QueryService::todayPeriod(tasks[index+direction],m_today))return false;
    if(m_view.startsWith(QStringLiteral("project:")) && tasks[index].headingId!=tasks[index+direction].headingId)return false;
    return true;
}
bool TaskWorkspace::moveTask(const QString& id,int direction)
{
    if(!canMoveTask(id,direction))return false;
    const auto& visible=m_tasks.items();const auto fromVisible=taskIndex(id);const auto target=visible[fromVisible+direction].id;
    auto tasks=QueryService::select(m_cache,m_view,m_today,{},false,{},0,0,{}, {},true,!smartView());
    int from=-1,to=-1;for(int i=0;i<tasks.size();++i){if(tasks[i].id==id)from=i;if(tasks[i].id==target)to=i;}if(from<0 || to<0)return false;
    tasks.move(from,to);const bool today=m_view==QStringLiteral("today");
    return runCatalog([tasks,today](TaskWorker& w){return w.repository->reorder(tasks,today,w.clock.nowUtc().toUTC().toString(Qt::ISODateWithMs));},QStringLiteral("reorder"));
}
int TaskWorkspace::completedToday() const
{
    int count=0;
    for(const auto& task:m_cache)if(task.deletedAtUtc.isEmpty() && task.status==QStringLiteral("COMPLETED") && QDateTime::fromString(task.completedAtUtc,Qt::ISODateWithMs).toLocalTime().date()==m_today)++count;
    return count;
}
bool TaskWorkspace::canDropTask(const QString& source,const QString& target) const
{
    if(source==target || !taskReorderingAllowed())return false;
    const auto a=m_taskIndex.constFind(source),b=m_taskIndex.constFind(target);if(a==m_taskIndex.cend() || b==m_taskIndex.cend())return false;
    if(taskIndex(source)<0 || taskIndex(target)<0 || a->parentTaskId!=b->parentTaskId || a->pinned!=b->pinned)return false;
    if(m_view==QStringLiteral("all") && (a->projectId!=b->projectId || a->areaId!=b->areaId))return false;
    if(m_view==QStringLiteral("today")){
        if((a->schedule && !a->schedule->allDay) || (b->schedule && !b->schedule->allDay))return false;
        if(QueryService::todayPeriod(*a,m_today)!=QueryService::todayPeriod(*b,m_today)){auto first=*a,second=*b;first.evening=false;second.evening=false;if(QueryService::todayPeriod(first,m_today)!=0 || QueryService::todayPeriod(second,m_today)!=0)return false;}
    }return true;
}
QString TaskWorkspace::dropTaskReason(const QString& source,const QString& target) const
{
    if(canDropTask(source,target))return {};
    if(!taskReorderingAllowed())return tr("当前结果自动排序；可拖到导航栏调整归属或日期。");
    return tr("只能在相同父任务、分类和置顶分组内排序；具体时段请在日历调整。");
}
bool TaskWorkspace::dropTask(const QString& source,const QString& target)
{
    if(!canDropTask(source,target))return false;
    if(source==target || !taskReorderingAllowed())return false;
    auto tasks=QueryService::select(m_cache,m_view,m_today,{},false,{},0,0,{}, {},true,!smartView());int from=-1,to=-1;
    for(int i=0;i<tasks.size();++i){if(tasks[i].id==source)from=i;if(tasks[i].id==target)to=i;}
    if(from<0 || to<0)return false;
    if(tasks[from].pinned!=tasks[to].pinned || tasks[from].parentTaskId!=tasks[to].parentTaskId)return false;
    if(m_view==QStringLiteral("all") && (tasks[from].projectId!=tasks[to].projectId || tasks[from].areaId!=tasks[to].areaId))return false;
    OrderChange change;change.taskId=source;const bool today=m_view==QStringLiteral("today");
    if(today){if(QueryService::todayPeriod(tasks[from],m_today)!=QueryService::todayPeriod(tasks[to],m_today)){auto sourceTask=tasks[from],targetTask=tasks[to];sourceTask.evening=false;targetTask.evening=false;if(QueryService::todayPeriod(sourceTask,m_today)!=0 || QueryService::todayPeriod(targetTask,m_today)!=0)return false;}change.evening=tasks[to].evening;}else if(m_view.startsWith(QStringLiteral("project:")))change.headingId=tasks[to].headingId;
    tasks.move(from,to);
    return runCatalog([tasks,today,change](TaskWorker& w){return w.repository->reorder(tasks,today,w.clock.nowUtc().toUTC().toString(Qt::ISODateWithMs),change);},QStringLiteral("reorder"));
}
bool TaskWorkspace::todayAfterRemovingFocus(const QString& id) const
{
    for(auto task:m_cache)if(task.id==id){task.todayFocus=false;return QueryService::inView(task,QStringLiteral("today"),m_today);}
    return false;
}
int TaskWorkspace::openProjectTasks(const QString& id)const
{return int(std::count_if(m_cache.begin(),m_cache.end(),[&](const auto& task){return task.projectId==id && task.status==QStringLiteral("OPEN") && task.deletedAtUtc.isEmpty();}));}
void TaskWorkspace::setTextScale(double scale)
{
    if(!qIsFinite(scale) || scale<1.0 || scale>2.0)return;
    QSettings settings(QFileInfo(m_path).dir().filePath(QStringLiteral("ui.ini")),QSettings::IniFormat);
    settings.setValue(QStringLiteral("appearance/textScale"),scale);settings.sync();
    if(settings.status()!=QSettings::NoError){m_error={ErrorCode::PersistenceError,QStringLiteral("settingsWrite")};emit stateChanged();return;}
    m_textScale=scale;emit stateChanged();
}

} // namespace daymark
