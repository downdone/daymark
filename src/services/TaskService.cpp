#include "TaskService.h"
#include "EventService.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QUuid>
#include <QTimeZone>
#include <algorithm>
#include <QSet>
#include <climits>
#include <QHash>
#include <functional>

namespace daymark {

QDate QueryService::plannedDate(const Task& task)
{
    if(task.schedule)return task.schedule->allDay?task.schedule->startOn:task.schedule->startUtc.toLocalTime().date();
    if(task.startOn)return *task.startOn;
    if(task.deadlineOn)return *task.deadlineOn;
    return QDateTime::fromString(task.remindAtUtc,Qt::ISODateWithMs).toLocalTime().date();
}
int QueryService::todayPeriod(const Task& task,const QDate& today)
{
    if(task.evening)return 3;QDateTime time;
    if(task.schedule && !task.schedule->allDay)time=task.schedule->startUtc.toLocalTime();
    else if(!task.remindAtUtc.isEmpty())time=QDateTime::fromString(task.remindAtUtc,Qt::ISODateWithMs).toLocalTime();
    if(!time.isValid() || time.date()!=today)return 0;
    return time.time().hour()<12?1:time.time().hour()<18?2:3;
}
bool QueryService::effectiveActive(const Task& t)
{
    return !t.syncRemoteDeletionPending && t.deletedAtUtc.isEmpty() && !t.areaArchived && t.status==QStringLiteral("OPEN") && t.planningState==QStringLiteral("ACTIVE")
        && (t.projectId.isEmpty() || (!t.projectDeleted && t.projectStatus==QStringLiteral("OPEN") && t.projectPlanning==QStringLiteral("ACTIVE")));
}
bool QueryService::inView(const Task& input, const QString& view, const QDate& d)
{
    const auto t=withUnifiedDates(input);
    if(t.syncRemoteDeletionPending)return false;
    if(view==QStringLiteral("trash"))return !t.deletedAtUtc.isEmpty();
    if(!t.deletedAtUtc.isEmpty()) return false;
    if(view==QStringLiteral("cancelled"))return t.status==QStringLiteral("CANCELLED");
    if(view==QStringLiteral("completed"))return t.status==QStringLiteral("COMPLETED");
    if(view==QStringLiteral("logbook")) return t.status!=QStringLiteral("OPEN");
    if(view==QStringLiteral("repeating"))return t.repeat && t.repeat->active && t.status==QStringLiteral("OPEN") && !t.areaArchived && (t.projectId.isEmpty() || (!t.projectDeleted && t.projectStatus==QStringLiteral("OPEN")));
    if(view==QStringLiteral("important"))return t.important && t.status==QStringLiteral("OPEN") && !t.areaArchived && (t.projectId.isEmpty() || (!t.projectDeleted && t.projectStatus==QStringLiteral("OPEN")));
    if(t.areaArchived)return false;
    if(t.status!=QStringLiteral("OPEN")) return false;
    const bool projectOpen=t.projectId.isEmpty() || (!t.projectDeleted && t.projectStatus==QStringLiteral("OPEN"));
    if(view.startsWith(QStringLiteral("project:")))return (t.projectId==view.mid(8) || t.projectAreaId==view.mid(8)) && projectOpen;
    if(view.startsWith(QStringLiteral("area:")))return (t.areaId==view.mid(5) || t.projectAreaId==view.mid(5)) && projectOpen;
    if(view.startsWith(QStringLiteral("tag:")))return t.tagIds.contains(view.mid(4)) && projectOpen;
    if(view==QStringLiteral("all"))return projectOpen;
    if(view==QStringLiteral("scheduled"))return projectOpen && plannedDate(t).isValid();
    if(view==QStringLiteral("inbox")) return projectOpen && t.projectId.isEmpty() && t.areaId.isEmpty();
    if(view==QStringLiteral("someday")) return projectOpen && (t.planningState==QStringLiteral("SOMEDAY") || t.projectPlanning==QStringLiteral("SOMEDAY"));
    if(!effectiveActive(t)) return false;
    if(view==QStringLiteral("today")) return t.todayFocus || (plannedDate(t).isValid() && plannedDate(t)<=d) || (!t.schedule && t.deadlineOn && *t.deadlineOn<=d);
    if(view==QStringLiteral("upcoming")) return t.startOn && *t.startOn>d;
    if(view==QStringLiteral("deadlines"))return t.deadlineOn && *t.deadlineOn>d && (!t.startOn || *t.startOn<=d);
    if(view==QStringLiteral("anytime")) return !t.startOn || *t.startOn<=d;
    return false;
}
QList<Task> QueryService::select(const QList<Task>& tasks, const QString& view, const QDate& d, const QString& search,bool includeCompleted,const QString& tagFilter,int upcomingDays,int historyDays,const QString& projectFilter,const QSet<QString>& collapsed,bool includeSubtasks,bool nestChildren)
{
    QList<Task> output;
    for(const auto& task:tasks) {
        if(task.syncRemoteDeletionPending)continue;
        if(!includeSubtasks && !task.parentTaskId.isEmpty())continue;
        const bool globalSearch=!search.isEmpty() && view!=QStringLiteral("logbook") && view!=QStringLiteral("completed") && view!=QStringLiteral("cancelled") && view!=QStringLiteral("trash");
        if(globalSearch) {
            if(!task.deletedAtUtc.isEmpty() || (!includeCompleted && task.status!=QStringLiteral("OPEN"))
                || (task.status==QStringLiteral("OPEN") && (task.areaArchived || (!task.projectId.isEmpty() && (task.projectDeleted || task.projectStatus!=QStringLiteral("OPEN"))))))continue;
        } else if(includeCompleted && view.startsWith(QStringLiteral("project:"))) {
            if(!task.deletedAtUtc.isEmpty() || (task.projectId!=view.mid(8) && task.projectAreaId!=view.mid(8)))continue;
        } else if(!inView(task,view,d)) continue;
        if(!tagFilter.isEmpty() && !task.tagIds.contains(tagFilter))continue;
        if(!projectFilter.isEmpty() && task.projectId!=projectFilter && task.projectAreaId!=projectFilter)continue;
        if(search.isEmpty() && upcomingDays>0 && ((view==QStringLiteral("upcoming") && task.startOn && *task.startOn>d.addDays(upcomingDays))
            || (view==QStringLiteral("deadlines") && task.deadlineOn && *task.deadlineOn>d.addDays(upcomingDays))))continue;
        if((view==QStringLiteral("logbook") || view==QStringLiteral("cancelled")) && historyDays>0) {
            const auto timestamp=task.completedAtUtc.isEmpty()?task.cancelledAtUtc:task.completedAtUtc;
            const auto date=QDateTime::fromString(timestamp,Qt::ISODateWithMs).toLocalTime().date();
            if(!date.isValid() || date<d.addDays(1-historyDays))continue;
        }
        if(!search.isEmpty() && !task.title.contains(search,Qt::CaseInsensitive) && !task.notes.contains(search,Qt::CaseInsensitive)) continue;
        output.append(withUnifiedDates(task));
    }
    std::stable_sort(output.begin(),output.end(),[&view,&d](const Task& a,const Task& b) {
        if(view==QStringLiteral("scheduled")){const auto ad=plannedDate(a),bd=plannedDate(b);if(ad!=bd)return ad<bd;}
        if(view==QStringLiteral("all") && a.categoryOrderPath!=b.categoryOrderPath && (!a.categoryOrderPath.isEmpty() || !b.categoryOrderPath.isEmpty()))return a.categoryOrderPath<b.categoryOrderPath;
        if(view==QStringLiteral("all")){const auto ag=a.areaName+QLatin1Char('/')+a.projectName,bg=b.areaName+QLatin1Char('/')+b.projectName;if(ag!=bg)return ag<bg;}
        if(view==QStringLiteral("today") && todayPeriod(a,d)!=todayPeriod(b,d))return todayPeriod(a,d)<todayPeriod(b,d);
        if(view==QStringLiteral("logbook") || view==QStringLiteral("completed") || view==QStringLiteral("cancelled") || view==QStringLiteral("trash")) {
            if(view==QStringLiteral("trash") && a.deletedAtUtc!=b.deletedAtUtc)return a.deletedAtUtc>b.deletedAtUtc;
            const auto at=a.completedAtUtc.isEmpty()?a.cancelledAtUtc:a.completedAtUtc;
            const auto bt=b.completedAtUtc.isEmpty()?b.cancelledAtUtc:b.completedAtUtc;
            if(at!=bt) return at>bt;
        }
        if(view==QStringLiteral("scheduled") || view==QStringLiteral("today")) {
            auto time=[&](const Task& task){if(task.schedule && !task.schedule->allDay && (view==QStringLiteral("scheduled") || task.schedule->startUtc.toLocalTime().date()==d))return task.schedule->startUtc;return QDateTime();};
            const auto first=time(a),second=time(b);if(first.isValid()!=second.isValid())return !first.isValid();if(first.isValid() && first!=second)return first<second;
        }
        if(a.pinned!=b.pinned)return a.pinned;
        if(view==QStringLiteral("upcoming") && a.startOn!=b.startOn) return a.startOn<b.startOn;
        if(view==QStringLiteral("deadlines") && a.deadlineOn!=b.deadlineOn)return a.deadlineOn<b.deadlineOn;
        if(view==QStringLiteral("anytime")) {
            if(a.areaName!=b.areaName)return a.areaName<b.areaName;
            if(a.projectName!=b.projectName)return a.projectName<b.projectName;
        }
        if(view.startsWith(QStringLiteral("project:"))) {
            if(a.headingOrder!=b.headingOrder)return a.headingOrder<b.headingOrder;
            if(a.headingId!=b.headingId)return a.headingId<b.headingId;
        }
        if(view==QStringLiteral("today")) {
            if(a.evening!=b.evening)return !a.evening;
            if(a.todayOrder!=b.todayOrder)return (a.todayOrder<0?LLONG_MAX:a.todayOrder)<(b.todayOrder<0?LLONG_MAX:b.todayOrder);
        }
        if(view==QStringLiteral("upcoming") || view==QStringLiteral("deadlines") || view==QStringLiteral("anytime") || view==QStringLiteral("someday")) {
            const auto dateA=a.startOn?a.startOn:a.deadlineOn;
            const auto dateB=b.startOn?b.startOn:b.deadlineOn;
            if(dateA!=dateB)return dateA<dateB;
        }
        if(a.sortOrder!=b.sortOrder) return a.sortOrder<b.sortOrder;
        return a.id<b.id;
    });
    if(!nestChildren){for(auto& task:output){task.listDepth=0;task.listChildCount=0;}return output;}
    QHash<QString,Task> visible;for(const auto& task:output)visible.insert(task.id,task);
    QHash<QString,QList<Task>> children;QList<Task> roots;
    for(const auto& task:output)if(!task.parentTaskId.isEmpty() && visible.contains(task.parentTaskId) && visible.value(task.parentTaskId).projectId==task.projectId && visible.value(task.parentTaskId).areaId==task.areaId && (view!=QStringLiteral("trash") || (!task.deletionGroupId.isEmpty() && task.deletionGroupId==visible.value(task.parentTaskId).deletionGroupId)))children[task.parentTaskId].append(task);else roots.append(task);
    QList<Task> tree;QSet<QString> emitted;QList<QPair<Task,int>> pending;
    for(auto it=roots.crbegin();it!=roots.crend();++it)pending.append({*it,0});
    while(!pending.isEmpty()){auto item=pending.takeLast();auto task=item.first;if(emitted.contains(task.id))continue;emitted.insert(task.id);task.listDepth=item.second;task.listChildCount=children.value(task.id).size();tree.append(task);if(search.isEmpty() && collapsed.contains(task.id))continue;const auto nested=children.value(task.id);for(auto it=nested.crbegin();it!=nested.crend();++it)pending.append({*it,item.second+1});}
    return tree;
}

Result<void> TaskService::validate(Task& t) const
{
    t.title=t.title.trimmed();
    if(t.notes.isNull())t.notes=QStringLiteral("");
    if(t.title.isEmpty() || t.title.toUcs4().size()>512)
        return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("titleInvalid"));
    if(t.planningState!=QStringLiteral("INBOX") && t.planningState!=QStringLiteral("ACTIVE") && t.planningState!=QStringLiteral("SOMEDAY"))
        return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("planningInvalid"));
    if((t.startOn && !t.startOn->isValid()) || (t.deadlineOn && !t.deadlineOn->isValid()))
        return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("dateInvalid"));
    if(t.startOn && t.deadlineOn && *t.startOn>*t.deadlineOn)
        return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("dateOrder"));
    if(t.planningState==QStringLiteral("INBOX") && (t.startOn || t.deadlineOn || t.todayFocus || t.schedule || !t.remindAtUtc.isEmpty()))
        t.planningState=QStringLiteral("ACTIVE");
    if(!QStringList{QStringLiteral("NONE"),QStringLiteral("DAILY"),QStringLiteral("WEEKLY"),QStringLiteral("MONTHLY"),QStringLiteral("YEARLY")}.contains(t.reminderRepeat) || t.reminderEvery<1 || t.reminderEvery>36500 || !QTimeZone(t.reminderZone.toUtf8()).isValid() || (t.reminderRepeat!=QStringLiteral("NONE") && (t.remindAtUtc.isEmpty() || !QDateTime::fromString(t.reminderAnchorUtc,Qt::ISODateWithMs).isValid())))return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("reminderInvalid"));
    if(t.schedule){const auto& s=*t.schedule;CalendarEvent e;e.title=t.title;e.allDay=s.allDay;e.startOn=s.startOn;e.endOn=s.endOn;e.startUtc=s.startUtc;e.endUtc=s.endUtc;e.timeZone=s.timeZone;e.alertMinutes=s.alertMinutes;const auto valid=EventService::validate(e);if(!valid.ok())return valid;if(t.repeat && t.repeat->active && (s.allDay?s.startOn.daysTo(s.endOn):s.startUtc.secsTo(s.endUtc)/86400)>366)return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("recurrenceInvalid"));}
    QSet<QString> linked;for(const auto& link:t.links){if(link.taskId==t.id || linked.contains(link.taskId) || (link.role!=QStringLiteral("preparation") && link.role!=QStringLiteral("followup")))return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("eventInvalid"));linked.insert(link.taskId);const auto target=m_repository.find(link.taskId);if(!target.ok())return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("eventTaskMissing"));}
    if(m_catalog){const auto result=m_catalog->validateTask(t);if(!result.ok())return result;}
    if(!QueryService::inView(t,QStringLiteral("today"),m_clock.localToday())) t.evening=false;
    return {};
}

