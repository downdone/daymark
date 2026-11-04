#include "services/TaskService.h"
#include "services/ReminderSchedule.h"
#include "models/ReminderPresentation.h"
#include "persistence/SqliteDatabase.h"
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>
using namespace daymark;
class ReminderClock:public IClock {
public:QDateTime now=QDateTime(QDate(2024,3,9),QTime(12,0),QTimeZone::UTC);
    QDate localToday()const override{return now.date();}QDateTime nowUtc()const override{return now;}
};
class ReminderTest:public QObject {
    Q_OBJECT
private slots:
    void nextPendingUsesEffectiveStateAndAbsoluteTime()
    {
        const auto now=QDateTime::fromString("2026-11-04T08:00:00Z",Qt::ISODate);
        Task future;future.id="future";future.planningState="ACTIVE";future.remindAtUtc=now.addSecs(60).toString(Qt::ISODateWithMs);
        Task past=future;past.id="past";past.remindAtUtc=now.addSecs(-60).toString(Qt::ISODateWithMs);
        QCOMPARE(ReminderSchedule::nextPending({future,past}),now.addSecs(-60));
        for(int exclusion=0;exclusion<7;++exclusion){auto excluded=past;
            if(exclusion==0)excluded.status="COMPLETED";
            if(exclusion==1)excluded.deletedAtUtc="deleted";
            if(exclusion==2)excluded.planningState="SOMEDAY";
            if(exclusion==3)excluded.areaArchived=true;
            if(exclusion==4){excluded.projectId="project";excluded.projectDeleted=true;}
            if(exclusion==5)excluded.syncRemoteDeletionPending=true;
            if(exclusion==6)excluded.reminderFiredAtUtc=now.toString(Qt::ISODateWithMs);
            QCOMPARE(ReminderSchedule::nextPending({excluded,future}),now.addSecs(60));
        }
        past.reminderFiredAtUtc=now.toString(Qt::ISODateWithMs);past.reminderRepeat="DAILY";
        QCOMPARE(ReminderSchedule::nextPending({future,past}),now.addSecs(-60));
        future.remindAtUtc="invalid";QVERIFY(!ReminderSchedule::nextPending({future}).isValid());QVERIFY(!ReminderSchedule::nextPending({}).isValid());
        // Comparing the saved UTC instant against now handles clock jumps both ways.
        const auto next=ReminderSchedule::nextPending({past});QVERIFY(next<=now);QVERIFY(next>now.addSecs(-120));
        future.remindAtUtc="2026-11-04T16:00:00.000+08:00";QCOMPARE(ReminderSchedule::nextPending({future}),now);
    }
    void popupPresentationPreservesRecurrenceAndExpiresSafely() {
        ReminderClock clock;const auto now=clock.now;
        Task task;task.id="popup";task.title="待办";task.planningState="ACTIVE";
        task.reminderFiredAtUtc=now.toString(Qt::ISODateWithMs);task.remindAtUtc=now.addDays(1).toString(Qt::ISODateWithMs);
        task.reminderRepeat="DAILY";task.reminderAnchorUtc=now.addDays(-2).toString(Qt::ISODateWithMs);
        const auto anchor=task.reminderAnchorUtc;const auto next=task.remindAtUtc;
        const QVariantMap candidate{{"taskId",task.id},{"firedAt",task.reminderFiredAtUtc},{"occurrenceAt",now.toString(Qt::ISODateWithMs)}};
        const QVariantList candidates{candidate};QHash<QString,Task> tasks{{task.id,task}};
        const auto key=ReminderPresentation::key(task.id,candidate.value("occurrenceAt").toString());
        QCOMPARE(ReminderPresentation::pending(candidates,tasks,{},now).size(),1);
        QVariantMap state{{key,QVariantMap{{"until",now.addSecs(600).toString(Qt::ISODateWithMs)}}}};
        QVERIFY(ReminderPresentation::pending(candidates,tasks,state,now.addSecs(599)).isEmpty());
        QCOMPARE(ReminderPresentation::pending(candidates,tasks,state,now.addSecs(600)).size(),1);
        state[key]=QVariantMap{{"dismissed",true}};QVERIFY(ReminderPresentation::pending(candidates,tasks,state,now).isEmpty());
        QCOMPARE(tasks[task.id].remindAtUtc,next);QCOMPARE(tasks[task.id].reminderAnchorUtc,anchor);
        tasks[task.id].status="COMPLETED";QVERIFY(ReminderPresentation::pending(candidates,tasks,{},now).isEmpty());tasks[task.id]=task;
        tasks[task.id].remindAtUtc.clear();QVERIFY(ReminderPresentation::pending(candidates,tasks,{},now).isEmpty());tasks[task.id]=task;
        tasks[task.id].reminderFiredAtUtc.clear();QVERIFY(ReminderPresentation::pending(candidates,tasks,{},now).isEmpty());
        QVERIFY(ReminderPresentation::pending(candidates,{{task.id,task}},{},now.addDays(2)).isEmpty());
        QVERIFY(ReminderPresentation::key("ab","c")!=ReminderPresentation::key("a","bc"));
    }
    void pendingQueueDoesNotInheritHistoryPageLimit() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath("popup-burst.sqlite")).ok());ReminderClock clock;TaskRepository repo(db.connection());TaskService service(repo,clock);
        for(int i=0;i<55;++i){auto task=service.create(QString::number(i),"today");QVERIFY(task.ok());TaskPatch patch;patch.remindAtUtc=FieldPatch<QString>::set(clock.now.addSecs(60).toString(Qt::ISODateWithMs));QVERIFY(service.edit(task.value.id,task.value.rowVersion,patch).ok());}
        clock.now=clock.now.addSecs(60);QCOMPARE(repo.claimDueReminders(clock.now.toString(Qt::ISODateWithMs)).value.size(),55);
        QCOMPARE(repo.reminderHistory().value.size(),50);const auto queue=repo.reminderCandidates();QVERIFY(queue.ok());QCOMPARE(queue.value.size(),55);
        QHash<QString,Task> index;for(const auto& task:repo.all().value)index.insert(task.id,task);
        QCOMPARE(ReminderPresentation::pending(queue.value,index,{},clock.now).size(),55);
        QCOMPARE(repo.claimDueReminders(clock.now.toString(Qt::ISODateWithMs)).value.size(),0);
    }
    void unclassifiedReminderBecomesScheduledAndCanFire() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath("unclassified.sqlite")).ok());ReminderClock clock;TaskRepository repo(db.connection());TaskService service(repo,clock);const auto task=service.create("reminder without a plan");QVERIFY(task.ok());TaskPatch patch;patch.remindAtUtc=FieldPatch<QString>::set(clock.now.addSecs(60).toString(Qt::ISODateWithMs));const auto saved=service.edit(task.value.id,task.value.rowVersion,patch);QVERIFY(saved.ok());QVERIFY(QueryService::inView(saved.value,"inbox",clock.localToday()));QVERIFY(QueryService::inView(saved.value,"scheduled",clock.localToday()));QVERIFY(QueryService::inView(saved.value,"today",clock.localToday()));clock.now=clock.now.addSecs(61);QCOMPARE(repo.claimDueReminders(clock.now.toString(Qt::ISODateWithMs)).value.size(),1);
    }
    void recurringReminderPreservesWallClockAndClaimsOnlyOnce() {
        const QTimeZone zone("America/New_York");const QDateTime anchor(QDate(2024,3,9),QTime(9,0),zone);
        QCOMPARE(ReminderSchedule::nextAfter(anchor,QStringLiteral("DAILY"),1,zone,anchor).toTimeZone(zone).time(),QTime(9,0));QCOMPARE(anchor.secsTo(ReminderSchedule::nextAfter(anchor,QStringLiteral("DAILY"),1,zone,anchor)),qint64(23*3600));
        const QDateTime monthEnd(QDate(2024,1,31),QTime(9,0),zone);QCOMPARE(ReminderSchedule::nextAfter(monthEnd,QStringLiteral("MONTHLY"),1,zone,monthEnd).toTimeZone(zone).date(),QDate(2024,3,31));
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("repeat-reminders.sqlite"))).ok());ReminderClock clock;clock.now=anchor.toUTC().addSecs(-60);TaskRepository repo(db.connection());TaskService service(repo,clock);
        auto task=service.create(QStringLiteral("daily"),QStringLiteral("today"));QVERIFY(task.ok());TaskPatch patch;patch.remindAtUtc=FieldPatch<QString>::set(anchor.toUTC().toString(Qt::ISODateWithMs));patch.reminderRepeat=FieldPatch<QString>::set(QStringLiteral("DAILY"));patch.reminderZone=FieldPatch<QString>::set(QStringLiteral("America/New_York"));auto saved=service.edit(task.value.id,task.value.rowVersion,patch);QVERIFY(saved.ok());
        clock.now=anchor.toUTC();const auto first=repo.claimDueReminders(clock.now.toString(Qt::ISODateWithMs));QVERIFY(first.ok());QCOMPARE(first.value.size(),1);QCOMPARE(repo.claimDueReminders(clock.now.toString(Qt::ISODateWithMs)).value.size(),0);
        auto current=repo.find(task.value.id);QVERIFY(current.ok());const auto next=QDateTime::fromString(current.value.remindAtUtc,Qt::ISODateWithMs).toTimeZone(zone);QCOMPARE(next.date(),QDate(2024,3,10));QCOMPARE(next.time(),QTime(9,0));
        clock.now=anchor.toUTC().addDays(100);const auto catchup=repo.claimDueReminders(clock.now.toString(Qt::ISODateWithMs));QVERIFY(catchup.ok());QCOMPARE(catchup.value.size(),1);QVERIFY(QDateTime::fromString(repo.find(task.value.id).value.remindAtUtc,Qt::ISODateWithMs)>clock.now);QCOMPARE(repo.claimDueReminders(clock.now.toString(Qt::ISODateWithMs)).value.size(),0);
        current=repo.find(task.value.id);QVERIFY(service.changeState(current.value.id,current.value.rowVersion,QStringLiteral("complete")).ok());clock.now=clock.now.addDays(3);QCOMPARE(repo.claimDueReminders(clock.now.toString(Qt::ISODateWithMs)).value.size(),0);
    }
    void deliveryClaimsAreDurableAndNoDuplicates()
    {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("reminders.sqlite"))).ok());
        ReminderClock clock;TaskRepository repo(db.connection());TaskService service(repo,clock);
        auto task=service.create(QStringLiteral("remind"),QStringLiteral("today"));QVERIFY(task.ok());
        TaskPatch patch;patch.remindAtUtc=FieldPatch<QString>::set(clock.now.addSecs(-1).toString(Qt::ISODateWithMs));QVERIFY(!service.edit(task.value.id,1,patch).ok());
        patch.remindAtUtc=FieldPatch<QString>::set(clock.now.addSecs(60).toString(Qt::ISODateWithMs));auto saved=service.edit(task.value.id,1,patch);QVERIFY(saved.ok());
        QCOMPARE(repo.claimDueReminders(clock.now.toString(Qt::ISODateWithMs)).value.size(),0);
        clock.now=clock.now.addSecs(61);auto due=repo.claimDueReminders(clock.now.toString(Qt::ISODateWithMs));QVERIFY(due.ok());QCOMPARE(due.value.size(),1);
        QVERIFY(!repo.find(task.value.id).value.reminderFiredAtUtc.isEmpty());QCOMPARE(repo.claimDueReminders(clock.now.toString(Qt::ISODateWithMs)).value.size(),0);
        auto clearedFlag=repo.find(task.value.id).value;clearedFlag.reminderFiredAtUtc.clear();QVERIFY(repo.update(clearedFlag,clearedFlag.rowVersion).ok());QCOMPARE(repo.claimDueReminders(clock.now.toString(Qt::ISODateWithMs)).value.size(),0);QCOMPARE(repo.reminderHistory().value.size(),1);
        auto current=repo.find(task.value.id).value;patch.remindAtUtc=FieldPatch<QString>::set(clock.now.addSecs(60).toString(Qt::ISODateWithMs));auto rearmed=service.edit(current.id,current.rowVersion,patch);QVERIFY(rearmed.ok());QVERIFY(rearmed.value.reminderFiredAtUtc.isEmpty());
        QVERIFY(service.changeState(current.id,rearmed.value.rowVersion,QStringLiteral("complete")).ok());clock.now=clock.now.addDays(1);
        QCOMPARE(repo.claimDueReminders(clock.now.toString(Qt::ISODateWithMs)).value.size(),0);
    }
};
QTEST_GUILESS_MAIN(ReminderTest)
#include "ReminderTest.moc"
