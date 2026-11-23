#include "services/RecurrenceService.h"
#include "services/TaskService.h"
#include "services/TaskCycleService.h"
#include "services/ReminderSchedule.h"
#include "persistence/SqliteDatabase.h"
#include <QTemporaryDir>
#include <QtTest>
#include <QTimeZone>
#include <QSqlQuery>
using namespace daymark;
class Clock:public IClock {
public: QDate day=QDate(2024,2,1);QDate localToday()const override{return day;}
    QTime time{12,0};
    QDateTime nowUtc()const override{return QDateTime(day,time,QTimeZone::UTC);}
};
class RecurrenceServiceTest:public QObject {
    Q_OBJECT
private slots:
    void oneTaskReopensAcrossThreeCyclesAndPreservesHistory()
    {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath("cycles.sqlite")).ok());
        Clock clock;TaskRepository tasks(db.connection());TaskCycleRepository cycles(db.connection());
        TaskCycleService lifecycle(cycles,tasks,clock);TaskService service(tasks,clock,nullptr,&lifecycle);
        const auto task=service.create("cycle",QStringLiteral("today"));QVERIFY(task.ok());const auto id=task.value.id;
        const auto child=service.createSubtask("subtask",id);QVERIFY(child.ok());
        TaskPatch edit;TaskCycle rule;rule.rule.frequency="DAILY";rule.zone="UTC";edit.cycle=rule;
        auto saved=service.edit(id,task.value.rowVersion,edit);QVERIFY(saved.ok());
        for(int round=1;round<=3;++round){
            const auto complete=service.changeState(id,saved.value.rowVersion,"complete");QVERIFY(complete.ok());
            QCOMPARE(complete.value.cycle->nextDate,clock.day.addDays(1));QCOMPARE(lifecycle.advance().value,0);
            QCOMPARE(cycles.history(id,0).value.size(),round);
            QCOMPARE(tasks.find(child.value.id).value.status,QStringLiteral("COMPLETED"));
            clock.day=clock.day.addDays(1);const auto due=lifecycle.advance();QVERIFY(due.ok());QCOMPARE(due.value,1);
            saved=tasks.find(id);QVERIFY(saved.ok());QCOMPARE(saved.value.id,id);QCOMPARE(saved.value.status,QStringLiteral("OPEN"));
            QCOMPARE(tasks.find(child.value.id).value.status,QStringLiteral("OPEN"));QCOMPARE(tasks.all().value.size(),2);
        }
        const auto complete=service.changeState(id,saved.value.rowVersion,"complete");QVERIFY(complete.ok());
        QVERIFY(service.changeState(id,complete.value.rowVersion,"reopen").ok());
        QCOMPARE(cycles.history(id,0).value.size(),4);
        QVERIFY(!cycles.history(id,0).value.first().toMap().value("revokedAt").toString().isEmpty());
    }
    void cycleCalendarBoundariesAndRollback()
    {
        Task task;task.startOn=QDate(2024,1,31);RecurrenceRule rule;rule.frequency="MONTHLY";
        auto first=TaskCycleService::first(task,rule,"UTC",*task.startOn);QVERIFY(first.ok());
        const auto next=TaskCycleService::next(task,first.value,QDateTime(QDate(2024,2,2),QTime(9,0),QTimeZone::UTC));
        QVERIFY(next.ok());QCOMPARE(next.value.first,QDate(2024,3,31));
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath("rollback.sqlite")).ok());
        Clock clock;TaskRepository tasks(db.connection());TaskCycleRepository cycles(db.connection());
        TaskCycleService lifecycle(cycles,tasks,clock);TaskService service(tasks,clock,nullptr,&lifecycle);
        const auto created=service.create("must remain open",QStringLiteral("today"));QVERIFY(created.ok());
        TaskPatch edit;TaskCycle cycle;cycle.zone="UTC";edit.cycle=cycle;
        const auto saved=service.edit(created.value.id,created.value.rowVersion,edit);QVERIFY(saved.ok());
        QSqlQuery q(db.connection());QVERIFY(q.exec("CREATE TRIGGER reject_cycle_record BEFORE INSERT ON task_cycle_completions BEGIN SELECT RAISE(ABORT,'test'); END"));
        QVERIFY(!service.changeState(saved.value.id,saved.value.rowVersion,"complete").ok());
        QCOMPARE(tasks.find(saved.value.id).value.status,QStringLiteral("OPEN"));
        QVERIFY(cycles.history(saved.value.id,0).value.isEmpty());
    }
    void relativeReminderCanFireBeforeTheSameTaskReopens()
    {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath("relative.sqlite")).ok());
        Clock clock;TaskRepository tasks(db.connection());TaskCycleRepository cycles(db.connection());
        TaskCycleService lifecycle(cycles,tasks,clock);TaskService service(tasks,clock,nullptr,&lifecycle);
        const auto task=service.create("timed cycle",QStringLiteral("today"));QVERIFY(task.ok());
        TaskSchedule schedule;schedule.allDay=false;schedule.timeZone="UTC";schedule.startUtc=clock.nowUtc();
        schedule.endUtc=schedule.startUtc.addSecs(3600);schedule.alertMinutes=30;
        TaskPatch edit;TaskCycle rule;rule.zone="UTC";edit.cycle=rule;edit.schedule=FieldPatch<TaskSchedule>::set(schedule);
        const auto saved=service.edit(task.value.id,task.value.rowVersion,edit);QVERIFY(saved.ok());
        const auto complete=service.changeState(saved.value.id,saved.value.rowVersion,"complete");QVERIFY(complete.ok());
        clock.day=clock.day.addDays(1);clock.time=QTime(11,30);
        QCOMPARE(ReminderSchedule::nextPending(tasks.all().value),clock.nowUtc());
        QCOMPARE(lifecycle.advance().value,0);
        const auto reminders=tasks.claimDueReminders(clock.nowUtc().toString(Qt::ISODateWithMs));
        QVERIFY(reminders.ok());QCOMPARE(reminders.value.size(),1);QCOMPARE(reminders.value.first().id,saved.value.id);
        QCOMPARE(tasks.find(saved.value.id).value.status,QStringLiteral("COMPLETED"));
        clock.time=QTime(12,0);const auto advance=lifecycle.advance();QVERIFY(advance.ok());QCOMPARE(advance.value,1);
        QCOMPARE(tasks.find(saved.value.id).value.status,QStringLiteral("OPEN"));
        QVERIFY(tasks.claimDueReminders(clock.nowUtc().toString(Qt::ISODateWithMs)).value.isEmpty());
    }
    void legacyCompletionCadenceConvertsWithoutCreatingAnotherTask()
    {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath("conversion.sqlite")).ok());
        Clock clock;TaskRepository tasks(db.connection());TaskCycleRepository cycles(db.connection());
        TaskCycleService lifecycle(cycles,tasks,clock);TaskService service(tasks,clock,nullptr,&lifecycle);
        const auto task=service.create("legacy cycle",QStringLiteral("today"));QVERIFY(task.ok());
        RecurrenceRule old;old.frequency="AFTER_DAYS";old.interval=3;
        TaskPatch install;install.repeat=FieldPatch<RecurrenceRule>::set(old);
        const auto legacy=service.edit(task.value.id,task.value.rowVersion,install);QVERIFY(legacy.ok());
        QVERIFY(legacy.value.repeat && legacy.value.repeat->active);

        TaskCycle cycle;cycle.rule=*legacy.value.repeat;cycle.zone="UTC";
        TaskPatch convert;convert.cycle=cycle;convert.convertLegacyCycleVersion=legacy.value.repeat->rowVersion;
        const auto current=service.edit(task.value.id,legacy.value.rowVersion,convert);QVERIFY(current.ok());
        QVERIFY(current.value.recurrenceId.isEmpty());
        QCOMPARE(current.value.cycle->rule.frequency,QStringLiteral("AFTER_DAYS"));
        QCOMPARE(current.value.id,task.value.id);
        clock.day=clock.day.addDays(4);
        const auto completed=service.changeState(task.value.id,current.value.rowVersion,"complete");QVERIFY(completed.ok());
        QCOMPARE(completed.value.cycle->nextDate,clock.day.addDays(3));
        clock.day=clock.day.addDays(3);
        const auto due=lifecycle.advance();QVERIFY(due.ok());QCOMPARE(due.value,1);
        QCOMPARE(tasks.all().value.size(),1);
        QCOMPARE(tasks.find(task.value.id).value.status,QStringLiteral("OPEN"));
        QCOMPARE(cycles.history(task.value.id,0).value.size(),1);
    }
    void inactiveRulesDoNotLoadTaskSnapshot()
    {
        QTemporaryDir directory;SqliteDatabase database;QVERIFY(database.open(directory.filePath("inactive.sqlite")).ok());Clock clock;RecurrenceRepository rules(database.connection());TaskRepository tasks(database.connection());RecurrenceService service(rules,tasks,clock);
        RecurrenceRule rule;rule.title="inactive";rule.notes=QStringLiteral("");rule.anchor=clock.day;rule.active=false;const auto saved=rules.save(rule,clock.nowUtc().toString(Qt::ISODateWithMs),clock.day);QVERIFY(saved.ok());
        // Shadow only this temporary connection's task reads. No active rule
        // means generating occurrences must not access the task snapshot.
        QSqlQuery guard(database.connection());QVERIFY(guard.exec("CREATE TEMP VIEW tasks AS SELECT * FROM missing_read_guard"));
        const auto none=service.generate();QVERIFY(none.ok());QCOMPARE(none.value,0);
        QVERIFY(guard.exec("UPDATE recurrence_templates SET active=1"));QVERIFY(!service.generate().ok());
    }
    void calendarRules()
    {
        RecurrenceRule r;r.anchor=QDate(2024,1,1);r.frequency=QStringLiteral("MONTHLY");r.monthDay=31;
        QVERIFY(RecurrenceService::occursOn(r,QDate(2024,1,31)));QVERIFY(!RecurrenceService::occursOn(r,QDate(2024,2,29)));QVERIFY(RecurrenceService::occursOn(r,QDate(2024,3,31)));
        r.frequency=QStringLiteral("YEARLY");r.monthDay=29;r.yearMonth=2;
        QVERIFY(RecurrenceService::occursOn(r,QDate(2024,2,29)));QVERIFY(!RecurrenceService::occursOn(r,QDate(2025,2,28)));
        r.frequency=QStringLiteral("WEEKLY");r.weekdays=1|64;r.interval=2;
        QVERIFY(RecurrenceService::occursOn(r,QDate(2024,1,7)));QVERIFY(!RecurrenceService::occursOn(r,QDate(2024,1,8)));QVERIFY(RecurrenceService::occursOn(r,QDate(2024,1,15)));
    }
    void idempotenceOverridesAndCompletion()
    {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("repeat.sqlite"))).ok());
        Clock clock;RecurrenceRepository repo(db.connection());TaskRepository tasks(db.connection());RecurrenceService service(repo,tasks,clock);TaskService edit(tasks,clock);
        RecurrenceRule invalid;invalid.title=QStringLiteral("never occurs");invalid.anchor=clock.day;invalid.frequency=QStringLiteral("YEARLY");invalid.yearMonth=2;invalid.monthDay=31;QVERIFY(!service.save(invalid).ok());QVERIFY(repo.all().value.isEmpty());
        RecurrenceRule r;r.title=QStringLiteral("daily");r.anchor=clock.day;auto saved=service.save(r);QVERIFY(saved.ok());
        auto generated=service.generate();QVERIFY(generated.ok());QCOMPARE(generated.value,31);QCOMPARE(service.generate().value,0);
        const auto all=tasks.all();QCOMPARE(all.value.size(),31);auto item=all.value[10];
        TaskPatch patch;patch.title=FieldPatch<QString>::set(QStringLiteral("personal"));auto changed=edit.edit(item.id,item.rowVersion,patch);QVERIFY(changed.ok());QVERIFY(changed.value.instanceOverridden);
        r=saved.value;r.title=QStringLiteral("updated");QVERIFY(service.save(r).ok());QVERIFY(service.generate().ok());
        QCOMPARE(tasks.find(item.id).value.title,QStringLiteral("personal"));
        auto stopped=service.stop(r.id,r.rowVersion+1,false);QVERIFY(stopped.ok());QCOMPARE(service.generate().value,0);
        r={};r.title=QStringLiteral("after");r.anchor=clock.day;r.frequency=QStringLiteral("AFTER_DAYS");r.interval=3;auto after=service.save(r);QVERIFY(after.ok());QCOMPARE(service.generate().value,1);
        Task occurrence;for(const auto& task:tasks.all().value)if(task.recurrenceId==after.value.id)occurrence=task;
        QVERIFY(edit.changeState(occurrence.id,occurrence.rowVersion,QStringLiteral("complete")).ok());QCOMPARE(service.generate().value,1);QCOMPARE(service.generate().value,0);
        int open=0;Task next;for(const auto& task:tasks.all().value)if(task.recurrenceId==after.value.id && task.status==QStringLiteral("OPEN")){++open;next=task;QCOMPARE(task.startOn.value(),clock.day.addDays(3));}QCOMPARE(open,1);
        QCOMPARE(edit.changeState(occurrence.id,tasks.find(occurrence.id).value.rowVersion,QStringLiteral("reopen")).error.key,QStringLiteral("recurrenceOpenInstance"));
        QVERIFY(edit.changeState(next.id,next.rowVersion,QStringLiteral("complete")).ok());QCOMPARE(service.generate().value,1);QCOMPARE(service.generate().value,0);
        open=0;for(const auto& task:tasks.all().value)if(task.recurrenceId==after.value.id && task.status==QStringLiteral("OPEN")){++open;QCOMPARE(task.startOn.value(),clock.day.addDays(3));QVERIFY(task.occurrenceOn>next.occurrenceOn);}QCOMPARE(open,1);
        Task tail;for(const auto& task:tasks.all().value)if(task.recurrenceId==after.value.id && task.status==QStringLiteral("OPEN"))tail=task;
        clock.day=clock.day.addDays(-1);
        QVERIFY(edit.changeState(tail.id,tail.rowVersion,QStringLiteral("complete")).ok());
        QCOMPARE(service.generate().value,1);QCOMPARE(service.generate().value,0);
        for(const auto& task:tasks.all().value)if(task.recurrenceId==after.value.id && task.status==QStringLiteral("OPEN")){QCOMPARE(task.syncOccurrenceKey,QStringLiteral("A:")+tail.id);QCOMPARE(task.startOn.value(),clock.day.addDays(3));}
    }
};
QTEST_GUILESS_MAIN(RecurrenceServiceTest)
#include "RecurrenceServiceTest.moc"