Result<Task> TaskService::create(const QString& title, const QString& view,const QDate& date,const std::optional<TaskSchedule>& schedule)
{
    if(view==QStringLiteral("repeating")){
        Result<Task> saved;const auto transaction=m_repository.atomically([&]{saved=create(title,QStringLiteral("anytime"),date,schedule);if(!saved.ok())return Result<void>{saved.error};RecurrenceRule rule;rule.weekdays=1<<(m_clock.localToday().dayOfWeek()-1);TaskPatch patch;patch.repeat=FieldPatch<RecurrenceRule>::set(rule);saved=edit(saved.value.id,saved.value.rowVersion,patch);return Result<void>{saved.error};});if(!transaction.ok())return {{},transaction.error};return saved;
    }
    auto t=draftForScope(view,m_clock.localToday(),date,schedule);
    t.id=QUuid::createUuid().toString(QUuid::WithoutBraces);t.title=title;t.notes=QStringLiteral("");
    t.createdAtUtc=t.updatedAtUtc=m_clock.nowUtc().toUTC().toString(Qt::ISODateWithMs);
    const auto valid=validate(t);
    if(!valid.ok()) return {Task{},valid.error};
    return m_repository.insert(t);
}

Task TaskService::draftForScope(const QString& view,const QDate& today,const QDate& date,const std::optional<TaskSchedule>& schedule)
{
    Task t;
    if(view==QStringLiteral("important")){t.planningState=QStringLiteral("ACTIVE");t.important=true;}
    else if(view==QStringLiteral("today")) { t.planningState=QStringLiteral("ACTIVE"); t.startOn=today; }
    else if(view==QStringLiteral("anytime")) t.planningState=QStringLiteral("ACTIVE");
    else if(view==QStringLiteral("someday")) t.planningState=QStringLiteral("SOMEDAY");
    else if(view==QStringLiteral("upcoming") || view==QStringLiteral("scheduled")) {t.planningState=QStringLiteral("ACTIVE");t.startOn=today.addDays(1);}
    else if(view.startsWith(QStringLiteral("project:"))) {t.planningState=QStringLiteral("ACTIVE");t.projectId=view.mid(8);}
    else if(view.startsWith(QStringLiteral("area:"))) {t.planningState=QStringLiteral("ACTIVE");t.areaId=view.mid(5);}
    else if(view.startsWith(QStringLiteral("date:"))) {t.planningState=QStringLiteral("ACTIVE");t.startOn=QDate::fromString(view.mid(5),Qt::ISODate);}
    if(view.startsWith(QStringLiteral("tag:"))){t.tagIds={view.mid(4)};t.planningState=QStringLiteral("ACTIVE");}
    if(date.isValid()){t.startOn=date;t.planningState=QStringLiteral("ACTIVE");}
    if(schedule){t.schedule=schedule;const auto canonical=QueryService::withUnifiedDates(t);t.startOn=canonical.startOn;t.deadlineOn=canonical.deadlineOn;}
    return t;
}

