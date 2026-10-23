#include "EventService.h"
#include <QSet>
namespace daymark {
Result<void> EventService::validate(const CalendarEvent& e)
{
    auto invalid=[](const QString& key){return Result<void>::failure(ErrorCode::ValidationError,key);};
    if(e.title.trimmed().isEmpty() || e.title.toUcs4().size()>512)return invalid(QStringLiteral("titleInvalid"));
    if(e.rowVersion<1)return invalid(QStringLiteral("eventInvalid"));
    if(e.allDay) {
        if(!e.startOn.isValid() || !e.endOn.isValid() || e.endOn<=e.startOn || e.startUtc.isValid() || e.endUtc.isValid() || !e.timeZone.isEmpty())return invalid(QStringLiteral("eventRange"));
    } else if(e.startOn.isValid() || e.endOn.isValid() || !e.startUtc.isValid() || !e.endUtc.isValid() || e.endUtc<=e.startUtc || !QTimeZone(e.timeZone.toUtf8()).isValid())return invalid(QStringLiteral("eventRange"));
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
    if(e.allDay)return e.startOn<=date && date<e.endOn;
    const auto start=date.startOfDay(zone),end=date.addDays(1).startOfDay(zone);
    return start.isValid() && end.isValid() && e.startUtc<end && e.endUtc>start;
}
QList<QDate> EventService::visibleDates(const CalendarEvent& e,const QDate& from,const QDate& through,const QTimeZone& zone)
{
    const auto first=e.allDay?e.startOn:e.startUtc.toTimeZone(zone).date();
    const auto last=e.allDay?e.endOn.addDays(-1):e.endUtc.addMSecs(-1).toTimeZone(zone).date();
    QList<QDate> dates;
    if(!first.isValid() || !last.isValid() || !from.isValid() || !through.isValid())return dates;
    const auto end=qMin(last,through);
    for(auto date=qMax(first,from);date<=end;date=date.addDays(1))if(e.allDay || onDate(e,date,zone))dates.append(date);
    return dates;
}
Result<void> EventService::save(CalendarEvent e)
{e.title=e.title.trimmed();const auto valid=validate(e);if(!valid.ok())return valid;return m_repository.save(e,m_clock.nowUtc().toString(Qt::ISODateWithMs));}
Result<void> EventService::setDeleted(const QString& id,qint64 version,bool deleted)
{return m_repository.setDeleted(id,version,deleted,m_clock.nowUtc().toString(Qt::ISODateWithMs));}
}
