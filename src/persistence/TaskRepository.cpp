#include "TaskRepository.h"
#include "services/ReminderSchedule.h"
#include <QSqlQuery>
#include <QVariant>
#include <QSet>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUuid>

namespace daymark {
namespace {
const QString selectTasks = QStringLiteral(
    "SELECT t.id,t.title,t.note_text,t.planning_state,t.status,t.start_on,t.deadline_on,"
    "t.today_focus,t.evening,t.created_at_utc,t.updated_at_utc,t.completed_at_utc,t.cancelled_at_utc,"
    "t.deleted_at_utc,t.row_version,t.sort_order,t.project_id,p.planning_state,p.status,CASE WHEN p.archived=1 THEN 'archived' ELSE p.deleted_at_utc END,"
    "t.direct_area_id,t.heading_id,p.title,t.remind_at_utc,t.reminder_fired_at_utc,o.sort_order,COALESCE(p.parent_id,p.area_id),t.recurrence_template_id,t.occurrence_on,t.instance_overridden,h.title,h.sort_order "
    ",COALESCE(c.title,a.name),CASE WHEN c.id IS NOT NULL THEN (c.archived=1 OR c.status<>'OPEN' OR c.planning_state<>'ACTIVE' OR c.deleted_at_utc IS NOT NULL) ELSE COALESCE(a.archived,0) END,t.parent_task_id,t.pinned,t.important,pt.title,t.reminder_repeat,t.reminder_every,t.reminder_anchor_utc,t.reminder_zone,t.deletion_group_id,t.completion_group_id FROM tasks t LEFT JOIN tasks pt ON pt.id=t.parent_task_id LEFT JOIN projects p ON p.id=t.project_id LEFT JOIN projects c ON c.id=p.parent_id LEFT JOIN headings h ON h.id=t.heading_id LEFT JOIN areas a ON a.id=COALESCE(t.direct_area_id,p.area_id) LEFT JOIN task_view_order o ON o.task_id=t.id AND o.view_code='today' ");
std::optional<QDate> dateValue(const QVariant& value)
{ return value.isNull() ? std::nullopt : std::optional<QDate>(QDate::fromString(value.toString(), Qt::ISODate)); }
QVariant dateSql(const std::optional<QDate>& date)
{ return date ? QVariant(date->toString(Qt::ISODate)) : QVariant(); }
Task readTask(const QSqlQuery& q)
{
    Task t;
    t.id=q.value(0).toString(); t.title=q.value(1).toString(); t.notes=q.value(2).toString();
    t.planningState=q.value(3).toString(); t.status=q.value(4).toString();
    t.startOn=dateValue(q.value(5)); t.deadlineOn=dateValue(q.value(6));
    t.todayFocus=q.value(7).toBool(); t.evening=q.value(8).toBool();
    t.createdAtUtc=q.value(9).toString(); t.updatedAtUtc=q.value(10).toString();
    t.completedAtUtc=q.value(11).toString(); t.cancelledAtUtc=q.value(12).toString();
    t.deletedAtUtc=q.value(13).toString(); t.rowVersion=q.value(14).toLongLong(); t.sortOrder=q.value(15).toLongLong();
    t.projectId=q.value(16).toString(); t.projectPlanning=q.value(17).toString();
    t.projectStatus=q.value(18).toString(); t.projectDeleted=!q.value(19).isNull();
    t.areaId=q.value(20).toString();t.headingId=q.value(21).toString();t.projectName=q.value(22).toString();
    t.remindAtUtc=q.value(23).toString();t.reminderFiredAtUtc=q.value(24).toString();
    t.todayOrder=q.value(25).isNull()?-1:q.value(25).toLongLong();
    t.projectAreaId=q.value(26).toString();
    t.recurrenceId=q.value(27).toString();t.occurrenceOn=QDate::fromString(q.value(28).toString(),Qt::ISODate);t.instanceOverridden=q.value(29).toBool();
    t.headingTitle=q.value(30).toString();t.headingOrder=q.value(31).isNull()?-1:q.value(31).toLongLong();
    t.areaName=q.value(32).toString();
    t.areaArchived=q.value(33).toBool();
    t.parentTaskId=q.value(34).toString();t.pinned=q.value(35).toBool();t.important=q.value(36).toBool();t.parentTitle=q.value(37).toString();
    t.reminderRepeat=q.value(38).toString();t.reminderEvery=q.value(39).toInt();t.reminderAnchorUtc=q.value(40).toString();t.reminderZone=q.value(41).toString();t.deletionGroupId=q.value(42).toString();t.completionGroupId=q.value(43).toString();
    return t;
}

bool loadDetails(QSqlDatabase db,QList<Task>& tasks,const QString& taskId={})
{
    QHash<QString,int> indices;for(int i=0;i<tasks.size();++i)indices.insert(tasks[i].id,i);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT tt.task_id,t.id,t.name FROM task_tags tt JOIN tags t ON t.id=tt.tag_id WHERE t.deleted_at_utc IS NULL ")
              +(taskId.isEmpty()?QString():QStringLiteral("AND tt.task_id=? "))+QStringLiteral("ORDER BY t.normalized_name,t.id"));
    if(!taskId.isEmpty())q.addBindValue(taskId);if(!q.exec())return false;
    while(q.next())if(indices.contains(q.value(0).toString())){
        auto& task=tasks[indices.value(q.value(0).toString())];task.tagIds<<q.value(1).toString();task.tagNames<<q.value(2).toString();
    }
    q.prepare(QStringLiteral("SELECT parent_task_id,count(*) FROM tasks WHERE parent_task_id IS NOT NULL AND deleted_at_utc IS NULL AND status='OPEN' ")+(taskId.isEmpty()?QString():QStringLiteral("AND parent_task_id=? "))+QStringLiteral("GROUP BY parent_task_id"));if(!taskId.isEmpty())q.addBindValue(taskId);if(!q.exec())return false;while(q.next())if(indices.contains(q.value(0).toString()))tasks[indices.value(q.value(0).toString())].pendingSubtasks=q.value(1).toInt();
    q.prepare(QStringLiteral("SELECT s.task_id,s.all_day,s.start_on,s.end_on,s.start_at_utc,s.end_at_utc,s.time_zone,s.alert_minutes FROM task_schedules s ")+(taskId.isEmpty()?QString():QStringLiteral("WHERE s.task_id=?")));if(!taskId.isEmpty())q.addBindValue(taskId);if(!q.exec())return false;
    while(q.next())if(indices.contains(q.value(0).toString())){TaskSchedule s;s.allDay=q.value(1).toBool();s.startOn=QDate::fromString(q.value(2).toString(),Qt::ISODate);s.endOn=QDate::fromString(q.value(3).toString(),Qt::ISODate);s.startUtc=QDateTime::fromString(q.value(4).toString(),Qt::ISODateWithMs);s.endUtc=QDateTime::fromString(q.value(5).toString(),Qt::ISODateWithMs);s.timeZone=q.value(6).toString();s.alertMinutes=q.value(7).toInt();tasks[indices.value(q.value(0).toString())].schedule=s;}
    q.prepare(QStringLiteral("SELECT source_id,target_id,role FROM task_links ")+(taskId.isEmpty()?QString():QStringLiteral("WHERE source_id=?")));if(!taskId.isEmpty())q.addBindValue(taskId);if(!q.exec())return false;while(q.next())if(indices.contains(q.value(0).toString()))tasks[indices.value(q.value(0).toString())].links.append({q.value(1).toString(),q.value(2).toString()});
    if(!taskId.isEmpty() && tasks.first().recurrenceId.isEmpty())return true;
    q.prepare(QStringLiteral("SELECT id,title,note_text,frequency,interval_n,anchor_on,weekdays_mask,month_day,year_month,end_on,active,row_version,source_task_id,task_defaults_json,deleted_at_utc FROM recurrence_templates")+(taskId.isEmpty()?QString():QStringLiteral(" WHERE id=?")));if(!taskId.isEmpty())q.addBindValue(tasks.first().recurrenceId);if(!q.exec())return false;
    QHash<QString,RecurrenceRule> rules;while(q.next()){RecurrenceRule r;r.id=q.value(0).toString();r.title=q.value(1).toString();r.notes=q.value(2).toString();r.frequency=q.value(3).toString();r.interval=q.value(4).toInt();r.anchor=QDate::fromString(q.value(5).toString(),Qt::ISODate);r.weekdays=q.value(6).toInt();r.monthDay=q.value(7).toInt();r.yearMonth=q.value(8).toInt();r.end=QDate::fromString(q.value(9).toString(),Qt::ISODate);r.active=q.value(10).toBool() && q.value(14).isNull();r.rowVersion=q.value(11).toLongLong();r.sourceTaskId=q.value(12).toString();r.defaultsJson=q.value(13).toString();rules.insert(r.id,r);}
    for(auto& task:tasks)if(rules.contains(task.recurrenceId))task.repeat=rules.value(task.recurrenceId);
    return true;
}
}