Result<Task> TaskService::createWithDetails(const QString& title,const TaskPatch& patch)
{
    Result<Task> saved;
    const auto transaction=m_repository.atomically([&] {
        saved=create(title);
        if(!saved.ok())return Result<void>{saved.error};
        saved=edit(saved.value.id,saved.value.rowVersion,patch);
        return Result<void>{saved.error};
    });
    if(!transaction.ok())return {{},transaction.error};
    return saved;
}

Result<Task> TaskService::createSubtask(const QString& title,const QString& parentId)
{
    const auto parent=m_repository.find(parentId);if(!parent.ok())return parent;
    if(!parent.value.deletedAtUtc.isEmpty())return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("taskParentInvalid"));
    if(!parent.value.parentTaskId.isEmpty())return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("taskParentDepth"));
    Task task;task.id=QUuid::createUuid().toString(QUuid::WithoutBraces);task.title=title;task.notes=QStringLiteral("");task.parentTaskId=parentId;
    task.projectId=parent.value.projectId;task.areaId=parent.value.areaId;task.planningState=parent.value.planningState==QStringLiteral("SOMEDAY")?QStringLiteral("SOMEDAY"):QStringLiteral("ACTIVE");
    task.createdAtUtc=task.updatedAtUtc=m_clock.nowUtc().toUTC().toString(Qt::ISODateWithMs);const auto valid=validate(task);if(!valid.ok())return {{},valid.error};return m_repository.insert(task);
}

