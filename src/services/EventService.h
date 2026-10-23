#pragma once
#include "persistence/EventRepository.h"
namespace daymark {
class EventService final {
public:
    EventService(EventRepository& repository,const IClock& clock):m_repository(repository),m_clock(clock){}
    static Result<void> validate(const CalendarEvent& event);
    static Result<QDateTime> localInstant(const QString& input,const QString& zone);
    static bool onDate(const CalendarEvent& event,const QDate& date,const QTimeZone& displayZone=QTimeZone::systemTimeZone());
    static QList<QDate> visibleDates(const CalendarEvent& event,const QDate& from,const QDate& through,const QTimeZone& displayZone);
    Result<void> save(CalendarEvent event);
    Result<void> setDeleted(const QString& id,qint64 version,bool deleted);
private:
    EventRepository& m_repository;
    const IClock& m_clock;
};
}
