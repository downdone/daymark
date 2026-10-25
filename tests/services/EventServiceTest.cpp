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
#include "../support/LegacyDatabase.h"
using namespace daymark;
class EventTestClock:public IClock {public:QDateTime now=QDateTime(QDate(2026,3,7),QTime(14,0),QTimeZone::UTC);QDate localToday()const override{return now.date();}QDateTime nowUtc()const override{return now;}};
class EventServiceTest:public QObject {
    Q_OBJECT
private slots:
    void seriesDatesAndDurableEventReminders() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("series.sqlite"))).ok());EventTestClock clock;EventRepository repo(db.connection());EventService service(repo,clock);
        CalendarEvent e;e.title=QStringLiteral("series");e.startOn=QDate(2028,1,31);e.endOn=e.startOn.addDays(1);e.repeatFrequency=QStringLiteral("MONTHLY");
        QVERIFY(!EventService::onDate(e,QDate(2028,2,29)));QVERIFY(EventService::onDate(e,QDate(2028,3,31)));
        e.startOn=QDate(2028,2,29);e.endOn=e.startOn.addDays(1);e.repeatFrequency=QStringLiteral("YEARLY");QVERIFY(!EventService::onDate(e,QDate(2029,2,28)));QVERIFY(EventService::onDate(e,QDate(2032,2,29)));
        e.allDay=false;e.startOn={};e.endOn={};e.timeZone=QStringLiteral("America/New_York");e.repeatFrequency=QStringLiteral("DAILY");e.startUtc=clock.now;e.endUtc=clock.now.addSecs(3600);e.alertMinutes=0;
        const auto projected=EventService::occurrences(e,QDate(2026,3,8),QDate(2026,3,8),QTimeZone(e.timeZone.toUtf8()));QVERIFY(!projected.isEmpty());bool wallTime=false;for(const auto& instance:projected)if(instance.startUtc.toTimeZone(QTimeZone(e.timeZone.toUtf8())).date()==QDate(2026,3,8)){QCOMPARE(instance.startUtc.time(),QTime(13,0));wallTime=true;}QVERIFY(wallTime);
        QVERIFY(service.save(e).ok());e=repo.all().value.first();QCOMPARE(e.repeatFrequency,QStringLiteral("DAILY"));QCOMPARE(e.alertMinutes,0);
        auto due=service.claimReminders();QVERIFY(due.ok());QCOMPARE(due.value.size(),1);QCOMPARE(service.claimReminders().value.size(),0);
        EventRepository reopened(db.connection());EventService restarted(reopened,clock);QCOMPARE(restarted.claimReminders().value.size(),0);
        clock.now=clock.now.addDays(1);QCOMPARE(restarted.claimReminders().value.size(),1);QCOMPARE(restarted.notifications().value.size(),2);
        DataTransfer transfer(db);const auto output=dir.filePath(QStringLiteral("series.json"));QVERIFY(transfer.exportJson(output).ok());const auto plan=transfer.preview(output,false);QVERIFY(plan.ok());QVERIFY(transfer.apply(plan.value,true).ok());QCOMPARE(restarted.claimReminders().value.size(),0);QCOMPARE(repo.all().value.first().repeatFrequency,QStringLiteral("DAILY"));
        e.rowVersion=TaskRepository(db.connection()).find(e.id).value.rowVersion;
        QVERIFY(service.setDeleted(e.id,e.rowVersion,true).ok());QVERIFY(restarted.claimReminders().value.isEmpty());QCOMPARE(restarted.notifications().value.size(),1);
        QVERIFY(service.setDeleted(e.id,e.rowVersion+1,false).ok());QVERIFY(restarted.claimReminders().value.isEmpty());
        e.repeatInterval=0;QVERIFY(!EventService::validate(e).ok());e.repeatInterval=1;e.repeatUntil=QDate(2026,1,1);QVERIFY(!EventService::validate(e).ok());
    }
    void schemaThreeUpgradePreservesEvents() {
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("legacy.sqlite"));
        QVERIFY(createLegacyDatabase(path,3));
        {const auto name=QUuid::createUuid().toString();{auto raw=QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),name);raw.setDatabaseName(path);QVERIFY(raw.open());QSqlQuery q(raw);QVERIFY(q.exec(QStringLiteral("INSERT INTO events(id,title,all_day,start_on,end_on,created_at_utc,updated_at_utc) VALUES('legacy','legacy',1,'2026-03-07','2026-03-08','2026-03-07T00:00:00.000Z','2026-03-07T00:00:00.000Z')")));q.finish();raw.close();}QSqlDatabase::removeDatabase(name);}

        SqliteDatabase db;QVERIFY(db.open(path).ok());const auto events=EventRepository(db.connection()).all();QVERIFY(events.ok());QCOMPARE(events.value.size(),1);QCOMPARE(events.value.first().title,QStringLiteral("legacy"));QCOMPARE(events.value.first().repeatFrequency,QStringLiteral("NONE"));QCOMPARE(events.value.first().alertMinutes,-1);QCOMPARE(QDir(dir.filePath(QStringLiteral("backups/migrations"))).entryList({QStringLiteral("*.sqlite")},QDir::Files).size(),1);
    }

    void allDayLeadEndAndReadOnlyFailure() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("lead.sqlite"))).ok());EventTestClock clock;EventRepository repo(db.connection());EventService service(repo,clock);
        CalendarEvent e;e.title=QStringLiteral("all day");e.startOn=QDate(2026,3,7);e.endOn=e.startOn.addDays(1);e.repeatFrequency=QStringLiteral("DAILY");e.repeatInterval=2;e.repeatUntil=QDate(2026,3,9);e.alertMinutes=5;QVERIFY(service.save(e).ok());
        clock.now=QDateTime(e.startOn,QTime(8,54),QTimeZone::systemTimeZone()).toUTC();QVERIFY(service.claimReminders().value.isEmpty());clock.now=clock.now.addSecs(60);
        QSqlQuery q(db.connection());QVERIFY(q.exec(QStringLiteral("PRAGMA query_only=ON")));QVERIFY(!service.claimReminders().ok());QVERIFY(q.exec(QStringLiteral("PRAGMA query_only=OFF")));QCOMPARE(service.claimReminders().value.size(),1);
        clock.now=clock.now.addDays(2);QCOMPARE(service.claimReminders().value.size(),1);clock.now=clock.now.addDays(2);QVERIFY(service.claimReminders().value.isEmpty());
        DataTransfer transfer(db);const auto output=dir.filePath(QStringLiteral("all-day.json"));QVERIFY(transfer.exportJson(output).ok());QVERIFY(transfer.preview(output,false).ok());
        CalendarEvent gap;gap.title=QStringLiteral("DST gap");gap.allDay=false;gap.timeZone=QStringLiteral("America/New_York");gap.startUtc=EventService::localInstant(QStringLiteral("2026-03-07 02:30"),gap.timeZone).value;gap.endUtc=gap.startUtc.addSecs(1800);gap.repeatFrequency=QStringLiteral("DAILY");
        QVERIFY(!EventService::onDate(gap,QDate(2026,3,8),QTimeZone(gap.timeZone.toUtf8())));QVERIFY(EventService::onDate(gap,QDate(2026,3,9),QTimeZone(gap.timeZone.toUtf8())));
    }
    void tenThousandEvents() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("benchmark.sqlite"))).ok());auto connection=db.connection();QVERIFY(connection.transaction());QSqlQuery q(connection);
        for(int i=0;i<10000;i++) {
            q.prepare(QStringLiteral("INSERT INTO tasks(id,title,planning_state,start_on,created_at_utc,updated_at_utc) VALUES(?,?,'ACTIVE','2026-10-23','2026-10-23T00:00:00.000Z','2026-10-23T00:00:00.000Z')"));q.addBindValue(QString::number(i));q.addBindValue(QStringLiteral("event %1").arg(i));QVERIFY(q.exec());
        }
        QVERIFY(q.exec(QStringLiteral("INSERT INTO task_schedules(task_id,all_day,start_on,end_on) SELECT id,1,'2026-10-23','2026-10-24' FROM tasks")));QVERIFY(connection.commit());QElapsedTimer timer;timer.start();const auto events=EventRepository(connection).all();QVERIFY(events.ok());QCOMPARE(events.value.size(),10000);const auto loaded=timer.elapsed();timer.restart();int count=0;const auto zone=QTimeZone::systemTimeZone();for(const auto& e:events.value)count+=EventService::visibleDates(e,QDate(2026,10,1),QDate(2026,11,11),zone).size();QCOMPARE(count,10000);const auto projected=timer.elapsed();qInfo()<<"10k events: load ms"<<loaded<<"visible-date index ms"<<projected;QVERIFY(loaded<5000 && projected<5000);
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
        QSqlQuery q(db.connection());QVERIFY(q.exec(QStringLiteral("CREATE TRIGGER fail_event_link BEFORE INSERT ON task_links BEGIN SELECT RAISE(ABORT,'injected'); END")));
        invalid=e;invalid.title=QStringLiteral("SQL rollback");QVERIFY(!service.save(invalid).ok());QCOMPARE(repo.all().value.first().title,e.title);QCOMPARE(repo.all().value.first().links.size(),1);QVERIFY(q.exec(QStringLiteral("DROP TRIGGER fail_event_link")));
        e.startOn=e.startOn.addDays(1);e.endOn=e.endOn.addDays(1);QVERIFY(service.save(e).ok());QCOMPARE(tasks.find(task.value.id).value.startOn,task.value.startOn);
        QCOMPARE(service.save(e).error.code,ErrorCode::Conflict);e=repo.all().value.first();
        QVERIFY(taskService.changeState(task.value.id,task.value.rowVersion,QStringLiteral("complete")).ok());QCOMPARE(repo.all().value.size(),1);
        DataTransfer transfer(db);const auto path=dir.filePath(QStringLiteral("events.json"));QVERIFY(transfer.exportJson(path).ok());const auto plan=DataTransfer::preview(path);QVERIFY(plan.ok());
        auto merge=plan.value;auto mergeData=merge.document.value(QStringLiteral("tables")).toObject();auto mergeLinks=mergeData.value(QStringLiteral("task_links")).toArray();mergeLinks.append(QJsonObject{{"source_id",e.id},{"target_id",other.value.id},{"role",QStringLiteral("followup")}});mergeData.insert(QStringLiteral("task_links"),mergeLinks);merge.document.insert(QStringLiteral("tables"),mergeData);QVERIFY(transfer.apply(merge,false).ok());QCOMPARE(repo.all().value.first().links.size(),1);
        QVERIFY(transfer.apply(plan.value,true).ok());QCOMPARE(repo.all().value.first().links.size(),1);
        auto currentTask=tasks.find(task.value.id).value;auto deleted=taskService.changeState(currentTask.id,currentTask.rowVersion,QStringLiteral("delete"));QVERIFY(deleted.ok());QCOMPARE(repo.all().value.size(),1);QCOMPARE(repo.all().value.first().links.size(),1);QVERIFY(taskService.changeState(currentTask.id,deleted.value.rowVersion,QStringLiteral("restore")).ok());QCOMPARE(repo.all().value.first().links.size(),1);
        QVERIFY(service.setDeleted(e.id,e.rowVersion,true).ok());QVERIFY(repo.all().value.isEmpty());QVERIFY(!tasks.find(task.value.id).value.deletedAtUtc.size());
        QVERIFY(service.setDeleted(e.id,e.rowVersion+1,false).ok());QCOMPARE(repo.all().value.first().links.size(),1);
        auto corrupted=plan.value;auto data=corrupted.document.value(QStringLiteral("tables")).toObject();auto schedules=data.value(QStringLiteral("task_schedules")).toArray();auto row=schedules.first().toObject();row.insert(QStringLiteral("end_on"),row.value(QStringLiteral("start_on")));schedules[0]=row;data.insert(QStringLiteral("task_schedules"),schedules);corrupted.document.insert(QStringLiteral("tables"),data);
        QVERIFY(!transfer.apply(corrupted,true).ok());QCOMPARE(repo.all().value.size(),1);

    }
    void schemaTwoUpgradeBackup() {
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("upgrade.sqlite"));
        QVERIFY(createLegacyDatabase(path,2));
        QFile blocker(dir.filePath(QStringLiteral("backups")));QVERIFY(blocker.open(QIODevice::WriteOnly));blocker.close();{SqliteDatabase db;QVERIFY(!db.open(path).ok());}QVERIFY(blocker.remove());
        SqliteDatabase db;QVERIFY(db.open(path).ok());QCOMPARE(QDir(dir.filePath(QStringLiteral("backups/migrations"))).entryList({QStringLiteral("*.sqlite")},QDir::Files).size(),1);QVERIFY(EventRepository(db.connection()).all().ok());
    }
};
QTEST_GUILESS_MAIN(EventServiceTest)
#include "EventServiceTest.moc"