Result<QList<Task>> TaskRepository::all() const
{
    QSqlQuery query(m_db);
    if (!query.exec(selectTasks + QStringLiteral("ORDER BY t.sort_order,t.id")))
        return Result<QList<Task>>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseRead"));
    QList<Task> tasks;
    while(query.next()) tasks.append(readTask(query));
    query.finish();
    if(!loadDetails(m_db,tasks))return Result<QList<Task>>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
    return {tasks, {}};
}

Result<Task> TaskRepository::find(const QString& id) const
{
    QSqlQuery query(m_db);
    query.prepare(selectTasks + QStringLiteral("WHERE t.id=?")); query.addBindValue(id);
    if (!query.exec()) return Result<Task>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseRead"));
    if (!query.next()) return Result<Task>::failure(ErrorCode::NotFound, QStringLiteral("notFound"));
    QList<Task> tasks{readTask(query)};query.finish();
    if(!loadDetails(m_db,tasks,id))return Result<Task>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
    return {tasks.first(), {}};
}

Result<QVariantList> TaskRepository::reminderHistory() const
{
    QSqlQuery q(m_db);if(!q.exec(QStringLiteral("SELECT h.task_id,t.title,h.fired_at_utc,h.scheduled_at_utc FROM task_reminder_history h JOIN tasks t ON t.id=h.task_id WHERE t.deleted_at_utc IS NULL ORDER BY h.fired_at_utc DESC,h.rowid DESC LIMIT 50")))return Result<QVariantList>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
    QVariantList result;while(q.next())result.append(QVariantMap{{QStringLiteral("taskId"),q.value(0)},{QStringLiteral("title"),q.value(1)},{QStringLiteral("firedAt"),q.value(2)},{QStringLiteral("occurrenceAt"),q.value(3)}});return {result,{}};
}

