#include "TaskRepository.h"
#include <QSqlQuery>
#include <QVariant>
#include <QSet>

namespace daymark {
namespace {
const QString selectTasks = QStringLiteral(
    "SELECT t.id,t.title,t.note_text,t.planning_state,t.status,t.start_on,t.deadline_on,"
    "t.today_focus,t.evening,t.created_at_utc,t.updated_at_utc,t.completed_at_utc,t.cancelled_at_utc,"
    "t.deleted_at_utc,t.row_version,t.sort_order,t.project_id,p.planning_state,p.status,p.deleted_at_utc,"
    "t.direct_area_id,t.heading_id,p.title,t.remind_at_utc,t.reminder_fired_at_utc,o.sort_order,p.area_id,t.recurrence_template_id,t.occurrence_on,t.instance_overridden,h.title,h.sort_order "
    "FROM tasks t LEFT JOIN projects p ON p.id=t.project_id LEFT JOIN headings h ON h.id=t.heading_id LEFT JOIN task_view_order o ON o.task_id=t.id AND o.view_code='today' ");
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
    q.prepare(QStringLiteral("SELECT task_id,id,title,is_done,sort_order FROM checklist_items WHERE deleted_at_utc IS NULL ")
              +(taskId.isEmpty()?QString():QStringLiteral("AND task_id=? "))+QStringLiteral("ORDER BY sort_order,id"));
    if(!taskId.isEmpty())q.addBindValue(taskId);if(!q.exec())return false;
    while(q.next())if(indices.contains(q.value(0).toString()))tasks[indices.value(q.value(0).toString())].checklist.append({q.value(1).toString(),q.value(2).toString(),q.value(3).toBool(),q.value(4).toLongLong()});
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

Result<Task> TaskRepository::insert(Task task)
{
    if (!m_db.transaction()) return Result<Task>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseWrite"));
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT INTO tasks(id,title,note_text,planning_state,status,start_on,deadline_on,today_focus,"
                                 "created_at_utc,updated_at_utc,project_id,direct_area_id,sort_order) VALUES(?,?,?,?,'OPEN',?,?,?,?,?,?,?,"
                                 "(SELECT COALESCE(MAX(sort_order),0)+1 FROM tasks))"));
    for (const auto& value : QList<QVariant>{task.id,task.title,task.notes,task.planningState,dateSql(task.startOn),dateSql(task.deadlineOn),
                                           task.todayFocus,task.createdAtUtc,task.updatedAtUtc,
                                           task.projectId.isEmpty()?QVariant():QVariant(task.projectId),
                                           task.areaId.isEmpty()?QVariant():QVariant(task.areaId)}) query.addBindValue(value);
    if (!query.exec()) { m_db.rollback(); return Result<Task>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseWrite")); }
    const auto inserted=find(task.id);
    if (!inserted.ok() || !m_db.commit()) { m_db.rollback(); return Result<Task>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseWrite")); }
    return inserted;
}

Result<Task> TaskRepository::update(Task task, qint64 expectedVersion)
{
    if (!m_db.transaction()) return Result<Task>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseWrite"));
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE tasks SET title=?,note_text=?,planning_state=?,status=?,start_on=?,deadline_on=?,today_focus=?,evening=?,"
                                 "updated_at_utc=?,completed_at_utc=?,cancelled_at_utc=?,deleted_at_utc=?,project_id=?,direct_area_id=?,heading_id=?,remind_at_utc=?,reminder_fired_at_utc=?,instance_overridden=?,row_version=row_version+1 WHERE id=? AND row_version=?"));
    auto nullable=[](const QString& s)->QVariant { return s.isEmpty()?QVariant():QVariant(s); };
    for(const auto& value : QList<QVariant>{task.title,task.notes,task.planningState,task.status,dateSql(task.startOn),dateSql(task.deadlineOn),
            task.todayFocus,task.evening,task.updatedAtUtc,nullable(task.completedAtUtc),nullable(task.cancelledAtUtc),nullable(task.deletedAtUtc),
            nullable(task.projectId),nullable(task.areaId),nullable(task.headingId),nullable(task.remindAtUtc),nullable(task.reminderFiredAtUtc),task.instanceOverridden,task.id,expectedVersion})
        query.addBindValue(value);
    if(!query.exec()) { m_db.rollback(); return Result<Task>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseWrite")); }
    if(query.numRowsAffected()!=1) { m_db.rollback(); return Result<Task>::failure(ErrorCode::Conflict, QStringLiteral("conflict")); }
    auto failed=[&] {m_db.rollback();return Result<Task>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));};
    query.prepare(QStringLiteral("DELETE FROM task_tags WHERE task_id=?"));query.addBindValue(task.id);if(!query.exec())return failed();
    for(const auto& tag:task.tagIds){query.prepare(QStringLiteral("INSERT INTO task_tags(task_id,tag_id) VALUES(?,?)"));query.addBindValue(task.id);query.addBindValue(tag);if(!query.exec())return failed();}
    query.prepare(QStringLiteral("UPDATE checklist_items SET deleted_at_utc=?,updated_at_utc=?,row_version=row_version+1 WHERE task_id=? AND deleted_at_utc IS NULL"));
    query.addBindValue(task.updatedAtUtc);query.addBindValue(task.updatedAtUtc);query.addBindValue(task.id);if(!query.exec())return failed();
    int order=0;
    for(const auto& item:task.checklist) {
        query.prepare(QStringLiteral("INSERT INTO checklist_items(id,task_id,title,is_done,sort_order,created_at_utc,updated_at_utc) VALUES(?,?,?,?,?,?,?) "
            "ON CONFLICT(id) DO UPDATE SET title=excluded.title,is_done=excluded.is_done,sort_order=excluded.sort_order,updated_at_utc=excluded.updated_at_utc,deleted_at_utc=NULL,row_version=checklist_items.row_version+1 WHERE checklist_items.task_id=excluded.task_id"));
        for(const auto& value:QList<QVariant>{item.id,task.id,item.title,item.done,order++,task.updatedAtUtc,task.updatedAtUtc})query.addBindValue(value);
        if(!query.exec() || query.numRowsAffected()!=1)return failed();
    }
    const auto updated=find(task.id);
    if(!updated.ok() || !m_db.commit()) { m_db.rollback(); return Result<Task>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseWrite")); }
    return updated;
}

Result<QList<Task>> TaskRepository::claimDueReminders(const QString& now)
{
    if(!m_db.transaction())return Result<QList<Task>>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    QSqlQuery q(m_db);
    if(!q.exec(selectTasks+QStringLiteral("WHERE t.deleted_at_utc IS NULL AND t.status='OPEN' AND t.planning_state='ACTIVE' AND (t.project_id IS NULL OR (p.deleted_at_utc IS NULL AND p.status='OPEN' AND p.planning_state='ACTIVE')) AND t.remind_at_utc IS NOT NULL AND t.reminder_fired_at_utc IS NULL ORDER BY t.remind_at_utc"))) {
        m_db.rollback();return Result<QList<Task>>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
    }
    QList<Task> due;
    const auto instant=QDateTime::fromString(now,Qt::ISODateWithMs);
    while(q.next()) {auto task=readTask(q);const auto at=QDateTime::fromString(task.remindAtUtc,Qt::ISODateWithMs);if(at.isValid() && at<=instant)due.append(task);}
    q.finish();
    for(auto& task:due) {
        q.prepare(QStringLiteral("UPDATE tasks SET reminder_fired_at_utc=?,row_version=row_version+1 WHERE id=? AND row_version=? AND reminder_fired_at_utc IS NULL"));
        q.addBindValue(now);q.addBindValue(task.id);q.addBindValue(task.rowVersion);
        if(!q.exec() || q.numRowsAffected()!=1){m_db.rollback();return Result<QList<Task>>::failure(ErrorCode::Conflict,QStringLiteral("conflict"));}
        task.reminderFiredAtUtc=now;++task.rowVersion;
    }
    if(!m_db.commit()){m_db.rollback();return Result<QList<Task>>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));}
    return {due,{}};
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
