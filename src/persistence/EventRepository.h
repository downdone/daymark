#pragma once
#include "domain/CalendarEvent.h"
#include <QSqlDatabase>
namespace daymark {
class EventRepository final {
public:
    explicit EventRepository(QSqlDatabase db):m_db(std::move(db)){}
    Result<QList<CalendarEvent>> all() const;
    Result<void> save(CalendarEvent event,const QString& now);
    Result<void> setDeleted(const QString& id,qint64 version,bool deleted,const QString& now);
private:
    QSqlDatabase m_db;
};
}
