#include "services/RecurrenceService.h"
#include "services/TaskService.h"
#include "persistence/SqliteDatabase.h"
#include <QTemporaryDir>
#include <QtTest>
#include <QTimeZone>
using namespace daymark;
class Clock:public IClock {
public: QDate day=QDate(2024,2,1);QDate localToday()const override{return day;}
    QDateTime nowUtc()const override{return QDateTime(day,QTime(12,0),QTimeZone::UTC);}
};
class RecurrenceServiceTest:public QObject {
    Q_OBJECT
private slots:
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
    }
};
QTEST_GUILESS_MAIN(RecurrenceServiceTest)
#include "RecurrenceServiceTest.moc"
