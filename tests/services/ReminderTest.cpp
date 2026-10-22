#include "services/TaskService.h"
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
        auto current=repo.find(task.value.id).value;patch.remindAtUtc=FieldPatch<QString>::set(clock.now.addSecs(60).toString(Qt::ISODateWithMs));auto rearmed=service.edit(current.id,current.rowVersion,patch);QVERIFY(rearmed.ok());QVERIFY(rearmed.value.reminderFiredAtUtc.isEmpty());
        QVERIFY(service.changeState(current.id,rearmed.value.rowVersion,QStringLiteral("complete")).ok());clock.now=clock.now.addDays(1);
        QCOMPARE(repo.claimDueReminders(clock.now.toString(Qt::ISODateWithMs)).value.size(),0);
    }
};
QTEST_GUILESS_MAIN(ReminderTest)
#include "ReminderTest.moc"
