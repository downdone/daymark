#include "TaskService.h"
#include <QUuid>
#include <algorithm>
#include <QSet>
#include <climits>

namespace daymark {

bool QueryService::effectiveActive(const Task& t)
{
    return t.deletedAtUtc.isEmpty() && t.status==QStringLiteral("OPEN") && t.planningState==QStringLiteral("ACTIVE")
        && (t.projectId.isEmpty() || (!t.projectDeleted && t.projectStatus==QStringLiteral("OPEN") && t.projectPlanning==QStringLiteral("ACTIVE")));
}
bool QueryService::inView(const Task& t, const QString& view, const QDate& d)
{
    if(!t.deletedAtUtc.isEmpty()) return false;
    if(view==QStringLiteral("logbook")) return t.status!=QStringLiteral("OPEN");
    if(t.status!=QStringLiteral("OPEN")) return false;
    const bool projectOpen=t.projectId.isEmpty() || (!t.projectDeleted && t.projectStatus==QStringLiteral("OPEN"));
    if(view.startsWith(QStringLiteral("project:")))return t.projectId==view.mid(8) && projectOpen;
    if(view.startsWith(QStringLiteral("area:")))return (t.areaId==view.mid(5) || t.projectAreaId==view.mid(5)) && projectOpen;
    if(view.startsWith(QStringLiteral("tag:")))return t.tagIds.contains(view.mid(4)) && projectOpen;
    if(view==QStringLiteral("inbox")) return projectOpen && t.planningState==QStringLiteral("INBOX");
    if(view==QStringLiteral("someday")) return projectOpen && (t.planningState==QStringLiteral("SOMEDAY") || t.projectPlanning==QStringLiteral("SOMEDAY"));
    if(!effectiveActive(t)) return false;
    if(view==QStringLiteral("today")) return t.todayFocus || (t.startOn && *t.startOn<=d) || (t.deadlineOn && *t.deadlineOn<=d);
    if(view==QStringLiteral("upcoming")) return t.startOn && *t.startOn>d;
    if(view==QStringLiteral("deadlines"))return t.deadlineOn && *t.deadlineOn>d && (!t.startOn || *t.startOn<=d);
    if(view==QStringLiteral("anytime")) return !t.startOn || *t.startOn<=d;
    return false;
}
QList<Task> QueryService::select(const QList<Task>& tasks, const QString& view, const QDate& d, const QString& search,bool includeCompleted,const QString& tagFilter,int upcomingDays)
{
    QList<Task> output;
    for(const auto& task:tasks) {
        const bool globalSearch=!search.isEmpty() && view!=QStringLiteral("logbook");
        if(globalSearch) {
            if(!task.deletedAtUtc.isEmpty() || (!includeCompleted && task.status!=QStringLiteral("OPEN"))
                || (task.status==QStringLiteral("OPEN") && !task.projectId.isEmpty() && (task.projectDeleted || task.projectStatus!=QStringLiteral("OPEN"))))continue;
        } else if(includeCompleted && view.startsWith(QStringLiteral("project:"))) {
            if(!task.deletedAtUtc.isEmpty() || task.projectId!=view.mid(8))continue;
        } else if(!inView(task,view,d)) continue;
        if(!tagFilter.isEmpty() && !task.tagIds.contains(tagFilter))continue;
        if(search.isEmpty() && upcomingDays>0 && ((view==QStringLiteral("upcoming") && task.startOn && *task.startOn>d.addDays(upcomingDays))
            || (view==QStringLiteral("deadlines") && task.deadlineOn && *task.deadlineOn>d.addDays(upcomingDays))))continue;
        if(!search.isEmpty() && !task.title.contains(search,Qt::CaseInsensitive) && !task.notes.contains(search,Qt::CaseInsensitive)) continue;
        output.append(task);
    }
    std::stable_sort(output.begin(),output.end(),[&view](const Task& a,const Task& b) {
        if(view==QStringLiteral("logbook")) {
            const auto at=a.completedAtUtc.isEmpty()?a.cancelledAtUtc:a.completedAtUtc;
            const auto bt=b.completedAtUtc.isEmpty()?b.cancelledAtUtc:b.completedAtUtc;
            if(at!=bt) return at>bt;
        }
        if(view==QStringLiteral("upcoming") && a.startOn!=b.startOn) return a.startOn<b.startOn;
        if(view==QStringLiteral("deadlines") && a.deadlineOn!=b.deadlineOn)return a.deadlineOn<b.deadlineOn;
        if(view.startsWith(QStringLiteral("project:"))) {
            if(a.headingOrder!=b.headingOrder)return a.headingOrder<b.headingOrder;
            if(a.headingId!=b.headingId)return a.headingId<b.headingId;
        }
        if(view==QStringLiteral("today")) {
            if(a.evening!=b.evening)return !a.evening;
            if(a.todayOrder!=b.todayOrder)return (a.todayOrder<0?LLONG_MAX:a.todayOrder)<(b.todayOrder<0?LLONG_MAX:b.todayOrder);
        }
        if(a.sortOrder!=b.sortOrder) return a.sortOrder<b.sortOrder;
        return a.id<b.id;
    });
    return output;
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
    if(t.planningState==QStringLiteral("INBOX") && (t.startOn || t.deadlineOn || t.todayFocus))
        t.planningState=QStringLiteral("ACTIVE");
    for(auto& item:t.checklist) {
        item.title=item.title.trimmed();
        if(item.title.isEmpty() || item.title.toUcs4().size()>512)return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("titleInvalid"));
    }
    if(m_catalog){const auto result=m_catalog->validateTask(t);if(!result.ok())return result;}
    if(!QueryService::inView(t,QStringLiteral("today"),m_clock.localToday())) t.evening=false;
    return {};
}

