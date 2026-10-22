#include "services/CalendarService.h"
#include "persistence/SqliteDatabase.h"
#include <QTemporaryDir>
#include <QtTest>
using namespace daymark;
class CalendarServiceTest:public QObject {
    Q_OBJECT
private slots:
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