Result<Task> TaskService::editOne(const QString& id, qint64 version, const TaskPatch& patch)
{
    auto existing=m_repository.find(id);
    if(!existing.ok()) return existing;
    auto t=existing.value;
    if(t.rowVersion!=version) return Result<Task>::failure(ErrorCode::Conflict,QStringLiteral("conflict"));
    if(!t.deletedAtUtc.isEmpty()) return Result<Task>::failure(ErrorCode::NotFound,QStringLiteral("notFound"));
    auto applyString=[](QString& target,const FieldPatch<QString>& p) {
        if(p.kind==FieldPatch<QString>::Kind::Set) target=p.value;
        else if(p.kind==FieldPatch<QString>::Kind::Clear) target=QStringLiteral("");
    };
    applyString(t.parentTaskId,patch.parentTaskId);
    if(patch.pinned.kind!=FieldPatch<bool>::Kind::Unspecified)t.pinned=patch.pinned.kind==FieldPatch<bool>::Kind::Set && patch.pinned.value;
    if(patch.important.kind!=FieldPatch<bool>::Kind::Unspecified)t.important=patch.important.kind==FieldPatch<bool>::Kind::Set && patch.important.value;
    if(!t.parentTaskId.isEmpty()) {
        QSet<QString> ancestors{id};auto next=t.parentTaskId;
        while(!next.isEmpty()) {
            if(ancestors.contains(next))return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("taskParentCycle"));ancestors.insert(next);
            const auto parent=m_repository.find(next);if(!parent.ok())return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("taskParentInvalid"));
            if(next==t.parentTaskId && t.parentTaskId!=existing.value.parentTaskId && patch.parentTaskId.kind==FieldPatch<QString>::Kind::Set && !parent.value.deletedAtUtc.isEmpty())return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("taskParentInvalid"));
            next=parent.value.parentTaskId;
        }
    }
    if(!t.parentTaskId.isEmpty() && t.parentTaskId!=existing.value.parentTaskId) {
        const auto parent=m_repository.find(t.parentTaskId);
        if(!parent.ok() || !parent.value.parentTaskId.isEmpty())return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("taskParentDepth"));
        const auto all=m_repository.all();if(!all.ok())return {{},all.error};
        if(std::any_of(all.value.cbegin(),all.value.cend(),[&](const Task& child){return child.parentTaskId==id;}))return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("taskHasChildren"));
    }
    applyString(t.reminderRepeat,patch.reminderRepeat);applyString(t.reminderZone,patch.reminderZone);
    if(patch.reminderEvery.kind==FieldPatch<int>::Kind::Set)t.reminderEvery=patch.reminderEvery.value;
    applyString(t.title,patch.title); applyString(t.notes,patch.notes); applyString(t.planningState,patch.planningState);
    applyString(t.projectId,patch.projectId);applyString(t.areaId,patch.areaId);applyString(t.headingId,patch.headingId);
    if(patch.projectId.kind==FieldPatch<QString>::Kind::Set && !t.projectId.isEmpty())t.areaId.clear();
    if(patch.areaId.kind==FieldPatch<QString>::Kind::Set && !t.areaId.isEmpty()){t.projectId.clear();t.headingId.clear();}
    if(t.projectId!=existing.value.projectId && patch.headingId.kind==FieldPatch<QString>::Kind::Unspecified)t.headingId.clear();
    if(patch.tagIds.kind==FieldPatch<QStringList>::Kind::Set)t.tagIds=patch.tagIds.value;
    else if(patch.tagIds.kind==FieldPatch<QStringList>::Kind::Clear)t.tagIds.clear();
    if(patch.evening.kind==FieldPatch<bool>::Kind::Set)t.evening=patch.evening.value;
    if(patch.remindAtUtc.kind!=FieldPatch<QString>::Kind::Unspecified) {
        applyString(t.remindAtUtc,patch.remindAtUtc);

    if(!t.remindAtUtc.isEmpty()) {
            const auto instant=QDateTime::fromString(t.remindAtUtc,Qt::ISODateWithMs);
            if(!instant.isValid() || instant<=m_clock.nowUtc())return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("reminderInvalid"));
            t.remindAtUtc=instant.toUTC().toString(Qt::ISODateWithMs);
        }
        t.reminderFiredAtUtc.clear();
    }
    auto applyDate=[](std::optional<QDate>& target,const FieldPatch<QDate>& p) {
        if(p.kind==FieldPatch<QDate>::Kind::Set) target=p.value;
        else if(p.kind==FieldPatch<QDate>::Kind::Clear) target.reset();
    };
    applyDate(t.startOn,patch.startOn); applyDate(t.deadlineOn,patch.deadlineOn);
    if(patch.links.kind==FieldPatch<QList<TaskLink>>::Kind::Set)t.links=patch.links.value;
    else if(patch.links.kind==FieldPatch<QList<TaskLink>>::Kind::Clear)t.links.clear();
    if(patch.schedule.kind==FieldPatch<TaskSchedule>::Kind::Set){t.schedule=patch.schedule.value;
        if(!t.startOn){const auto& s=*t.schedule;t.startOn=s.allDay?s.startOn:s.startUtc.toTimeZone(QTimeZone(s.timeZone.toUtf8())).date();}
        if(t.planningState==QStringLiteral("INBOX"))t.planningState=QStringLiteral("ACTIVE");
        if(t.schedule->alertMinutes>=0){const auto& s=*t.schedule;const auto at=s.allDay?QDateTime(s.startOn,QTime(9,0),QTimeZone::systemTimeZone(),QDateTime::TransitionResolution::Reject):s.startUtc;const auto reminder=at.addSecs(-60*s.alertMinutes);t.remindAtUtc=reminder.toUTC().toString(Qt::ISODateWithMs);t.reminderZone=s.allDay?QString::fromUtf8(QTimeZone::systemTimeZoneId()):s.timeZone;t.reminderFiredAtUtc.clear();}
    } else if(patch.schedule.kind==FieldPatch<TaskSchedule>::Kind::Clear)t.schedule.reset();
    if(patch.remindAtUtc.kind!=FieldPatch<QString>::Kind::Unspecified && t.schedule){applyString(t.remindAtUtc,patch.remindAtUtc);t.schedule->alertMinutes=-1;}
    const auto timing=synchronizeTime(t,existing.value,patch);if(!timing.ok())return {{},timing.error};
    if((!t.projectId.isEmpty() || !t.areaId.isEmpty()) && t.planningState==QStringLiteral("INBOX"))t.planningState=QStringLiteral("ACTIVE");
    if(patch.todayFocus.kind==FieldPatch<bool>::Kind::Set) t.todayFocus=patch.todayFocus.value;
    else if(patch.todayFocus.kind==FieldPatch<bool>::Kind::Clear) t.todayFocus=false;
    if(!t.schedule && t.planningState!=QStringLiteral("SOMEDAY") && t.todayFocus && t.startOn && *t.startOn>m_clock.localToday()) t.startOn=m_clock.localToday();
    if(t.remindAtUtc!=existing.value.remindAtUtc)t.reminderAnchorUtc=t.remindAtUtc;
    if(t.remindAtUtc.isEmpty()){t.reminderRepeat=QStringLiteral("NONE");t.reminderAnchorUtc.clear();}
    if(t.reminderRepeat!=QStringLiteral("NONE") && existing.value.reminderRepeat==QStringLiteral("NONE") && patch.reminderZone.kind==FieldPatch<QString>::Kind::Unspecified)t.reminderZone=QString::fromUtf8(QTimeZone::systemTimeZoneId());
    if(patch.repeat.kind!=FieldPatch<RecurrenceRule>::Kind::Unspecified){
        if(patch.repeatVersion.kind==FieldPatch<qint64>::Kind::Set && (!existing.value.repeat || patch.repeatVersion.value!=existing.value.repeat->rowVersion))return Result<Task>::failure(ErrorCode::Conflict,QStringLiteral("conflict"));
        t.repeatChanged=patch.repeat.kind==FieldPatch<RecurrenceRule>::Kind::Set || (t.repeat && t.repeat->active);t.replacedRecurrenceId=t.repeatChanged?t.recurrenceId:QString();t.replacedRecurrenceVersion=existing.value.repeat?existing.value.repeat->rowVersion:0;
        t.repeatCutoff=qMax(m_clock.localToday().addDays(1),t.startOn.value_or(m_clock.localToday()));
        if(patch.repeat.kind==FieldPatch<RecurrenceRule>::Kind::Clear){if(t.repeat)t.repeat->active=false;}
        else {auto r=patch.repeat.value;
            if(existing.value.repeat && r.rowVersion!=existing.value.repeat->rowVersion)return Result<Task>::failure(ErrorCode::Conflict,QStringLiteral("conflict"));
            if(!t.startOn)t.startOn=m_clock.localToday();if(t.planningState==QStringLiteral("INBOX"))t.planningState=QStringLiteral("ACTIVE");
            r.anchor=*t.startOn;r.monthDay=r.anchor.day();r.yearMonth=r.anchor.month();r.title=t.title;r.notes=t.notes.isNull()?QStringLiteral(""):t.notes;r.id=QUuid::createUuid().toString(QUuid::WithoutBraces);r.sourceTaskId=t.id;r.rowVersion=1;r.active=true;
            if(r.interval<1 || r.interval>36500 || !QStringList{"DAILY","WEEKLY","MONTHLY","YEARLY","AFTER_DAYS"}.contains(r.frequency) || r.weekdays<0 || r.weekdays>127 || (r.frequency=="WEEKLY" && r.weekdays==0) || r.monthDay<1 || r.monthDay>31 || r.yearMonth<1 || r.yearMonth>12 || (r.frequency=="YEARLY" && !QDate(2000,r.yearMonth,r.monthDay).isValid()) || (r.end.isValid() && r.end<r.anchor))return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("recurrenceInvalid"));
            if(r.frequency!=QStringLiteral("WEEKLY"))r.weekdays=0;
            auto defaults=TaskRepository::seedFor(t);defaults.insert(QStringLiteral("recurrence_time_zone"),QString::fromUtf8(QTimeZone::systemTimeZoneId()));const auto previous=existing.value.repeat?QJsonDocument::fromJson(existing.value.repeat->defaultsJson.toUtf8()).object():QJsonObject();
            for(const auto& key:{QStringLiteral("legacy_event_task_id"),QStringLiteral("reminder_not_before_utc")})if(previous.contains(key))defaults.insert(key,previous.value(key));
            auto ancestors=previous.value(QStringLiteral("reminder_ancestors")).toArray(),sources=previous.value(QStringLiteral("reminder_sources")).toArray();
            if(!existing.value.recurrenceId.isEmpty() && !ancestors.contains(existing.value.recurrenceId))ancestors.append(existing.value.recurrenceId);
            if(!sources.contains(t.id))sources.append(t.id);if(existing.value.repeat && !existing.value.repeat->sourceTaskId.isEmpty() && !sources.contains(existing.value.repeat->sourceTaskId))sources.append(existing.value.repeat->sourceTaskId);
            defaults.insert(QStringLiteral("reminder_ancestors"),ancestors);defaults.insert(QStringLiteral("reminder_sources"),sources);
            defaults.insert(QStringLiteral("materialize_from"),(existing.value.recurrenceId.isEmpty()?qMax(m_clock.localToday(),r.anchor):t.repeatCutoff).toString(Qt::ISODate));
            t.recurrenceId=r.id;t.occurrenceOn=r.anchor;t.syncOccurrenceKey=QStringLiteral("D:")+r.anchor.toString(Qt::ISODate);t.syncOccurrenceOn=r.anchor;r.defaultsJson=QString::fromUtf8(QJsonDocument(defaults).toJson(QJsonDocument::Compact));t.repeat=r;t.instanceOverridden=false;
        }
    }
    const auto valid=validate(t);
    if(!valid.ok()) return {Task{},valid.error};
    if(!t.recurrenceId.isEmpty() && !t.repeatChanged && t!=existing.value)t.instanceOverridden=true;
    t.updatedAtUtc=m_clock.nowUtc().toUTC().toString(Qt::ISODateWithMs);
    return m_repository.update(t,version);
}

