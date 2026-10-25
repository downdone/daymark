#include "TaskService.h"
#include "EventService.h"
#include <QTimeZone>

namespace daymark {
bool QueryService::isOverdue(const Task& task,const QDate& today)
{
    const auto end=task.schedule?(task.schedule->allDay?task.schedule->endOn.addDays(-1):task.schedule->endUtc.toLocalTime().date()):task.deadlineOn.value_or(QDate());
    return effectiveActive(task) && end.isValid() && end<today;
}
Task QueryService::withUnifiedDates(const Task& input)
{
    auto task=input;
    if(task.schedule) {
        const auto& s=*task.schedule;
        if(s.allDay){task.startOn=s.startOn;task.deadlineOn=s.endOn.addDays(-1);}
        else {const QTimeZone zone(s.timeZone.toUtf8());task.startOn=s.startUtc.toTimeZone(zone).date();task.deadlineOn=s.endUtc.toTimeZone(zone).date();}
    }
    return task;
}
Result<void> TaskService::synchronizeTime(Task& task,const Task& before,const TaskPatch& patch) const
{
    const bool supplied=patch.schedule.kind==FieldPatch<TaskSchedule>::Kind::Set;
    const bool startChanged=task.startOn!=before.startOn,endChanged=task.deadlineOn!=before.deadlineOn;
    if(!task.schedule)return {};
    auto& schedule=*task.schedule;
    if(!supplied && (startChanged || endChanged)) {
        const auto prior=QueryService::withUnifiedDates(before);
        if((startChanged && !task.startOn) || (endChanged && !task.deadlineOn)){task.schedule.reset();return {};}
        const auto start=startChanged?task.startOn:prior.startOn;
        auto end=endChanged?task.deadlineOn:prior.deadlineOn;
        if(startChanged && patch.deadlineOn.kind==FieldPatch<QDate>::Kind::Unspecified && start && prior.startOn && end)end=end->addDays(prior.startOn->daysTo(*start));
        if(!start || !end)return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("dateInvalid"));
        if(schedule.allDay){schedule.startOn=*start;schedule.endOn=end->addDays(1);}
        else {
            const QTimeZone zone(schedule.timeZone.toUtf8());
            const auto at=[&](const QDate& date,const QDateTime& original){return EventService::localInstant(date.toString(Qt::ISODate)+QLatin1Char(' ')+original.toTimeZone(zone).time().toString(QStringLiteral("HH:mm:ss.zzz")),schedule.timeZone);};
            const auto first=at(*start,schedule.startUtc),last=at(*end,schedule.endUtc);
            if(!first.ok() || !last.ok())return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("eventTime"));
            schedule.startUtc=first.value;schedule.endUtc=last.value;
        }
    }
    if(supplied || startChanged || endChanged) {
        const auto canonical=QueryService::withUnifiedDates(task);task.startOn=canonical.startOn;task.deadlineOn=canonical.deadlineOn;
        if(schedule.alertMinutes>=0 && patch.remindAtUtc.kind==FieldPatch<QString>::Kind::Unspecified){const auto at=schedule.allDay?QDateTime(schedule.startOn,QTime(9,0),QTimeZone::systemTimeZone(),QDateTime::TransitionResolution::Reject):schedule.startUtc;task.remindAtUtc=at.addSecs(-60*schedule.alertMinutes).toUTC().toString(Qt::ISODateWithMs);task.reminderFiredAtUtc.clear();}
    }
    return {};
}
}
