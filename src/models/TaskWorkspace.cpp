#include "TaskWorkspace.h"
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
          {Evening,"evening"},{ProjectName,"projectName"},{TagSummary,"tagSummary"},{Section,"taskSection"},{HeadingTitle,"headingTitle"},{ChecklistPending,"checklistPending"},{ContextGroup,"contextGroup"},{InToday,"inToday"},{ViewSection,"viewSection"}}; }
QVariant TaskListModel::data(const QModelIndex& index,int role) const
{
    if(!index.isValid() || index.row()<0 || index.row()>=m_tasks.size()) return {};
    const auto& t=m_tasks[index.row()];
    switch(role) {
    case ViewSection:return m_groupRole ? data(index,m_groupRole) : QVariant(QString());
    case Id:return t.id; case Title:return t.title; case Notes:return t.notes; case Done:return t.status!=QStringLiteral("OPEN");
    case Status:return t.status; case StartOn:return t.startOn?t.startOn->toString(Qt::ISODate):QString();
    case DeadlineOn:return t.deadlineOn?t.deadlineOn->toString(Qt::ISODate):QString(); case Focus:return t.todayFocus;
    case Version:return t.rowVersion;
    case Evening:return t.evening;
    case ProjectName:return t.projectName.isEmpty()?t.areaName:t.projectName;
    case ContextGroup:return t.areaName.isEmpty() && t.projectName.isEmpty()?tr("未归属"):t.areaName+(t.areaName.isEmpty() || t.projectName.isEmpty()?QString():QStringLiteral(" / "))+t.projectName;
    case InToday:return QueryService::inView(t,QStringLiteral("today"),m_today);
    case TagSummary:return t.tagNames.join(QStringLiteral(" · "));
    case Section:return t.evening?tr("今晚"):tr("今天");
    case HeadingTitle:return t.headingTitle.isEmpty()?tr("未分组"):t.headingTitle;
    case ChecklistPending:return int(std::count_if(t.checklist.begin(),t.checklist.end(),[](const auto& item){return !item.done;}));
    case Overdue:return QueryService::effectiveActive(t) && t.deadlineOn && *t.deadlineOn<m_today;
    default:return {};
    }
}
QVariantMap TaskListModel::taskMap(const Task& t)
{
    return {{"taskId",t.id},{"title",t.title},{"notes",t.notes},{"planningState",t.planningState},{"taskStatus",t.status},
            {"startOn",t.startOn?t.startOn->toString(Qt::ISODate):QString()},{"deadlineOn",t.deadlineOn?t.deadlineOn->toString(Qt::ISODate):QString()},
            {"todayFocus",t.todayFocus},{"rowVersion",t.rowVersion},
            {"projectId",t.projectId},{"areaId",t.areaId},{"headingId",t.headingId},{"tagIds",t.tagIds},
            {"tagNames",t.tagNames},{"evening",t.evening},{"remindAtUtc",t.remindAtUtc},
            {"createdAtUtc",t.createdAtUtc},{"updatedAtUtc",t.updatedAtUtc},{"completedAtUtc",t.completedAtUtc},{"recurrenceId",t.recurrenceId},
            {"checklist",[&]{QVariantList list;for(const auto& item:t.checklist)list.append(QVariantMap{{"id",item.id},{"title",item.title},{"done",item.done}});return list;}()}};
}
void TaskListModel::replace(const QList<Task>& tasks,const QDate& today,int groupRole)
{
    const bool groupingChanged=m_groupRole!=groupRole;m_groupRole=groupRole;
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
        if(m_clock.localToday()!=m_today) { m_today=m_clock.localToday();refresh(); }
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
        else if(stored.metaType().id()==QMetaType::Bool)it.value()=stored;
        else if(stored.toString()==QStringLiteral("true") || stored.toString()==QStringLiteral("false"))it.value()=stored.toBool();
    }
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
            if(result.ok()) {self->m_cache=result.value;self->refresh();}
            if(catalogs.ok())self->m_catalog=catalogs.value;
            if(rules.ok())self->m_rules=rules.value;
            if(events.ok())self->m_events=events.value;else {self->m_error=events.error;self->m_ready=false;}
            emit self->stateChanged();emit self->operationFinished(QStringLiteral("load"),self->m_ready);
            if(self->m_ready)self->maintain();
        },Qt::QueuedConnection);
    },Qt::QueuedConnection);
}
void TaskWorkspace::refresh()
{
    m_taskIndex.clear();for(const auto& task:m_cache)m_taskIndex.insert(task.id,task);
    const int groupRole=m_view==QStringLiteral("today") ? TaskListModel::Section : m_view==QStringLiteral("upcoming") ? TaskListModel::StartOn : m_view.startsWith(QStringLiteral("project:")) ? TaskListModel::HeadingTitle : m_view==QStringLiteral("anytime") ? TaskListModel::ContextGroup : 0;
    m_tasks.replace(QueryService::select(m_cache,m_view,m_today,m_search,m_includeCompleted,m_tagFilter,m_upcomingDays,m_historyDays,m_projectFilter),m_today,groupRole);
    m_todayTasks.replace(QueryService::select(m_cache,QStringLiteral("today"),m_today),m_today);
    m_deadlineTasks.replace(QueryService::select(m_cache,QStringLiteral("deadlines"),m_today,{},false,{},m_upcomingDays),m_today);
}
void TaskWorkspace::setView(const QString& view)
{
    if(!view.startsWith(QStringLiteral("project:")) && !view.startsWith(QStringLiteral("area:")) && !view.startsWith(QStringLiteral("tag:"))
       && !QStringList{QStringLiteral("inbox"),QStringLiteral("today"),QStringLiteral("upcoming"),QStringLiteral("anytime"),QStringLiteral("someday"),QStringLiteral("logbook"),QStringLiteral("month"),QStringLiteral("week")}.contains(view))return;
    m_view=view;refresh();emit stateChanged();
}
void TaskWorkspace::setSearch(const QString& search) {m_search=search.trimmed();m_searchTimer.start();}
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
        if(self) QMetaObject::invokeMethod(self.data(),[self,operation,result]{if(self) self->finishTask(operation,result);},Qt::QueuedConnection);
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
    if(result.ok() && !result.value.recurrenceId.isEmpty())maintain();
}
bool TaskWorkspace::createTask(const QString& title)
{
    const auto view=m_view;
    return runTask(QStringLiteral("create"),[title,view](TaskWorker& w){return w.service->create(title,view);});
}
bool TaskWorkspace::saveTask(const QString& id,qint64 version,const QString& title,const QString& notes,
                             const QString& planning,const QString& start,const QString& deadline,bool focus)
{ return saveTaskWithDetails(id,version,title,notes,planning,start,deadline,focus,{}); }
bool TaskWorkspace::saveTaskWithDetails(const QString& id,qint64 version,const QString& title,const QString& notes,
                                      const QString& planning,const QString& start,const QString& deadline,bool focus,const QVariantMap& details)
{
    TaskPatch patch;patch.title=FieldPatch<QString>::set(title);patch.notes=FieldPatch<QString>::set(notes);
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
    const bool movedToToday=focus && !start.isEmpty() && QDate::fromString(start,Qt::ISODate)>m_today && planning!=QStringLiteral("SOMEDAY");
    const bool accepted=runTask(QStringLiteral("save"),[id,version,patch](TaskWorker& w){
        return w.service->edit(id,version,patch);
    });
    if(accepted && movedToToday)m_pendingInfo=tr("已加入今天，计划开始日期已调整为今天。");
    return accepted;
}
bool TaskWorkspace::changeState(const QString& id,qint64 version,const QString& action)
{ return runTask(action,[id,version,action](TaskWorker& w){return w.service->changeState(id,version,action);}); }
void TaskWorkspace::setHistoryDays(int days)
{if(days!=0 && days!=7 && days!=30)return;m_historyDays=days;refresh();emit stateChanged();}
bool TaskWorkspace::setPreference(const QString& key,const QVariant& value)
{
    if(!m_preferences.contains(key))return false;
    if(key==QStringLiteral("backupDays")){const int days=value.toInt();if(days!=1 && days!=7 && days!=30)return false;}
    else if(key==QStringLiteral("desktopCalendarOpacity")){bool ok=false;const int opacity=value.toInt(&ok);if(!ok || opacity<50 || opacity>100)return false;}
    else if(value.metaType().id()!=QMetaType::Bool)return false;
    QSettings settings(QFileInfo(m_path).dir().filePath(QStringLiteral("ui.ini")),QSettings::IniFormat);
    settings.setValue(QStringLiteral("preferences/")+key,value);settings.sync();
    if(settings.status()!=QSettings::NoError){settingsFailed();return false;}
    m_preferences[key]=value;emit stateChanged();return true;
}
QVariantList TaskWorkspace::unscheduledTasks()const
{QVariantList result;for(const auto& task:CalendarService::unscheduled(m_cache))result.append(TaskListModel::taskMap(task));return result;}
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
    else if(action!=QStringLiteral("complete")){m_error={ErrorCode::ValidationError,QStringLiteral("actionInvalid")};emit stateChanged();return false;}
    return runCatalog([items,action,patch](TaskWorker& w){return w.service->batch(items,action==QStringLiteral("complete")?action:QStringLiteral("edit"),patch);},QStringLiteral("batch"));
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
    if(key==QStringLiteral("dateOrder"))return tr("计划开始日期不能晚于截止日期。请调整日期后重试。");
    if(key==QStringLiteral("conflict"))return tr("任务已被修改。请重新加载后打开任务，再应用修改；当前输入已保留。");
    if(key==QStringLiteral("notFound"))return tr("任务已不存在或已删除。请重新加载列表。");
    if(key==QStringLiteral("schemaTooNew"))return tr("数据库由较新版本创建。请使用新版Daymark，当前数据库未覆盖。");
    if(key==QStringLiteral("unknownSchema"))return tr("数据库格式无法识别，未修改原数据。请使用单独数据目录或恢复备份。");
    if(key==QStringLiteral("backupFailed"))return tr("备份失败。请检查磁盘空间和数据目录写入权限后重试。");
    if(key==QStringLiteral("planningInvalid"))return tr("规划状态无效。请选择收集箱、已激活或以后再说。");
    if(key==QStringLiteral("ownershipInvalid"))return tr("归属无效。请确认项目、领域或分组仍存在，且分组属于所选项目。");
    if(key==QStringLiteral("tagConflict"))return tr("标签名称已存在（不区分大小写），或标签无法写入。请修改名称后重试。");
    if(key==QStringLiteral("reminderInvalid"))return tr("提醒必须是有效的将来时间。请重新选择时间，或清除提醒。");
    if(key==QStringLiteral("integrityFailed"))return tr("本地数据完整性检查失败。请停止编辑并保留当前数据，使用已验证的备份在独立目录恢复。");
    if(key==QStringLiteral("calendarAmbiguous"))return tr("同日标记包含开始和截止。请明确选择要移动的日期字段。");
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
        {"hidden",item.archived || std::any_of(m_catalog.begin(),m_catalog.end(),[&](const auto& area){return area.kind==QStringLiteral("area") && area.id==item.parentId && area.archived;})}});
    return result;
}
QVariantList TaskWorkspace::catalogOf(const QString& kind,const QString& parent) const
{
    QVariantList output;
    for(const auto& value:catalog()){const auto item=value.toMap();if(item.value(QStringLiteral("kind")).toString()==kind
        && (parent.isNull() || parent.isEmpty() || item.value(QStringLiteral("parentId")).toString()==parent))output.append(value);}
    return output;
}
bool TaskWorkspace::runCatalog(std::function<Result<void>(TaskWorker&)> command,const QString& operation)
{
    if(!m_ready || !begin(operation!=QStringLiteral("maintenance")))return false;
    QPointer<TaskWorkspace> self(this);auto* worker=m_worker;
    QMetaObject::invokeMethod(worker,[self,worker,operation,command=std::move(command)]{
        const auto result=command(*worker);
        const auto generated=result.ok() && operation!=QStringLiteral("batch") && operation!=QStringLiteral("integrity")?worker->recurrenceService->generate():Result<int>{{},result.error};
        const auto tasks=worker->repository->all();const auto catalog=worker->catalog->all();const auto rules=worker->recurrence->all();
        const auto events=EventRepository(worker->database.connection()).all();
        if(self)QMetaObject::invokeMethod(self.data(),[self,result,tasks,catalog,rules,events,generated,operation]{
            if(!self)return;
            self->m_busy=false;if(operation!=QStringLiteral("maintenance") || !result.ok())self->m_error=result.error;
            if(tasks.ok())self->m_cache=tasks.value;else self->m_error=tasks.error;
            if(catalog.ok())self->m_catalog=catalog.value;else self->m_error=catalog.error;
            if(rules.ok())self->m_rules=rules.value;else self->m_error=rules.error;
            if(events.ok())self->m_events=events.value;else self->m_error=events.error;
            if(!generated.ok())self->m_error=generated.error;
            self->refresh();emit self->stateChanged();emit self->operationFinished(operation,result.ok() && tasks.ok() && catalog.ok() && rules.ok() && events.ok() && generated.ok());
            if(operation==QStringLiteral("batch") && result.ok())self->maintain();
        },Qt::QueuedConnection);
    },Qt::QueuedConnection);return true;
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
    return runCatalog([item](TaskWorker& w){const auto result=w.catalogService->save(item);return Result<void>{result.error};});
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
    auto stringField=[&](const char* key,FieldPatch<QString>& field){if(details.contains(QLatin1String(key)))field=FieldPatch<QString>::set(details.value(QLatin1String(key)).toString());};
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
        }
    }
    if(details.contains(QStringLiteral("evening")))patch.evening=FieldPatch<bool>::set(details.value(QStringLiteral("evening")).toBool());
    if(details.contains(QStringLiteral("tagIds")))patch.tagIds=FieldPatch<QStringList>::set(details.value(QStringLiteral("tagIds")).toStringList());
    if(details.contains(QStringLiteral("checklist"))) {
        QList<ChecklistItem> items;
        for(const auto& value:details.value(QStringLiteral("checklist")).toList()) {const auto item=value.toMap();items.append({item.value(QStringLiteral("id")).toString(),item.value(QStringLiteral("title")).toString(),item.value(QStringLiteral("done")).toBool(),items.size()});}
        patch.checklist=FieldPatch<QList<ChecklistItem>>::set(items);
    }
    return true;
}