Result<Task> TaskService::moveToView(const QString& id,qint64 version,const QString& view,const QDate& start)
{
    auto existing=m_repository.find(id);
    if(!existing.ok())return existing;
    auto task=existing.value;
    if(task.rowVersion!=version)return Result<Task>::failure(ErrorCode::Conflict,QStringLiteral("conflict"));
    if(task.status!=QStringLiteral("OPEN") || !task.deletedAtUtc.isEmpty())return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("actionInvalid"));
    if(view==QStringLiteral("logbook") || view==QStringLiteral("completed"))return changeState(id,version,QStringLiteral("complete"));
    if(view==QStringLiteral("inbox")){TaskPatch destination;destination.projectId=FieldPatch<QString>::clear();destination.areaId=FieldPatch<QString>::clear();destination.headingId=FieldPatch<QString>::clear();return edit(id,version,destination);}
    task.todayFocus=false;task.evening=false;
    if(view==QStringLiteral("inbox")){task.planningState=QStringLiteral("INBOX");task.startOn.reset();task.deadlineOn.reset();}
    else if(view==QStringLiteral("someday"))task.planningState=QStringLiteral("SOMEDAY");
    else if(view==QStringLiteral("important")){task.important=true;task.todayFocus=existing.value.todayFocus;task.evening=existing.value.evening;}
    else if(view==QStringLiteral("today")){
        task.planningState=QStringLiteral("ACTIVE");task.todayFocus=true;
        if(!task.schedule && task.startOn && *task.startOn>m_clock.localToday())task.startOn=m_clock.localToday();
    } else if(view==QStringLiteral("upcoming")){
        if(!start.isValid() || start<=m_clock.localToday())return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("futureStartRequired"));
        task.planningState=QStringLiteral("ACTIVE");task.startOn=start;
        TaskPatch dateChange;dateChange.startOn=FieldPatch<QDate>::set(start);const auto timing=synchronizeTime(task,existing.value,dateChange);if(!timing.ok())return {{},timing.error};
    } else if(view==QStringLiteral("anytime")){
        task.planningState=QStringLiteral("ACTIVE");
        if(task.startOn && *task.startOn>m_clock.localToday())task.startOn.reset();
    } else return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("actionInvalid"));
    const auto valid=validate(task);if(!valid.ok())return {Task{},valid.error};
    if(!QueryService::inView(task,view,m_clock.localToday()))return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("destinationUnavailable"));
    TaskPatch patch;patch.important=FieldPatch<bool>::set(task.important);patch.planningState=FieldPatch<QString>::set(task.planningState);
    patch.startOn=task.startOn?FieldPatch<QDate>::set(*task.startOn):FieldPatch<QDate>::clear();
    patch.deadlineOn=task.deadlineOn?FieldPatch<QDate>::set(*task.deadlineOn):FieldPatch<QDate>::clear();
    if(task.schedule!=existing.value.schedule && task.schedule)patch.schedule=FieldPatch<TaskSchedule>::set(*task.schedule);
    patch.todayFocus=FieldPatch<bool>::set(task.todayFocus);patch.evening=FieldPatch<bool>::set(task.evening);
    return edit(id,version,patch);
}

