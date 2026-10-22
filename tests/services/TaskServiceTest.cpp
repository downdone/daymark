#include "persistence/SqliteDatabase.h"
#include "persistence/TaskRepository.h"
#include "services/TaskService.h"
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace daymark;

class FixedClock final : public IClock {
public:
    QDate localToday() const override { return {2026,10,22}; }
    QDateTime nowUtc() const override { return QDateTime(QDate(2026,10,22),QTime(8,0),QTimeZone::UTC); }
};

class TaskServiceTest : public QObject {
    Q_OBJECT
private slots:
    void lifecyclePersistsAndBacksUpWal()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const auto path=dir.filePath(QStringLiteral("tasks.sqlite"));
        QString deletedId;
        {
            SqliteDatabase db; QVERIFY(db.open(path).ok());
            {
                QSqlQuery wal(db.connection());
                QVERIFY(wal.exec(QStringLiteral("PRAGMA wal_autocheckpoint=0")));
                QVERIFY(wal.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)")));
            }
            TaskRepository repo(db.connection()); FixedClock clock; TaskService service(repo,clock);
            QVERIFY(!service.create(QStringLiteral("   ")).ok());
            for(int i=0;i<20;++i) QVERIFY(service.create(QStringLiteral("中文 task %1").arg(i)).ok());
            QCOMPARE(repo.all().value.size(),20);
            auto task=repo.all().value.first();
            auto completed=service.changeState(task.id,task.rowVersion,QStringLiteral("complete"));
            QVERIFY(completed.ok()); QCOMPARE(completed.value.status,QStringLiteral("COMPLETED"));
            QVERIFY(!completed.value.completedAtUtc.isEmpty());
            auto reopened=service.changeState(task.id,completed.value.rowVersion,QStringLiteral("reopen"));
            QVERIFY(reopened.ok()); QVERIFY(reopened.value.completedAtUtc.isEmpty());
            auto cancelled=service.changeState(task.id,reopened.value.rowVersion,QStringLiteral("cancel"));
            QVERIFY(cancelled.ok());QCOMPARE(cancelled.value.status,QStringLiteral("CANCELLED"));QVERIFY(!cancelled.value.cancelledAtUtc.isEmpty());
            auto activeAgain=service.changeState(task.id,cancelled.value.rowVersion,QStringLiteral("reopen"));
            QVERIFY(activeAgain.ok());QVERIFY(activeAgain.value.cancelledAtUtc.isEmpty());
            auto deleted=service.changeState(task.id,activeAgain.value.rowVersion,QStringLiteral("delete"));
            QVERIFY(deleted.ok()); deletedId=task.id;
            QCOMPARE(repo.all().value.size(),20); // Soft deletion retains the row.
            QCOMPARE(QueryService::select(repo.all().value,QStringLiteral("inbox"),clock.localToday()).size(),19);
            QVERIFY(QFileInfo(path+QStringLiteral("-wal")).size()>0);
            auto snapshot=db.createSnapshot(dir.filePath(QStringLiteral("backups"))); QVERIFY(snapshot.ok());
            SqliteDatabase backup; QVERIFY(backup.open(snapshot.value).ok());
            TaskRepository copied(backup.connection()); QCOMPARE(copied.all().value,repo.all().value);
        }
        SqliteDatabase reopened; QVERIFY(reopened.open(path).ok());
        TaskRepository repo(reopened.connection()); FixedClock clock; TaskService service(repo,clock);
        QCOMPARE(repo.all().value.size(),20);
        auto deleted=repo.find(deletedId); QVERIFY(!deleted.value.deletedAtUtc.isEmpty());
        QVERIFY(service.changeState(deletedId,deleted.value.rowVersion,QStringLiteral("restore")).ok());
        QCOMPARE(QueryService::select(repo.all().value,QStringLiteral("inbox"),clock.localToday()).size(),20);
    }

    void validationPatchAndConflictAreAtomic()
    {
        QTemporaryDir dir; SqliteDatabase db; QVERIFY(db.open(dir.filePath(QStringLiteral("tasks.sqlite"))).ok());
        TaskRepository repo(db.connection()); FixedClock clock; TaskService service(repo,clock);
        const auto emoji=QString::fromUcs4(U"😀");
        QVERIFY(service.create(emoji.repeated(512)).ok());
        QVERIFY(!service.create(emoji.repeated(513)).ok());
        auto task=service.create(QStringLiteral("  Date task  ")).value;
        QCOMPARE(task.title,QStringLiteral("Date task"));
        TaskPatch patch;
        patch.startOn=FieldPatch<QDate>::set(QDate(2026,10,24));
        patch.deadlineOn=FieldPatch<QDate>::set(QDate(2026,10,23));
        QVERIFY(!service.edit(task.id,task.rowVersion,patch).ok());
        QCOMPARE(repo.find(task.id).value,task);
        patch.deadlineOn=FieldPatch<QDate>::set(QDate(2026,10,25));
        auto updated=service.edit(task.id,task.rowVersion,patch); QVERIFY(updated.ok());
        QCOMPARE(updated.value.planningState,QStringLiteral("ACTIVE"));
        QCOMPARE(service.edit(task.id,task.rowVersion,patch).error.code,ErrorCode::Conflict);
        TaskPatch clear; clear.startOn=FieldPatch<QDate>::clear();
        auto cleared=service.edit(task.id,updated.value.rowVersion,clear); QVERIFY(cleared.ok());
        QVERIFY(!cleared.value.startOn); QVERIFY(cleared.value.deadlineOn.has_value());
        TaskPatch invalid; invalid.startOn=FieldPatch<QDate>::set(QDate(2026,2,30));
        QVERIFY(!service.edit(task.id,cleared.value.rowVersion,invalid).ok());
        QCOMPARE(repo.find(task.id).value,cleared.value);
        QSqlQuery query(db.connection()); QVERIFY(query.exec(QStringLiteral("PRAGMA query_only=ON")));
        const auto count=repo.all().value.size();
        QVERIFY(!service.create(QStringLiteral("must not persist")).ok());
        QCOMPARE(repo.all().value.size(),count);
    }

    void focusAndSomedayKeepDistinctDateSemantics()
    {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("tasks.sqlite"))).ok());
        TaskRepository repo(db.connection());FixedClock clock;TaskService service(repo,clock);
        auto task=service.create(QStringLiteral("plan"),QStringLiteral("upcoming"));QVERIFY(task.ok());
        QCOMPARE(*task.value.startOn,clock.localToday().addDays(1));
        TaskPatch someday; someday.planningState=FieldPatch<QString>::set(QStringLiteral("SOMEDAY"));
        someday.todayFocus=FieldPatch<bool>::set(true);
        auto paused=service.edit(task.value.id,task.value.rowVersion,someday);QVERIFY(paused.ok());
        QCOMPARE(paused.value.startOn,task.value.startOn); // Paused dates must not be rewritten.
        QVERIFY(!QueryService::inView(paused.value,QStringLiteral("today"),clock.localToday()));
        TaskPatch activate;activate.planningState=FieldPatch<QString>::set(QStringLiteral("ACTIVE"));
        activate.notes=FieldPatch<QString>::set(QString());
        auto today=service.edit(task.value.id,paused.value.rowVersion,activate);QVERIFY(today.ok());
        QCOMPARE(*today.value.startOn,clock.localToday());QVERIFY(today.value.notes.isEmpty());
        QCOMPARE(QueryService::select({paused.value},QStringLiteral("inbox"),clock.localToday(),QStringLiteral("PLAN")).size(),1);
        auto completed=service.changeState(task.value.id,today.value.rowVersion,QStringLiteral("complete"));QVERIFY(completed.ok());
        auto duplicate=service.changeState(task.value.id,completed.value.rowVersion,QStringLiteral("complete"));
        QVERIFY(duplicate.ok());QCOMPARE(duplicate.value.rowVersion,completed.value.rowVersion);
        QFile blocked(dir.filePath(QStringLiteral("blocked")));QVERIFY(blocked.open(QIODevice::WriteOnly));blocked.close();
        QVERIFY(!db.createSnapshot(blocked.fileName()+QStringLiteral("/backups")).ok());
        QCOMPARE(repo.find(task.value.id).value,completed.value);
    }

    void repositoryJoinsProjectVisibility()
    {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("tasks.sqlite"))).ok());
        TaskRepository repo(db.connection());FixedClock clock;TaskService service(repo,clock);
        const auto task=service.create(QStringLiteral("project task"),QStringLiteral("today"));QVERIFY(task.ok());
        QSqlQuery sql(db.connection());
        QVERIFY(sql.exec(QStringLiteral("INSERT INTO projects(id,title,created_at_utc,updated_at_utc) VALUES('p','test','2026-10-22T00:00:00Z','2026-10-22T00:00:00Z')")));
        sql.prepare(QStringLiteral("UPDATE tasks SET project_id='p' WHERE id=?"));sql.addBindValue(task.value.id);QVERIFY(sql.exec());
        QVERIFY(QueryService::inView(repo.find(task.value.id).value,QStringLiteral("today"),clock.localToday()));
        QVERIFY(sql.exec(QStringLiteral("UPDATE projects SET planning_state='SOMEDAY' WHERE id='p'")));
        QVERIFY(!QueryService::inView(repo.find(task.value.id).value,QStringLiteral("today"),clock.localToday()));
        QVERIFY(QueryService::inView(repo.find(task.value.id).value,QStringLiteral("someday"),clock.localToday()));
        QVERIFY(sql.exec(QStringLiteral("UPDATE projects SET planning_state='ACTIVE',status='COMPLETED' WHERE id='p'")));
        QVERIFY(!QueryService::inView(repo.find(task.value.id).value,QStringLiteral("today"),clock.localToday()));
        QVERIFY(sql.exec(QStringLiteral("UPDATE projects SET status='OPEN' WHERE id='p'")));
        QVERIFY(QueryService::inView(repo.find(task.value.id).value,QStringLiteral("today"),clock.localToday()));
    }

    void unifiedViewsHonorDatesAndProjectState()
    {
        const QDate today(2026,10,22);
        Task task; task.planningState=QStringLiteral("ACTIVE"); task.title=QStringLiteral("中文 Review");
        QVERIFY(QueryService::inView(task,QStringLiteral("anytime"),today));
        QVERIFY(!QueryService::inView(task,QStringLiteral("today"),today));
        task.deadlineOn=today.addDays(1);
        QVERIFY(QueryService::inView(task,QStringLiteral("anytime"),today));
        QVERIFY(!QueryService::inView(task,QStringLiteral("upcoming"),today));
        task.startOn=today.addDays(1);
        QVERIFY(QueryService::inView(task,QStringLiteral("upcoming"),today));
        QVERIFY(!QueryService::inView(task,QStringLiteral("anytime"),today));
        QVERIFY(QueryService::inView(task,QStringLiteral("today"),today.addDays(1)));
        task.startOn=today.addDays(-1);
        QVERIFY(QueryService::inView(task,QStringLiteral("today"),today));
        QVERIFY(QueryService::inView(task,QStringLiteral("anytime"),today));
        task.planningState=QStringLiteral("SOMEDAY");
        QVERIFY(!QueryService::inView(task,QStringLiteral("today"),today));
        QVERIFY(QueryService::inView(task,QStringLiteral("someday"),today));
        task.planningState=QStringLiteral("INBOX");
        QVERIFY(QueryService::inView(task,QStringLiteral("inbox"),today));
        QVERIFY(!QueryService::inView(task,QStringLiteral("today"),today));
        task.planningState=QStringLiteral("ACTIVE"); task.projectId=QStringLiteral("project");
        task.projectPlanning=QStringLiteral("SOMEDAY"); task.projectStatus=QStringLiteral("OPEN");
        QVERIFY(QueryService::inView(task,QStringLiteral("someday"),today));
        QVERIFY(!QueryService::inView(task,QStringLiteral("today"),today));
        task.projectPlanning=QStringLiteral("ACTIVE"); task.projectStatus=QStringLiteral("COMPLETED");
        QVERIFY(!QueryService::inView(task,QStringLiteral("today"),today));
        task.projectStatus=QStringLiteral("OPEN");
        QVERIFY(QueryService::inView(task,QStringLiteral("today"),today));
        QCOMPARE(QueryService::select({task},QStringLiteral("today"),today,QStringLiteral("review")).size(),1);
        task.status=QStringLiteral("COMPLETED");
        QVERIFY(QueryService::inView(task,QStringLiteral("logbook"),today));
        QVERIFY(!QueryService::inView(task,QStringLiteral("today"),today));
    }

    void unknownAndFutureSchemasAreNotOverwritten()
    {
        QTemporaryDir dir; const auto path=dir.filePath(QStringLiteral("tasks.sqlite"));
        {
            SqliteDatabase db; QVERIFY(db.open(path).ok());
            TaskRepository repo(db.connection()); FixedClock clock; TaskService service(repo,clock);
            QVERIFY(service.create(QStringLiteral("retained")).ok());
            QSqlQuery query(db.connection()); QVERIFY(query.exec(QStringLiteral("PRAGMA user_version=2")));
        }
        SqliteDatabase future; QCOMPARE(future.open(path).error.key,QStringLiteral("schemaTooNew"));
        auto raw=future.connection(); QVERIFY(raw.open());
        QSqlQuery query(raw); QVERIFY(query.exec(QStringLiteral("SELECT count(*) FROM tasks"))); QVERIFY(query.next()); QCOMPARE(query.value(0).toInt(),1); query.finish();
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version=0"))); query.finish(); raw.close();
        SqliteDatabase unknown; QCOMPARE(unknown.open(path).error.key,QStringLiteral("unknownSchema"));
    }
};

QTEST_GUILESS_MAIN(TaskServiceTest)
#include "TaskServiceTest.moc"
