#include "services/EventService.h"
#include "services/TaskService.h"
#include "persistence/DataTransfer.h"
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QElapsedTimer>
#include <QtTest>
using namespace daymark;
class EventServiceTest:public QObject {
    Q_OBJECT
private slots:
    void tenThousandEvents() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("benchmark.sqlite"))).ok());auto connection=db.connection();QVERIFY(connection.transaction());QSqlQuery q(connection);
        for(int i=0;i<10000;i++) {
            q.prepare(QStringLiteral("INSERT INTO events(id,title,all_day,start_on,end_on,created_at_utc,updated_at_utc) VALUES(?,?,1,'2026-10-23','2026-10-24','2026-10-23T00:00:00.000Z','2026-10-23T00:00:00.000Z')"));q.addBindValue(QString::number(i));q.addBindValue(QStringLiteral("event %1").arg(i));QVERIFY(q.exec());
        }
        QVERIFY(connection.commit());QElapsedTimer timer;timer.start();const auto events=EventRepository(connection).all();QVERIFY(events.ok());QCOMPARE(events.value.size(),10000);const auto loaded=timer.elapsed();timer.restart();int count=0;const auto zone=QTimeZone::systemTimeZone();for(const auto& e:events.value)count+=EventService::visibleDates(e,QDate(2026,10,1),QDate(2026,11,11),zone).size();QCOMPARE(count,10000);const auto projected=timer.elapsed();qInfo()<<"10k events: load ms"<<loaded<<"visible-date index ms"<<projected;QVERIFY(loaded<5000 && projected<5000);
    }
    void rangesAndDst() {
        CalendarEvent e;e.title=QStringLiteral("event");e.startOn=QDate(2026,10,23);e.endOn=e.startOn.addDays(1);
        QVERIFY(EventService::validate(e).ok());QVERIFY(EventService::onDate(e,e.startOn));QVERIFY(!EventService::onDate(e,e.endOn));
        e.endOn=e.startOn;QVERIFY(!EventService::validate(e).ok());
        QVERIFY(!EventService::localInstant(QStringLiteral("2026-03-08 02:30"),QStringLiteral("America/New_York")).ok());
        QVERIFY(!EventService::localInstant(QStringLiteral("2026-11-01 01:30"),QStringLiteral("America/New_York")).ok());
        QVERIFY(EventService::localInstant(QStringLiteral("2026-03-08 03:30"),QStringLiteral("America/New_York")).ok());
        QVERIFY(!EventService::localInstant(QStringLiteral("2026-03-08 03:30"),QStringLiteral("invalid/zone")).ok());
        e.allDay=false;e.startOn={};e.endOn={};e.timeZone=QStringLiteral("Asia/Shanghai");
        e.startUtc=EventService::localInstant(QStringLiteral("2026-10-23 23:00"),e.timeZone).value;e.endUtc=EventService::localInstant(QStringLiteral("2026-10-24 00:00"),e.timeZone).value;
        QVERIFY(EventService::validate(e).ok());QVERIFY(EventService::onDate(e,QDate(2026,10,23),QTimeZone(e.timeZone.toUtf8())));QVERIFY(!EventService::onDate(e,QDate(2026,10,24),QTimeZone(e.timeZone.toUtf8())));
    }
    void relationsConflictRollbackAndRoundtrip() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("events.sqlite"))).ok());
        SystemClock clock;TaskRepository tasks(db.connection());TaskService taskService(tasks,clock);const auto task=taskService.create(QStringLiteral("prepare"));QVERIFY(task.ok());
        const auto other=taskService.create(QStringLiteral("follow up"));QVERIFY(other.ok());
        EventRepository repo(db.connection());EventService service(repo,clock);CalendarEvent e;e.title=QStringLiteral("review");e.startOn=QDate(2026,10,23);e.endOn=e.startOn.addDays(1);e.links={{task.value.id,QStringLiteral("preparation")}};
        QVERIFY(service.save(e).ok());e=repo.all().value.first();QCOMPARE(e.links.size(),1);
        auto invalid=e;invalid.title=QStringLiteral("must rollback");invalid.links.append({QStringLiteral("missing"),QStringLiteral("followup")});QVERIFY(!service.save(invalid).ok());QCOMPARE(repo.all().value.first().title,e.title);
        QSqlQuery q(db.connection());QVERIFY(q.exec(QStringLiteral("CREATE TRIGGER fail_event_link BEFORE INSERT ON event_tasks BEGIN SELECT RAISE(ABORT,'injected'); END")));
        invalid=e;invalid.title=QStringLiteral("SQL rollback");QVERIFY(!service.save(invalid).ok());QCOMPARE(repo.all().value.first().title,e.title);QCOMPARE(repo.all().value.first().links.size(),1);QVERIFY(q.exec(QStringLiteral("DROP TRIGGER fail_event_link")));
        e.startOn=e.startOn.addDays(1);e.endOn=e.endOn.addDays(1);QVERIFY(service.save(e).ok());QCOMPARE(tasks.find(task.value.id).value.startOn,task.value.startOn);
        QCOMPARE(service.save(e).error.code,ErrorCode::Conflict);e=repo.all().value.first();
        QVERIFY(taskService.changeState(task.value.id,task.value.rowVersion,QStringLiteral("complete")).ok());QCOMPARE(repo.all().value.size(),1);
        DataTransfer transfer(db);const auto path=dir.filePath(QStringLiteral("events.json"));QVERIFY(transfer.exportJson(path).ok());const auto plan=DataTransfer::preview(path);QVERIFY(plan.ok());
        auto merge=plan.value;auto mergeData=merge.document.value(QStringLiteral("tables")).toObject();auto mergeLinks=mergeData.value(QStringLiteral("event_tasks")).toArray();mergeLinks.append(QJsonObject{{"event_id",e.id},{"task_id",other.value.id},{"role",QStringLiteral("followup")}});mergeData.insert(QStringLiteral("event_tasks"),mergeLinks);merge.document.insert(QStringLiteral("tables"),mergeData);QVERIFY(transfer.apply(merge,false).ok());QCOMPARE(repo.all().value.first().links.size(),1);
        QVERIFY(transfer.apply(plan.value,true).ok());QCOMPARE(repo.all().value.first().links.size(),1);
        auto currentTask=tasks.find(task.value.id).value;auto deleted=taskService.changeState(currentTask.id,currentTask.rowVersion,QStringLiteral("delete"));QVERIFY(deleted.ok());QCOMPARE(repo.all().value.size(),1);QVERIFY(repo.all().value.first().links.isEmpty());QVERIFY(taskService.changeState(currentTask.id,deleted.value.rowVersion,QStringLiteral("restore")).ok());QCOMPARE(repo.all().value.first().links.size(),1);
        QVERIFY(service.setDeleted(e.id,e.rowVersion,true).ok());QVERIFY(repo.all().value.isEmpty());QVERIFY(!tasks.find(task.value.id).value.deletedAtUtc.size());
        QVERIFY(service.setDeleted(e.id,e.rowVersion+1,false).ok());QCOMPARE(repo.all().value.first().links.size(),1);
        auto corrupted=plan.value;auto data=corrupted.document.value(QStringLiteral("tables")).toObject();auto events=data.value(QStringLiteral("events")).toArray();auto row=events.first().toObject();row.insert(QStringLiteral("end_on"),row.value(QStringLiteral("start_on")));events[0]=row;data.insert(QStringLiteral("events"),events);corrupted.document.insert(QStringLiteral("tables"),data);
        QVERIFY(!transfer.apply(corrupted,true).ok());QCOMPARE(repo.all().value.size(),1);
        auto legacy=plan.value;data=legacy.document.value(QStringLiteral("tables")).toObject();data.remove(QStringLiteral("events"));data.remove(QStringLiteral("event_tasks"));legacy.document.insert(QStringLiteral("tables"),data);legacy.document.insert(QStringLiteral("schemaVersion"),2);QVERIFY(transfer.apply(legacy,true).ok());QVERIFY(repo.all().value.isEmpty());
    }
    void schemaTwoUpgradeBackup() {
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("upgrade.sqlite"));
        {SqliteDatabase db;QVERIFY(db.open(path).ok());QSqlQuery q(db.connection());QVERIFY(q.exec(QStringLiteral("DROP TABLE event_tasks")));QVERIFY(q.exec(QStringLiteral("DROP TABLE events")));QVERIFY(q.exec(QStringLiteral("PRAGMA user_version=2")));}
        QFile blocker(dir.filePath(QStringLiteral("backups")));QVERIFY(blocker.open(QIODevice::WriteOnly));blocker.close();{SqliteDatabase db;QVERIFY(!db.open(path).ok());}QVERIFY(blocker.remove());
        SqliteDatabase db;QVERIFY(db.open(path).ok());QCOMPARE(QDir(dir.filePath(QStringLiteral("backups/migrations"))).entryList({QStringLiteral("*.sqlite")},QDir::Files).size(),1);QVERIFY(EventRepository(db.connection()).all().ok());
    }
};
QTEST_GUILESS_MAIN(EventServiceTest)
#include "EventServiceTest.moc"
