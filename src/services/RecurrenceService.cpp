#include "RecurrenceService.h"
#include <QSet>
#include <QTimeZone>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
namespace daymark {
QList<QDate> RecurrenceService::nextDates(const RecurrenceRule& rule,const QDate& from,int limit)
{
    QList<QDate> result;if(!rule.anchor.isValid() || !from.isValid() || rule.interval<1 || limit<1 || limit>8)return result;
    const auto begin=qMax(from,rule.anchor);
    auto add=[&](QDate date){if(date.year()<=9999 && date>=begin && occursOn(rule,date) && result.size()<limit)result.append(date);};
    if(rule.frequency==QStringLiteral("DAILY")){
        const qint64 step=qMax<qint64>(0,(rule.anchor.daysTo(begin)+rule.interval-1)/rule.interval);
        for(int i=0;i<limit;++i)add(rule.anchor.addDays((step+i)*rule.interval));
    }else if(rule.frequency==QStringLiteral("WEEKLY")){
        const auto monday=rule.anchor.addDays(1-rule.anchor.dayOfWeek());const qint64 step=monday.daysTo(begin)/(7LL*rule.interval);
        for(int i=0;i<=limit && result.size()<limit;++i)for(int d=0;d<7;++d)add(monday.addDays((step+i)*7LL*rule.interval+d));
    }else if(rule.frequency==QStringLiteral("MONTHLY")){
        const qint64 months=(begin.year()-rule.anchor.year())*12LL+begin.month()-rule.anchor.month(),step=months/rule.interval;
        const QDate first(rule.anchor.year(),rule.anchor.month(),1);
        for(int i=0;i<limit*24 && result.size()<limit;++i){const auto month=first.addMonths((step+i)*rule.interval);add(QDate(month.year(),month.month(),rule.monthDay));}
    }else if(rule.frequency==QStringLiteral("YEARLY")){
        const qint64 step=(begin.year()-rule.anchor.year())/rule.interval;
        for(int i=0;i<limit*400 && result.size()<limit;++i)add(QDate(rule.anchor.year()+(step+i)*rule.interval,rule.yearMonth,rule.monthDay));
    }
    return result;
}
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
Result<int> RecurrenceService::generate(const QDate& from,const QDate& through,const QStringList& completionPriority)
{
    const auto rules=m_rules.all();if(!rules.ok())return {{},rules.error};
    const auto tasks=m_tasks.all();if(!tasks.ok())return {{},tasks.error};
    int generated=0;const auto today=m_clock.localToday();
    for(const auto& rule:rules.value) {
        if(!rule.active)continue;
        if(!completionPriority.isEmpty() && (rule.frequency!=QStringLiteral("AFTER_DAYS") || !std::any_of(tasks.value.cbegin(),tasks.value.cend(),[&](const Task& t){return completionPriority.contains(t.id) && t.recurrenceId==rule.id && t.status==QStringLiteral("COMPLETED");})))continue;
        QList<QDate> dates;QDate occurrenceKey;QString predecessorId;
        if(rule.frequency==QStringLiteral("AFTER_DAYS")) {
            bool hasOpen=false;QDateTime last;QDate highestOccurrence;
            for(const auto& task:tasks.value)if(task.recurrenceId==rule.id) {
                if(task.status==QStringLiteral("OPEN") && task.deletedAtUtc.isEmpty())hasOpen=true;
                highestOccurrence=qMax(highestOccurrence,task.occurrenceOn);
                const auto completed=QDateTime::fromString(task.completedAtUtc,Qt::ISODateWithMs);
                if(task.status==QStringLiteral("COMPLETED") && completed.isValid() && (!last.isValid() || completed>last || (completed==last && task.id<predecessorId))){last=completed;predecessorId=task.id;}
            }
            if(!completionPriority.isEmpty()){QString preferred;for(const auto& task:tasks.value)if(task.recurrenceId==rule.id && task.status==QStringLiteral("COMPLETED") && completionPriority.contains(task.id) && (preferred.isEmpty() || task.id<preferred)){preferred=task.id;last=QDateTime::fromString(task.completedAtUtc,Qt::ISODateWithMs);predecessorId=task.id;}}
            if(!hasOpen) {
                const auto zoneId=QJsonDocument::fromJson(rule.defaultsJson.toUtf8()).object().value(QStringLiteral("recurrence_time_zone")).toString(QString::fromUtf8(QTimeZone::systemTimeZoneId()));const QTimeZone ruleZone(zoneId.toUtf8());if(!ruleZone.isValid())return Result<int>::failure(ErrorCode::ValidationError,QStringLiteral("recurrenceInvalid"));
                const auto next=last.isValid()?last.toTimeZone(ruleZone).date().addDays(rule.interval):rule.anchor;
                if(!rule.end.isValid() || next<=rule.end) {
                    dates.append(next);
                    // start_on is the schedule; occurrence_on is an immutable cycle identity.
                    occurrenceKey=highestOccurrence>=next?highestOccurrence.addDays(1):next;
                }
            }
        } else {
            int duration=0;const auto seed=TaskRepository::fromSeed(rule,rule.anchor);if(seed.ok() && seed.value.schedule){const auto& s=*seed.value.schedule;duration=s.allDay?s.startOn.daysTo(s.endOn):s.startUtc.toTimeZone(QTimeZone(s.timeZone.toUtf8())).date().daysTo(s.endUtc.toTimeZone(QTimeZone(s.timeZone.toUtf8())).date())+1;}
            const auto notBefore=QDate::fromString(QJsonDocument::fromJson(rule.defaultsJson.toUtf8()).object().value(QStringLiteral("materialize_from")).toString(),Qt::ISODate);
            for(auto date=qMax(qMax(rule.anchor,notBefore),(from.isValid()?from:today.addDays(-30)).addDays(-qMin(366,duration))),last=through.isValid()?through:today.addDays(30);date<=last;date=date.addDays(1))if(occursOn(rule,date))dates.append(date);
        }
        const auto result=m_rules.materialize(rule,dates,m_clock.nowUtc().toUTC().toString(Qt::ISODateWithMs),occurrenceKey,predecessorId);
        if(!result.ok())return result;generated+=result.value;
    }
    return {generated,{}};
}
}
