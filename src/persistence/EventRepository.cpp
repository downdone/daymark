#include "EventRepository.h"
#include <QSqlQuery>
#include <QUuid>
namespace daymark {
Result<QList<CalendarEvent>> EventRepository::all() const
{
    QSqlQuery q(m_db);
    if(!q.exec(QStringLiteral("SELECT id,title,note_text,all_day,start_on,end_on,start_at_utc,end_at_utc,time_zone,row_version FROM events WHERE deleted_at_utc IS NULL ORDER BY all_day DESC,start_on,start_at_utc,id")))return Result<QList<CalendarEvent>>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
    QList<CalendarEvent> result;
    QHash<QString,int> positions;
    while(q.next()) {
        CalendarEvent e;e.id=q.value(0).toString();e.title=q.value(1).toString();e.notes=q.value(2).toString();e.allDay=q.value(3).toBool();
        e.startOn=QDate::fromString(q.value(4).toString(),Qt::ISODate);e.endOn=QDate::fromString(q.value(5).toString(),Qt::ISODate);
        e.startUtc=QDateTime::fromString(q.value(6).toString(),Qt::ISODateWithMs);e.endUtc=QDateTime::fromString(q.value(7).toString(),Qt::ISODateWithMs);
        e.timeZone=q.value(8).toString();e.rowVersion=q.value(9).toLongLong();positions.insert(e.id,result.size());result.append(e);
    }
    if(!q.exec(QStringLiteral("SELECT l.event_id,l.task_id,l.role FROM event_tasks l JOIN events e ON e.id=l.event_id JOIN tasks t ON t.id=l.task_id WHERE e.deleted_at_utc IS NULL AND t.deleted_at_utc IS NULL ORDER BY l.event_id,l.task_id")))return Result<QList<CalendarEvent>>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
    while(q.next())if(positions.contains(q.value(0).toString()))result[positions.value(q.value(0).toString())].links.append({q.value(1).toString(),q.value(2).toString()});
    return {result,{}};
}
Result<void> EventRepository::save(CalendarEvent e,const QString& now)
{
    if(!m_db.transaction())return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    auto fail=[&](ErrorCode code,const QString& key){m_db.rollback();return Result<void>::failure(code,key);};
    QSqlQuery q(m_db);
    if(e.id.isEmpty()) {
        e.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        q.prepare(QStringLiteral("INSERT INTO events(id,title,note_text,all_day,start_on,end_on,start_at_utc,end_at_utc,time_zone,created_at_utc,updated_at_utc) VALUES(?,?,?,?,?,?,?,?,?,?,?)"));
    } else q.prepare(QStringLiteral("UPDATE events SET title=?,note_text=?,all_day=?,start_on=?,end_on=?,start_at_utc=?,end_at_utc=?,time_zone=?,updated_at_utc=?,row_version=row_version+1 WHERE id=? AND row_version=? AND deleted_at_utc IS NULL"));
    const bool inserting=q.lastQuery().startsWith(QStringLiteral("INSERT"));
    if(inserting)q.addBindValue(e.id);
    q.addBindValue(e.title);q.addBindValue(e.notes.isNull()?QStringLiteral(""):e.notes);q.addBindValue(e.allDay?1:0);
    q.addBindValue(e.allDay?QVariant(e.startOn.toString(Qt::ISODate)):QVariant());q.addBindValue(e.allDay?QVariant(e.endOn.toString(Qt::ISODate)):QVariant());
    q.addBindValue(e.allDay?QVariant():QVariant(e.startUtc.toUTC().toString(Qt::ISODateWithMs)));q.addBindValue(e.allDay?QVariant():QVariant(e.endUtc.toUTC().toString(Qt::ISODateWithMs)));
    q.addBindValue(e.allDay?QVariant():QVariant(e.timeZone));
    if(inserting)q.addBindValue(now);q.addBindValue(now);
    if(!inserting){q.addBindValue(e.id);q.addBindValue(e.rowVersion);}
    if(!q.exec())return fail(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    if(q.numRowsAffected()!=1)return fail(ErrorCode::Conflict,QStringLiteral("eventConflict"));
    q.prepare(QStringLiteral("DELETE FROM event_tasks WHERE event_id=?"));q.addBindValue(e.id);if(!q.exec())return fail(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    for(const auto& link:e.links) {
        q.prepare(QStringLiteral("SELECT id FROM tasks WHERE id=? AND deleted_at_utc IS NULL"));q.addBindValue(link.taskId);
        if(!q.exec() || !q.next())return fail(ErrorCode::Conflict,QStringLiteral("eventTaskMissing"));q.finish();
        q.prepare(QStringLiteral("INSERT INTO event_tasks(event_id,task_id,role) VALUES(?,?,?)"));q.addBindValue(e.id);q.addBindValue(link.taskId);q.addBindValue(link.role);
        if(!q.exec())return fail(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    }
    if(!m_db.commit())return fail(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));return {};
}
Result<void> EventRepository::setDeleted(const QString& id,qint64 version,bool deleted,const QString& now)
{
    QSqlQuery q(m_db);q.prepare(QStringLiteral("UPDATE events SET deleted_at_utc=?,updated_at_utc=?,row_version=row_version+1 WHERE id=? AND row_version=? AND ")+(deleted?QStringLiteral("deleted_at_utc IS NULL"):QStringLiteral("deleted_at_utc IS NOT NULL")));
    q.addBindValue(deleted?QVariant(now):QVariant());q.addBindValue(now);q.addBindValue(id);q.addBindValue(version);
    if(!q.exec())return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    if(q.numRowsAffected()!=1)return Result<void>::failure(ErrorCode::Conflict,QStringLiteral("eventConflict"));return {};
}
}
