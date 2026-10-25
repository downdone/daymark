#include "EventRepository.h"
#include "TaskRepository.h"
#include "RecurrenceRepository.h"
#include "services/TaskService.h"
#include "services/EventService.h"
#include "services/RecurrenceService.h"
#include <QSqlQuery>
namespace daymark {
namespace {
class CommandClock final:public IClock {public:explicit CommandClock(const QString& now):at(QDateTime::fromString(now,Qt::ISODateWithMs)){}QDate localToday()const override{return at.toLocalTime().date();}QDateTime nowUtc()const override{return at;}QDateTime at;};
}
// Compatibility adapter: CalendarEvent is a projection of a Task, not storage.
Result<QList<CalendarEvent>> EventRepository::all() const
{
    TaskRepository repository(m_db);const auto tasks=repository.all();if(!tasks.ok())return {{},tasks.error};QList<CalendarEvent> result;
    for(const auto& task:tasks.value)if(task.schedule && task.deletedAtUtc.isEmpty()){
        result.append(EventService::projection(task));
    }
    return {result,{}};
}
Result<QVariantList> EventRepository::notifications() const
{return TaskRepository(m_db).reminderHistory();}
Result<QVariantList> EventRepository::claimReminders(const QVariantList&)
{return Result<QVariantList>::failure(ErrorCode::UnsupportedPlatform,QStringLiteral("actionInvalid"));}
Result<QVariantList> EventRepository::claimTaskReminders(const QString& now)
{
    const auto due=TaskRepository(m_db).claimDueReminders(now);if(!due.ok())return {{},due.error};QVariantList rows;for(const auto& task:due.value)rows.append(QVariantMap{{"taskId",task.id},{"eventId",task.id},{"title",task.title},{"firedAt",now}});return {rows,{}};
}
Result<void> EventRepository::save(CalendarEvent event,const QString& now)
{
    QSqlQuery q(m_db);if(!q.exec(QStringLiteral("SAVEPOINT calendar_task")))return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    auto failed=[&](Error error){q.exec(QStringLiteral("ROLLBACK TO calendar_task"));q.exec(QStringLiteral("RELEASE calendar_task"));return Result<void>{error};};
    CommandClock clock(now);TaskRepository repository(m_db);TaskService service(repository,clock);const bool creating=event.id.isEmpty();
    Task task;if(creating){const auto created=service.create(event.title,QStringLiteral("anytime"));if(!created.ok())return failed(created.error);task=created.value;}else{const auto found=repository.find(event.id);if(!found.ok())return failed(found.error);task=found.value;if(task.rowVersion!=event.rowVersion)return failed({ErrorCode::Conflict,QStringLiteral("eventConflict")});}
    TaskPatch patch;patch.title=FieldPatch<QString>::set(event.title);patch.notes=FieldPatch<QString>::set(event.notes);patch.links=FieldPatch<QList<TaskLink>>::set(event.links);
    TaskSchedule s;s.allDay=event.allDay;s.startOn=event.startOn;s.endOn=event.endOn;s.startUtc=event.startUtc;s.endUtc=event.endUtc;s.timeZone=event.timeZone;s.alertMinutes=event.alertMinutes;patch.schedule=FieldPatch<TaskSchedule>::set(s);
    const auto anchor=s.allDay?s.startOn:s.startUtc.toTimeZone(QTimeZone(s.timeZone.toUtf8())).date();patch.startOn=FieldPatch<QDate>::set(anchor);
    if(event.repeatFrequency!=QStringLiteral("NONE")){
        RecurrenceRule r;r.frequency=event.repeatFrequency;r.interval=event.repeatInterval;r.end=event.repeatUntil;r.anchor=anchor;r.weekdays=1<<(anchor.dayOfWeek()-1);r.monthDay=anchor.day();r.yearMonth=anchor.month();if(task.repeat)r.rowVersion=task.repeat->rowVersion;
        patch.repeat=FieldPatch<RecurrenceRule>::set(r);
    }else if(task.repeat && task.repeat->active)patch.repeat=FieldPatch<RecurrenceRule>::clear();
    const auto saved=service.edit(task.id,task.rowVersion,patch);if(!saved.ok())return failed(saved.error);
    RecurrenceRepository rules(m_db);const auto generated=RecurrenceService(rules,repository,clock).generate();if(!generated.ok())return failed(generated.error);
    if(!q.exec(QStringLiteral("RELEASE calendar_task")))return failed({ErrorCode::PersistenceError,QStringLiteral("databaseWrite")});return {};
}
Result<void> EventRepository::setDeleted(const QString& id,qint64 version,bool deleted,const QString& now)
{
    CommandClock clock(now);TaskRepository repository(m_db);const auto changed=TaskService(repository,clock).changeState(id,version,deleted?QStringLiteral("delete"):QStringLiteral("restore"));if(!changed.ok())return {changed.error};return {};
}
}