QVariantList TaskWorkspace::calendarDays() const
{return calendarDaysFor(m_calendarDate.toString(Qt::ISODate),m_view==QStringLiteral("week"));}
QVariantList TaskWorkspace::calendarDaysFor(const QString& anchor,bool week) const
{
    QVariantList result;
    const auto date=QDate::fromString(anchor,Qt::ISODate);if(!date.isValid())return {};
    const auto days=CalendarService::days(m_cache,date,week,m_sundayFirst,m_calendarCompleted);
    QHash<QDate,QVariantList> eventDays;
    const auto zone=QTimeZone::systemTimeZone();
    if(!days.isEmpty())for(const auto& event:m_events) {
        const auto dates=EventService::visibleDates(event,days.first().date,days.last().date,zone);
        if(dates.isEmpty())continue;
        const auto map=eventMap(event);for(const auto& date:dates)eventDays[date].append(map);
    }
    for(const auto& day:days) {
        QVariantList entries;
        for(const auto& entry:day.entries) {
            auto map=TaskListModel::taskMap(entry.task);map.insert(QStringLiteral("kind"),entry.kind);entries.append(map);
        }
        result.append(QVariantMap{{"date",day.date.toString(Qt::ISODate)},{"day",day.date.day()},{"inMonth",day.inMonth},
            {"today",day.date==m_today},{"weekend",day.date.dayOfWeek()>5},{"entries",entries},{"events",eventDays.value(day.date)}});
    }
    return result;
}
QString TaskWorkspace::systemTimeZone() const{return QString::fromUtf8(QTimeZone::systemTimeZoneId());}
QVariantMap TaskWorkspace::eventMap(const CalendarEvent& e) const
{
    const QTimeZone zone(e.timeZone.toUtf8());
    const auto start=e.startUtc.toTimeZone(zone),end=e.endUtc.toTimeZone(zone);
    const auto localStart=e.startUtc.toLocalTime(),localEnd=e.endUtc.toLocalTime();
    auto timeInput=[](const QTime& time){return time.toString(time.second()!=0 || time.msec()!=0?QStringLiteral("HH:mm:ss.zzz"):QStringLiteral("HH:mm"));};
    QVariantList links;
    for(const auto& link:e.links){auto item=task(link.taskId);if(!item.isEmpty()){item.insert(QStringLiteral("role"),link.role);links.append(item);}}
    const auto time=e.allDay?tr("全天"):localStart.toString(QStringLiteral("HH:mm"))+QStringLiteral("–")+localEnd.toString(localStart.date()==localEnd.date()?QStringLiteral("HH:mm"):QStringLiteral("M/d HH:mm"));
    return {{"eventId",e.id},{"title",e.title},{"notes",e.notes},{"allDay",e.allDay},{"rowVersion",e.rowVersion},{"zone",e.timeZone},
        {"startDate",e.allDay?e.startOn.toString(Qt::ISODate):start.date().toString(Qt::ISODate)},{"endDate",e.allDay?e.endOn.addDays(-1).toString(Qt::ISODate):end.date().toString(Qt::ISODate)},
        {"startTime",timeInput(start.time())},{"endTime",timeInput(end.time())},{"timeLabel",time},{"links",links}};
}
QVariantList TaskWorkspace::calendarEvents() const{QVariantList result;for(const auto& e:m_events)result.append(eventMap(e));return result;}
QVariantMap TaskWorkspace::calendarEvent(const QString& id) const{for(const auto& e:m_events)if(e.id==id)return eventMap(e);return {};}
QVariantList TaskWorkspace::eventsForTask(const QString& id) const {
    if(task(id).isEmpty())return {};
    QVariantList result;for(const auto& e:m_events)for(const auto& link:e.links)if(link.taskId==id){auto item=eventMap(e);item.insert(QStringLiteral("role"),link.role);result.append(item);break;}return result;
}
QVariantList TaskWorkspace::eventTaskChoices(const QString& search) const {
    QVariantList result;for(const auto& t:m_cache)if(t.deletedAtUtc.isEmpty() && (search.trimmed().isEmpty() || t.title.contains(search.trimmed(),Qt::CaseInsensitive) || t.notes.contains(search.trimmed(),Qt::CaseInsensitive))){result.append(TaskListModel::taskMap(t));if(result.size()>=200)break;}return result;
}
bool TaskWorkspace::saveEvent(const QVariantMap& value) {
    return runCatalog([value](TaskWorker& w){
        CalendarEvent e;e.id=value.value(QStringLiteral("eventId")).toString();e.rowVersion=value.value(QStringLiteral("rowVersion"),1).toLongLong();e.title=value.value(QStringLiteral("title")).toString();e.notes=value.value(QStringLiteral("notes")).toString();e.allDay=value.value(QStringLiteral("allDay"),true).toBool();
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
QString TaskWorkspace::calendarTitle() const
{
    if(m_view!=QStringLiteral("week"))return m_calendarDate.toString(QStringLiteral("yyyy年M月"));
    const auto days=CalendarService::days({},m_calendarDate,true,m_sundayFirst);
    return days.first().date.toString(Qt::ISODate)+QStringLiteral(" — ")+days.last().date.toString(Qt::ISODate);
}
QString TaskWorkspace::calendarTitleFor(const QString& anchor,bool week) const
{
    const auto date=QDate::fromString(anchor,Qt::ISODate);if(!date.isValid())return {};
    if(!week)return date.toString(QStringLiteral("yyyy年M月"));
    const auto days=CalendarService::days({},date,true,m_sundayFirst);
    return days.first().date.toString(Qt::ISODate)+QStringLiteral(" — ")+days.last().date.toString(Qt::ISODate);
}
QString TaskWorkspace::shiftCalendar(const QString& anchor,int offset,bool week) const
{
    const auto date=QDate::fromString(anchor,Qt::ISODate);if(!date.isValid())return {};
    return (offset==0?m_clock.localToday():(week?date.addDays(offset*7):date.addMonths(offset))).toString(Qt::ISODate);
}
void TaskWorkspace::stepCalendar(int offset)
{
    m_calendarDate=offset==0?m_clock.localToday():(m_view==QStringLiteral("week")?m_calendarDate.addDays(offset*7):m_calendarDate.addMonths(offset));
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
bool TaskWorkspace::createForDate(const QString& title,const QString& date)
{
    return runTask(QStringLiteral("calendarCreate"),[title,date](TaskWorker& w){return w.service->create(title,QStringLiteral("date:")+date);});
}
bool TaskWorkspace::moveCalendar(const QString& id,qint64 version,const QString& kind,const QString& date)
{
    return runTask(QStringLiteral("calendarMove"),[id,version,kind,date](TaskWorker& w){return CalendarService::move(*w.service,id,version,kind,QDate::fromString(date,Qt::ISODate));});
}
QVariantList TaskWorkspace::recurrenceRules() const
{
    QVariantList result;
    for(const auto& r:m_rules)result.append(QVariantMap{{"id",r.id},{"title",r.title},{"notes",r.notes},{"frequency",r.frequency},
        {"anchor",r.anchor.toString(Qt::ISODate)},{"end",r.end.toString(Qt::ISODate)},{"interval",r.interval},{"weekdays",r.weekdays},
        {"monthDay",r.monthDay},{"yearMonth",r.yearMonth},{"active",r.active},{"rowVersion",r.rowVersion}});
    return result;
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
QVariantList TaskWorkspace::notifications() const
{
    QList<Task> fired;
    for(const auto& task:m_cache)if(!task.reminderFiredAtUtc.isEmpty() && task.deletedAtUtc.isEmpty())fired.append(task);
    std::stable_sort(fired.begin(),fired.end(),[](const auto& a,const auto& b){return a.reminderFiredAtUtc>b.reminderFiredAtUtc;});
    QVariantList result;for(int i=0;i<qMin(50,int(fired.size()));++i){auto map=TaskListModel::taskMap(fired[i]);map.insert(QStringLiteral("firedAt"),fired[i].reminderFiredAtUtc);result.append(map);}return result;
}
void TaskWorkspace::maintain()
{
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
QString TaskWorkspace::localReminder(const QString& utc) const
{return QDateTime::fromString(utc,Qt::ISODateWithMs).toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));}
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
bool TaskWorkspace::canMoveTask(const QString& id,int direction) const
{
    if(!m_ready || m_busy || !m_search.isEmpty() || !m_tagFilter.isEmpty() || !m_projectFilter.isEmpty() || m_view==QStringLiteral("upcoming") || m_view==QStringLiteral("logbook") || (direction!=-1 && direction!=1))return false;
    const auto tasks=QueryService::select(m_cache,m_view,m_today);
    int index=-1;for(int i=0;i<tasks.size();++i)if(tasks[i].id==id){index=i;break;}
    if(index<0 || index+direction<0 || index+direction>=tasks.size())return false;
    if(m_view==QStringLiteral("today") && tasks[index].evening!=tasks[index+direction].evening)return false;
    if(m_view.startsWith(QStringLiteral("project:")) && tasks[index].headingId!=tasks[index+direction].headingId)return false;
    return true;
}
bool TaskWorkspace::moveTask(const QString& id,int direction)
{
    if(!canMoveTask(id,direction))return false;
    auto tasks=QueryService::select(m_cache,m_view,m_today);
    int index=0;while(index<tasks.size() && tasks[index].id!=id)++index;
    tasks.swapItemsAt(index,index+direction);const bool today=m_view==QStringLiteral("today");
    return runCatalog([tasks,today](TaskWorker& w){return w.repository->reorder(tasks,today,w.clock.nowUtc().toUTC().toString(Qt::ISODateWithMs));},QStringLiteral("reorder"));
}
int TaskWorkspace::completedToday() const
{
    int count=0;
    for(const auto& task:m_cache)if(task.deletedAtUtc.isEmpty() && task.status==QStringLiteral("COMPLETED") && QDateTime::fromString(task.completedAtUtc,Qt::ISODateWithMs).toLocalTime().date()==m_today)++count;
    return count;
}
bool TaskWorkspace::dropTask(const QString& source,const QString& target)
{
    if(source==target || !m_search.isEmpty() || (m_view!=QStringLiteral("today") && !m_view.startsWith(QStringLiteral("project:"))))return false;
    auto tasks=QueryService::select(m_cache,m_view,m_today);int from=-1,to=-1;
    for(int i=0;i<tasks.size();++i){if(tasks[i].id==source)from=i;if(tasks[i].id==target)to=i;}
    if(from<0 || to<0)return false;
    OrderChange change;change.taskId=source;const bool today=m_view==QStringLiteral("today");
    if(today)change.evening=tasks[to].evening;else change.headingId=tasks[to].headingId;
    tasks.move(from,to);
    return runCatalog([tasks,today,change](TaskWorker& w){return w.repository->reorder(tasks,today,w.clock.nowUtc().toUTC().toString(Qt::ISODateWithMs),change);},QStringLiteral("reorder"));
}
QVariantMap TaskWorkspace::datePicker(const QString& anchor,int offset) const
{
    auto date=QDate::fromString(anchor,Qt::ISODate);if(!date.isValid())date=m_today;date=QDate(date.year(),date.month(),1).addMonths(qBound(-1,offset,1));
    QVariantList days;for(const auto& day:CalendarService::days({},date,false,m_sundayFirst))days.append(QVariantMap{{"date",day.date.toString(Qt::ISODate)},{"day",day.date.day()},{"inMonth",day.inMonth},{"today",day.date==m_today}});
    return {{"anchor",date.toString(Qt::ISODate)},{"title",date.toString(QStringLiteral("yyyy年M月"))},{"days",days}};
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
