#include "TaskRepository.h"
#include <QSqlQuery>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimeZone>
#include <QUuid>
namespace daymark {
namespace {
QString iso(const QDateTime& instant){return instant.toUTC().toString(Qt::ISODateWithMs);}
}
QJsonObject TaskRepository::seedFor(const Task& task)
{
    const auto anchor=task.startOn.value_or(task.occurrenceOn);
    QJsonObject seed{{"seed_on",anchor.toString(Qt::ISODate)},{"planning_state",task.planningState==QStringLiteral("INBOX")?QStringLiteral("ACTIVE"):task.planningState},{"project_id",task.projectId},{"area_id",task.areaId},{"heading_id",task.headingId},{"parent_task_id",task.parentTaskId},{"important",task.important},{"tag_ids",QJsonArray::fromStringList(task.tagIds)},{"remind_at_utc",task.remindAtUtc},{"reminder_repeat",task.reminderRepeat},{"reminder_every",task.reminderEvery},{"reminder_zone",task.reminderZone}};
    if(task.deadlineOn && anchor.isValid())seed.insert(QStringLiteral("deadline_days"),anchor.daysTo(*task.deadlineOn));
    if(task.schedule){const auto& s=*task.schedule;seed.insert(QStringLiteral("schedule"),QJsonObject{{"all_day",s.allDay},{"start_on",s.startOn.toString(Qt::ISODate)},{"end_on",s.endOn.toString(Qt::ISODate)},{"start_utc",iso(s.startUtc)},{"end_utc",iso(s.endUtc)},{"zone",s.timeZone},{"alert",s.alertMinutes}});}
    QJsonArray links;for(const auto& link:task.links)links.append(QJsonObject{{"task_id",link.taskId},{"role",link.role}});seed.insert(QStringLiteral("links"),links);return seed;
}
Result<Task> TaskRepository::fromSeed(const RecurrenceRule& rule,const QDate& date)
{
    Task task;task.id=QUuid::createUuid().toString(QUuid::WithoutBraces);task.title=rule.title;task.notes=rule.notes;task.planningState=QStringLiteral("ACTIVE");task.startOn=date;task.recurrenceId=rule.id;task.occurrenceOn=date;
    const auto seed=QJsonDocument::fromJson(rule.defaultsJson.toUtf8()).object();if(seed.isEmpty())return {task,{}};
    const auto anchor=QDate::fromString(seed.value(QStringLiteral("seed_on")).toString(),Qt::ISODate);if(!anchor.isValid())return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("recurrenceInvalid"));
    const auto days=anchor.daysTo(date);task.planningState=seed.value(QStringLiteral("planning_state")).toString(QStringLiteral("ACTIVE"));task.projectId=seed.value(QStringLiteral("project_id")).toString();task.areaId=seed.value(QStringLiteral("area_id")).toString();task.headingId=seed.value(QStringLiteral("heading_id")).toString();task.parentTaskId=seed.value(QStringLiteral("parent_task_id")).toString();task.important=seed.value(QStringLiteral("important")).toBool();
    if(seed.contains(QStringLiteral("deadline_days")))task.deadlineOn=date.addDays(seed.value(QStringLiteral("deadline_days")).toInteger());for(const auto& id:seed.value(QStringLiteral("tag_ids")).toArray())task.tagIds.append(id.toString());
    task.reminderRepeat=seed.value(QStringLiteral("reminder_repeat")).toString(QStringLiteral("NONE"));task.reminderEvery=seed.value(QStringLiteral("reminder_every")).toInt(1);task.reminderZone=seed.value(QStringLiteral("reminder_zone")).toString(QStringLiteral("UTC"));
    if(seed.contains(QStringLiteral("schedule"))){const auto json=seed.value(QStringLiteral("schedule")).toObject();TaskSchedule s;s.allDay=json.value(QStringLiteral("all_day")).toBool();s.alertMinutes=json.value(QStringLiteral("alert")).toInt(-1);
        if(s.allDay){s.startOn=QDate::fromString(json.value(QStringLiteral("start_on")).toString(),Qt::ISODate).addDays(days);s.endOn=QDate::fromString(json.value(QStringLiteral("end_on")).toString(),Qt::ISODate).addDays(days);}
        else {s.timeZone=json.value(QStringLiteral("zone")).toString();const QTimeZone zone(s.timeZone.toUtf8());const auto start=QDateTime::fromString(json.value(QStringLiteral("start_utc")).toString(),Qt::ISODateWithMs).toTimeZone(zone),end=QDateTime::fromString(json.value(QStringLiteral("end_utc")).toString(),Qt::ISODateWithMs).toTimeZone(zone);s.startUtc=QDateTime(start.date().addDays(days),start.time(),zone,QDateTime::TransitionResolution::Reject).toUTC();s.endUtc=QDateTime(end.date().addDays(days),end.time(),zone,QDateTime::TransitionResolution::Reject).toUTC();if(!s.startUtc.isValid() || !s.endUtc.isValid() || s.endUtc<=s.startUtc)return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("recurrenceTimeSkipped"));}
        task.schedule=s;
    }
    const auto reminder=QDateTime::fromString(seed.value(QStringLiteral("remind_at_utc")).toString(),Qt::ISODateWithMs);
    if(task.schedule && task.schedule->alertMinutes>=0){const auto& s=*task.schedule;const auto start=s.allDay?QDateTime(s.startOn,QTime(9,0),QTimeZone::systemTimeZone(),QDateTime::TransitionResolution::Reject):s.startUtc;task.remindAtUtc=iso(start.addSecs(-60*s.alertMinutes));task.reminderZone=s.allDay?QString::fromUtf8(QTimeZone::systemTimeZoneId()):s.timeZone;}
    else if(reminder.isValid()){const QTimeZone zone(task.reminderZone.toUtf8());const auto local=reminder.toTimeZone(zone);const QDateTime shifted(local.date().addDays(days),local.time(),zone,QDateTime::TransitionResolution::Reject);if(!shifted.isValid())return Result<Task>::failure(ErrorCode::ValidationError,QStringLiteral("recurrenceTimeSkipped"));task.remindAtUtc=iso(shifted);}
    task.reminderAnchorUtc=task.remindAtUtc;for(const auto& value:seed.value(QStringLiteral("links")).toArray()){const auto link=value.toObject();task.links.append({link.value(QStringLiteral("task_id")).toString(),link.value(QStringLiteral("role")).toString()});}return {task,{}};
}
Result<void> TaskRepository::writeComponents(const Task& task)
{
    QSqlQuery q(m_db);auto fail=[] {return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));};
    q.prepare(QStringLiteral("DELETE FROM task_schedules WHERE task_id=?"));q.addBindValue(task.id);if(!q.exec())return fail();
    if(task.schedule){const auto& s=*task.schedule;q.prepare(QStringLiteral("INSERT INTO task_schedules(task_id,all_day,start_on,end_on,start_at_utc,end_at_utc,time_zone,alert_minutes) VALUES(?,?,?,?,?,?,?,?)"));
        for(const auto& v:QList<QVariant>{task.id,s.allDay,s.allDay?QVariant(s.startOn.toString(Qt::ISODate)):QVariant(),s.allDay?QVariant(s.endOn.toString(Qt::ISODate)):QVariant(),s.allDay?QVariant():QVariant(iso(s.startUtc)),s.allDay?QVariant():QVariant(iso(s.endUtc)),s.allDay?QVariant():QVariant(s.timeZone),s.alertMinutes})q.addBindValue(v);if(!q.exec())return fail();}
    q.prepare(QStringLiteral("DELETE FROM task_links WHERE source_id=?"));q.addBindValue(task.id);if(!q.exec())return fail();
    for(const auto& link:task.links){q.prepare(QStringLiteral("INSERT INTO task_links(source_id,target_id,role) VALUES(?,?,?)"));q.addBindValue(task.id);q.addBindValue(link.taskId);q.addBindValue(link.role);if(!q.exec())return fail();}
    q.prepare(QStringLiteral("DELETE FROM task_tags WHERE task_id=?"));q.addBindValue(task.id);if(!q.exec())return fail();
    for(const auto& id:task.tagIds){q.prepare(QStringLiteral("INSERT INTO task_tags(task_id,tag_id) VALUES(?,?)"));q.addBindValue(task.id);q.addBindValue(id);if(!q.exec())return fail();}return {};
}
Result<void> TaskRepository::writeRepeat(const Task& task)
{
    if(!task.repeatChanged)return {};QSqlQuery q(m_db);auto fail=[] {return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));};
    if(!task.replacedRecurrenceId.isEmpty()){
        q.prepare(QStringLiteral("UPDATE recurrence_templates SET active=0,updated_at_utc=?,row_version=row_version+1 WHERE id=? AND row_version=?"));q.addBindValue(task.updatedAtUtc);q.addBindValue(task.replacedRecurrenceId);q.addBindValue(task.replacedRecurrenceVersion);if(!q.exec())return fail();if(q.numRowsAffected()!=1)return Result<void>::failure(ErrorCode::Conflict,QStringLiteral("conflict"));
        q.prepare(QStringLiteral("UPDATE tasks SET deleted_at_utc=?,updated_at_utc=?,row_version=row_version+1 WHERE recurrence_template_id=? AND id<>? AND start_on>=? AND instance_overridden=0 AND status='OPEN' AND deleted_at_utc IS NULL"));q.addBindValue(task.updatedAtUtc);q.addBindValue(task.updatedAtUtc);q.addBindValue(task.replacedRecurrenceId);q.addBindValue(task.id);q.addBindValue(task.repeatCutoff.toString(Qt::ISODate));if(!q.exec())return fail();
    }
    if(task.repeat && task.repeat->active){const auto& r=*task.repeat;q.prepare(QStringLiteral("INSERT INTO recurrence_templates(id,title,note_text,task_defaults_json,mode,frequency,interval_n,anchor_on,weekdays_mask,month_day,year_month,end_on,active,created_at_utc,updated_at_utc,source_task_id) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,1,?,?,?)"));
        for(const auto& v:QList<QVariant>{r.id,r.title,r.notes,r.defaultsJson,r.frequency==QStringLiteral("AFTER_DAYS")?QStringLiteral("AFTER_COMPLETION"):QStringLiteral("FIXED"),r.frequency,r.interval,r.anchor.toString(Qt::ISODate),r.weekdays,r.monthDay,r.yearMonth,r.end.isValid()?QVariant(r.end.toString(Qt::ISODate)):QVariant(),task.updatedAtUtc,task.updatedAtUtc,task.id})q.addBindValue(v);if(!q.exec())return fail();}
    return {};
}
}
