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
QString CalendarService::timeSummary(const Task& input,const QDate& today,const QTimeZone& zone)
{
    const auto task=QueryService::withUnifiedDates(input);
    auto dateText=[today](const QDate& date){return date.toString(date.year()==today.year()?QStringLiteral("M-d"):QStringLiteral("yyyy-M-d"));};
    if(task.schedule && !task.schedule->allDay){const auto start=task.schedule->startUtc.toTimeZone(zone),end=task.schedule->endUtc.toTimeZone(zone);return dateText(start.date())+QStringLiteral(" ")+start.time().toString(QStringLiteral("HH:mm"))+QStringLiteral("–")+(start.date()==end.date()?QString():dateText(end.date())+QStringLiteral(" "))+end.time().toString(QStringLiteral("HH:mm"));}
    if(task.startOn && task.deadlineOn && *task.startOn!=*task.deadlineOn)return dateText(*task.startOn)+QStringLiteral(" → ")+dateText(*task.deadlineOn);
    if(task.startOn)return dateText(*task.startOn);
    if(task.deadlineOn)return QStringLiteral("结束 ")+dateText(*task.deadlineOn);
    return {};
}
QList<CalendarTimeBlock> CalendarService::timeBlocks(const QList<Task>& tasks,const QDate& from,int days,const QTimeZone& zone,bool includeCompleted)
{
    QList<CalendarTimeBlock> result;if(!from.isValid() || !zone.isValid() || days<1 || days>7)return result;
    for(const auto& task:tasks){if(!eligible(task,includeCompleted) || !task.schedule || task.schedule->allDay)continue;const auto& schedule=*task.schedule;
        for(int day=0;day<days;++day){const auto date=from.addDays(day);const auto start=date.startOfDay(zone),end=date.addDays(1).startOfDay(zone);if(!start.isValid() || !end.isValid() || schedule.startUtc>=end || schedule.endUtc<=start)continue;
            result.append({task,date,start.msecsTo(qMax(start,schedule.startUtc))/60000.0,start.msecsTo(qMin(end,schedule.endUtc))/60000.0,0,1,schedule.startUtc<start,schedule.endUtc>end});}
    }
    std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){if(a.date!=b.date)return a.date<b.date;if(a.startMinute!=b.startMinute)return a.startMinute<b.startMinute;if(a.endMinute!=b.endMinute)return a.endMinute>b.endMinute;return a.task.id<b.task.id;});
    for(int first=0;first<result.size();){int last=first+1;double end=result[first].endMinute;while(last<result.size() && result[last].date==result[first].date && result[last].startMinute<end){end=qMax(end,result[last].endMinute);++last;}
        QList<double> lanes;for(int i=first;i<last;++i){int lane=0;while(lane<lanes.size() && lanes[lane]>result[i].startMinute)++lane;if(lane==lanes.size())lanes.append(0);lanes[lane]=result[i].endMinute;result[i].lane=lane;}
        for(int i=first;i<last;++i){result[i].laneCount=lanes.size();result[i].cluster=first;}first=last;
    }return result;
}
Result<Task> CalendarService::moveTime(TaskService& service,const Task& before,qint64 version,const QDate& date,int minute,const QString& edge,const QTimeZone& zone)
{
    if(!date.isValid() || !zone.isValid() || minute<0)return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("dateInvalid"));
    const auto day=date.startOfDay(zone),next=date.addDays(1).startOfDay(zone);if(!day.isValid() || minute>day.secsTo(next)/60)return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("dateInvalid"));
    const auto target=day.addSecs(minute*60).toUTC();TaskSchedule schedule=before.schedule.value_or(TaskSchedule{});
    const bool hadTime=before.schedule && !schedule.allDay;const auto duration=hadTime?schedule.startUtc.msecsTo(schedule.endUtc):3600000LL;
    schedule.allDay=false;if(schedule.timeZone.isEmpty())schedule.timeZone=QString::fromUtf8(zone.id());
    if(edge==QStringLiteral("move")){schedule.startUtc=target;schedule.endUtc=target.addMSecs(duration);}
    else if(hadTime && edge==QStringLiteral("start"))schedule.startUtc=target;
    else if(hadTime && edge==QStringLiteral("end"))schedule.endUtc=target;
    else return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("dateInvalid"));
    if(schedule.endUtc<=schedule.startUtc)return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("dateRange"));
    TaskPatch patch;patch.schedule=FieldPatch<TaskSchedule>::set(schedule);return service.edit(before.id,version,patch);
}
Result<Task> CalendarService::moveDate(TaskService& service,const Task& before,qint64 version,const QDate& from,const QDate& date,const QString& edge,const QTimeZone& zone)
{
    if(!date.isValid() || !from.isValid() || !zone.isValid())return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("dateInvalid"));
    if(edge==QStringLiteral("start") || edge==QStringLiteral("deadline"))return move(service,before.id,version,edge,date);
    if(edge!=QStringLiteral("move"))return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("calendarAmbiguous"));
    const auto delta=from.daysTo(date);TaskPatch patch;
    if(before.planningState==QStringLiteral("INBOX"))patch.planningState=FieldPatch<QString>::set(QStringLiteral("ACTIVE"));
    if(before.schedule){auto schedule=*before.schedule;
        if(schedule.allDay){schedule.startOn=schedule.startOn.addDays(delta);schedule.endOn=schedule.endOn.addDays(delta);}
        else {
            const auto start=schedule.startUtc.toTimeZone(zone);
            const QDateTime target(start.date().addDays(delta),start.time(),zone,QDateTime::TransitionResolution::Reject);
            if(!target.isValid())return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("dateInvalid"));
            const auto duration=schedule.startUtc.msecsTo(schedule.endUtc);schedule.startUtc=target.toUTC();schedule.endUtc=schedule.startUtc.addMSecs(duration);
        }
        patch.schedule=FieldPatch<TaskSchedule>::set(schedule);
    }else {
        if(before.startOn)patch.startOn=FieldPatch<QDate>::set(before.startOn->addDays(delta));
        if(before.deadlineOn)patch.deadlineOn=FieldPatch<QDate>::set(before.deadlineOn->addDays(delta));
        if(!before.startOn && !before.deadlineOn)patch.startOn=FieldPatch<QDate>::set(date);
    }
    return service.edit(before.id,version,patch);
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
    for(const auto& task:tasks){auto eligibleTask=task;if(eligibleTask.planningState==QStringLiteral("INBOX"))eligibleTask.planningState=QStringLiteral("ACTIVE");if(QueryService::effectiveActive(eligibleTask) && !task.schedule && !task.startOn && !task.deadlineOn)result.append(task);}
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
