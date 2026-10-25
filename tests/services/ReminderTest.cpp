#include "services/TaskService.h"
#include "services/ReminderSchedule.h"
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
