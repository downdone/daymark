#include "TaskCycleService.h"

#include "RecurrenceService.h"
#include "TaskService.h"
#include <QCryptographicHash>
#include <QTimeZone>

namespace daymark {
namespace {
QString utc(const QDateTime& instant){return instant.isValid()?instant.toUTC().toString(Qt::ISODateWithMs):QString();}
Error invalid(){return {ErrorCode::ValidationError,QStringLiteral("cycleInvalid")};}
Error conflict(){return {ErrorCode::Conflict,QStringLiteral("cycleConflict")};}

struct Slot {
    QDateTime start,end;
    std::optional<TaskSchedule> schedule;
};

Result<Slot> slotOn(const Task& task,const QDate& date,const QTimeZone& zone)
{
    if(!date.isValid() || !zone.isValid())return {{},invalid()};
    Slot slot;
    const auto midnight=[&](const QDate& day){return day.startOfDay(zone).toUTC();};
    if(!task.schedule){
        slot.start=midnight(date);
        if(task.deadlineOn && task.startOn)slot.end=midnight(date.addDays(task.startOn->daysTo(*task.deadlineOn)+1));
        return slot.start.isValid()?Result<Slot>{slot,{}}:Result<Slot>{{},invalid()};
    }
    slot.schedule=*task.schedule;
    auto& schedule=*slot.schedule;
    if(schedule.allDay){
        schedule.endOn=date.addDays(schedule.startOn.daysTo(schedule.endOn));schedule.startOn=date;
        slot.start=midnight(date);slot.end=midnight(schedule.endOn);
    }else{
        const auto first=schedule.startUtc.toTimeZone(zone),last=schedule.endUtc.toTimeZone(zone);
        const auto following=date.addDays(first.date().daysTo(last.date()));
        slot.start=QDateTime(date,first.time(),zone,QDateTime::TransitionResolution::Reject).toUTC();
        slot.end=QDateTime(following,last.time(),zone,QDateTime::TransitionResolution::Reject).toUTC();
        schedule.startUtc=slot.start;schedule.endUtc=slot.end;
    }
    return slot.start.isValid() && slot.end.isValid() && slot.end>slot.start ? Result<Slot>{slot,{}} : Result<Slot>{{},invalid()};
}
void reopenTask(Task& task,const QDateTime& now)
{
    task.status=QStringLiteral("OPEN");task.completedAtUtc.clear();task.completionGroupId.clear();
    task.todayFocus=false;task.evening=false;task.updatedAtUtc=utc(now);
}
QString relativeReminder(const Task& task,const QDate& date,const QDateTime& start,const QTimeZone& zone)
{
    if(!task.schedule || task.schedule->alertMinutes<0 || !start.isValid())return {};
    const auto base=task.schedule->allDay?QDateTime(date,QTime(9,0),zone,QDateTime::TransitionResolution::Reject):start;
    return utc(base.addSecs(-60*task.schedule->alertMinutes));
}
}

QString TaskCycleService::occurrenceKey(const QString& taskId,const TaskCycle& cycle)
{
    const auto key=taskId+QLatin1Char('/')+QString::number(cycle.ruleRevision)+QLatin1Char('/')+
        cycle.currentDate.toString(Qt::ISODate)+QLatin1Char('/')+utc(cycle.currentStartUtc);
    return QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(),QCryptographicHash::Sha256).toHex());
}