Result<QVariantList> TaskRepository::editHistory(const QString& id) const
{
    QSqlQuery query(m_db);query.prepare(QStringLiteral("SELECT edited_at_utc,fields_json FROM task_edit_history WHERE task_id=? ORDER BY edited_at_utc DESC,rowid DESC LIMIT 200"));query.addBindValue(id);
    if(!query.exec())return Result<QVariantList>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
    QVariantList rows;while(query.next()){QStringList fields;for(const auto& field:QJsonDocument::fromJson(query.value(1).toString().toUtf8()).array())fields.append(field.toString());rows.append(QVariantMap{{QStringLiteral("editedAt"),query.value(0)},{QStringLiteral("summary"),fields.join(QStringLiteral("、"))}});}return {rows,{}};
}

Result<Task> TaskRepository::insert(Task task)
{
    QSqlQuery query(m_db);if(!query.exec(QStringLiteral("SAVEPOINT task_insert")))return Result<Task>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    auto failed=[&]{query.exec(QStringLiteral("ROLLBACK TO task_insert"));query.exec(QStringLiteral("RELEASE task_insert"));return Result<Task>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));};
    auto nullable=[](const QString& s)->QVariant{return s.isEmpty()?QVariant():QVariant(s);};
    const QStringList columns={"id","title","note_text","planning_state","status","start_on","deadline_on","today_focus","evening","created_at_utc","updated_at_utc","completed_at_utc","cancelled_at_utc","deleted_at_utc","project_id","direct_area_id","heading_id","parent_task_id","pinned","important","remind_at_utc","reminder_fired_at_utc","reminder_repeat","reminder_every","reminder_anchor_utc","reminder_zone","recurrence_template_id","occurrence_on","instance_overridden","deletion_group_id","completion_group_id"};
    const QList<QVariant> values={task.id,task.title,task.notes,task.planningState,task.status,dateSql(task.startOn),dateSql(task.deadlineOn),task.todayFocus,task.evening,task.createdAtUtc,task.updatedAtUtc,nullable(task.completedAtUtc),nullable(task.cancelledAtUtc),nullable(task.deletedAtUtc),nullable(task.projectId),nullable(task.areaId),nullable(task.headingId),nullable(task.parentTaskId),task.pinned,task.important,nullable(task.remindAtUtc),nullable(task.reminderFiredAtUtc),task.reminderRepeat,task.reminderEvery,nullable(task.reminderAnchorUtc),task.reminderZone,nullable(task.recurrenceId),task.occurrenceOn.isValid()?QVariant(task.occurrenceOn.toString(Qt::ISODate)):QVariant(),task.instanceOverridden,nullable(task.deletionGroupId),nullable(task.completionGroupId)};
    QStringList placeholders;for(int i=0;i<columns.size();++i)placeholders.append(QStringLiteral("?"));
    query.prepare(QStringLiteral("INSERT INTO tasks(%1,sort_order) VALUES(%2,(SELECT COALESCE(MAX(sort_order),0)+1 FROM tasks))").arg(columns.join(','),placeholders.join(',')));for(const auto& value:values)query.addBindValue(value);if(!query.exec())return failed();
    if(!writeComponents(task).ok())return failed();const auto inserted=find(task.id);if(!inserted.ok() || !query.exec(QStringLiteral("RELEASE task_insert")))return failed();return inserted;
}

