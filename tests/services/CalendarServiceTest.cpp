#include "services/CalendarService.h"
#include "persistence/SqliteDatabase.h"
#include <QTemporaryDir>
#include <QtTest>
using namespace daymark;
class CalendarServiceTest:public QObject {
    Q_OBJECT
private slots:
    void calendarRangeMoveAndEdgeAreAtomic() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath("range-move.sqlite")).ok());SystemClock clock;TaskRepository repo(db.connection());TaskService service(repo,clock);
        auto saved=service.create("range");QVERIFY(saved.ok());TaskPatch dates;dates.startOn=FieldPatch<QDate>::set(QDate(2026,10,24));dates.deadlineOn=FieldPatch<QDate>::set(QDate(2026,11,20));saved=service.edit(saved.value.id,saved.value.rowVersion,dates);QVERIFY(saved.ok());
        auto moved=CalendarService::moveDate(service,saved.value,saved.value.rowVersion,QDate(2026,10,30),QDate(2026,11,2),"move",QTimeZone::UTC);QVERIFY(moved.ok());QCOMPARE(*moved.value.startOn,QDate(2026,10,27));QCOMPARE(*moved.value.deadlineOn,QDate(2026,11,23));
        auto edge=CalendarService::moveDate(service,moved.value,moved.value.rowVersion,QDate(2026,11,23),QDate(2026,11,24),"deadline",QTimeZone::UTC);QVERIFY(edge.ok());QCOMPARE(edge.value.startOn,moved.value.startOn);QCOMPARE(*edge.value.deadlineOn,QDate(2026,11,24));
        const auto before=repo.find(saved.value.id).value;QVERIFY(!CalendarService::moveDate(service,before,before.rowVersion,QDate(2026,11,24),QDate(2026,10,1),"deadline",QTimeZone::UTC).ok());QCOMPARE(repo.find(before.id).value,before);QVERIFY(!CalendarService::moveDate(service,before,saved.value.rowVersion,QDate(2026,11,24),QDate(2026,12,1),"move",QTimeZone::UTC).ok());QCOMPARE(repo.find(before.id).value,before);
        Task inbox;inbox.id="inbox";inbox.planningState="INBOX";QCOMPARE(CalendarService::unscheduled({inbox}).size(),1);
        auto captured=service.create("inbox date","inbox");QVERIFY(captured.ok());auto arranged=CalendarService::moveDate(service,captured.value,captured.value.rowVersion,QDate(2026,10,25),QDate(2026,10,25),"move",QTimeZone::UTC);QVERIFY(arranged.ok());QVERIFY(QueryService::effectiveActive(arranged.value));QCOMPARE(CalendarService::entries({arranged.value},QDate(2026,10,25)).size(),1);
    }
    void timeBlocksOverlapAndRespectDstAndMidnight() {
        const QTimeZone zone("America/New_York");QList<Task> tasks;
        auto add=[&](const QString& id,const QString& start,const QString& end){Task task;task.id=id;task.planningState="ACTIVE";TaskSchedule schedule;schedule.allDay=false;schedule.timeZone="America/New_York";schedule.startUtc=QDateTime::fromString(start,Qt::ISODate).toUTC();schedule.endUtc=QDateTime::fromString(end,Qt::ISODate).toUTC();task.schedule=schedule;tasks.append(task);};
        add("a","2026-03-08T01:30:00-05:00","2026-03-08T03:30:00-04:00");add("b","2026-03-08T03:00:00-04:00","2026-03-08T04:00:00-04:00");add("c","2026-03-08T04:00:00-04:00","2026-03-09T00:00:00-04:00");
        auto blocks=CalendarService::timeBlocks(tasks,QDate(2026,3,8),2,zone);QCOMPARE(blocks.size(),3);QCOMPARE(blocks[0].startMinute,90.0);QCOMPARE(blocks[0].endMinute,150.0);QCOMPARE(blocks[0].laneCount,2);QCOMPARE(blocks[1].laneCount,2);QCOMPARE(blocks[2].laneCount,1);QCOMPARE(blocks[2].endMinute,1380.0);
        tasks.clear();add("fall","2026-11-01T00:00:00-04:00","2026-11-02T00:00:00-05:00");blocks=CalendarService::timeBlocks(tasks,QDate(2026,11,1),2,zone);QCOMPARE(blocks.size(),1);QCOMPARE(blocks[0].endMinute,1500.0);QVERIFY(!blocks[0].continuesAfter);
    }
    void timeMoveKeepsIdentityDurationAndRejectsInvalidBoundary() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath("time-move.sqlite")).ok());SystemClock clock;TaskRepository repo(db.connection());TaskService service(repo,clock);
        auto saved=service.create("timed");QVERIFY(saved.ok());TaskSchedule schedule;schedule.allDay=false;schedule.timeZone="Asia/Tokyo";schedule.startUtc=QDateTime::fromString("2026-10-25T03:00:00Z",Qt::ISODate);schedule.endUtc=schedule.startUtc.addSecs(5400);TaskPatch patch;patch.schedule=FieldPatch<TaskSchedule>::set(schedule);saved=service.edit(saved.value.id,saved.value.rowVersion,patch);QVERIFY(saved.ok());
        auto moved=CalendarService::moveTime(service,saved.value,saved.value.rowVersion,QDate(2026,11,1),90,"move",QTimeZone("America/New_York"));QVERIFY(moved.ok());QCOMPARE(moved.value.id,saved.value.id);QCOMPARE(moved.value.schedule->timeZone,QString("Asia/Tokyo"));QCOMPARE(moved.value.schedule->startUtc.msecsTo(moved.value.schedule->endUtc),5400000LL);QCOMPARE(moved.value.schedule->startUtc,QDateTime::fromString("2026-11-01T05:30:00Z",Qt::ISODate));
        const auto before=repo.find(saved.value.id).value;QVERIFY(!CalendarService::moveTime(service,before,before.rowVersion,QDate(2026,11,1),30,"end",QTimeZone("America/New_York")).ok());QCOMPARE(repo.find(before.id).value,before);QVERIFY(!CalendarService::moveTime(service,before,saved.value.rowVersion,QDate(2026,11,2),90,"move",QTimeZone::UTC).ok());QCOMPARE(repo.find(before.id).value,before);
    }
    void taskRangesIncludeInteriorDaysAndKeepEndpointMeaning() {
        Task task;task.id=QStringLiteral("range");task.planningState=QStringLiteral("ACTIVE");task.startOn=QDate(2026,10,24);task.deadlineOn=QDate(2026,11,20);
        const auto ranges=CalendarService::ranges({task},QDate(2026,11,1),QDate(2026,11,30));QCOMPARE(ranges.size(),1);
        QCOMPARE(ranges.first().start,QDate(2026,10,24));QCOMPARE(ranges.first().end,QDate(2026,11,20));
        QCOMPARE(CalendarService::entries({task},QDate(2026,10,24)).first().kind,QStringLiteral("start"));
        QCOMPARE(CalendarService::entries({task},QDate(2026,11,1)).first().kind,QStringLiteral("range"));
        QCOMPARE(CalendarService::entries({task},QDate(2026,11,20)).first().kind,QStringLiteral("deadline"));
        QVERIFY(CalendarService::entries({task},QDate(2026,11,21)).isEmpty());
        int count=0;for(const auto& day:CalendarService::days({task},QDate(2026,10,24),false,false)){QVERIFY(day.entries.size()<=1);count+=day.entries.size();}QCOMPARE(count,16);
        task.startOn=QDate(1000,1,1);task.deadlineOn=QDate(9000,1,1);count=0;
        for(const auto& day:CalendarService::days({task},QDate(2024,2,29),false,false)){QCOMPARE(day.entries.size(),1);++count;}QCOMPARE(count,42);
        task.planningState=QStringLiteral("SOMEDAY");QVERIFY(CalendarService::ranges({task},QDate(2024,2,1),QDate(2024,3,1)).isEmpty());
        task.planningState=QStringLiteral("ACTIVE");task.status=QStringLiteral("COMPLETED");QVERIFY(CalendarService::ranges({task},QDate(2024,2,1),QDate(2024,3,1)).isEmpty());
        QCOMPARE(CalendarService::ranges({task},QDate(2024,2,1),QDate(2024,3,1),true).size(),1);
        task.status=QStringLiteral("OPEN");task.startOn=task.deadlineOn;QVERIFY(CalendarService::ranges({task},QDate(9000,1,1),QDate(9000,1,2)).isEmpty());
        task.startOn.reset();QVERIFY(CalendarService::ranges({task},QDate(9000,1,1),QDate(9000,1,2)).isEmpty());
    }
    void completedAndUnscheduledRespectEffectiveState()
    {
        Task open;open.id=QStringLiteral("open");open.planningState=QStringLiteral("ACTIVE");
        Task completed=open;completed.id=QStringLiteral("done");completed.status=QStringLiteral("COMPLETED");completed.startOn=QDate(2026,10,22);
        Task someday=open;someday.planningState=QStringLiteral("SOMEDAY");
        QCOMPARE(CalendarService::unscheduled({open,completed,someday}).size(),1);
        QVERIFY(CalendarService::entries({completed},QDate(2026,10,22)).isEmpty());
        QCOMPARE(CalendarService::entries({completed},QDate(2026,10,22),true).size(),1);
        completed.projectId=QStringLiteral("p");completed.projectStatus=QStringLiteral("COMPLETED");
        QVERIFY(CalendarService::entries({completed},QDate(2026,10,22),true).isEmpty());
    }
    void dateGridBoundaries()
    {
        auto days=CalendarService::days({},QDate(2024,2,29),false,false);
        QCOMPARE(days.size(),42);QCOMPARE(days.first().date,QDate(2024,1,29));
        QCOMPARE(days[31].date,QDate(2024,2,29));
        days=CalendarService::days({},QDate(2023,1,1),true,false);
        QCOMPARE(days.first().date,QDate(2022,12,26));QCOMPARE(days.last().date,QDate(2023,1,1));
        days=CalendarService::days({},QDate(2023,1,1),true,true);
        QCOMPARE(days.first().date,QDate(2023,1,1));QCOMPARE(days.size(),7);
        QVERIFY(CalendarService::days({},QDate(),false,false).isEmpty());
    }
    void markersAndMovesAreAtomic()
    {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("calendar.sqlite"))).ok());
        SystemClock clock;TaskRepository repo(db.connection());TaskService service(repo,clock);
        auto task=service.create(QStringLiteral("双标记"),QStringLiteral("date:2024-02-29"));QVERIFY(task.ok());
        TaskPatch patch;patch.deadlineOn=FieldPatch<QDate>::set(QDate(2024,2,29));
        auto saved=service.edit(task.value.id,1,patch);QVERIFY(saved.ok());
        auto entries=CalendarService::entries(repo.all().value,QDate(2024,2,29));
        QCOMPARE(entries.size(),1);QCOMPARE(entries.first().kind,QStringLiteral("both"));
        QVERIFY(!CalendarService::move(service,saved.value.id,saved.value.rowVersion,QStringLiteral("both"),QDate(2024,3,1)).ok());
        QVERIFY(!CalendarService::move(service,saved.value.id,saved.value.rowVersion,QStringLiteral("start"),QDate(2024,3,1)).ok());
        QCOMPARE(repo.find(saved.value.id).value,saved.value);
        auto moved=CalendarService::move(service,saved.value.id,saved.value.rowVersion,QStringLiteral("deadline"),QDate(2024,3,1));QVERIFY(moved.ok());
        QCOMPARE(CalendarService::entries(repo.all().value,QDate(2024,2,29)).first().kind,QStringLiteral("start"));
        QCOMPARE(CalendarService::entries(repo.all().value,QDate(2024,3,1)).first().kind,QStringLiteral("deadline"));
        QVERIFY(!CalendarService::move(service,moved.value.id,saved.value.rowVersion,QStringLiteral("start"),QDate(2024,2,28)).ok());
        patch={};patch.planningState=FieldPatch<QString>::set(QStringLiteral("SOMEDAY"));
        QVERIFY(service.edit(moved.value.id,moved.value.rowVersion,patch).ok());
        QVERIFY(CalendarService::entries(repo.all().value,QDate(2024,3,1)).isEmpty());
        QVERIFY(!service.create(QStringLiteral("invalid"),QStringLiteral("date:2024-02-30")).ok());
    }
};
QTEST_GUILESS_MAIN(CalendarServiceTest)
#include "CalendarServiceTest.moc"
