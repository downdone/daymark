#include "ReminderSchedule.h"
#include <algorithm>
namespace daymark {
QDateTime ReminderSchedule::nextAfter(const QDateTime& anchor,const QString& frequency,int interval,const QTimeZone& zone,const QDateTime& now)
{
    if(!anchor.isValid() || !now.isValid() || !zone.isValid() || interval<1 || frequency==QStringLiteral("NONE"))return {};
    const auto local=anchor.toTimeZone(zone);const auto date=local.date(),through=now.toTimeZone(zone).date();qint64 index=1;
    if(frequency==QStringLiteral("DAILY"))index=qMax<qint64>(1,date.daysTo(through)/interval);
    else if(frequency==QStringLiteral("WEEKLY"))index=qMax<qint64>(1,date.daysTo(through)/(qint64(interval)*7));
    else if(frequency==QStringLiteral("MONTHLY"))index=qMax<qint64>(1,((through.year()-date.year())*12+through.month()-date.month())/interval);
    else if(frequency==QStringLiteral("YEARLY"))index=qMax<qint64>(1,(through.year()-date.year())/interval);
    else return {};
    // Gregorian month/day validity repeats within 400 years; bound impossible schedules.
    for(int tries=0;tries<4801;++tries,++index){QDate next;
        if(frequency==QStringLiteral("DAILY"))next=date.addDays(index*interval);
        else if(frequency==QStringLiteral("WEEKLY"))next=date.addDays(index*interval*7);
        else if(frequency==QStringLiteral("MONTHLY")){const auto month=QDate(date.year(),date.month(),1).addMonths(index*interval);if(!month.isValid() || month.year()>9999)return {};next=QDate(month.year(),month.month(),date.day());}
        else {const auto year=qint64(date.year())+index*interval;if(year>9999)return {};next=QDate(int(year),date.month(),date.day());}
        if(!next.isValid())continue;if(next.year()>9999)return {};
        const QDateTime candidate(next,local.time(),zone,QDateTime::TransitionResolution::Reject);if(candidate.isValid() && candidate>now)return candidate.toUTC();
    }return {};
}
}