Result<TaskCycle> TaskCycleService::first(const Task& task,RecurrenceRule rule,const QString& zoneId,const QDate& today)
{
    const auto timed=task.schedule && !task.schedule->allDay;
    const auto effectiveZone=timed?task.schedule->timeZone:zoneId;
    const QTimeZone zone(effectiveZone.toUtf8());
    if(!zone.isValid() || !task.parentTaskId.isEmpty() || task.reminderRepeat!=QStringLiteral("NONE") ||
        rule.interval<1 || rule.interval>36500 || rule.weekdays<0 || rule.weekdays>127 ||
        !QStringList{"DAILY","WEEKLY","MONTHLY","YEARLY","AFTER_DAYS"}.contains(rule.frequency))return {{},invalid()};
    const auto current=QueryService::withUnifiedDates(task).startOn.value_or(today);
    if(!current.isValid() || (rule.end.isValid() && rule.end<current))return {{},invalid()};
    rule.anchor=current;rule.monthDay=current.day();rule.yearMonth=current.month();rule.active=true;
    if(rule.frequency=="WEEKLY" && !rule.weekdays)rule.weekdays=1<<(current.dayOfWeek()-1);
    if(rule.frequency!="WEEKLY")rule.weekdays=0;
    const auto slot=slotOn(task,current,zone);if(!slot.ok())return {{},slot.error};
    TaskCycle cycle;cycle.rule=std::move(rule);cycle.zone=effectiveZone;cycle.currentDate=current;
    cycle.currentStartUtc=slot.value.start;cycle.currentEndUtc=slot.value.end;return {cycle,{}};
}

Result<QPair<QDate,QDateTime>> TaskCycleService::next(const Task& task,const TaskCycle& cycle,const QDateTime& after)
{
    const QTimeZone zone(cycle.zone.toUtf8());
    if(!after.isValid() || !zone.isValid())return {{},invalid()};
    const auto afterDate=after.toTimeZone(zone).date();
    const bool afterCompletion=cycle.rule.frequency==QStringLiteral("AFTER_DAYS");
    auto search=afterCompletion?qMax(cycle.currentDate,afterDate).addDays(cycle.rule.interval)
                               :qMax(cycle.currentDate.addDays(1),afterDate);
    for(int retry=0;retry<16;++retry){
        QDate date=search;
        if(afterCompletion){
            if(!date.isValid() || (cycle.rule.end.isValid() && date>cycle.rule.end))return {};
        }else{
            const auto dates=RecurrenceService::nextDates(cycle.rule,search,1);
            if(dates.isEmpty())return {};
            date=dates.first();
        }
        const auto slot=slotOn(task,date,zone);
        if(slot.ok() && slot.value.start>after)return {{date,slot.value.start},{}};
        search=date.addDays(1);
    }
    return {{},invalid()};
}

Result<void> TaskCycleService::configure(const Task& task,const TaskCycle& proposed)
{
    if(task.cycle.value_or(TaskCycle{}).rowVersion!=proposed.rowVersion)return {conflict()};
    if(task.status!=QStringLiteral("OPEN") || !task.recurrenceId.isEmpty())return {invalid()};
    const auto current=first(task,proposed.rule,proposed.zone,m_clock.localToday());if(!current.ok())return {current.error};
    auto cycle=current.value;cycle.ruleRevision=task.cycle ? task.cycle->ruleRevision+1 : 1;
    return m_cycles.save(task.id,cycle,proposed.rowVersion);
}

Result<void> TaskCycleService::stop(const Task& task,qint64 version)
{
    if(!task.cycle || task.cycle->rowVersion!=version)return {conflict()};
    auto cycle=*task.cycle;cycle.rule.active=false;cycle.nextDate={};cycle.nextStartUtc={};
    if(task.status==QStringLiteral("COMPLETED") && task.cycle->nextStartUtc.isValid()){
        auto stopped=task;stopped.remindAtUtc.clear();stopped.reminderFiredAtUtc.clear();stopped.reminderAnchorUtc.clear();
        const auto saved=m_tasks.update(stopped,stopped.rowVersion,false);if(!saved.ok())return {saved.error};
    }
    return m_cycles.save(task.id,cycle,version);
}
Result<void> TaskCycleService::convertLegacy(const Task& task,qint64 version)
{return m_cycles.detachLegacy(task,version,m_clock.nowUtc());}

