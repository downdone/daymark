#include "EventService.h"
#include <QSet>
#include <algorithm>
namespace daymark {
CalendarEvent EventService::projection(const Task& task)
{
    const auto& s=*task.schedule;CalendarEvent e;e.id=task.id;e.title=task.title;e.notes=task.notes;e.rowVersion=task.rowVersion;e.materialized=true;e.allDay=s.allDay;e.startOn=s.startOn;e.endOn=s.endOn;e.startUtc=s.startUtc;e.endUtc=s.endUtc;e.timeZone=s.timeZone;e.alertMinutes=s.alertMinutes;e.links=task.links;
    if(task.repeat && task.repeat->active){e.repeatFrequency=task.repeat->frequency;e.repeatInterval=task.repeat->interval;e.repeatUntil=task.repeat->end;}return e;
}

QString EventService::occurrenceKey(const CalendarEvent& e)
{return e.allDay?e.startOn.toString(Qt::ISODate):e.startUtc.toUTC().toString(Qt::ISODateWithMs);}
Result<void> EventService::validate(const CalendarEvent& e)
{
    auto invalid=[](const QString& key){return Result<void>::failure(ErrorCode::ValidationError,key);};
    if(e.title.trimmed().isEmpty() || e.title.toUcs4().size()>512)return invalid(QStringLiteral("titleInvalid"));
    if(e.rowVersion<1)return invalid(QStringLiteral("eventInvalid"));
    if(e.allDay) {
        if(!e.startOn.isValid() || !e.endOn.isValid() || e.endOn<=e.startOn || e.startUtc.isValid() || e.endUtc.isValid() || !e.timeZone.isEmpty())return invalid(QStringLiteral("eventRange"));
    } else if(e.startOn.isValid() || e.endOn.isValid() || !e.startUtc.isValid() || !e.endUtc.isValid() || e.endUtc<=e.startUtc || !QTimeZone(e.timeZone.toUtf8()).isValid())return invalid(QStringLiteral("eventRange"));
    const QStringList frequencies={"NONE","DAILY","WEEKLY","MONTHLY","YEARLY"};
    const auto anchor=e.allDay?e.startOn:e.startUtc.toTimeZone(QTimeZone(e.timeZone.toUtf8())).date();
    const int duration=e.allDay?e.startOn.daysTo(e.endOn):int(e.startUtc.secsTo(e.endUtc)/86400);
    if(!frequencies.contains(e.repeatFrequency) || e.repeatInterval<1 || e.repeatInterval>365 || !QList<int>{-1,0,5,15,30,60,1440}.contains(e.alertMinutes) || (e.repeatUntil.isValid() && e.repeatUntil<anchor) || (e.repeatFrequency!="NONE" && duration>366))return invalid(QStringLiteral("eventSeriesInvalid"));
    QSet<QString> ids;
    for(const auto& link:e.links) {
        if(link.taskId.isEmpty() || ids.contains(link.taskId) || (link.role!=QStringLiteral("preparation") && link.role!=QStringLiteral("followup")))return invalid(QStringLiteral("eventInvalid"));
        ids.insert(link.taskId);
    }
    return {};
}
Result<QDateTime> EventService::localInstant(const QString& input,const QString& zoneId)
{
    const auto format=input.size()==23?QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"):input.size()==19?QStringLiteral("yyyy-MM-dd HH:mm:ss"):QStringLiteral("yyyy-MM-dd HH:mm");
    const auto parsed=QDateTime::fromString(input,format);const QTimeZone zone(zoneId.toUtf8());
    if(!zone.isValid() || !parsed.isValid() || parsed.toString(format)!=input)return Result<QDateTime>::failure(ErrorCode::ValidationError,QStringLiteral("eventTime"));
    const QDateTime result(parsed.date(),parsed.time(),zone,QDateTime::TransitionResolution::Reject);
    if(!result.isValid())return Result<QDateTime>::failure(ErrorCode::ValidationError,QStringLiteral("eventTime"));return {result.toUTC(),{}};
}
bool EventService::onDate(const CalendarEvent& e,const QDate& date,const QTimeZone& zone)
{
    if(!date.isValid())return false;
    if(!e.materialized && e.repeatFrequency!=QStringLiteral("NONE"))return !visibleDates(e,date,date,zone).isEmpty();
    if(e.allDay)return e.startOn<=date && date<e.endOn;
    const auto start=date.startOfDay(zone),end=date.addDays(1).startOfDay(zone);
    return start.isValid() && end.isValid() && e.startUtc<end && e.endUtc>start;
}
QList<QDate> EventService::visibleDates(const CalendarEvent& e,const QDate& from,const QDate& through,const QTimeZone& zone)
{
    if(!e.materialized && e.repeatFrequency!=QStringLiteral("NONE")) {
        QSet<QDate> merged;for(const auto& instance:occurrences(e,from,through,zone))for(const auto& date:visibleDates(instance,from,through,zone))merged.insert(date);
        auto dates=merged.values();std::sort(dates.begin(),dates.end());return dates;
    }
    const auto first=e.allDay?e.startOn:e.startUtc.toTimeZone(zone).date();
    const auto last=e.allDay?e.endOn.addDays(-1):e.endUtc.addMSecs(-1).toTimeZone(zone).date();
    QList<QDate> dates;
    if(!first.isValid() || !last.isValid() || !from.isValid() || !through.isValid())return dates;
    const auto end=qMin(last,through);
    for(auto date=qMax(first,from);date<=end;date=date.addDays(1))if(e.allDay || onDate(e,date,zone))dates.append(date);
    return dates;
}
QList<CalendarEvent> EventService::occurrences(const CalendarEvent& e,const QDate& from,const QDate& through,const QTimeZone& displayZone)
{
    if(!from.isValid() || !through.isValid() || through<from)return {};
    if(e.materialized){auto instance=e;instance.repeatFrequency=QStringLiteral("NONE");return {instance};}
    if(e.repeatFrequency==QStringLiteral("NONE"))return {e};
    if(!validate(e).ok())return {};
    const QTimeZone zone=e.allDay?displayZone:QTimeZone(e.timeZone.toUtf8());
    const auto start=e.startUtc.toTimeZone(zone),end=e.endUtc.toTimeZone(zone);
    const QDate anchor=e.allDay?e.startOn:start.date();
    const int duration=anchor.daysTo(e.allDay?e.endOn:end.date());
    QList<CalendarEvent> result;
    for(auto date=qMax(anchor,from.addDays(-duration-2));date<=through.addDays(2);date=date.addDays(1)) {
        if(e.repeatUntil.isValid() && date>e.repeatUntil)break;
        const auto days=anchor.daysTo(date);const int months=(date.year()-anchor.year())*12+date.month()-anchor.month();
        bool match=e.repeatFrequency=="DAILY"?days%e.repeatInterval==0:e.repeatFrequency=="WEEKLY"?days%(7*e.repeatInterval)==0:e.repeatFrequency=="MONTHLY"?months%e.repeatInterval==0 && date.day()==anchor.day():(date.year()-anchor.year())%e.repeatInterval==0 && date.month()==anchor.month() && date.day()==anchor.day();
        if(!match)continue;
        auto instance=e;instance.repeatFrequency=QStringLiteral("NONE");
        if(e.allDay){instance.startOn=date;instance.endOn=date.addDays(duration);}
        else {const QDateTime localStart(date,start.time(),zone,QDateTime::TransitionResolution::Reject),localEnd(date.addDays(duration),end.time(),zone,QDateTime::TransitionResolution::Reject);if(!localStart.isValid() || !localEnd.isValid() || localEnd<=localStart)continue;instance.startUtc=localStart.toUTC();instance.endUtc=localEnd.toUTC();}
        if(!visibleDates(instance,from,through,displayZone).isEmpty())result.append(instance);
    }
    return result;
}
Result<QVariantList> EventService::notifications() const
{return m_repository.notifications();}
Result<QVariantList> EventService::claimReminders()
{
    return m_repository.claimTaskReminders(m_clock.nowUtc().toUTC().toString(Qt::ISODateWithMs));
}
Result<void> EventService::save(CalendarEvent e)
{e.title=e.title.trimmed();const auto valid=validate(e);if(!valid.ok())return valid;return m_repository.save(e,m_clock.nowUtc().toString(Qt::ISODateWithMs));}
Result<void> EventService::setDeleted(const QString& id,qint64 version,bool deleted)
{return m_repository.setDeleted(id,version,deleted,m_clock.nowUtc().toString(Qt::ISODateWithMs));}
}