Result<Task> TaskRepository::update(Task task, qint64 expectedVersion,bool recordEdit)
{
    const auto before=find(task.id);if(!before.ok())return before;
    QSqlQuery query(m_db);
    if(!query.exec(QStringLiteral("SAVEPOINT task_update")))return Result<Task>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    const auto rollback=[&]{QSqlQuery cleanup(m_db);cleanup.exec(QStringLiteral("ROLLBACK TO task_update"));cleanup.exec(QStringLiteral("RELEASE task_update"));};
    const auto rule=writeRepeat(task);if(!rule.ok()){rollback();return {{},rule.error};}
    query.prepare(QStringLiteral("UPDATE tasks SET title=?,note_text=?,planning_state=?,status=?,start_on=?,deadline_on=?,today_focus=?,evening=?,"
                                 "updated_at_utc=?,completed_at_utc=?,cancelled_at_utc=?,deleted_at_utc=?,project_id=?,direct_area_id=?,heading_id=?,remind_at_utc=?,reminder_fired_at_utc=?,instance_overridden=?,parent_task_id=?,pinned=?,important=?,reminder_repeat=?,reminder_every=?,reminder_anchor_utc=?,reminder_zone=?,recurrence_template_id=?,occurrence_on=?,deletion_group_id=?,completion_group_id=?,row_version=row_version+1 WHERE id=? AND row_version=?"));
    auto nullable=[](const QString& s)->QVariant { return s.isEmpty()?QVariant():QVariant(s); };
    for(const auto& value : QList<QVariant>{task.title,task.notes,task.planningState,task.status,dateSql(task.startOn),dateSql(task.deadlineOn),
            task.todayFocus,task.evening,task.updatedAtUtc,nullable(task.completedAtUtc),nullable(task.cancelledAtUtc),nullable(task.deletedAtUtc),
            nullable(task.projectId),nullable(task.areaId),nullable(task.headingId),nullable(task.remindAtUtc),nullable(task.reminderFiredAtUtc),task.instanceOverridden,nullable(task.parentTaskId),task.pinned,task.important,task.reminderRepeat,task.reminderEvery,nullable(task.reminderAnchorUtc),task.reminderZone,nullable(task.recurrenceId),task.occurrenceOn.isValid()?QVariant(task.occurrenceOn.toString(Qt::ISODate)):QVariant(),nullable(task.deletionGroupId),nullable(task.completionGroupId),task.id,expectedVersion})
        query.addBindValue(value);
    if(!query.exec()) { rollback(); return Result<Task>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseWrite")); }
    if(query.numRowsAffected()!=1) { rollback(); return Result<Task>::failure(ErrorCode::Conflict, QStringLiteral("conflict")); }
    auto failed=[&] {rollback();return Result<Task>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));};
    QJsonArray fields;const auto& old=before.value;
    if(old.title!=task.title)fields.append(QStringLiteral("标题"));if(old.notes!=task.notes)fields.append(QStringLiteral("备注"));
    if(old.planningState!=task.planningState)fields.append(QStringLiteral("任务安排"));if(old.status!=task.status)fields.append(QStringLiteral("完成状态"));
    if(old.startOn!=task.startOn || old.deadlineOn!=task.deadlineOn) {
        fields.append(QStringLiteral("日期"));
        const auto dates=[](const Task& value){return QStringLiteral("%1 — %2").arg(value.startOn?value.startOn->toString(Qt::ISODate):QStringLiteral("未设置"),value.deadlineOn?value.deadlineOn->toString(Qt::ISODate):QStringLiteral("未设置"));};
        fields.append(QStringLiteral("原日期：%1；新日期：%2").arg(dates(old),dates(task)));
    }
    if(old.todayFocus!=task.todayFocus || old.evening!=task.evening)fields.append(QStringLiteral("今天安排"));
    if(old.projectId!=task.projectId || old.areaId!=task.areaId || old.headingId!=task.headingId)fields.append(QStringLiteral("分类"));
    if(old.schedule!=task.schedule)fields.append(QStringLiteral("时间安排"));if(task.repeatChanged)fields.append(QStringLiteral("任务循环"));if(old.links!=task.links)fields.append(QStringLiteral("关联任务"));
    if(old.parentTaskId!=task.parentTaskId)fields.append(QStringLiteral("父任务"));if(old.pinned!=task.pinned)fields.append(QStringLiteral("置顶"));if(old.important!=task.important)fields.append(QStringLiteral("特别关注"));
    if(old.reminderRepeat!=task.reminderRepeat || old.reminderEvery!=task.reminderEvery || old.reminderZone!=task.reminderZone)fields.append(QStringLiteral("提醒循环"));
    if(old.tagIds!=task.tagIds)fields.append(QStringLiteral("标签"));if(old.remindAtUtc!=task.remindAtUtc)fields.append(QStringLiteral("提醒"));
    if(old.deletedAtUtc!=task.deletedAtUtc)fields.append(task.deletedAtUtc.isEmpty()?QStringLiteral("恢复删除"):QStringLiteral("删除"));
    if(recordEdit && !fields.isEmpty()) {
        query.prepare(QStringLiteral("INSERT INTO task_edit_history(id,task_id,edited_at_utc,fields_json) VALUES(?,?,?,?)"));
        query.addBindValue(QUuid::createUuid().toString(QUuid::WithoutBraces));query.addBindValue(task.id);query.addBindValue(task.updatedAtUtc);query.addBindValue(QString::fromUtf8(QJsonDocument(fields).toJson(QJsonDocument::Compact)));if(!query.exec())return failed();
    }
    const auto components=writeComponents(task);if(!components.ok())return failed();
    const auto updated=find(task.id);
    if(!updated.ok() || !query.exec(QStringLiteral("RELEASE task_update"))) { rollback(); return Result<Task>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseWrite")); }
    return updated;
}

