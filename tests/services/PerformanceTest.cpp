#include "services/CalendarService.h"
#include "persistence/SqliteDatabase.h"
#include <QElapsedTimer>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>
#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif
using namespace daymark;
class PerformanceTest:public QObject {
    Q_OBJECT
private slots:
    void futureReminderPollingBenchmark()
    {
        QTemporaryDir directory;SqliteDatabase database;QVERIFY(database.open(directory.filePath("future-reminders.sqlite")).ok());auto connection=database.connection();QVERIFY(connection.transaction());QSqlQuery query(connection);
        query.prepare("INSERT INTO tasks(id,title,planning_state,remind_at_utc,created_at_utc,updated_at_utc) VALUES(?,?,'ACTIVE',?,?,?)");const QString now="2026-11-04T08:00:00.000Z",future="2026-11-05T08:00:00.000Z";
        for(int i=0;i<10000;++i){query.bindValue(0,QString::number(i));query.bindValue(1,QStringLiteral("synthetic reminder"));query.bindValue(2,future);query.bindValue(3,now);query.bindValue(4,now);QVERIFY(query.exec());}QVERIFY(connection.commit());TaskRepository repository(connection);QList<qint64> samples;
        for(int sample=0;sample<6;++sample){QElapsedTimer timer;timer.start();const auto due=repository.claimDueReminders(now);const auto ns=timer.nsecsElapsed();QVERIFY(due.ok());QVERIFY(due.value.isEmpty());if(sample)samples.append(ns);}
        std::sort(samples.begin(),samples.end());qInfo()<<"PERF future reminders rows=10000 median_us="<<samples[2]/1000.0;
    }
    void hierarchicalSelectionBenchmark()
    {
        QList<Task> tasks;tasks.reserve(40000);
        for(int root=0;root<10000;++root){Task task;task.id=QStringLiteral("root-%1").arg(root);task.planningState="ACTIVE";task.sortOrder=root*4;tasks.append(task);for(int child=0;child<3;++child){auto nested=task;nested.id=QStringLiteral("child-%1-%2").arg(root).arg(child);nested.parentTaskId=task.id;nested.sortOrder+=child+1;tasks.append(nested);}}
        QList<qint64> samples;
        for(int sample=0;sample<6;++sample){QElapsedTimer clock;clock.start();const auto selected=QueryService::select(tasks,"all",QDate(2026,11,4));const auto ns=clock.nsecsElapsed();QCOMPARE(selected.size(),40000);QCOMPARE(selected[0].listChildCount,3);QCOMPARE(selected[1].listDepth,1);QCOMPARE(selected[39999].parentTaskId,selected[39996].id);if(sample)samples.append(ns);}
        std::sort(samples.begin(),samples.end());
        quint64 peakPrivate=0;
#ifdef Q_OS_WIN
        using ReadMemory=BOOL(WINAPI*)(HANDLE,PPROCESS_MEMORY_COUNTERS,DWORD);const auto read=reinterpret_cast<ReadMemory>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"K32GetProcessMemoryInfo"));PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);if(read && read(GetCurrentProcess(),reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&memory),sizeof(memory)))peakPrivate=memory.PeakPagefileUsage;
#endif
        qInfo()<<"PERF tree projection rows="<<tasks.size()<<"median_us="<<samples[2]/1000.0<<"peak_process_private_bytes="<<peakPrivate;
    }
    void snapshotIndexFootprint()
    {
        QList<Task> tasks;tasks.reserve(40000);for(int i=0;i<40000;++i){Task task;task.id=QString::number(i);task.notes=QStringLiteral("synthetic notes");tasks.append(task);}
        auto memory=[]()->QPair<quint64,quint64>{
#ifdef Q_OS_WIN
            using ReadMemory=BOOL(WINAPI*)(HANDLE,PPROCESS_MEMORY_COUNTERS,DWORD);
            const auto read=reinterpret_cast<ReadMemory>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"K32GetProcessMemoryInfo"));
            PROCESS_MEMORY_COUNTERS_EX state{};state.cb=sizeof(state);
            if(read && read(GetCurrentProcess(),reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&state),sizeof(state)))return {state.PrivateUsage,state.WorkingSetSize};