Result<Task> TaskService::create(const QString& title, const QString& view)
{
    Task t; t.id=QUuid::createUuid().toString(QUuid::WithoutBraces); t.title=title;
    t.notes=QStringLiteral("");
    t.createdAtUtc=m_clock.nowUtc().toUTC().toString(Qt::ISODateWithMs); t.updatedAtUtc=t.createdAtUtc;
    if(view==QStringLiteral("today")) { t.planningState=QStringLiteral("ACTIVE"); t.todayFocus=true; }
    else if(view==QStringLiteral("anytime")) t.planningState=QStringLiteral("ACTIVE");
    else if(view==QStringLiteral("someday")) t.planningState=QStringLiteral("SOMEDAY");
    else if(view==QStringLiteral("upcoming")) {t.planningState=QStringLiteral("ACTIVE");t.startOn=m_clock.localToday().addDays(1);}
    else if(view.startsWith(QStringLiteral("project:"))) {t.planningState=QStringLiteral("ACTIVE");t.projectId=view.mid(8);}
    else if(view.startsWith(QStringLiteral("area:"))) {t.planningState=QStringLiteral("ACTIVE");t.areaId=view.mid(5);}
    else if(view.startsWith(QStringLiteral("date:"))) {t.planningState=QStringLiteral("ACTIVE");t.startOn=QDate::fromString(view.mid(5),Qt::ISODate);}
    const auto valid=validate(t);
    if(!valid.ok()) return {Task{},valid.error};
    return m_repository.insert(t);
}

