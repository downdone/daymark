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
        const auto today=QDate::currentDate();const auto now=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);int matches=0;
        for(int i=0;i<20;++i) {
            q.prepare(QStringLiteral("INSERT INTO areas(id,name,created_at_utc,updated_at_utc) VALUES(?,?,?,?)"));
            q.addBindValue(QStringLiteral("area-%1").arg(i));q.addBindValue(QStringLiteral("领域 %1").arg(i));q.addBindValue(now);q.addBindValue(now);QVERIFY(q.exec());
        }
        for(int i=0;i<200;++i) {
            q.prepare(QStringLiteral("INSERT INTO projects(id,title,area_id,created_at_utc,updated_at_utc) VALUES(?,?,?,?,?)"));
            q.addBindValue(QStringLiteral("project-%1").arg(i));q.addBindValue(QStringLiteral("项目 %1").arg(i));q.addBindValue(QStringLiteral("area-%1").arg(i%20));q.addBindValue(now);q.addBindValue(now);QVERIFY(q.exec());
        }
        for(int i=0;i<50;++i) {
            q.prepare(QStringLiteral("INSERT INTO tags(id,name,normalized_name,created_at_utc,updated_at_utc) VALUES(?,?,?,?,?)"));
            q.addBindValue(QStringLiteral("tag-%1").arg(i));q.addBindValue(QStringLiteral("标签 %1").arg(i));q.addBindValue(QStringLiteral("tag %1").arg(i));q.addBindValue(now);q.addBindValue(now);QVERIFY(q.exec());
        }
        q.prepare(QStringLiteral("INSERT INTO tasks(id,title,note_text,planning_state,start_on,sort_order,created_at_utc,updated_at_utc,project_id,status,completed_at_utc) VALUES(?,?,?,'ACTIVE',?,?,?,?,?,?,?)"));
        for(int i=0;i<10000;++i) {
            q.bindValue(0,QString::number(i));q.bindValue(1,QStringLiteral("任务 %1").arg(i));q.bindValue(2,i%37==0?QStringLiteral("needle 中文"):QStringLiteral("note"));
            q.bindValue(3,today.addDays(i%60-30).toString(Qt::ISODate));q.bindValue(4,i);q.bindValue(5,now);q.bindValue(6,now);
            q.bindValue(7,QStringLiteral("project-%1").arg(i%200));q.bindValue(8,i%10<3?QStringLiteral("COMPLETED"):QStringLiteral("OPEN"));q.bindValue(9,i%10<3?QVariant(now):QVariant());
            QVERIFY(q.exec());if(i%37==0 && i%10>=3)++matches;
        }
        QSqlQuery details(connection);
        details.prepare(QStringLiteral("INSERT INTO checklist_items(id,task_id,title,is_done,sort_order,created_at_utc,updated_at_utc) VALUES(?,?,?,?,?,?,?)"));
        QSqlQuery tags(connection);tags.prepare(QStringLiteral("INSERT INTO task_tags(task_id,tag_id) VALUES(?,?)"));
        for(int i=0;i<10000;++i)for(int j=0;j<3;++j) {
            details.bindValue(0,QStringLiteral("check-%1-%2").arg(i).arg(j));details.bindValue(1,QString::number(i));details.bindValue(2,QStringLiteral("清单 %1").arg(j));details.bindValue(3,j==0);details.bindValue(4,j);details.bindValue(5,now);details.bindValue(6,now);QVERIFY(details.exec());
            tags.bindValue(0,QString::number(i));tags.bindValue(1,QStringLiteral("tag-%1").arg((i+j)%50));QVERIFY(tags.exec());
        }
        QVERIFY(connection.commit());TaskRepository repo(connection);QElapsedTimer timer;timer.start();const auto tasks=repo.all();const auto loadMs=timer.elapsed();QVERIFY(tasks.ok());QCOMPARE(tasks.value.size(),10000);
        timer.restart();const auto found=QueryService::select(tasks.value,QStringLiteral("inbox"),today,QStringLiteral("NEEDLE"));const auto searchMs=timer.elapsed();QCOMPARE(found.size(),matches);
        timer.restart();const auto days=CalendarService::days(tasks.value,today,false,false);const auto calendarMs=timer.elapsed();QCOMPARE(days.size(),42);
        qInfo().nospace()<<"10k mixed benchmark (20 areas, 200 projects, 50 tags, 30k checklist, 30% complete; ms): load="<<loadMs<<", search="<<searchMs<<", month="<<calendarMs;
        QVERIFY2(loadMs<3000,"10k load exceeded 3s");QVERIFY2(searchMs<1000,"Search blocked for more than 1s");QVERIFY2(calendarMs<1000,"Month indexing exceeded 1s");
    }
};
QTEST_GUILESS_MAIN(PerformanceTest)
#include "PerformanceTest.moc"
