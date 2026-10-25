#include "CalendarService.h"
#include <algorithm>
#include <QHash>

namespace daymark {
namespace {
bool eligible(const Task& task,bool includeCompleted) {
    if(QueryService::effectiveActive(task))return true;
    if(!includeCompleted || task.status!=QStringLiteral("COMPLETED"))return false;
    auto open=task;open.status=QStringLiteral("OPEN");return QueryService::effectiveActive(open);
}
}
QList<CalendarTaskRange> CalendarService::ranges(const QList<Task>& tasks,const QDate& from,const QDate& to,bool includeCompleted)
{
    QList<CalendarTaskRange> result;
    if(!from.isValid() || !to.isValid() || from>to)return result;
    for(const auto& task:tasks)if(!task.schedule && eligible(task,includeCompleted) && task.startOn && task.deadlineOn
        && task.startOn->isValid() && task.deadlineOn->isValid() && *task.startOn<*task.deadlineOn
        && *task.startOn<=to && *task.deadlineOn>=from)result.append({task,*task.startOn,*task.deadlineOn});
    return result;
}
QList<CalendarEntry> CalendarService::entries(const QList<Task>& tasks,const QDate& date,bool includeCompleted)
{
    QList<CalendarEntry> result;
    if(!date.isValid())return result;
    for(const auto& task:tasks) {
        if(task.schedule || !eligible(task,includeCompleted))continue;
        const bool start=task.startOn && *task.startOn==date;
        const bool deadline=task.deadlineOn && *task.deadlineOn==date;
        const bool interior=task.startOn && task.deadlineOn && *task.startOn<date && date<*task.deadlineOn;
        if(start || deadline || interior)result.append({task,start?(deadline?QStringLiteral("both"):QStringLiteral("start")):deadline?QStringLiteral("deadline"):QStringLiteral("range")});
    }
    std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){
        if(a.task.sortOrder!=b.task.sortOrder)return a.task.sortOrder<b.task.sortOrder;
        return a.task.id<b.task.id;
    });
    return result;
}
QList<CalendarDay> CalendarService::days(const QList<Task>& tasks,const QDate& anchor,bool week,bool sundayFirst,bool includeCompleted)
{
    QList<CalendarDay> result;
    if(!anchor.isValid())return result;
    const QDate first=week?anchor:QDate(anchor.year(),anchor.month(),1);
    const int offset=sundayFirst?first.dayOfWeek()%7:first.dayOfWeek()-1;
    const auto from=first.addDays(-offset);
    const auto to=from.addDays(week?6:41);
    // Index only the visible range: O(tasks + emitted date references), not full scans per day.
    QHash<QDate,QList<Task>> indexed;
    for(const auto& task:tasks) {
        if(task.schedule || !eligible(task,includeCompleted))continue;
        if(task.startOn)indexed[*task.startOn].append(task);
        if(task.deadlineOn && task.deadlineOn!=task.startOn)indexed[*task.deadlineOn].append(task);
        if(task.startOn && task.deadlineOn && *task.startOn<*task.deadlineOn)
            for(auto date=qMax(from,task.startOn->addDays(1)),last=qMin(to,task.deadlineOn->addDays(-1));date<=last;date=date.addDays(1))indexed[date].append(task);
    }
    for(int i=0;i<(week?7:42);++i) {
        const auto date=from.addDays(i);
        result.append({date,date.month()==anchor.month() && date.year()==anchor.year(),entries(indexed.value(date),date,includeCompleted)});
    }
    return result;
}
QList<Task> CalendarService::unscheduled(const QList<Task>& tasks)
{
    QList<Task> result;
    for(const auto& task:tasks)if(QueryService::effectiveActive(task) && !task.schedule && !task.startOn && !task.deadlineOn)result.append(task);
    return result;
}
Result<Task> CalendarService::move(TaskService& service,const QString& id,qint64 version,const QString& kind,const QDate& date)
{
    if(!date.isValid())return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("dateInvalid"));
    TaskPatch patch;
    if(kind==QStringLiteral("start"))patch.startOn=FieldPatch<QDate>::set(date);
    else if(kind==QStringLiteral("deadline"))patch.deadlineOn=FieldPatch<QDate>::set(date);
    else return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("calendarAmbiguous"));
    return service.edit(id,version,patch);
}
}