Result<Task> TaskService::edit(const QString& id, qint64 version, const TaskPatch& patch)
{
    auto existing=m_repository.find(id);
    if(!existing.ok()) return existing;
    auto t=existing.value;
    if(!t.recurrenceId.isEmpty())t.instanceOverridden=true;
    if(t.rowVersion!=version) return Result<Task>::failure(ErrorCode::Conflict,QStringLiteral("conflict"));
    if(!t.deletedAtUtc.isEmpty()) return Result<Task>::failure(ErrorCode::NotFound,QStringLiteral("notFound"));
    auto applyString=[](QString& target,const FieldPatch<QString>& p) {
        if(p.kind==FieldPatch<QString>::Kind::Set) target=p.value;
        else if(p.kind==FieldPatch<QString>::Kind::Clear) target=QStringLiteral("");
    };
    applyString(t.title,patch.title); applyString(t.notes,patch.notes); applyString(t.planningState,patch.planningState);
    applyString(t.projectId,patch.projectId);applyString(t.areaId,patch.areaId);applyString(t.headingId,patch.headingId);
    if(patch.projectId.kind==FieldPatch<QString>::Kind::Set && !t.projectId.isEmpty())t.areaId.clear();
    if(patch.areaId.kind==FieldPatch<QString>::Kind::Set && !t.areaId.isEmpty()){t.projectId.clear();t.headingId.clear();}
    if(t.projectId!=existing.value.projectId && patch.headingId.kind==FieldPatch<QString>::Kind::Unspecified)t.headingId.clear();
    if(patch.tagIds.kind==FieldPatch<QStringList>::Kind::Set)t.tagIds=patch.tagIds.value;
    else if(patch.tagIds.kind==FieldPatch<QStringList>::Kind::Clear)t.tagIds.clear();
    if(patch.checklist.kind==FieldPatch<QList<ChecklistItem>>::Kind::Set)t.checklist=patch.checklist.value;
    else if(patch.checklist.kind==FieldPatch<QList<ChecklistItem>>::Kind::Clear)t.checklist.clear();
    QSet<QString> checklistIds;
    for(auto& item:t.checklist){
        if(item.id.isEmpty())item.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        if(checklistIds.contains(item.id))return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("checklistInvalid"));
        checklistIds.insert(item.id);
    }
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
    if((!t.projectId.isEmpty() || !t.areaId.isEmpty()) && t.planningState==QStringLiteral("INBOX"))t.planningState=QStringLiteral("ACTIVE");
    if(patch.todayFocus.kind==FieldPatch<bool>::Kind::Set) t.todayFocus=patch.todayFocus.value;
    else if(patch.todayFocus.kind==FieldPatch<bool>::Kind::Clear) t.todayFocus=false;
    if(t.planningState!=QStringLiteral("SOMEDAY") && t.todayFocus && t.startOn && *t.startOn>m_clock.localToday()) t.startOn=m_clock.localToday();
    const auto valid=validate(t);
    if(!valid.ok()) return {Task{},valid.error};
    t.updatedAtUtc=m_clock.nowUtc().toUTC().toString(Qt::ISODateWithMs);
    return m_repository.update(t,version);
}

Result<Task> TaskService::changeState(const QString& id,qint64 version,const QString& action)
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
    if(action==QStringLiteral("complete")) { t.status=QStringLiteral("COMPLETED"); t.completedAtUtc=now; t.cancelledAtUtc.clear(); }
    else if(action==QStringLiteral("cancel")) { t.status=QStringLiteral("CANCELLED"); t.cancelledAtUtc=now; t.completedAtUtc.clear(); }
    else if(action==QStringLiteral("reopen")) { t.status=QStringLiteral("OPEN"); t.completedAtUtc.clear(); t.cancelledAtUtc.clear(); }
    else if(action==QStringLiteral("delete")) t.deletedAtUtc=now;
    else if(action==QStringLiteral("restore")) t.deletedAtUtc.clear();
    else return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("actionInvalid"));
    if(!t.recurrenceId.isEmpty() && (action==QStringLiteral("delete") || action==QStringLiteral("restore")))t.instanceOverridden=true;
    t.updatedAtUtc=now;
    const auto valid=validate(t);
    if(!valid.ok()) return {Task{},valid.error};
    return m_repository.update(t,version);
}

} // namespace daymark
