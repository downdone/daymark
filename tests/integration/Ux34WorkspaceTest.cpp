#include "models/TaskWorkspace.h"
#include "../support/WorkspaceWait.h"
#include <QTemporaryDir>
#include <QSettings>
#include <QFileInfo>
#include <QtTest>
using namespace daymark;
class Ux34WorkspaceTest:public QObject {
    Q_OBJECT
private slots:
    void todayListCalendarEntryAndExplicitBrowsing() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath("today-entry.sqlite"));QVERIFY(daymark::test::awaitIdle(w,true));
        for(const auto& scale:{"day","week","month"}) {
            w.setView("today");w.selectCalendarDate("1998-05-12");w.setView(scale);
            QCOMPARE(w.calendarDate(),w.dateOffset(0));QCOMPARE(w.scope(),QString("today"));
            w.stepCalendar(-1);const auto browsed=w.calendarDate();w.clearError();QCOMPARE(w.calendarDate(),browsed);
        }
        w.setView("all");w.selectCalendarDate("1998-05-12");w.setView("day");QCOMPARE(w.calendarDate(),QString("1998-05-12"));
        w.setScope("today");QCOMPARE(w.calendarDate(),QString("1998-05-12"));
    }
    void rangeDraftUsesZoneMidnightAndRejectsInvalidTimeWithoutWrites() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath("range-preview.sqlite"));QVERIFY(daymark::test::awaitIdle(w,true));
        const auto count=w.tasks()->rowCount();const auto date=w.calendarDate();
        auto midnight=w.timeRangeDraft({{"zone","UTC"},{"startDate","2026-10-27"},{"startMinute",1425},{"endMinute",1440}});
        QVERIFY(midnight.value("valid").toBool());QCOMPARE(midnight.value("endDate").toString(),QString("2026-10-28"));QCOMPARE(midnight.value("endTime").toString(),QString("00:00"));
        auto exact=w.timeRangeDraft({{"zone","UTC"},{"startDate","2026-10-27"},{"endDate","2026-10-27"},{"startTime","09:07"},{"endTime","09:43"}});
        QVERIFY(exact.value("valid").toBool());QCOMPARE(exact.value("duration").toInt(),36);
        const auto local=QDateTime(QDate(2026,10,27),QTime(9,7),QTimeZone::UTC).toLocalTime();
        QCOMPARE(exact.value("displayDate").toString(),local.date().toString(Qt::ISODate));
        QCOMPARE(exact.value("displayMinute").toLongLong(),local.date().startOfDay(QTimeZone::systemTimeZone()).secsTo(local)/60);
        QVERIFY(!w.timeRangeDraft({{"zone","UTC"},{"startDate","2026-10-27"},{"startMinute",600},{"endMinute",599}}).value("valid").toBool());
        QVERIFY(!w.timeRangeDraft({{"zone","bad-zone"}}).value("valid").toBool());
        if(QTimeZone("America/New_York").isValid()) {
            QCOMPARE(w.timeRangePreview("2026-03-08","America/New_York",{}).value("minutes").toInt(),1380);
            QCOMPARE(w.timeRangePreview("2026-11-01","America/New_York",{}).value("minutes").toInt(),1500);
            for(const auto& local:{QString("2026-03-08 02:30"),QString("2026-11-01 01:30")})
                QVERIFY(!w.timeRangeDraft({{"zone","America/New_York"},{"startDate",local.left(10)},{"endDate",local.left(10)},{"startTime",local.mid(11)},{"endTime","04:00"}}).value("valid").toBool());
        }
        QCOMPARE(w.tasks()->rowCount(),count);QCOMPARE(w.calendarDate(),date);QVERIFY(!w.busy());
    }
    void rangeBackgroundExcludesEditedTaskWithoutChangingScope() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath("range-background.sqlite"));QVERIFY(daymark::test::awaitIdle(w,true));
        QVERIFY(w.createForTime("own",w.dateOffset(0),540));QVERIFY(daymark::test::awaitIdle(w));const auto id=w.tasks()->items().first().id;
        w.setView("important");
        QCOMPARE(w.timeRangePreview(w.dateOffset(0),w.systemTimeZone(),{}).value("blocks").toList().size(),1);
        QCOMPARE(w.timeRangePreview(w.dateOffset(0),w.systemTimeZone(),id).value("blocks").toList().size(),0);
        QCOMPARE(w.scope(),QString("important"));QVERIFY(!w.task(id).value("important").toBool());
    }
    void yearEntryReturnsToTodayAndScopeChangesKeepNavigation() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath("year.sqlite"));QVERIFY(daymark::test::awaitIdle(w,true));
        w.setView("month");w.selectCalendarDate("1998-05-12");w.setView("year");QCOMPARE(w.calendarDate(),w.dateOffset(0));
        w.stepCalendar(1);const auto browsed=w.calendarDate();QVERIFY(browsed!=w.dateOffset(0));w.setScope("all");QCOMPARE(w.calendarDate(),browsed);QCOMPARE(w.calendarScale(),QString("year"));
        w.setView("week");w.setView("year");QCOMPARE(w.calendarDate(),w.dateOffset(0));
    }
    void calendarDesktopLayerSettingsStayIndependentAndMutuallyExclusive() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath("layer.sqlite"));QVERIFY(daymark::test::awaitIdle(w,true));QWindow calendar;calendar.setGeometry(40,50,500,400);
        QVERIFY(w.attachDesktopCalendarWindow(&calendar));const auto geometry=calendar.geometry();
        QVERIFY(w.setPreference("desktopCalendarTopmost",true));QVERIFY(calendar.flags().testFlag(Qt::WindowStaysOnTopHint));
        QVERIFY(w.setPreference("desktopCalendarPinned",true));QVERIFY(w.preferences().value("desktopCalendarPinned").toBool());QVERIFY(!w.preferences().value("desktopCalendarTopmost").toBool());QVERIFY(calendar.flags().testFlag(Qt::WindowStaysOnBottomHint));QCOMPARE(calendar.geometry(),geometry);QVERIFY(!calendar.isVisible());
        TaskWorkspace reopened;reopened.start(dir.filePath("layer.sqlite"));QVERIFY(daymark::test::awaitIdle(reopened,true));QWindow second;QVERIFY(reopened.attachDesktopCalendarWindow(&second));QVERIFY(second.flags().testFlag(Qt::WindowStaysOnBottomHint));
        QVERIFY(w.setPreference("desktopCalendarTopmost",true));QVERIFY(!w.preferences().value("desktopCalendarPinned").toBool());QVERIFY(!calendar.flags().testFlag(Qt::WindowStaysOnBottomHint));
        QVERIFY(w.setPreference("desktopCalendarPinned",true));QVERIFY(w.setPreference("desktopCalendarPinned",false));QVERIFY(!calendar.flags().testFlag(Qt::WindowStaysOnTopHint));QVERIFY(!calendar.flags().testFlag(Qt::WindowStaysOnBottomHint));
    }
    void calendarLayerRollsBackWhenSettingsCannotBeWritten() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath("failed-layer.sqlite"));QVERIFY(daymark::test::awaitIdle(w,true));
        QWindow calendar;QVERIFY(w.attachDesktopCalendarWindow(&calendar));
        QVERIFY(QDir().mkdir(dir.filePath("ui.ini")));
        QVERIFY(!w.setPreference("desktopCalendarPinned",true));
        QVERIFY(!w.preferences().value("desktopCalendarPinned").toBool());
        QVERIFY(!calendar.flags().testFlag(Qt::WindowStaysOnBottomHint));
        QVERIFY(!calendar.flags().testFlag(Qt::WindowStaysOnTopHint));
        QVERIFY(!w.errorText().isEmpty());
    }
    void calendarDraftAndAtomicCreation() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath("draft.sqlite"));QVERIFY(daymark::test::awaitIdle(w,true));
        QVERIFY(w.saveCatalog({{"kind","project"},{"title","Draft category"}}));QVERIFY(daymark::test::awaitIdle(w));
        const auto category=w.categoryTree().first().toMap();w.setView("project:"+category.value("id").toString());
        const auto date=w.dateOffset(2);const auto seed=w.calendarDraft(date,540,45);
        QCOMPARE(seed.value("projectId"),category.value("id"));QCOMPARE(seed.value("schedule").toMap().value("endTime").toString(),QString("09:45"));
        QCOMPARE(w.tasks()->rowCount(),0);QVERIFY(w.calendarDraft("2026-02-30").isEmpty());QVERIFY(w.calendarDraft(date,2000).isEmpty());
        const auto global=w.calendarDraft(date,540,60,true);QVERIFY(global.value("projectId").toString().isEmpty());
        QVariantMap details{{"projectId",category.value("id")},{"schedule",seed.value("schedule")},{"important",true},{"pinned",true},
            {"repeat",QVariantMap{{"frequency","DAILY"},{"interval",0}}}};
        // The insert succeeds before edit validation fails: neither task nor
        // recurrence may survive that transaction.
        QVERIFY(w.createTaskWithDetails("Invalid draft","notes",date,date,details));QVERIFY(daymark::test::awaitIdle(w));QVERIFY(!w.errorText().isEmpty());QCOMPARE(w.tasks()->rowCount(),0);
        details.remove("repeat");details.insert("tagIds",QStringList{"missing-tag"});
        QVERIFY(w.createTaskWithDetails("Invalid tag","notes",date,date,details));QVERIFY(daymark::test::awaitIdle(w));QVERIFY(!w.errorText().isEmpty());QCOMPARE(w.tasks()->rowCount(),0);
        details.remove("tagIds");
        QVERIFY(w.createTaskWithDetails("Complete draft","draft notes",date,date,details));QVERIFY(daymark::test::awaitIdle(w));QVERIFY2(w.errorText().isEmpty(),qPrintable(w.errorText()));QCOMPARE(w.tasks()->rowCount(),1);
        const auto task=w.tasks()->items().first();QCOMPARE(task.notes,QString("draft notes"));QCOMPARE(task.projectId,category.value("id").toString());QVERIFY(task.important && task.pinned && task.schedule);QCOMPARE(task.schedule->startUtc.secsTo(task.schedule->endUtc),45*60);
        TaskWorkspace reopened;reopened.start(dir.filePath("draft.sqlite"));QVERIFY(daymark::test::awaitIdle(reopened,true));reopened.setView("all");QCOMPARE(reopened.tasks()->rowCount(),1);
    }
    void continuousWeeksAreBoundedUniqueAndReadOnly() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath("weeks.sqlite"));QVERIFY(daymark::test::awaitIdle(w,true));
        w.setView("inbox");QVERIFY(w.createForDate("cross month","2026-10-30"));QVERIFY(daymark::test::awaitIdle(w));const auto task=w.tasks()->items().first();QVERIFY(w.saveTaskWithDetails(task.id,task.rowVersion,task.title,task.notes,"ACTIVE","2026-10-30","2026-11-04",false,{}));QVERIFY(daymark::test::awaitIdle(w));const auto version=w.task(task.id).value("rowVersion");
        const auto weeks=w.calendarWeeksFor("2026-10-25");QVERIFY(weeks.size()>=17 && weeks.size()<=19);QSet<QString> dates,starts;int references=0;
        for(const auto& value:weeks){const auto row=value.toMap();QVERIFY(!starts.contains(row.value("key").toString()));starts.insert(row.value("key").toString());const auto days=row.value("days").toList();QCOMPARE(days.size(),7);for(const auto& entry:days){const auto day=entry.toMap();const auto date=day.value("date").toString();QVERIFY(!dates.contains(date));dates.insert(date);for(const auto& band:day.value("bands").toList()){QCOMPARE(band.toMap().value("week").toInt(),0);if(band.toMap().value("taskId")==task.id)++references;}}}
        QCOMPARE(references,2);QVERIFY(dates.contains("2026-10-31") && dates.contains("2026-11-01"));QCOMPARE(w.task(task.id).value("rowVersion"),version);QVERIFY(w.calendarWeeksFor("2026-10-25",3,4).isEmpty());
        const auto leap=w.calendarWeeksFor("2028-02-29",0,0);bool seen=false;for(const auto& value:leap)for(const auto& d:value.toMap().value("days").toList())if(d.toMap().value("date")=="2028-02-29")seen=true;QVERIFY(seen);
    }
    void completedCalendarAndCancelledAreDistinct() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath("status.sqlite"));QVERIFY(daymark::test::awaitIdle(w,true));w.setView("inbox");QVERIFY(w.createForTime("done",w.dateOffset(0),540,90));QVERIFY(daymark::test::awaitIdle(w));const auto task=w.tasks()->items().first();QVERIFY(w.changeState(task.id,task.rowVersion,"complete"));QVERIFY(daymark::test::awaitIdle(w));
        w.setView("all");QCOMPARE(w.timelineFor(w.dateOffset(0),false).value("blocks").toList().size(),0);w.setCalendarCompleted(true);QCOMPARE(w.timelineFor(w.dateOffset(0),false).value("blocks").toList().size(),1);QCOMPARE(w.timelineFor(w.dateOffset(0),false).value("blocks").toList().first().toMap().value("taskStatus").toString(),QString("COMPLETED"));
        w.setView("inbox");QVERIFY(w.createTask("cancelled"));QVERIFY(daymark::test::awaitIdle(w));const auto open=w.tasks()->items().first();QVERIFY(w.changeState(open.id,open.rowVersion,"cancel"));QVERIFY(daymark::test::awaitIdle(w));w.setView("cancelled");QCOMPARE(w.tasks()->rowCount(),1);QCOMPARE(w.tasks()->items().first().status,QString("CANCELLED"));w.setView("completed");QCOMPARE(w.tasks()->rowCount(),1);QCOMPARE(w.tasks()->items().first().id,task.id);
        const auto preview=w.recurrencePreview({{"anchor","2028-02-29"},{"frequency","YEARLY"},{"interval",1}});QVERIFY(preview.contains("2028-2-29"));QVERIFY(preview.contains("2032-2-29"));QVERIFY(!preview.contains("2029-2-29"));
    }
    void timeCreationScopeProjectionAndYearAreReadOnly() {
        QTemporaryDir dir;const auto path=dir.filePath("calendar.sqlite");TaskWorkspace w;w.start(path);QVERIFY(daymark::test::awaitIdle(w,true));
        QVERIFY(w.saveCatalog({{"kind","project"},{"title","Calendar scope"}}));QVERIFY(daymark::test::awaitIdle(w));const auto category=w.categoryTree().first().toMap();const auto scope="project:"+category.value("id").toString();w.setView(scope);
        QVERIFY(w.createForTime("A",w.dateOffset(0),9*60));QVERIFY(daymark::test::awaitIdle(w));QVERIFY(w.errorText().isEmpty());const auto first=w.tasks()->items().first();QVERIFY(first.schedule);QCOMPARE(first.projectId,category.value("id").toString());
        QVERIFY(w.createForTime("B",w.dateOffset(0),9*60+30));QVERIFY(daymark::test::awaitIdle(w));const auto count=w.tasks()->rowCount();const auto version=w.task(first.id).value("rowVersion");w.setView("week");auto timeline=w.timelineFor(w.dateOffset(0),true,true);QCOMPARE(timeline.value("days").toList().size(),7);QCOMPARE(timeline.value("blocks").toList().size(),2);for(const auto& block:timeline.value("blocks").toList())QCOMPARE(block.toMap().value("laneCount").toInt(),2);
        w.setView("year");const auto months=w.calendarMonthsFor("2024-02-29");QCOMPARE(months.size(),12);bool leap=false;for(const auto& day:months[1].toMap().value("days").toList())if(day.toMap().value("date")=="2024-02-29")leap=true;QVERIFY(leap);QCOMPARE(w.task(first.id).value("rowVersion"),version);QCOMPARE(w.scope(),scope);
        QVERIFY(!w.createForTime("bad","2026-02-30",0));QVERIFY(!w.errorText().isEmpty());w.setView(scope);QCOMPARE(w.tasks()->rowCount(),count);
        TaskWorkspace reopened;reopened.start(path);QVERIFY(daymark::test::awaitIdle(reopened,true));QCOMPARE(reopened.calendarScale(),QString("year"));
    }
    void rankAndBatchImpactRemainConsistent() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath("rank.sqlite"));QVERIFY(daymark::test::awaitIdle(w,true));
        for(const auto& title:QStringList{"Z","A"}){QVERIFY(w.saveCatalog({{"kind","project"},{"title",title}}));QVERIFY(daymark::test::awaitIdle(w));}
        const auto categories=w.categoryTree();for(const auto& value:categories){w.setView("project:"+value.toMap().value("id").toString());QVERIFY(w.createTask(value.toMap().value("title").toString()));QVERIFY(daymark::test::awaitIdle(w));}
        w.setView("all");QCOMPARE(w.tasks()->items().first().projectId,categories.first().toMap().value("id").toString());const auto root=w.tasks()->items().first();QVERIFY(w.createSubtask("child",root.id));QVERIFY(daymark::test::awaitIdle(w));QVERIFY(!w.canDropTask(root.id,w.subtaskChoices(root.id).first().toMap().value("taskId").toString()));QVERIFY(!w.dropTaskReason(root.id,w.subtaskChoices(root.id).first().toMap().value("taskId").toString()).isEmpty());
        const QVariantList selected{QVariantMap{{"taskId",root.id},{"rowVersion",root.rowVersion}}};QCOMPARE(w.batchImpact(selected,"complete"),2);QCOMPARE(w.batchImpact(selected,"tags"),1);QCOMPARE(w.catalogImpact("project",root.projectId).value("tasks").toInt(),2);
    }
};
QTEST_MAIN(Ux34WorkspaceTest)
#include "Ux34WorkspaceTest.moc"