Result<void> TaskRepository::atomically(const std::function<Result<void>()>& command)
{
    // Savepoints compose with batch, recurrence and tree commands on one connection.
    const auto name=QStringLiteral("command_")+QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-'));
    QSqlQuery query(m_db);if(!query.exec(QStringLiteral("SAVEPOINT ")+name))return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    auto rollback=[&]{query.exec(QStringLiteral("ROLLBACK TO ")+name);query.exec(QStringLiteral("RELEASE ")+name);};
    const auto result=command();if(!result.ok()){rollback();return result;}
    if(!query.exec(QStringLiteral("RELEASE ")+name)){rollback();return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));}
    return {};
}

Result<QList<Task>> TaskRepository::claimDueReminders(const QString& now)
{
    if(!m_db.transaction())return Result<QList<Task>>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    QSqlQuery q(m_db);
    if(!q.exec(selectTasks+QStringLiteral("WHERE t.deleted_at_utc IS NULL AND COALESCE(c.archived,a.archived,0)=0 AND (c.id IS NULL OR (c.deleted_at_utc IS NULL AND c.status='OPEN' AND c.planning_state='ACTIVE')) AND COALESCE(p.archived,0)=0 AND t.status='OPEN' AND t.planning_state='ACTIVE' AND (t.project_id IS NULL OR (p.deleted_at_utc IS NULL AND p.status='OPEN' AND p.planning_state='ACTIVE')) AND t.remind_at_utc IS NOT NULL AND (t.reminder_fired_at_utc IS NULL OR t.reminder_repeat<>'NONE') ORDER BY t.remind_at_utc"))) {
        m_db.rollback();return Result<QList<Task>>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
    }
    QList<Task> due;
    const auto instant=QDateTime::fromString(now,Qt::ISODateWithMs);
    while(q.next()) {auto task=readTask(q);const auto at=QDateTime::fromString(task.remindAtUtc,Qt::ISODateWithMs);if(at.isValid() && at<=instant)due.append(task);}
    q.finish();
    QList<Task> claimed;
    for(auto& task:due) {
        const auto scheduled=task.remindAtUtc;
        QDateTime next;if(task.reminderRepeat!=QStringLiteral("NONE"))next=ReminderSchedule::nextAfter(QDateTime::fromString(task.reminderAnchorUtc,Qt::ISODateWithMs),task.reminderRepeat,task.reminderEvery,QTimeZone(task.reminderZone.toUtf8()),instant);
        if(task.reminderRepeat!=QStringLiteral("NONE") && !next.isValid()){m_db.rollback();return Result<QList<Task>>::failure(ErrorCode::ValidationError,QStringLiteral("reminderInvalid"));}
        q.prepare(QStringLiteral("INSERT INTO task_reminder_history(id,task_id,scheduled_at_utc,fired_at_utc) VALUES(?,?,?,?) ON CONFLICT(task_id,scheduled_at_utc) DO NOTHING"));
        q.addBindValue(QUuid::createUuid().toString(QUuid::WithoutBraces));q.addBindValue(task.id);q.addBindValue(scheduled);q.addBindValue(now);if(!q.exec()){m_db.rollback();return Result<QList<Task>>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));}
        const bool newlyClaimed=q.numRowsAffected()==1;
        q.prepare(QStringLiteral("UPDATE tasks SET reminder_fired_at_utc=?,remind_at_utc=?,row_version=row_version+1 WHERE id=? AND row_version=? AND remind_at_utc=?"));
        q.addBindValue(now);q.addBindValue(next.isValid()?next.toString(Qt::ISODateWithMs):scheduled);q.addBindValue(task.id);q.addBindValue(task.rowVersion);q.addBindValue(scheduled);
        if(!q.exec() || q.numRowsAffected()!=1){m_db.rollback();return Result<QList<Task>>::failure(ErrorCode::Conflict,QStringLiteral("conflict"));}
        task.reminderFiredAtUtc=now;if(next.isValid())task.remindAtUtc=next.toString(Qt::ISODateWithMs);++task.rowVersion;if(newlyClaimed)claimed.append(task);
    }
    if(!m_db.commit()){m_db.rollback();return Result<QList<Task>>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));}
    return {claimed,{}};
}
Result<void> TaskRepository::reorder(const QList<Task>& tasks,bool today,const QString& now,const OrderChange& change)
{
    if(!m_db.transaction())return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    QSqlQuery q(m_db);int order=0;QSet<QString> seen;
    for(const auto& task:tasks) {
        if(seen.contains(task.id)){m_db.rollback();return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("orderInvalid"));}seen.insert(task.id);
        q.prepare(today?QStringLiteral("UPDATE tasks SET evening=?,heading_id=?,instance_overridden=CASE WHEN recurrence_template_id IS NOT NULL THEN 1 ELSE instance_overridden END,row_version=row_version+1,updated_at_utc=? WHERE id=? AND row_version=? AND deleted_at_utc IS NULL"):QStringLiteral("UPDATE tasks SET sort_order=?,evening=?,heading_id=?,instance_overridden=CASE WHEN recurrence_template_id IS NOT NULL THEN 1 ELSE instance_overridden END,row_version=row_version+1,updated_at_utc=? WHERE id=? AND row_version=? AND deleted_at_utc IS NULL"));
        if(!today)q.addBindValue(order);
        q.addBindValue(change.taskId==task.id && change.evening?*change.evening:task.evening);
        const auto heading=change.taskId==task.id && change.headingId?*change.headingId:task.headingId;
        q.addBindValue(heading.isEmpty()?QVariant():QVariant(heading));q.addBindValue(now);q.addBindValue(task.id);q.addBindValue(task.rowVersion);
        if(!q.exec() || q.numRowsAffected()!=1){m_db.rollback();return Result<void>::failure(ErrorCode::Conflict,QStringLiteral("conflict"));}
        if(today) {
            q.prepare(QStringLiteral("INSERT INTO task_view_order(view_code,task_id,sort_order) VALUES('today',?,?) ON CONFLICT(view_code,task_id) DO UPDATE SET sort_order=excluded.sort_order"));
            q.addBindValue(task.id);q.addBindValue(order);if(!q.exec()){m_db.rollback();return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));}
        }
        ++order;
    }
    if(!m_db.commit()){m_db.rollback();return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));}return {};
}
Result<bool> TaskRepository::hasOpenCompletionSibling(const Task& task) const
{
    if(task.recurrenceId.isEmpty())return {false,{}};
    QSqlQuery q(m_db);q.prepare(QStringLiteral("SELECT count(*) FROM tasks t JOIN recurrence_templates r ON r.id=t.recurrence_template_id WHERE r.frequency='AFTER_DAYS' AND t.recurrence_template_id=? AND t.id<>? AND t.status='OPEN' AND t.deleted_at_utc IS NULL"));
    q.addBindValue(task.recurrenceId);q.addBindValue(task.id);
    if(!q.exec() || !q.next())return Result<bool>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));return {q.value(0).toInt()>0,{}};
}

} // namespace daymark