Result<void> TaskCycleService::complete(const Task& before,const Task& after,const QStringList& members,bool skipped)
{
    if(!before.cycle || !before.cycle->rule.active)return skipped?Result<void>{invalid()}:Result<void>{};
    auto cycle=*before.cycle;const auto now=m_clock.nowUtc();
    const auto nextSlot=next(before,cycle,now);if(!nextSlot.ok())return {nextSlot.error};
    const auto recorded=m_cycles.record(before,cycle,occurrenceKey(before.id,cycle),
        skipped?QStringLiteral("SKIPPED"):QStringLiteral("COMPLETED"),now,after.completionGroupId,members);
    if(!recorded.ok())return recorded;
    cycle.nextDate=nextSlot.value.first;cycle.nextStartUtc=nextSlot.value.second;
    auto task=after;task.remindAtUtc=relativeReminder(task,cycle.nextDate,cycle.nextStartUtc,QTimeZone(cycle.zone.toUtf8()));
    task.reminderFiredAtUtc.clear();task.reminderAnchorUtc=task.remindAtUtc;
    const auto saved=m_tasks.update(task,task.rowVersion,false);if(!saved.ok())return {saved.error};
    return m_cycles.save(before.id,cycle,cycle.rowVersion);
}

Result<void> TaskCycleService::undo(const Task& before)
{
    if(!before.cycle)return {};
    auto cycle=*before.cycle;const auto now=m_clock.nowUtc();
    if(cycle.nextStartUtc.isValid() && cycle.nextStartUtc<=now)return {conflict()};
    const auto revoked=m_cycles.revoke(before.id,occurrenceKey(before.id,cycle),now);
    if(!revoked.ok())return !cycle.rule.active && revoked.error.code==ErrorCode::Conflict?Result<void>{}:Result<void>{revoked.error};
    auto task=before;task.remindAtUtc=revoked.value.first;task.reminderAnchorUtc=task.remindAtUtc;task.reminderFiredAtUtc=revoked.value.second;
    const auto saved=m_tasks.update(task,task.rowVersion,false);if(!saved.ok())return {saved.error};
    cycle.nextDate={};cycle.nextStartUtc={};return m_cycles.save(before.id,cycle,cycle.rowVersion);
}

Result<int> TaskCycleService::advance(int limit)
{
    const auto now=m_clock.nowUtc();
    const auto ids=m_cycles.due(now,qBound(1,limit,128));if(!ids.ok())return {{},ids.error};
    int count=0;
    for(const auto& id:ids.value){
        const auto result=m_tasks.atomically([&]() -> Result<void> {
            const auto loaded=m_tasks.find(id);if(!loaded.ok())return {loaded.error};
            auto task=loaded.value;if(!task.cycle || !task.cycle->rule.active || !task.cycle->nextStartUtc.isValid() ||
                task.cycle->nextStartUtc>now || task.status!=QStringLiteral("COMPLETED"))return {conflict()};
            auto cycle=*task.cycle;const auto zone=QTimeZone(cycle.zone.toUtf8());
            const auto slot=slotOn(task,cycle.nextDate,zone);if(!slot.ok())return {slot.error};
            auto members=m_tasks.all();if(!members.ok())return {members.error};
            const auto group=task.completionGroupId;
            const auto priorStart=QueryService::withUnifiedDates(task).startOn.value_or(cycle.currentDate);
            const auto days=priorStart.daysTo(cycle.nextDate);task.startOn=cycle.nextDate;
            if(task.deadlineOn)task.deadlineOn=task.deadlineOn->addDays(days);
            task.schedule=slot.value.schedule;
            const auto reminder=relativeReminder(task,cycle.nextDate,slot.value.start,zone);
            if(reminder!=task.remindAtUtc)task.reminderFiredAtUtc.clear();
            task.remindAtUtc=reminder;task.reminderAnchorUtc=reminder;
            reopenTask(task,now);const auto saved=m_tasks.update(task,task.rowVersion);if(!saved.ok())return {saved.error};
            for(auto& member:members.value){
                if(group.isEmpty() || member.parentTaskId!=id || member.completionGroupId!=group ||
                    !member.deletedAtUtc.isEmpty() || member.status!=QStringLiteral("COMPLETED"))continue;
                reopenTask(member,now);const auto savedMember=m_tasks.update(member,member.rowVersion);
                if(!savedMember.ok())return {savedMember.error};
            }
            cycle.currentDate=cycle.nextDate;cycle.currentStartUtc=slot.value.start;cycle.currentEndUtc=slot.value.end;
            cycle.nextDate={};cycle.nextStartUtc={};return m_cycles.save(id,cycle,cycle.rowVersion);
        });
        if(!result.ok())return {{},result.error};++count;
    }
    return {count,{}};
}
}
