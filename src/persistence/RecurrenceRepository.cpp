#include "RecurrenceRepository.h"
#include "TaskRepository.h"
#include <QSqlQuery>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>
#include <QVariant>
namespace daymark {
Result<QList<RecurrenceRule>> RecurrenceRepository::all() const
{
    QSqlQuery q(m_db);
    if(!q.exec(QStringLiteral("SELECT id,title,note_text,frequency,interval_n,anchor_on,weekdays_mask,month_day,year_month,end_on,active,row_version,source_task_id,task_defaults_json FROM recurrence_templates WHERE deleted_at_utc IS NULL ORDER BY created_at_utc,id")))
        return Result<QList<RecurrenceRule>>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
    QList<RecurrenceRule> rules;
    while(q.next()) {
        RecurrenceRule r;r.id=q.value(0).toString();r.title=q.value(1).toString();r.notes=q.value(2).toString();r.frequency=q.value(3).toString();
        r.interval=q.value(4).toInt();r.anchor=QDate::fromString(q.value(5).toString(),Qt::ISODate);r.weekdays=q.value(6).toInt();
        r.monthDay=q.value(7).toInt();r.yearMonth=q.value(8).toInt();r.end=QDate::fromString(q.value(9).toString(),Qt::ISODate);
        r.active=q.value(10).toBool();r.rowVersion=q.value(11).toLongLong();r.sourceTaskId=q.value(12).toString();r.defaultsJson=q.value(13).toString();rules.append(r);
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
    QSqlQuery q(m_db);if(!q.exec(QStringLiteral("SAVEPOINT materialize")))return Result<int>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    auto fail=[&](Error error){q.exec(QStringLiteral("ROLLBACK TO materialize"));q.exec(QStringLiteral("RELEASE materialize"));return Result<int>{{},error};};
    TaskRepository tasks(m_db);int inserted=0;
    const auto defaults=QJsonDocument::fromJson(rule.defaultsJson.toUtf8()).object();const auto legacy=defaults.value(QStringLiteral("legacy_event_task_id")).toString();
    QStringList ancestors,sources;for(const auto& id:defaults.value(QStringLiteral("reminder_ancestors")).toArray())ancestors.append(id.toString());for(const auto& id:defaults.value(QStringLiteral("reminder_sources")).toArray())sources.append(id.toString());if(!legacy.isEmpty())sources.append(legacy);
    const auto reminderCutoff=QDateTime::fromString(defaults.value(QStringLiteral("reminder_not_before_utc")).toString(),Qt::ISODateWithMs);

    for(const auto& date:dates){
        auto seed=TaskRepository::fromSeed(rule,date);if(!seed.ok()){if(seed.error.key==QStringLiteral("recurrenceTimeSkipped"))continue;return fail(seed.error);}auto task=seed.value;
        // Future instances of a child must follow its living parent, never
        // recreate work beneath a deleted/completed parent or an old category.
        if(!task.parentTaskId.isEmpty()){const auto parent=tasks.find(task.parentTaskId);if(!parent.ok())return fail(parent.error);if(!parent.value.deletedAtUtc.isEmpty() || parent.value.status!=QStringLiteral("OPEN"))continue;task.projectId=parent.value.projectId;task.areaId=parent.value.areaId;task.headingId=parent.value.headingId;}
        // Deleting a category moves its tasks out, including future instances
        // whose immutable seed still names the old category. Archived categories
        // retain their references so unarchiving restores the same organization.
        if(!task.projectId.isEmpty()){q.prepare(QStringLiteral("SELECT id FROM projects WHERE id=? AND deleted_at_utc IS NULL"));q.addBindValue(task.projectId);if(!q.exec())return fail({ErrorCode::PersistenceError,QStringLiteral("databaseRead")});if(!q.next()){task.projectId.clear();task.headingId.clear();}q.finish();}
        if(!task.areaId.isEmpty()){q.prepare(QStringLiteral("SELECT id FROM areas WHERE id=? AND deleted_at_utc IS NULL"));q.addBindValue(task.areaId);if(!q.exec())return fail({ErrorCode::PersistenceError,QStringLiteral("databaseRead")});if(!q.next())task.areaId.clear();q.finish();}
        if(!task.headingId.isEmpty()){q.prepare(QStringLiteral("SELECT id FROM headings WHERE id=? AND project_id=? AND deleted_at_utc IS NULL"));q.addBindValue(task.headingId);q.addBindValue(task.projectId);if(!q.exec())return fail({ErrorCode::PersistenceError,QStringLiteral("databaseRead")});if(!q.next())task.headingId.clear();q.finish();}
        QStringList tags;for(const auto& id:task.tagIds){q.prepare(QStringLiteral("SELECT id FROM tags WHERE id=? AND deleted_at_utc IS NULL"));q.addBindValue(id);if(!q.exec())return fail({ErrorCode::PersistenceError,QStringLiteral("databaseRead")});if(q.next())tags.append(id);q.finish();}task.tagIds=tags;
        task.occurrenceOn=occurrenceKey.isValid()?occurrenceKey:date;task.createdAtUtc=task.updatedAtUtc=now;
        q.prepare(QStringLiteral("SELECT id,instance_overridden,status,deleted_at_utc FROM tasks WHERE recurrence_template_id=? AND occurrence_on=?"));q.addBindValue(rule.id);q.addBindValue(task.occurrenceOn.toString(Qt::ISODate));if(!q.exec())return fail({ErrorCode::PersistenceError,QStringLiteral("databaseRead")});
        QString restoreId;if(q.next()){if(q.value(1).toBool() || q.value(2).toString()!=QStringLiteral("OPEN") || q.value(3).isNull()){q.finish();continue;}restoreId=q.value(0).toString();}q.finish();
        if(!restoreId.isEmpty())task.id=restoreId;
        QString claimId,claimTaskId;
        if(!task.remindAtUtc.isEmpty()){
            auto ids=sources;ids.append(task.id);QStringList conditions,placeholders;for(int i=0;i<ids.size();++i)placeholders.append(QStringLiteral("?"));conditions.append(QStringLiteral("h.task_id IN (")+placeholders.join(',')+QLatin1Char(')'));placeholders.clear();for(int i=0;i<ancestors.size();++i)placeholders.append(QStringLiteral("?"));if(!ancestors.isEmpty())conditions.append(QStringLiteral("t.recurrence_template_id IN (")+placeholders.join(',')+QLatin1Char(')'));
            q.prepare(QStringLiteral("SELECT h.id,h.task_id,h.fired_at_utc FROM task_reminder_history h JOIN tasks t ON t.id=h.task_id WHERE h.scheduled_at_utc=? AND (")+conditions.join(QStringLiteral(" OR "))+QStringLiteral(") ORDER BY h.fired_at_utc LIMIT 1"));q.addBindValue(task.remindAtUtc);for(const auto& id:ids)q.addBindValue(id);for(const auto& id:ancestors)q.addBindValue(id);if(!q.exec())return fail({ErrorCode::PersistenceError,QStringLiteral("databaseRead")});if(q.next()){claimId=q.value(0).toString();claimTaskId=q.value(1).toString();task.reminderFiredAtUtc=q.value(2).toString();}q.finish();
            if(claimId.isEmpty() && reminderCutoff.isValid() && QDateTime::fromString(task.remindAtUtc,Qt::ISODateWithMs)<reminderCutoff){task.remindAtUtc.clear();task.reminderAnchorUtc.clear();task.reminderRepeat=QStringLiteral("NONE");}
        }
        if(restoreId.isEmpty()){const auto created=tasks.insert(task);if(!created.ok())return fail(created.error);}
        else {const auto old=tasks.find(restoreId);if(!old.ok())return fail(old.error);task.id=restoreId;task.createdAtUtc=old.value.createdAtUtc;const auto saved=tasks.update(task,old.value.rowVersion,false);if(!saved.ok())return fail(saved.error);}
        if(!claimId.isEmpty() && claimTaskId!=task.id){
            if(claimTaskId==legacy){q.prepare(QStringLiteral("UPDATE task_reminder_history SET task_id=? WHERE id=?"));q.addBindValue(task.id);q.addBindValue(claimId);}
            else {q.prepare(QStringLiteral("INSERT INTO task_reminder_history(id,task_id,scheduled_at_utc,fired_at_utc) VALUES(?,?,?,?) ON CONFLICT(task_id,scheduled_at_utc) DO NOTHING"));q.addBindValue(QUuid::createUuid().toString(QUuid::WithoutBraces));q.addBindValue(task.id);q.addBindValue(task.remindAtUtc);q.addBindValue(task.reminderFiredAtUtc);}
            if(!q.exec())return fail({ErrorCode::PersistenceError,QStringLiteral("databaseWrite")});
        }
        ++inserted;
    }
    if(!q.exec(QStringLiteral("RELEASE materialize")))return fail({ErrorCode::PersistenceError,QStringLiteral("databaseWrite")});return {inserted,{}};
}
}
