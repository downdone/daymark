#include "models/TaskWorkspace.h"
#include <QTemporaryDir>
#include <QSettings>
#include <QFileInfo>
#include <QtTest>
using namespace daymark;
class Ux34WorkspaceTest:public QObject {
    Q_OBJECT
private slots:
    void calendarDraftAndAtomicCreation() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath("draft.sqlite"));QTRY_VERIFY(w.ready() && !w.busy());
        QVERIFY(w.saveCatalog({{"kind","project"},{"title","Draft category"}}));QTRY_VERIFY(!w.busy());
        const auto category=w.categoryTree().first().toMap();w.setView("project:"+category.value("id").toString());
        const auto date=w.dateOffset(2);const auto seed=w.calendarDraft(date,540,45);
        QCOMPARE(seed.value("projectId"),category.value("id"));QCOMPARE(seed.value("schedule").toMap().value("endTime").toString(),QString("09:45"));
        QCOMPARE(w.tasks()->rowCount(),0);QVERIFY(w.calendarDraft("2026-02-30").isEmpty());QVERIFY(w.calendarDraft(date,2000).isEmpty());
        const auto global=w.calendarDraft(date,540,60,true);QVERIFY(global.value("projectId").toString().isEmpty());
        QVariantMap details{{"projectId",category.value("id")},{"schedule",seed.value("schedule")},{"important",true},{"pinned",true},
            {"repeat",QVariantMap{{"frequency","DAILY"},{"interval",0}}}};
        // The insert succeeds before edit validation fails: neither task nor
        // recurrence may survive that transaction.
        QVERIFY(w.createTaskWithDetails("Invalid draft","notes",date,date,details));QTRY_VERIFY(!w.busy());QVERIFY(!w.errorText().isEmpty());QCOMPARE(w.tasks()->rowCount(),0);
        details.remove("repeat");details.insert("tagIds",QStringList{"missing-tag"});
        QVERIFY(w.createTaskWithDetails("Invalid tag","notes",date,date,details));QTRY_VERIFY(!w.busy());QVERIFY(!w.errorText().isEmpty());QCOMPARE(w.tasks()->rowCount(),0);
        details.remove("tagIds");
        QVERIFY(w.createTaskWithDetails("Complete draft","draft notes",date,date,details));QTRY_VERIFY(!w.busy());QVERIFY2(w.errorText().isEmpty(),qPrintable(w.errorText()));QCOMPARE(w.tasks()->rowCount(),1);
        const auto task=w.tasks()->items().first();QCOMPARE(task.notes,QString("draft notes"));QCOMPARE(task.projectId,category.value("id").toString());QVERIFY(task.important && task.pinned && task.schedule);QCOMPARE(task.schedule->startUtc.secsTo(task.schedule->endUtc),45*60);
        TaskWorkspace reopened;reopened.start(dir.filePath("draft.sqlite"));QTRY_VERIFY(reopened.ready() && !reopened.busy());reopened.setView("all");QCOMPARE(reopened.tasks()->rowCount(),1);
    }
    void continuousWeeksAreBoundedUniqueAndReadOnly() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath("weeks.sqlite"));QTRY_VERIFY(w.ready() && !w.busy());
        w.setView("inbox");QVERIFY(w.createForDate("cross month","2026-10-30"));QTRY_VERIFY(!w.busy());const auto task=w.tasks()->items().first();QVERIFY(w.saveTaskWithDetails(task.id,task.rowVersion,task.title,task.notes,"ACTIVE","2026-10-30","2026-11-04",false,{}));QTRY_VERIFY(!w.busy());const auto version=w.task(task.id).value("rowVersion");
        const auto weeks=w.calendarWeeksFor("2026-10-25");QVERIFY(weeks.size()>=17 && weeks.size()<=19);QSet<QString> dates,starts;int references=0;
        for(const auto& value:weeks){const auto row=value.toMap();QVERIFY(!starts.contains(row.value("key").toString()));starts.insert(row.value("key").toString());const auto days=row.value("days").toList();QCOMPARE(days.size(),7);for(const auto& entry:days){const auto day=entry.toMap();const auto date=day.value("date").toString();QVERIFY(!dates.contains(date));dates.insert(date);for(const auto& band:day.value("bands").toList()){QCOMPARE(band.toMap().value("week").toInt(),0);if(band.toMap().value("taskId")==task.id)++references;}}}
        QCOMPARE(references,2);QVERIFY(dates.contains("2026-10-31") && dates.contains("2026-11-01"));QCOMPARE(w.task(task.id).value("rowVersion"),version);QVERIFY(w.calendarWeeksFor("2026-10-25",3,4).isEmpty());
        const auto leap=w.calendarWeeksFor("2028-02-29",0,0);bool seen=false;for(const auto& value:leap)for(const auto& d:value.toMap().value("days").toList())if(d.toMap().value("date")=="2028-02-29")seen=true;QVERIFY(seen);
    }
    void completedCalendarAndCancelledAreDistinct() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath("status.sqlite"));QTRY_VERIFY(w.ready() && !w.busy());w.setView("inbox");QVERIFY(w.createForTime("done",w.dateOffset(0),540,90));QTRY_VERIFY(!w.busy());const auto task=w.tasks()->items().first();QVERIFY(w.changeState(task.id,task.rowVersion,"complete"));QTRY_VERIFY(!w.busy());
        w.setView("all");QCOMPARE(w.timelineFor(w.dateOffset(0),false).value("blocks").toList().size(),0);w.setCalendarCompleted(true);QCOMPARE(w.timelineFor(w.dateOffset(0),false).value("blocks").toList().size(),1);QCOMPARE(w.timelineFor(w.dateOffset(0),false).value("blocks").toList().first().toMap().value("taskStatus").toString(),QString("COMPLETED"));
        w.setView("inbox");QVERIFY(w.createTask("cancelled"));QTRY_VERIFY(!w.busy());const auto open=w.tasks()->items().first();QVERIFY(w.changeState(open.id,open.rowVersion,"cancel"));QTRY_VERIFY(!w.busy());w.setView("cancelled");QCOMPARE(w.tasks()->rowCount(),1);QCOMPARE(w.tasks()->items().first().status,QString("CANCELLED"));w.setView("completed");QCOMPARE(w.tasks()->rowCount(),1);QCOMPARE(w.tasks()->items().first().id,task.id);
        const auto preview=w.recurrencePreview({{"anchor","2028-02-29"},{"frequency","YEARLY"},{"interval",1}});QVERIFY(preview.contains("2028-2-29"));QVERIFY(preview.contains("2032-2-29"));QVERIFY(!preview.contains("2029-2-29"));
    }
    void timeCreationScopeProjectionAndYearAreReadOnly() {
        QTemporaryDir dir;const auto path=dir.filePath("calendar.sqlite");TaskWorkspace w;w.start(path);QTRY_VERIFY(w.ready() && !w.busy());
        QVERIFY(w.saveCatalog({{"kind","project"},{"title","Calendar scope"}}));QTRY_VERIFY(!w.busy());const auto category=w.categoryTree().first().toMap();const auto scope="project:"+category.value("id").toString();w.setView(scope);
        QVERIFY(w.createForTime("A",w.dateOffset(0),9*60));QTRY_VERIFY(!w.busy());QVERIFY(w.errorText().isEmpty());const auto first=w.tasks()->items().first();QVERIFY(first.schedule);QCOMPARE(first.projectId,category.value("id").toString());
        QVERIFY(w.createForTime("B",w.dateOffset(0),9*60+30));QTRY_VERIFY(!w.busy());const auto count=w.tasks()->rowCount();const auto version=w.task(first.id).value("rowVersion");w.setView("week");auto timeline=w.timelineFor(w.dateOffset(0),true,true);QCOMPARE(timeline.value("days").toList().size(),7);QCOMPARE(timeline.value("blocks").toList().size(),2);for(const auto& block:timeline.value("blocks").toList())QCOMPARE(block.toMap().value("laneCount").toInt(),2);
        w.setView("year");const auto months=w.calendarMonthsFor("2024-02-29");QCOMPARE(months.size(),12);bool leap=false;for(const auto& day:months[1].toMap().value("days").toList())if(day.toMap().value("date")=="2024-02-29")leap=true;QVERIFY(leap);QCOMPARE(w.task(first.id).value("rowVersion"),version);QCOMPARE(w.scope(),scope);
        QVERIFY(!w.createForTime("bad","2026-02-30",0));QVERIFY(!w.errorText().isEmpty());w.setView(scope);QCOMPARE(w.tasks()->rowCount(),count);
        TaskWorkspace reopened;reopened.start(path);QTRY_VERIFY(reopened.ready() && !reopened.busy());QCOMPARE(reopened.calendarScale(),QString("year"));
    }
    void rankAndBatchImpactRemainConsistent() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath("rank.sqlite"));QTRY_VERIFY(w.ready() && !w.busy());
        for(const auto& title:QStringList{"Z","A"}){QVERIFY(w.saveCatalog({{"kind","project"},{"title",title}}));QTRY_VERIFY(!w.busy());}
        const auto categories=w.categoryTree();for(const auto& value:categories){w.setView("project:"+value.toMap().value("id").toString());QVERIFY(w.createTask(value.toMap().value("title").toString()));QTRY_VERIFY(!w.busy());}
        w.setView("all");QCOMPARE(w.tasks()->items().first().projectId,categories.first().toMap().value("id").toString());const auto root=w.tasks()->items().first();QVERIFY(w.createSubtask("child",root.id));QTRY_VERIFY(!w.busy());QVERIFY(!w.canDropTask(root.id,w.subtaskChoices(root.id).first().toMap().value("taskId").toString()));QVERIFY(!w.dropTaskReason(root.id,w.subtaskChoices(root.id).first().toMap().value("taskId").toString()).isEmpty());
        const QVariantList selected{QVariantMap{{"taskId",root.id},{"rowVersion",root.rowVersion}}};QCOMPARE(w.batchImpact(selected,"complete"),2);QCOMPARE(w.batchImpact(selected,"tags"),1);QCOMPARE(w.catalogImpact("project",root.projectId).value("tasks").toInt(),2);
    }
};
QTEST_MAIN(Ux34WorkspaceTest)
#include "Ux34WorkspaceTest.moc"
