#include "UnifiedTaskMigration.h"
#include "TaskRepository.h"
#include "RecurrenceRepository.h"
#include <QSqlQuery>
#include <QJsonDocument>
#include <QTimeZone>
#include <QUuid>
namespace daymark {
Result<void> convertLegacyEvents(QSqlDatabase db,const QDateTime& now)
{
    auto fail=[] {return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("migrationFailed"));};
    QSqlQuery q(db);if(!q.exec(QStringLiteral("PRAGMA defer_foreign_keys=ON")))return fail();
    if(!q.exec(QStringLiteral("SELECT id,title,note_text,all_day,start_on,end_on,start_at_utc,end_at_utc,time_zone,repeat_frequency,repeat_interval,repeat_until,alert_minutes,created_at_utc,updated_at_utc,deleted_at_utc,row_version FROM events WHERE task_migrated=0")))return fail();
    QList<Task> pending;
    while(q.next()){
        Task task;task.id=q.value(0).toString();task.title=q.value(1).toString();task.notes=q.value(2).toString();task.planningState=QStringLiteral("ACTIVE");
        TaskSchedule s;s.allDay=q.value(3).toBool();s.startOn=QDate::fromString(q.value(4).toString(),Qt::ISODate);s.endOn=QDate::fromString(q.value(5).toString(),Qt::ISODate);s.startUtc=QDateTime::fromString(q.value(6).toString(),Qt::ISODateWithMs);s.endUtc=QDateTime::fromString(q.value(7).toString(),Qt::ISODateWithMs);s.timeZone=q.value(8).toString();s.alertMinutes=q.value(12).toInt();task.schedule=s;
        task.startOn=s.allDay?s.startOn:s.startUtc.toTimeZone(QTimeZone(s.timeZone.toUtf8())).date();
        task.createdAtUtc=q.value(13).toString();task.updatedAtUtc=q.value(14).toString();task.deletedAtUtc=q.value(15).toString();task.rowVersion=q.value(16).toLongLong();
        if(q.value(9).toString()!=QStringLiteral("NONE")){RecurrenceRule r;r.id=QUuid::createUuidV5(QUuid(QStringLiteral("55802cac-a4c9-47c5-8b47-daf50f4fa326")),task.id.toUtf8()).toString(QUuid::WithoutBraces);r.title=task.title;r.notes=task.notes;r.anchor=*task.startOn;r.frequency=q.value(9).toString();r.interval=q.value(10).toInt();r.end=QDate::fromString(q.value(11).toString(),Qt::ISODate);r.weekdays=1<<(r.anchor.dayOfWeek()-1);r.monthDay=r.anchor.day();r.yearMonth=r.anchor.month();r.active=task.deletedAtUtc.isEmpty();r.sourceTaskId=task.id;task.repeat=r;task.recurrenceId=r.id;task.occurrenceOn=r.anchor;}
        if(s.alertMinutes>=0){const auto at=s.allDay?QDateTime(s.startOn,QTime(9,0),QTimeZone::systemTimeZone(),QDateTime::TransitionResolution::Reject):s.startUtc;const auto due=at.addSecs(-s.alertMinutes*60);if(due.isValid() && due>=now.addDays(-1)){task.remindAtUtc=due.toUTC().toString(Qt::ISODateWithMs);task.reminderAnchorUtc=task.remindAtUtc;}task.reminderZone=s.allDay?QString::fromUtf8(QTimeZone::systemTimeZoneId()):s.timeZone;}
        pending.append(task);
    }q.finish();
    // Compatibility for old importers that wrote a checklist after schema6.
    // Active legacy rows become full children once; no current command writes them.
    if(!q.exec(QStringLiteral("INSERT INTO tasks(id,title,note_text,project_id,direct_area_id,planning_state,status,parent_task_id,sort_order,row_version,created_at_utc,updated_at_utc,completed_at_utc) SELECT c.id,c.title,'',t.project_id,t.direct_area_id,t.planning_state,CASE WHEN c.is_done=1 THEN 'COMPLETED' ELSE 'OPEN' END,c.task_id,c.sort_order,c.row_version,c.created_at_utc,c.updated_at_utc,CASE WHEN c.is_done=1 THEN c.updated_at_utc ELSE NULL END FROM checklist_items c JOIN tasks t ON t.id=c.task_id WHERE c.deleted_at_utc IS NULL")))return fail();
    if(!q.exec(QStringLiteral("UPDATE checklist_items SET deleted_at_utc=updated_at_utc WHERE deleted_at_utc IS NULL")))return fail();
    TaskRepository tasks(db);
    for(auto task:pending){
        q.prepare(QStringLiteral("SELECT task_id,role FROM event_tasks WHERE event_id=?"));q.addBindValue(task.id);if(!q.exec())return fail();while(q.next())task.links.append({q.value(0).toString(),q.value(1).toString()});q.finish();
        if(task.repeat){auto& r=*task.repeat;auto seed=TaskRepository::seedFor(task);seed.insert(QStringLiteral("legacy_event_task_id"),task.id);seed.insert(QStringLiteral("reminder_not_before_utc"),now.addDays(-1).toUTC().toString(Qt::ISODateWithMs));r.defaultsJson=QString::fromUtf8(QJsonDocument(seed).toJson(QJsonDocument::Compact));
            q.prepare(QStringLiteral("INSERT INTO recurrence_templates(id,title,note_text,task_defaults_json,mode,frequency,interval_n,anchor_on,weekdays_mask,month_day,year_month,end_on,active,created_at_utc,updated_at_utc,source_task_id) VALUES(?,?,?,?,'FIXED',?,?,?,?,?,?,?,?,?,?,?)"));
            for(const auto& value:QList<QVariant>{r.id,r.title,r.notes,r.defaultsJson,r.frequency,r.interval,r.anchor.toString(Qt::ISODate),r.weekdays,r.monthDay,r.yearMonth,r.end.isValid()?QVariant(r.end.toString(Qt::ISODate)):QVariant(),r.active,task.createdAtUtc,task.updatedAtUtc,task.id})q.addBindValue(value);if(!q.exec())return fail();
        }
        const auto inserted=tasks.insert(task);if(!inserted.ok())return fail();
        q.prepare(QStringLiteral("UPDATE tasks SET row_version=? WHERE id=?"));q.addBindValue(task.rowVersion);q.addBindValue(task.id);if(!q.exec())return fail();
        q.prepare(QStringLiteral("SELECT occurrence_at_utc,lead_minutes,fired_at_utc FROM event_reminder_history WHERE event_id=?"));q.addBindValue(task.id);if(!q.exec())return fail();QList<QPair<QString,QString>> history;while(q.next())history.append({QDateTime::fromString(q.value(0).toString(),Qt::ISODateWithMs).addSecs(-60*q.value(1).toInt()).toUTC().toString(Qt::ISODateWithMs),q.value(2).toString()});q.finish();
        for(const auto& item:history){q.prepare(QStringLiteral("INSERT INTO task_reminder_history(id,task_id,scheduled_at_utc,fired_at_utc) VALUES(?,?,?,?) ON CONFLICT(task_id,scheduled_at_utc) DO NOTHING"));for(const auto& v:QStringList{QUuid::createUuid().toString(QUuid::WithoutBraces),task.id,item.first,item.second})q.addBindValue(v);if(!q.exec())return fail();if(item.first==task.remindAtUtc){q.prepare(QStringLiteral("UPDATE tasks SET reminder_fired_at_utc=? WHERE id=?"));q.addBindValue(item.second);q.addBindValue(task.id);if(!q.exec())return fail();}}
        q.prepare(QStringLiteral("UPDATE events SET task_migrated=1 WHERE id=?"));q.addBindValue(task.id);if(!q.exec())return fail();
    }
    // An old repeating task also gets a concrete source task, even if the old
    // generator never materialized an occurrence. Existing instances keep IDs.
    RecurrenceRepository rules(db);const auto all=rules.all();if(!all.ok())return fail();
    for(auto rule:all.value)if(rule.sourceTaskId.isEmpty()){
        q.prepare(QStringLiteral("SELECT id FROM tasks WHERE recurrence_template_id=? ORDER BY occurrence_on,id LIMIT 1"));q.addBindValue(rule.id);if(!q.exec())return fail();QString id;if(q.next())id=q.value(0).toString();q.finish();
        Task task;if(id.isEmpty()){const auto seed=TaskRepository::fromSeed(rule,rule.anchor);if(!seed.ok())return fail();task=seed.value;task.id=QUuid::createUuidV5(QUuid(QStringLiteral("783af4df-bc3c-40b8-ad7d-7fe7813c5f7c")),rule.id.toUtf8()).toString(QUuid::WithoutBraces);task.createdAtUtc=task.updatedAtUtc=now.toUTC().toString(Qt::ISODateWithMs);const auto created=tasks.insert(task);if(!created.ok())return fail();task=created.value;}
        else {const auto found=tasks.find(id);if(!found.ok())return fail();task=found.value;}
        task.title=rule.title;task.notes=rule.notes;task.startOn=rule.anchor;
        q.prepare(QStringLiteral("UPDATE recurrence_templates SET source_task_id=?,task_defaults_json=? WHERE id=?"));q.addBindValue(task.id);q.addBindValue(QString::fromUtf8(QJsonDocument(TaskRepository::seedFor(task)).toJson(QJsonDocument::Compact)));q.addBindValue(rule.id);if(!q.exec())return fail();
    }
    return {};
}
}
