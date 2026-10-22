#include "services/CalendarService.h"
#include "persistence/SqliteDatabase.h"
#include <QElapsedTimer>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>
using namespace daymark;
class PerformanceTest:public QObject {
    Q_OBJECT
private slots:
    void tenThousandTasks()
    {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("benchmark.sqlite"))).ok());
        auto connection=db.connection();QVERIFY(connection.transaction());QSqlQuery q(connection);
        q.prepare(QStringLiteral("INSERT INTO tasks(id,title,note_text,planning_state,start_on,sort_order,created_at_utc,updated_at_utc) VALUES(?,?,?,'ACTIVE',?,?,?,?)"));
        const auto today=QDate::currentDate();const auto now=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);int matches=0;
        for(int i=0;i<10000;++i) {
            q.bindValue(0,QString::number(i));q.bindValue(1,QStringLiteral("任务 %1").arg(i));q.bindValue(2,i%37==0?QStringLiteral("needle 中文"):QStringLiteral("note"));
            q.bindValue(3,today.addDays(i%60-30).toString(Qt::ISODate));q.bindValue(4,i);q.bindValue(5,now);q.bindValue(6,now);QVERIFY(q.exec());if(i%37==0)++matches;
        }
        QVERIFY(connection.commit());TaskRepository repo(connection);QElapsedTimer timer;timer.start();const auto tasks=repo.all();const auto loadMs=timer.elapsed();QVERIFY(tasks.ok());QCOMPARE(tasks.value.size(),10000);
        timer.restart();const auto found=QueryService::select(tasks.value,QStringLiteral("inbox"),today,QStringLiteral("NEEDLE"));const auto searchMs=timer.elapsed();QCOMPARE(found.size(),matches);
        timer.restart();const auto days=CalendarService::days(tasks.value,today,false,false);const auto calendarMs=timer.elapsed();QCOMPARE(days.size(),42);
        qInfo().nospace()<<"10k benchmark (ms): load="<<loadMs<<", search="<<searchMs<<", month="<<calendarMs;
        QVERIFY2(loadMs<3000,"10k load exceeded 3s");QVERIFY2(searchMs<1000,"Search blocked for more than 1s");QVERIFY2(calendarMs<1000,"Month indexing exceeded 1s");
    }
};
QTEST_GUILESS_MAIN(PerformanceTest)
#include "PerformanceTest.moc"
