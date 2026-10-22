#include "RecurrenceService.h"
#include <QSet>
namespace daymark {
bool RecurrenceService::occursOn(const RecurrenceRule& r,const QDate& d)
{
    if(!d.isValid() || !r.anchor.isValid() || r.interval<1 || d<r.anchor || (r.end.isValid() && d>r.end))return false;
    if(r.frequency==QStringLiteral("DAILY"))return r.anchor.daysTo(d)%r.interval==0;
    if(r.frequency==QStringLiteral("WEEKLY")) {
        const auto anchorWeek=r.anchor.addDays(1-r.anchor.dayOfWeek());
        return (anchorWeek.daysTo(d)/7)%r.interval==0 && (r.weekdays & (1<<(d.dayOfWeek()-1)));
    }
    if(r.frequency==QStringLiteral("MONTHLY"))return d.day()==r.monthDay && ((d.year()-r.anchor.year())*12+d.month()-r.anchor.month())%r.interval==0;
    if(r.frequency==QStringLiteral("YEARLY"))return d.month()==r.yearMonth && d.day()==r.monthDay && (d.year()-r.anchor.year())%r.interval==0;
    return false;
}
Result<RecurrenceRule> RecurrenceService::save(RecurrenceRule r)
{
    r.title=r.title.trimmed();if(r.notes.isNull())r.notes=QStringLiteral("");
    if(r.title.isEmpty() || r.title.toUcs4().size()>512 || !r.anchor.isValid() || r.interval<1 || r.interval>36500
       || (r.end.isValid() && r.end<r.anchor) || r.weekdays<0 || r.weekdays>127
       || !QStringList{QStringLiteral("DAILY"),QStringLiteral("WEEKLY"),QStringLiteral("MONTHLY"),QStringLiteral("YEARLY"),QStringLiteral("AFTER_DAYS")}.contains(r.frequency)
       || (r.frequency==QStringLiteral("WEEKLY") && r.weekdays==0)
       || r.monthDay<1 || r.monthDay>31 || r.yearMonth<1 || r.yearMonth>12
       || (r.frequency==QStringLiteral("YEARLY") && !QDate(2000,r.yearMonth,r.monthDay).isValid()))
        return Result<RecurrenceRule>::failure(ErrorCode::ValidationError,QStringLiteral("recurrenceInvalid"));
    if(!r.id.isEmpty()) {
        const auto rules=m_rules.all();if(!rules.ok())return {{},rules.error};
        for(const auto& existing:rules.value)if(existing.id==r.id && existing.frequency!=r.frequency)
            return Result<RecurrenceRule>::failure(ErrorCode::ValidationError,QStringLiteral("recurrenceModeImmutable"));
    }
    return m_rules.save(r,m_clock.nowUtc().toUTC().toString(Qt::ISODateWithMs),m_clock.localToday());
}
Result<void> RecurrenceService::stop(const QString& id,qint64 version,bool removeFuture)
{return m_rules.stop(id,version,m_clock.nowUtc().toUTC().toString(Qt::ISODateWithMs),removeFuture,m_clock.localToday());}
Result<int> RecurrenceService::generate()
{
    const auto rules=m_rules.all();if(!rules.ok())return {{},rules.error};
    const auto tasks=m_tasks.all();if(!tasks.ok())return {{},tasks.error};
    int generated=0;const auto today=m_clock.localToday();
    for(const auto& rule:rules.value) {
        if(!rule.active)continue;
        QList<QDate> dates;QDate occurrenceKey;
        if(rule.frequency==QStringLiteral("AFTER_DAYS")) {
            bool hasOpen=false;QDateTime last;QDate highestOccurrence;
            for(const auto& task:tasks.value)if(task.recurrenceId==rule.id) {
                if(task.status==QStringLiteral("OPEN") && task.deletedAtUtc.isEmpty())hasOpen=true;
                highestOccurrence=qMax(highestOccurrence,task.occurrenceOn);
                const auto completed=QDateTime::fromString(task.completedAtUtc,Qt::ISODateWithMs);
                if(task.status==QStringLiteral("COMPLETED") && completed.isValid() && (!last.isValid() || completed>last))last=completed;
            }
            if(!hasOpen) {
                const auto next=last.isValid()?last.toLocalTime().date().addDays(rule.interval):rule.anchor;
                if(!rule.end.isValid() || next<=rule.end) {
                    dates.append(next);
                    // start_on is the schedule; occurrence_on is an immutable cycle identity.
                    occurrenceKey=highestOccurrence>=next?highestOccurrence.addDays(1):next;
                }
            }
        } else for(auto date=qMax(rule.anchor,today.addDays(-30));date<=today.addDays(30);date=date.addDays(1))
            if(occursOn(rule,date))dates.append(date);
        const auto result=m_rules.materialize(rule,dates,m_clock.nowUtc().toUTC().toString(Qt::ISODateWithMs),occurrenceKey);
        if(!result.ok())return result;generated+=result.value;
    }
    return {generated,{}};
}
}