Result<Task> TaskService::changeStateOne(const QString& id,qint64 version,const QString& action,const QString& group)
{
    auto existing=m_repository.find(id);
    if(!existing.ok()) return existing;
    auto t=existing.value;
    if(t.rowVersion!=version) return Result<Task>::failure(ErrorCode::Conflict,QStringLiteral("conflict"));
    if(!t.deletedAtUtc.isEmpty() && action!=QStringLiteral("restore")) return Result<Task>::failure(ErrorCode::NotFound,QStringLiteral("notFound"));
    const auto now=m_clock.nowUtc().toUTC().toString(Qt::ISODateWithMs);
    if(action==QStringLiteral("reopen") || (action==QStringLiteral("restore") && t.status==QStringLiteral("OPEN"))) {
        const auto sibling=m_repository.hasOpenCompletionSibling(t);if(!sibling.ok())return {{},sibling.error};
        if(sibling.value)return Result<Task>::failure(ErrorCode::Conflict,QStringLiteral("recurrenceOpenInstance"));
    }
    if((action==QStringLiteral("complete") && t.status==QStringLiteral("COMPLETED"))
        || (action==QStringLiteral("cancel") && t.status==QStringLiteral("CANCELLED"))
        || (action==QStringLiteral("reopen") && t.status==QStringLiteral("OPEN")))return existing;
    if(action==QStringLiteral("complete")) { t.status=QStringLiteral("COMPLETED"); t.completionGroupId=group; t.completedAtUtc=now; t.cancelledAtUtc.clear(); }
    else if(action==QStringLiteral("cancel")) { t.status=QStringLiteral("CANCELLED"); t.cancelledAtUtc=now; t.completionGroupId.clear(); t.completedAtUtc.clear(); }
    else if(action==QStringLiteral("reopen")) { t.status=QStringLiteral("OPEN"); t.completionGroupId.clear(); t.completedAtUtc.clear(); t.cancelledAtUtc.clear(); }
    else if(action==QStringLiteral("delete")) {t.deletedAtUtc=now;t.deletionGroupId=group;}
    else if(action==QStringLiteral("restore")) {t.deletedAtUtc.clear();t.deletionGroupId.clear();if(!t.parentTaskId.isEmpty()){const auto parent=m_repository.find(t.parentTaskId);if(!parent.ok() || !parent.value.deletedAtUtc.isEmpty())t.parentTaskId.clear();}if(t.projectDeleted || t.areaArchived){t.projectId.clear();t.areaId.clear();t.headingId.clear();}}
    else return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("actionInvalid"));
    if(!t.recurrenceId.isEmpty() && (action==QStringLiteral("delete") || action==QStringLiteral("restore")))t.instanceOverridden=true;
    t.updatedAtUtc=now;
    const auto valid=validate(t);
    if(!valid.ok()) return {Task{},valid.error};
    return m_repository.update(t,version);
}

