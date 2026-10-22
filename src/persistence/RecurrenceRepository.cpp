#include "RecurrenceRepository.h"
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>
namespace daymark {
Result<QList<RecurrenceRule>> RecurrenceRepository::all() const
{
    QSqlQuery q(m_db);
    if(!q.exec(QStringLiteral("SELECT id,title,note_text,frequency,interval_n,anchor_on,weekdays_mask,month_day,year_month,end_on,active,row_version FROM recurrence_templates WHERE deleted_at_utc IS NULL ORDER BY created_at_utc,id")))
        return Result<QList<RecurrenceRule>>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
    QList<RecurrenceRule> rules;
    while(q.next()) {
        RecurrenceRule r;r.id=q.value(0).toString();r.title=q.value(1).toString();r.notes=q.value(2).toString();r.frequency=q.value(3).toString();
        r.interval=q.value(4).toInt();r.anchor=QDate::fromString(q.value(5).toString(),Qt::ISODate);r.weekdays=q.value(6).toInt();
        r.monthDay=q.value(7).toInt();r.yearMonth=q.value(8).toInt();r.end=QDate::fromString(q.value(9).toString(),Qt::ISODate);
        r.active=q.value(10).toBool();r.rowVersion=q.value(11).toLongLong();rules.append(r);
    }
    return {rules,{}};
}
Result<RecurrenceRule> RecurrenceRepository::save(RecurrenceRule r,const QString& now,const QDate& today)
{
    const bool creating=r.id.isEmpty();if(creating)r.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    if(!m_db.transaction())return Result<RecurrenceRule>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    auto failed=[&](ErrorCode code=ErrorCode::PersistenceError){m_db.rollback();return Result<RecurrenceRule>::failure(code,code==ErrorCode::Conflict?QStringLiteral("conflict"):QStringLiteral("databaseWrite"));};
    QSqlQuery q(m_db);
    if(creating) {
        q.prepare(QStringLiteral("INSERT INTO recurrence_templates(id,title,note_text,mode,frequency,interval_n,anchor_on,weekdays_mask,month_day,year_month,end_on,active,created_at_utc,updated_at_utc) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
        q.addBindValue(r.id);
    } else q.prepare(QStringLiteral("UPDATE recurrence_templates SET title=?,note_text=?,mode=?,frequency=?,interval_n=?,anchor_on=?,weekdays_mask=?,month_day=?,year_month=?,end_on=?,active=?,updated_at_utc=?,row_version=row_version+1 WHERE id=? AND row_version=? AND deleted_at_utc IS NULL"));
    for(const auto& value:QList<QVariant>{r.title,r.notes,r.frequency==QStringLiteral("AFTER_DAYS")?QStringLiteral("AFTER_COMPLETION"):QStringLiteral("FIXED"),r.frequency,r.interval,r.anchor.toString(Qt::ISODate),r.weekdays,r.monthDay,r.yearMonth,r.end.isValid()?QVariant(r.end.toString(Qt::ISODate)):QVariant(),r.active})q.addBindValue(value);
    if(creating)q.addBindValue(now);
    q.addBindValue(now);
    if(!creating){q.addBindValue(r.id);q.addBindValue(r.rowVersion);}
    if(!q.exec())return failed();if(q.numRowsAffected()!=1)return failed(ErrorCode::Conflict);
    if(!creating) {
        // Only untouched future instances are regenerated. Preserve history, today and personal edits.
        q.prepare(QStringLiteral("UPDATE tasks SET deleted_at_utc=?,updated_at_utc=?,row_version=row_version+1 WHERE recurrence_template_id=? AND status='OPEN' AND instance_overridden=0 AND start_on>?"));
        q.addBindValue(now);q.addBindValue(now);q.addBindValue(r.id);q.addBindValue(today.toString(Qt::ISODate));if(!q.exec())return failed();
        ++r.rowVersion;
    }
    if(!m_db.commit())return failed();return {r,{}};
}
Result<void> RecurrenceRepository::stop(const QString& id,qint64 version,const QString& now,bool removeFuture,const QDate& today)
{
    if(!m_db.transaction())return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE recurrence_templates SET active=0,deleted_at_utc=?,updated_at_utc=?,row_version=row_version+1 WHERE id=? AND row_version=? AND deleted_at_utc IS NULL"));
    q.addBindValue(now);q.addBindValue(now);q.addBindValue(id);q.addBindValue(version);
    if(!q.exec() || q.numRowsAffected()!=1){m_db.rollback();return Result<void>::failure(ErrorCode::Conflict,QStringLiteral("conflict"));}
    if(removeFuture) {
        q.prepare(QStringLiteral("UPDATE tasks SET deleted_at_utc=?,updated_at_utc=?,row_version=row_version+1 WHERE recurrence_template_id=? AND status='OPEN' AND start_on>?"));
        q.addBindValue(now);q.addBindValue(now);q.addBindValue(id);q.addBindValue(today.toString(Qt::ISODate));
        if(!q.exec()){m_db.rollback();return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));}
    }
    if(!m_db.commit()){m_db.rollback();return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));}
    return {};
}
Result<int> RecurrenceRepository::materialize(const RecurrenceRule& rule,const QList<QDate>& dates,const QString& now,const QDate& occurrenceKey)
{
    if(!m_db.transaction())return Result<int>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    QSqlQuery q(m_db);int inserted=0;
    for(const auto& date:dates) {
        q.prepare(QStringLiteral("INSERT INTO tasks(id,title,note_text,planning_state,status,start_on,recurrence_template_id,occurrence_on,created_at_utc,updated_at_utc,sort_order) VALUES(?,?,?,'ACTIVE','OPEN',?,?,?,?,?,(SELECT COALESCE(MAX(sort_order),0)+1 FROM tasks)) ON CONFLICT(recurrence_template_id,occurrence_on) DO UPDATE SET title=excluded.title,note_text=excluded.note_text,deleted_at_utc=NULL,updated_at_utc=excluded.updated_at_utc,row_version=tasks.row_version+1 WHERE tasks.instance_overridden=0 AND tasks.deleted_at_utc IS NOT NULL AND tasks.status='OPEN'"));
        for(const auto& value:QList<QVariant>{QUuid::createUuid().toString(QUuid::WithoutBraces),rule.title,rule.notes,date.toString(Qt::ISODate),rule.id,(occurrenceKey.isValid()?occurrenceKey:date).toString(Qt::ISODate),now,now})q.addBindValue(value);
        if(!q.exec()){m_db.rollback();return Result<int>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));}
        inserted+=q.numRowsAffected();
    }
    if(!m_db.commit()){m_db.rollback();return Result<int>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));}
    return {inserted,{}};
}
}
