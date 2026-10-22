#include "CalendarService.h"
#include <algorithm>
#include <QHash>

namespace daymark {
QList<CalendarEntry> CalendarService::entries(const QList<Task>& tasks,const QDate& date)
{
    QList<CalendarEntry> result;
    if(!date.isValid())return result;
    for(const auto& task:tasks) {
        if(!QueryService::effectiveActive(task))continue;
        const bool start=task.startOn && *task.startOn==date;
        const bool deadline=task.deadlineOn && *task.deadlineOn==date;
        if(start || deadline)result.append({task,start?(deadline?QStringLiteral("both"):QStringLiteral("start")):QStringLiteral("deadline")});
    }
    std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){
        if(a.task.sortOrder!=b.task.sortOrder)return a.task.sortOrder<b.task.sortOrder;
        return a.task.id<b.task.id;
    });
    return result;
}
QList<CalendarDay> CalendarService::days(const QList<Task>& tasks,const QDate& anchor,bool week,bool sundayFirst)
{
    QList<CalendarDay> result;
    if(!anchor.isValid())return result;
    const QDate first=week?anchor:QDate(anchor.year(),anchor.month(),1);
    const int offset=sundayFirst?first.dayOfWeek()%7:first.dayOfWeek()-1;
    const auto from=first.addDays(-offset);
    // Index dates once: a month refresh is O(tasks + visible days), not 42 full scans.
    QHash<QDate,QList<Task>> indexed;
    for(const auto& task:tasks) {
        if(!QueryService::effectiveActive(task))continue;
        if(task.startOn)indexed[*task.startOn].append(task);
        if(task.deadlineOn && task.deadlineOn!=task.startOn)indexed[*task.deadlineOn].append(task);
    }
    for(int i=0;i<(week?7:42);++i) {
        const auto date=from.addDays(i);
        result.append({date,date.month()==anchor.month() && date.year()==anchor.year(),entries(indexed.value(date),date)});
    }
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