Result<void> TaskService::batch(const QList<QPair<QString,qint64>>& selection,const QString& action,const TaskPatch& patch)
{
    if(selection.isEmpty() || (action!=QStringLiteral("complete") && action!=QStringLiteral("reopen") && action!=QStringLiteral("edit") && action!=QStringLiteral("delete") && action!=QStringLiteral("restore")))
        return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("actionInvalid"));
    return m_repository.atomically([&] {
        QSet<QString> seen;
        // Validate the entire user's snapshot before our own cascades increment descendants.
        for(const auto& item:selection){if(seen.contains(item.first))return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("actionInvalid"));seen.insert(item.first);const auto current=m_repository.find(item.first);if(!current.ok())return Result<void>{current.error};if(current.value.rowVersion!=item.second)return Result<void>::failure(ErrorCode::Conflict,QStringLiteral("conflict"));if((action==QStringLiteral("delete") && !current.value.deletedAtUtc.isEmpty()) || (action==QStringLiteral("restore") && current.value.deletedAtUtc.isEmpty()))return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("actionInvalid"));}
        const auto snapshot=m_repository.all();if(!snapshot.ok())return Result<void>{snapshot.error};QHash<QString,QString> parents;for(const auto& task:snapshot.value)parents.insert(task.id,task.parentTaskId);
        auto depth=[&](const QString& id){int value=0;QSet<QString> ancestors;auto parent=parents.value(id);while(!parent.isEmpty() && !ancestors.contains(parent)){ancestors.insert(parent);++value;parent=parents.value(parent);}return value;};
        auto ordered=selection;std::stable_sort(ordered.begin(),ordered.end(),[&](const auto& a,const auto& b){return depth(a.first)<depth(b.first);});
        for(const auto& item:ordered) {
            const auto current=m_repository.find(item.first);if(!current.ok())return Result<void>{current.error};
            // An earlier selected ancestor already applied this tree command.
            if((action==QStringLiteral("delete") && !current.value.deletedAtUtc.isEmpty()) || (action==QStringLiteral("restore") && current.value.deletedAtUtc.isEmpty()))continue;
            const auto result=action!=QStringLiteral("edit")?changeState(item.first,current.value.rowVersion,action):edit(item.first,current.value.rowVersion,patch);
            if(!result.ok())return Result<void>{result.error};
        }
        return Result<void>{};
    });
}

} // namespace daymark
