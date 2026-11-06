#include "services/TaskTimeRecognition.h"
#include <QtTest>
#include <QElapsedTimer>
using namespace daymark;
class TaskTimeRecognitionTest final : public QObject {
    Q_OBJECT
    const QTimeZone zone{QByteArray("Asia/Shanghai")};
    const QDateTime now{QDate(2026,11,6),QTime(10,0),zone};
private slots:
    void recognition_data() {
        QTest::addColumn<QString>("text");QTest::addColumn<int>("state");QTest::addColumn<QString>("date");QTest::addColumn<QString>("time");QTest::addColumn<bool>("reminder");QTest::addColumn<QString>("target");
        auto add=[](const char* name,QString text,int state,QString date={},QString time={},bool reminder=false,QString target="planned") {QTest::newRow(name)<<text<<state<<date<<time<<reminder<<target;};
        add("date","11月12日完成报告",1,"2026-11-12");add("tomorrow","明天下午3点联系客户",1,"2026-11-07","15:00",true);
        add("chinese","明天下午三点半开会",1,"2026-11-07","15:30",true);add("minute","明天上午十点二十分开会",1,"2026-11-07","10:20",true);
        add("noon","今天中午12点",1,"2026-11-06","12:00",true);add("colon","明天 18:45",1,"2026-11-07","18:45",true);
        add("fullwidth","明天18：45开会",1,"2026-11-07","18:45",true);add("relative","20分钟后提醒我取快递",1,"2026-11-06","10:20",true);
        add("relative-hour","2小时后提交",1,"2026-11-06","12:00",true);add("deadline","11月12日前交报告",1,"2026-11-12",{},false,"deadline");
        add("deadline-prefix","截止2026-11-12交报告",1,"2026-11-12",{},false,"deadline");add("no-reminder","明天15点开会，不要提醒",1,"2026-11-07",{});
        add("nextweek","下周一9点开会",1,"2026-11-09","09:00",true);add("thisweek","本周日整理",1,"2026-11-08");
        add("weekday","周三9点",1,"2026-11-11","09:00",true);add("newyear","1月2日盘点",1,"2027-01-02");
        add("leap","2月29日检查",1,"2028-02-29");add("pastdate","2026-01-01归档",1,"2026-01-01");
        add("month-report","整理11月报告",0);add("seatbelt","购买3点式安全带",0);add("version","升级v1.8.0",0);add("plain","联系客户",0);
        add("unsupported-morning","明早8点开会",2);add("unsupported-yesterday","昨天15点开会",2);
        add("unsupported-nextmonth","下个月15点开会",2);add("unsupported-weekend","周末9点开会",2);add("unsupported-days","3天后15点开会",2);
        add("unsupported-english","tomorrow 15:00",2);add("unsupported-pm","9:00 pm",2);
        add("multiple","明天或后天开会",2);add("two-clock","明天9点到10点开会",2);add("invalid-date","2026-02-29交付",2);
        add("invalid-clock","明天25点开会",2);add("past-clock","今天9点开会",2);add("vague","明天上午开会",2);
        add("complex","每周一开会",2);add("relative-conflict","明天20分钟后开会",2);add("zero","0分钟后取快递",2);add("date-range","11月12日至14日出差",2);add("clock-deadline","明天15点前提交",2);
    }
    void recognition() {
        QFETCH(QString,text);QFETCH(int,state);QFETCH(QString,date);QFETCH(QString,time);QFETCH(bool,reminder);QFETCH(QString,target);
        const auto result=TaskTimeRecognition::parse(text,now,zone);
        QCOMPARE(int(result.state),state);QCOMPARE(result.date.toString(Qt::ISODate),date);QCOMPARE(result.time.toString("HH:mm"),time);QCOMPARE(result.reminderUtc.isValid(),reminder);QCOMPARE(result.target,target);
        if(reminder)QCOMPARE(result.reminderUtc.toTimeZone(zone).date(),result.date);
        for(auto span:result.spans){QVERIFY(span.first>=0);QVERIFY(span.first+span.second<=text.size());}
    }
    void preferencesAndManual() {
        TimeRecognitionOptions options;options.enabled=false;QCOMPARE(TaskTimeRecognition::parse("明天15点",now,zone,options).state,RecognizedTaskTime::State::None);
        options.enabled=true;options.dateOnlyReminder=true;options.dateOnlyTime=QTime(8,30);
        const auto result=TaskTimeRecognition::parse("明天交付",now,zone,options);QCOMPARE(result.reminderUtc.toTimeZone(zone).time(),QTime(8,30));
        QVERIFY(!TaskTimeRecognition::parse("明天交付，不提醒",now,zone,options).reminderUtc.isValid());
        QVERIFY(!TaskTimeRecognition::manual({QDate(2026,11,6),QTime(9,0),"planned",true},now,zone).ok());
        QVERIFY(TaskTimeRecognition::manual({QDate(2026,11,7),QTime(9,0),"planned",true},now,zone).ok());
        QVERIFY(!TaskTimeRecognition::manual({QDate(2026,11,7),QTime(9,0),"deadline",true},now,zone).ok());
        QVERIFY(!TaskTimeRecognition::manual({QDate(2026,11,7),{},"planned",true},now,zone).ok());
    }
    void dstDoesNotGuess() {
        const QTimeZone ny{QByteArray("America/New_York")};if(!ny.isValid())QSKIP("IANA New York zone not available");
        const QDateTime base(QDate(2026,1,1),QTime(0,0),QTimeZone::UTC);
        QCOMPARE(TaskTimeRecognition::parse("2026-03-08 02:30开会",base,ny).state,RecognizedTaskTime::State::Choice);
        QCOMPARE(TaskTimeRecognition::parse("2026-11-01 01:30开会",base,ny).state,RecognizedTaskTime::State::Choice);
    }
    void boundsAndRelativeClock() {
        QCOMPARE(TaskTimeRecognition::parse(QString(1025,'x'),now,zone).state,RecognizedTaskTime::State::Choice);
        const auto a=TaskTimeRecognition::parse("20分钟后",now,zone),b=TaskTimeRecognition::parse("20分钟后",now.addSecs(90),zone);
        QCOMPARE(a.reminderUtc.secsTo(b.reminderUtc),90);
        QElapsedTimer timer;timer.start();for(int i=0;i<1000;++i)TaskTimeRecognition::parse("明天下午三点半联系客户",now,zone);
        QVERIFY2(timer.elapsed()<3000,"bounded parsing of 1000 short inputs exceeded three seconds");
    }
};
QTEST_GUILESS_MAIN(TaskTimeRecognitionTest)
#include "TaskTimeRecognitionTest.moc"