#endif
            return {};
        };
        const auto before=memory();const bool full=qEnvironmentVariable("DAYMARK_PERF_INDEX")=="full";
        QHash<QString,Task> fullIndex;QHash<QString,qsizetype> rowIndex;
        if(full){fullIndex.reserve(tasks.size());for(const auto& task:tasks)fullIndex.insert(task.id,task);QCOMPARE(fullIndex.size(),tasks.size());}
        else{rowIndex.reserve(tasks.size());for(qsizetype i=0;i<tasks.size();++i)rowIndex.insert(tasks[i].id,i);QCOMPARE(rowIndex.size(),tasks.size());QCOMPARE(tasks[rowIndex.value("39999")].id,QString("39999"));}
        const auto after=memory();
        qInfo()<<"PERF isolated snapshot index tasks="<<tasks.size()<<"full_task_values="<<full<<"private_bytes_delta="<<qint64(after.first)-qint64(before.first)<<"working_set_delta="<<qint64(after.second)-qint64(before.second)<<"platform_memory_available="<<(before.first>0);
    }
    void sparseCalendarRange()
    {
        const QDate today(2026,11,4);QList<Task> tasks;tasks.reserve(40000);
        for(int i=0;i<40000;++i){Task task;task.id=QString::number(i);task.planningState="ACTIVE";task.title=QStringLiteral("synthetic");task.startOn=today.addDays(100+i);tasks.append(task);}
        for(const bool week:{true,false}){
            QList<qint64> samples;
            for(int sample=0;sample<7;++sample){QElapsedTimer clock;clock.start();const auto days=CalendarService::days(tasks,today,week,false);const auto ns=clock.nsecsElapsed();QCOMPARE(days.size(),week?7:42);for(const auto& day:days)QVERIFY(day.entries.isEmpty());if(sample)samples.append(ns);}
            std::sort(samples.begin(),samples.end());qInfo()<<"PERF sparse calendar tasks="<<tasks.size()<<"days="<<(week?7:42)<<"median_us="<<samples[samples.size()/2]/1000.0<<"min_us="<<samples.first()/1000.0<<"max_us="<<samples.last()/1000.0<<"task_struct_bytes="<<sizeof(Task);
        }
    }
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
        details.prepare(QStringLiteral("INSERT INTO tasks(id,parent_task_id,title,status,sort_order,created_at_utc,updated_at_utc,planning_state) VALUES(?,?,?,?,?,?,?,'ACTIVE')"));
        QSqlQuery tags(connection);tags.prepare(QStringLiteral("INSERT INTO task_tags(task_id,tag_id) VALUES(?,?)"));
        for(int i=0;i<10000;++i)for(int j=0;j<3;++j) {
            details.bindValue(0,QStringLiteral("child-%1-%2").arg(i).arg(j));details.bindValue(1,QString::number(i));details.bindValue(2,QStringLiteral("子任务 %1").arg(j));details.bindValue(3,j==0?QStringLiteral("COMPLETED"):QStringLiteral("OPEN"));details.bindValue(4,j);details.bindValue(5,now);details.bindValue(6,now);QVERIFY(details.exec());
            tags.bindValue(0,QString::number(i));tags.bindValue(1,QStringLiteral("tag-%1").arg((i+j)%50));QVERIFY(tags.exec());
        }
        QVERIFY(connection.commit());TaskRepository repo(connection);QElapsedTimer timer;timer.start();const auto tasks=repo.all();const auto loadMs=timer.elapsed();QVERIFY(tasks.ok());QCOMPARE(tasks.value.size(),40000);
        timer.restart();const auto found=QueryService::select(tasks.value,QStringLiteral("inbox"),today,QStringLiteral("NEEDLE"));const auto searchMs=timer.elapsed();QCOMPARE(found.size(),matches);
        timer.restart();const auto days=CalendarService::days(tasks.value,today,false,false);const auto calendarMs=timer.elapsed();QCOMPARE(days.size(),42);
        qInfo().nospace()<<"10k mixed benchmark (20 areas, 200 projects, 50 tags, 30k subtask, 30% complete; ms): load="<<loadMs<<", search="<<searchMs<<", month="<<calendarMs;
        QVERIFY2(loadMs<3000,"10k load exceeded 3s");QVERIFY2(searchMs<1000,"Search blocked for more than 1s");QVERIFY2(calendarMs<1000,"Month indexing exceeded 1s");
    }
};
QTEST_GUILESS_MAIN(PerformanceTest)
#include "PerformanceTest.moc"
