// Read-only calendar, recurrence and reminder projections for the QML adapter.
// No SQL access or writes belong here; commands remain in TaskWorkspace.cpp and services.
#include "TaskWorkspace.h"
#include "services/CalendarService.h"
#include "services/EventService.h"
#include "services/RecurrenceService.h"
#include "services/ReminderSchedule.h"
#include <QTimeZone>
#include <QLocale>
#include <QMap>
#include <algorithm>

namespace daymark {
QVariantMap TaskWorkspace::calendarDraft(const QString& date,int minute,int duration,bool global) const
{
    const auto target=QDate::fromString(date,Qt::ISODate);
    if(!target.isValid() || target.toString(Qt::ISODate)!=date)return {};
    std::optional<TaskSchedule> schedule;
    if(minute>=0){
        const auto zone=QTimeZone::systemTimeZone();const auto from=target.startOfDay(zone),until=target.addDays(1).startOfDay(zone);
        if(minute>=from.secsTo(until)/60 || duration<1 || duration>7*25*60)return {};
        TaskSchedule value;value.allDay=false;value.timeZone=QString::fromUtf8(zone.id());value.startUtc=from.addSecs(minute*60).toUTC();value.endUtc=value.startUtc.addSecs(duration*60);schedule=value;
    }
    return TaskListModel::taskMap(TaskService::draftForScope(global?QStringLiteral("inbox"):m_scope,m_today,target,schedule));
}
QList<Task> TaskWorkspace::calendarSelection() const
{
    auto candidates=m_cache;
    // Include completed tasks using the same scope predicates, keeping their real status.
    if(m_calendarCompleted)for(auto& item:candidates)if(item.status==QStringLiteral("COMPLETED"))item.status=QStringLiteral("OPEN");
    auto result=QueryService::select(candidates,m_scope,m_today,m_search,false,m_tagFilter,0,0,m_projectFilter,{},true,false);
    for(auto& item:result)if(m_taskIndex.contains(item.id))item.status=m_taskIndex.value(item.id).status;
    return result;
}
QVariantList TaskWorkspace::unscheduledTasks()const
{return unscheduledFor();}
QVariantList TaskWorkspace::unscheduledFor(bool scoped)const
{QVariantList result;for(const auto& task:CalendarService::unscheduled(scoped?calendarSelection():m_cache))result.append(TaskListModel::taskMap(task));return result;}
QVariantList TaskWorkspace::calendarDays() const
{return calendarDaysFor(m_calendarDate.toString(Qt::ISODate),m_view==QStringLiteral("week"),true);}
QVariantList TaskWorkspace::calendarDaysFor(const QString& anchor,bool week,bool scoped) const
{
    QVariantList result;
    const auto date=QDate::fromString(anchor,Qt::ISODate);if(!date.isValid())return {};
    const auto selected=scoped?calendarSelection():m_cache;
    QSet<QString> included;for(const auto& item:selected)included.insert(item.id);
    const auto days=CalendarService::days(selected,date,week,m_sundayFirst,m_calendarCompleted);
    QHash<QDate,QVariantList> eventDays;
    QVariantList spans;
    if(!days.isEmpty())for(const auto& range:CalendarService::ranges(selected,days.first().date,days.last().date,m_calendarCompleted)){
        auto map=TaskListModel::taskMap(range.task);map.insert(QStringLiteral("sourceType"),QStringLiteral("task"));map.insert(QStringLiteral("multiDay"),true);
        map.insert(QStringLiteral("firstVisibleDate"),range.start.toString(Qt::ISODate));map.insert(QStringLiteral("lastVisibleDate"),range.end.toString(Qt::ISODate));spans.append(map);
    }
    const auto zone=QTimeZone::systemTimeZone();
    if(!days.isEmpty())for(const auto& event:m_events) {
        if(!included.contains(event.id))continue;
        const auto found=m_taskIndex.constFind(event.id);if(found==m_taskIndex.cend())continue;auto visible=found.value();if(visible.status==QStringLiteral("COMPLETED") && m_calendarCompleted)visible.status=QStringLiteral("OPEN");if(!QueryService::effectiveActive(visible))continue;
        for(const auto& instance:EventService::occurrences(event,days.first().date,days.last().date,zone)) {
            const auto dates=EventService::visibleDates(instance,days.first().date,days.last().date,zone);
            auto map=eventMap(instance);map.insert(QStringLiteral("sourceType"),QStringLiteral("event"));map.insert(QStringLiteral("recurring"),event.repeatFrequency!=QStringLiteral("NONE"));
            const auto first=instance.allDay?instance.startOn:instance.startUtc.toTimeZone(zone).date();
            const auto last=instance.allDay?instance.endOn.addDays(-1):instance.endUtc.addMSecs(-1).toTimeZone(zone).date();
            map.insert(QStringLiteral("multiDay"),first<last);map.insert(QStringLiteral("firstVisibleDate"),first.toString(Qt::ISODate));map.insert(QStringLiteral("lastVisibleDate"),last.toString(Qt::ISODate));
            if(first<last && !dates.isEmpty())spans.append(map);
            for(const auto& date:dates)eventDays[date].append(map);
        }
    }
    // Allocate week-local lanes in stable start/longest/title/identity order.
    std::sort(spans.begin(),spans.end(),[](const QVariant& l,const QVariant& r){
        const auto a=l.toMap(),b=r.toMap();
        for(const auto& key:{QStringLiteral("firstVisibleDate"),QStringLiteral("lastVisibleDate"),QStringLiteral("title"),QStringLiteral("sourceType"),QStringLiteral("taskId"),QStringLiteral("eventId"),QStringLiteral("occurrenceKey")}) {
            const auto x=a.value(key).toString(),y=b.value(key).toString();if(x!=y)return key==QStringLiteral("lastVisibleDate")?x>y:x<y;
        }return false;
    });
    QList<QVariantList> bands(days.size()/7);QList<int> laneCounts(days.size()/7,0);QHash<QDate,int> hiddenBands,hiddenTaskBands,hiddenEventBands;
    for(int weekIndex=0;weekIndex<bands.size();++weekIndex) {
        QList<int> occupied;int hiddenLane=3;
        const auto first=days[weekIndex*7].date,last=first.addDays(6);
        for(const auto& value:spans) {
            auto segment=value.toMap();const auto actualFirst=QDate::fromString(segment.value("firstVisibleDate").toString(),Qt::ISODate),actualLast=QDate::fromString(segment.value("lastVisibleDate").toString(),Qt::ISODate);
            if(actualFirst>last || actualLast<first)continue;
            const auto begin=qMax(first,actualFirst),end=qMin(last,actualLast);const int column=first.daysTo(begin),span=begin.daysTo(end)+1;
            const int mask=((1<<span)-1)<<column;int lane=0;while(lane<occupied.size() && (occupied[lane]&mask))++lane;
            if(lane>=3)lane=hiddenLane++;else {if(lane==occupied.size())occupied.append(0);occupied[lane]|=mask;}
            segment.insert("week",weekIndex);segment.insert("column",column);segment.insert("span",span);segment.insert("lane",lane);segment.insert("startsHere",begin==actualFirst);segment.insert("endsHere",end==actualLast);
            bands[weekIndex].append(segment);
            if(lane>=3)for(auto date=begin;date<=end;date=date.addDays(1)){++hiddenBands[date];if(segment.value("sourceType")==QStringLiteral("task"))++hiddenTaskBands[date];else ++hiddenEventBands[date];}
        }
        laneCounts[weekIndex]=qMin(3,occupied.size());
    }
    QHash<QPair<QString,QString>,QVariantMap> mappedTasks;
    for(int dayIndex=0;dayIndex<days.size();++dayIndex) {
        const auto& day=days[dayIndex];
        QVariantList entries;
        for(const auto& entry:day.entries) {
            const QPair<QString,QString> key{entry.task.id,entry.kind};
            if(!mappedTasks.contains(key)){
                auto map=TaskListModel::taskMap(entry.task);map.insert(QStringLiteral("kind"),entry.kind);
                map.insert(QStringLiteral("multiDay"),entry.task.startOn && entry.task.deadlineOn && *entry.task.startOn<*entry.task.deadlineOn);mappedTasks.insert(key,map);
            }
            entries.append(mappedTasks.value(key));
        }
        auto events=eventDays.value(day.date);
        std::sort(events.begin(),events.end(),[](const QVariant& left,const QVariant& right){
            const auto a=left.toMap(),b=right.toMap();
            if(a.value("allDay").toBool()!=b.value("allDay").toBool())return a.value("allDay").toBool();
            const auto first=a.value("occurrenceKey").toString(),second=b.value("occurrenceKey").toString();
            if(first!=second)return first<second;
            const int title=QString::compare(a.value("title").toString(),b.value("title").toString(),Qt::CaseInsensitive);
            return title!=0?title<0:a.value("eventId").toString()<b.value("eventId").toString();
        });
        result.append(QVariantMap{{"date",day.date.toString(Qt::ISODate)},{"day",day.date.day()},{"inMonth",day.inMonth},
            {"today",day.date==m_today},{"weekend",day.date.dayOfWeek()>5},{"entries",entries},{"events",events},
            {"week",dayIndex/7},{"bandLanes",laneCounts[dayIndex/7]},{"hiddenBands",hiddenBands.value(day.date)},
            {"hiddenTaskBands",hiddenTaskBands.value(day.date)},{"hiddenEventBands",hiddenEventBands.value(day.date)},{"bands",dayIndex%7==0?bands[dayIndex/7]:QVariantList{}}});
    }
    return result;
}
QString TaskWorkspace::shiftCalendarScale(const QString& anchor,int offset,const QString& scale) const
{
    const auto date=QDate::fromString(anchor,Qt::ISODate);if(!date.isValid())return {};
    return (offset==0?m_clock.localToday():scale==QStringLiteral("day")?date.addDays(offset):scale==QStringLiteral("week")?date.addDays(offset*7):scale==QStringLiteral("year")?date.addYears(offset):date.addMonths(offset)).toString(Qt::ISODate);
}
QString TaskWorkspace::titleForScale(const QString& anchor,const QString& scale) const
{
    const auto date=QDate::fromString(anchor,Qt::ISODate);if(!date.isValid())return {};
    return scale==QStringLiteral("day")?QLocale(QLocale::Chinese).toString(date,QStringLiteral("yyyy年M月d日 dddd")):scale==QStringLiteral("year")?date.toString(QStringLiteral("yyyy年")):calendarTitleFor(anchor,scale==QStringLiteral("week"));
}
QVariantList TaskWorkspace::calendarWeeksFor(const QString& anchor,int before,int after,bool scoped) const
{
    const auto date=QDate::fromString(anchor,Qt::ISODate);
    if(!date.isValid() || before<0 || before>2 || after<0 || after>3)return {};
    const QDate month(date.year(),date.month(),1);
    const auto first=month.addMonths(-before),last=month.addMonths(after);
    const auto lastDay=last.addDays(last.daysInMonth()-1);
    const int tail=m_sundayFirst?lastDay.dayOfWeek()%7:lastDay.dayOfWeek()-1;
    const auto lastWeek=lastDay.addDays(-tail);
    QMap<QDate,QVariantList> indexed;
    for(auto current=first;current<=last;current=current.addMonths(1)){
        const auto days=calendarDaysFor(current.toString(Qt::ISODate),false,scoped);
        for(int i=0;i+6<days.size();i+=7){
            const auto weekDate=QDate::fromString(days[i].toMap().value("date").toString(),Qt::ISODate);
            if(weekDate>lastWeek || indexed.contains(weekDate))continue;
            QVariantList week;
            for(int d=0;d<7;++d){auto day=days[i+d].toMap();day.insert("week",0);day.insert("column",d);day.insert("inMonth",true);
                auto bands=day.value("bands").toList();for(auto& value:bands){auto band=value.toMap();band.insert("week",0);value=band;}day.insert("bands",bands);week.append(day);
            }
            indexed.insert(weekDate,week);
        }
    }
    QVariantList result;
    for(auto it=indexed.cbegin();it!=indexed.cend();++it){
        QString title;for(const auto& value:it.value()){const auto day=QDate::fromString(value.toMap().value("date").toString(),Qt::ISODate);if(day.day()==1){title=day.toString(QStringLiteral("yyyy年M月"));break;}}
        auto owner=it.key().addDays(3);for(const auto& value:it.value()){const auto day=QDate::fromString(value.toMap().value("date").toString(),Qt::ISODate);if(day.day()==1){owner=day;break;}}
        result.append(QVariantMap{{"key",it.key().toString(Qt::ISODate)},{"month",QDate(owner.year(),owner.month(),1).toString(Qt::ISODate)},{"title",title},{"days",it.value()}});
    }
    return result;
}
QVariantList TaskWorkspace::calendarMonthsFor(const QString& anchor) const
{
    const auto date=QDate::fromString(anchor,Qt::ISODate);if(!date.isValid())return {};QVariantList result;
    for(int month=1;month<=12;++month){auto map=datePicker(QDate(date.year(),month,1).toString(Qt::ISODate),0);map.insert(QStringLiteral("month"),month);result.append(map);}return result;
}
QVariantMap TaskWorkspace::timelineFor(const QString& anchor,bool week,bool scoped) const
{
    return timelineInZone(anchor,week,scoped,QTimeZone::systemTimeZone(),{});
}
QVariantMap TaskWorkspace::timeRangePreview(const QString& date,const QString& zone,const QString& omittedTask) const
{
    return timelineInZone(date,false,false,QTimeZone(zone.toUtf8()),omittedTask);
}
QVariantMap TaskWorkspace::timelineInZone(const QString& anchor,bool week,bool scoped,const QTimeZone& zone,const QString& omittedTask) const
{
    auto from=QDate::fromString(anchor,Qt::ISODate);if(!from.isValid() || !zone.isValid())return {};if(week)from=CalendarService::days({},from,true,m_sundayFirst).first().date;
    const int count=week?7:1;auto selected=scoped?calendarSelection():m_cache;
    if(!omittedTask.isEmpty())selected.removeIf([&](const Task& task){return task.id==omittedTask;});
    const auto dates=calendarDaysFor(from.toString(Qt::ISODate),true,scoped);QHash<QString,QVariantMap> dateMap;for(const auto& value:dates){const auto map=value.toMap();dateMap.insert(map.value("date").toString(),map);}
    QVariantList days,blocks,clusters;QHash<QString,int> clusterIndex;int maximum=0;
    for(int i=0;i<count;++i){const auto date=from.addDays(i);const auto start=date.startOfDay(zone),end=date.addDays(1).startOfDay(zone);const int minutes=start.secsTo(end)/60;maximum=qMax(maximum,minutes);QVariantList ticks,entries;for(int at=0;at<minutes;at+=60){const auto time=start.addSecs(at*60);ticks.append(QVariantMap{{"minute",at},{"label",time.toString(minutes==1440?QStringLiteral("HH:mm"):QStringLiteral("HH:mm t"))}});}const auto map=dateMap.value(date.toString(Qt::ISODate));entries=map.value("entries").toList();for(const auto& value:map.value("events").toList())if(value.toMap().value("allDay").toBool())entries.append(value);
        days.append(QVariantMap{{"date",date.toString(Qt::ISODate)},{"title",QLocale(QLocale::Chinese).toString(date,week?QStringLiteral("d日 ddd"):QStringLiteral("M月d日 dddd"))},{"today",date==m_clock.nowUtc().toTimeZone(zone).date()},{"minutes",minutes},{"nowMinute",start.secsTo(m_clock.nowUtc())/60.0},{"ticks",ticks},{"entries",entries}});
    }
    for(const auto& block:CalendarService::timeBlocks(selected,from,count,zone,m_calendarCompleted)){auto map=TaskListModel::taskMap(block.task);map.insert("date",block.date.toString(Qt::ISODate));map.insert("cluster",QString::number(block.cluster));map.insert("day",from.daysTo(block.date));map.insert("startMinute",block.startMinute);map.insert("endMinute",block.endMinute);map.insert("lane",block.lane);map.insert("laneCount",block.laneCount);map.insert("continuesBefore",block.continuesBefore);map.insert("continuesAfter",block.continuesAfter);map.insert("timeLabel",CalendarService::timeSummary(block.task,m_today,zone));map.insert("shortTimeLabel",(block.continuesBefore?QStringLiteral("← "):QString())+block.task.schedule->startUtc.toTimeZone(zone).time().toString(QStringLiteral("HH:mm"))+QStringLiteral("–")+block.task.schedule->endUtc.toTimeZone(zone).time().toString(QStringLiteral("HH:mm"))+(block.continuesAfter?QStringLiteral(" →"):QString()));blocks.append(map);
        const auto key=QString::number(block.cluster);
        if(!clusterIndex.contains(key)){clusterIndex.insert(key,clusters.size());clusters.append(QVariantMap{{"key",key},{"day",from.daysTo(block.date)},{"startMinute",block.startMinute},{"endMinute",block.endMinute},{"laneCount",block.laneCount},{"items",QVariantList{map}}});}
        else {const int index=clusterIndex.value(key);auto group=clusters[index].toMap();group.insert("endMinute",qMax(group.value("endMinute").toDouble(),block.endMinute));auto items=group.value("items").toList();items.append(map);group.insert("items",items);clusters[index]=group;}
    }
    return {{"days",days},{"blocks",blocks},{"clusters",clusters},{"minutes",maximum},{"from",from.toString(Qt::ISODate)},{"zone",QString::fromUtf8(zone.id())}};
}
QVariantMap TaskWorkspace::timeRangeDraft(const QVariantMap& values) const
{
    const QTimeZone zone(values.value(QStringLiteral("zone")).toString().toUtf8());
    auto invalid=[](const QString& message){return QVariantMap{{"valid",false},{"error",message}};};
    if(!zone.isValid())return invalid(tr("时区无效，请先修正时区。"));
    QString startDate=values.value("startDate").toString(),endDate=values.value("endDate").toString();
    QString startTime=values.value("startTime").toString(),endTime=values.value("endTime").toString();
    if(values.contains("startMinute")) {
        const auto date=QDate::fromString(startDate,Qt::ISODate);
        const auto midnight=date.startOfDay(zone),next=date.addDays(1).startOfDay(zone);
        bool firstOk=false,lastOk=false;
        const int first=values.value("startMinute").toInt(&firstOk),last=values.value("endMinute").toInt(&lastOk);
        if(!date.isValid() || !midnight.isValid() || !next.isValid() || !firstOk || !lastOk || first<0 || last<=first || last>midnight.secsTo(next)/60)
            return invalid(tr("请选择有效的开始和结束时间。"));
        const auto start=midnight.addSecs(first*60),end=midnight.addSecs(last*60);
        startDate=start.date().toString(Qt::ISODate);endDate=end.date().toString(Qt::ISODate);
        startTime=start.time().toString(QStringLiteral("HH:mm"));endTime=end.time().toString(QStringLiteral("HH:mm"));
    }
    const auto first=EventService::localInstant(startDate+QLatin1Char(' ')+startTime,QString::fromUtf8(zone.id()));
    const auto last=EventService::localInstant(endDate+QLatin1Char(' ')+endTime,QString::fromUtf8(zone.id()));
    if(!first.ok() || !last.ok())return invalid(tr("日期或时间无效，或该时刻因夏令时不存在或不唯一。请调整后重试。"));
    if(last.value<=first.value)return invalid(tr("结束时间需要晚于开始时间。"));
    const auto midnight=QDate::fromString(startDate,Qt::ISODate).startOfDay(zone);
    const auto displayStart=first.value.toLocalTime();
    return {{"valid",true},{"startDate",startDate},{"endDate",endDate},{"startTime",startTime},{"endTime",endTime},
            {"zone",QString::fromUtf8(zone.id())},{"minute",midnight.secsTo(first.value)/60},{"duration",first.value.secsTo(last.value)/60},
            {"displayDate",displayStart.date().toString(Qt::ISODate)},
            {"displayMinute",displayStart.date().startOfDay(QTimeZone::systemTimeZone()).secsTo(first.value)/60}};
}
QString TaskWorkspace::systemTimeZone() const{return QString::fromUtf8(QTimeZone::systemTimeZoneId());}
QString TaskWorkspace::defaultReminderLocal() const
{return m_clock.nowUtc().addSecs(3600).toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));}
QString TaskWorkspace::reminderPreview(const QString& local,const QString& frequency,int interval) const
{
    const auto zone=QTimeZone::systemTimeZone();const QDateTime anchor(QDate::fromString(local.left(10),Qt::ISODate),QTime::fromString(local.mid(11),QStringLiteral("HH:mm")),zone,QDateTime::TransitionResolution::Reject);
    if(!anchor.isValid())return tr("请输入有效的提醒日期和时间。");
    if(anchor>m_clock.nowUtc())return tr("下次提醒：%1").arg(anchor.toString(QStringLiteral("yyyy-M-d HH:mm")));
    if(frequency==QStringLiteral("NONE"))return tr("此提醒时间已过去，请选择将来的时间。");
    const auto next=ReminderSchedule::nextAfter(anchor,frequency,interval,zone,m_clock.nowUtc());
    return next.isValid()?tr("下次提醒：%1").arg(next.toLocalTime().toString(QStringLiteral("yyyy-M-d HH:mm"))):tr("此规则没有有效的下次提醒。");
}
QString TaskWorkspace::minuteLabel(const QString& date,int minute,const QString& zoneId) const
{
    const auto day=QDate::fromString(date,Qt::ISODate);const auto zone=zoneId.isEmpty()?QTimeZone::systemTimeZone():QTimeZone(zoneId.toUtf8());if(!day.isValid() || !zone.isValid())return {};
    const auto start=day.startOfDay(zone);return start.addSecs(minute*60).toString(start.secsTo(day.addDays(1).startOfDay(zone))==86400?QStringLiteral("HH:mm"):QStringLiteral("HH:mm t"));
}
QVariantMap TaskWorkspace::eventMap(const CalendarEvent& e) const
{
    const QTimeZone zone(e.timeZone.toUtf8());
    const auto start=e.startUtc.toTimeZone(zone),end=e.endUtc.toTimeZone(zone);
    const auto localStart=e.startUtc.toLocalTime(),localEnd=e.endUtc.toLocalTime();
    auto timeInput=[](const QTime& time){return time.toString(time.second()!=0 || time.msec()!=0?QStringLiteral("HH:mm:ss.zzz"):QStringLiteral("HH:mm"));};
    QVariantList links;
    for(const auto& link:e.links){auto item=task(link.taskId);if(!item.isEmpty()){item.insert(QStringLiteral("role"),link.role);links.append(item);}}
    const auto time=e.allDay?tr("全天"):localStart.toString(QStringLiteral("HH:mm"))+QStringLiteral("–")+localEnd.toString(localStart.date()==localEnd.date()?QStringLiteral("HH:mm"):QStringLiteral("M/d HH:mm"));
    return {{"taskId",e.id},{"taskStatus",task(e.id).value(QStringLiteral("taskStatus"))},{"occurrenceKey",EventService::occurrenceKey(e)},{"repeatFrequency",e.repeatFrequency},{"repeatInterval",e.repeatInterval},{"repeatUntil",e.repeatUntil.toString(Qt::ISODate)},{"alertMinutes",e.alertMinutes},{"eventId",e.id},{"title",e.title},{"notes",e.notes},{"allDay",e.allDay},{"rowVersion",e.rowVersion},{"zone",e.timeZone},
        {"startDate",e.allDay?e.startOn.toString(Qt::ISODate):start.date().toString(Qt::ISODate)},{"endDate",e.allDay?e.endOn.addDays(-1).toString(Qt::ISODate):end.date().toString(Qt::ISODate)},
        {"startTime",timeInput(start.time())},{"endTime",timeInput(end.time())},{"timeLabel",time},{"links",links}};
}
QVariantList TaskWorkspace::recurringEvents() const
{
    QVariantList result;for(const auto& e:m_events)if(e.repeatFrequency!=QStringLiteral("NONE"))result.append(calendarEventPreview(e.id));
    std::sort(result.begin(),result.end(),[](const QVariant& a,const QVariant& b){return a.toMap().value("title").toString().localeAwareCompare(b.toMap().value("title").toString())<0;});return result;
}
QVariantList TaskWorkspace::calendarEvents() const{QVariantList result;for(const auto& e:m_events)result.append(eventMap(e));return result;}
QVariantMap TaskWorkspace::calendarEvent(const QString& id) const{for(const auto& e:m_events)if(e.id==id)return eventMap(e);return {};}
QVariantMap TaskWorkspace::calendarEventPreview(const QString& id,const QString& key) const
{
    for(const auto& series:m_events)if(series.id==id) {
        CalendarEvent instance=series;
        if(!key.isEmpty()) {
            const auto date=series.allDay?QDate::fromString(key,Qt::ISODate):QDateTime::fromString(key,Qt::ISODateWithMs).toLocalTime().date();
            if(!date.isValid())return {};
            bool found=false;
            for(const auto& candidate:EventService::occurrences(series,date,date,QTimeZone::systemTimeZone()))
                if(EventService::occurrenceKey(candidate)==key){instance=candidate;found=true;break;}
            if(!found)return {};
        }
        auto result=eventMap(instance);const bool recurring=series.repeatFrequency!=QStringLiteral("NONE");
        QString rule=tr("不重复");
        const QStringList frequencies={QStringLiteral("DAILY"),QStringLiteral("WEEKLY"),QStringLiteral("MONTHLY"),QStringLiteral("YEARLY")};
        const auto index=frequencies.indexOf(series.repeatFrequency);
        if(index>=0){const QStringList units={tr("天"),tr("周"),tr("月"),tr("年")};rule=tr("每 %1 %2").arg(series.repeatInterval).arg(units[index]);if(series.repeatUntil.isValid())rule+=tr("，至 %1（含当天）").arg(series.repeatUntil.toString(Qt::ISODate));}
        const auto first=series.allDay?series.startOn:series.startUtc.toTimeZone(QTimeZone(series.timeZone.toUtf8())).date();
        QString dateLabel;
        if(instance.allDay){dateLabel=instance.startOn.toString(Qt::ISODate);if(instance.endOn.addDays(-1)!=instance.startOn)dateLabel+=QStringLiteral(" — ")+instance.endOn.addDays(-1).toString(Qt::ISODate);dateLabel+=tr(" · 全天");}
        else dateLabel=instance.startUtc.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))+QStringLiteral(" — ")+instance.endUtc.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))+tr("（本机时区）");
        const auto alert=series.alertMinutes<0?tr("不提醒"):series.alertMinutes==0?tr("开始时提醒"):tr("提前 %1 分钟").arg(series.alertMinutes);
        QVariantMap timeline;
        const auto localStart=instance.startUtc.toLocalTime(),localEnd=instance.endUtc.toLocalTime();
        if(!instance.allDay && localStart.date()==localEnd.date()) {
            const int startMinute=localStart.time().hour()*60+localStart.time().minute(),endMinute=localEnd.time().hour()*60+localEnd.time().minute();
            if(endMinute>startMinute) {
            const int from=qMax(0,(startMinute/60-1)*60),through=qMin(1440,qMax(from+180,(endMinute/60+1)*60));const int range=through-from;
            QVariantList labels;const int step=qMax(60,((range+239)/240)*60);
            for(int minute=from;minute<=through;minute+=step)labels.append(QVariantMap{{"label",QStringLiteral("%1:00").arg(minute/60,2,10,QChar('0'))},{"position",double(minute-from)/range}});
            timeline={{"labels",labels},{"start",double(startMinute-from)/range},{"end",double(endMinute-from)/range}};
            }
        }
        result.insert(QStringLiteral("timeline"),timeline);
        result.insert(QStringLiteral("recurring"),recurring);result.insert(QStringLiteral("ruleLabel"),rule);
        result.insert(QStringLiteral("dateLabel"),dateLabel);result.insert(QStringLiteral("alertLabel"),alert+(series.alertMinutes>=0 && series.allDay?tr(" · 当天09:00为基准"):QString()));
        result.insert(QStringLiteral("contextLabel"),key.isEmpty() && recurring?tr("系列起始安排"):tr("本次安排"));
        result.insert(QStringLiteral("scopeLabel"),recurring?tr("循环起点 %1 · 默认编辑本次任务").arg(first.toString(Qt::ISODate)):tr("此任务的日期和提醒"));
        return result;
    }
    return {};
}
QVariantList TaskWorkspace::eventsForTask(const QString& id) const {
    if(task(id).isEmpty())return {};
    QVariantList result;for(const auto& e:m_events)for(const auto& link:e.links)if(link.taskId==id){auto item=eventMap(e);item.insert(QStringLiteral("role"),link.role);result.append(item);break;}return result;
}
QVariantList TaskWorkspace::eventTaskChoices(const QString& search) const {
    QVariantList result;for(const auto& t:m_cache)if(t.deletedAtUtc.isEmpty() && !t.syncRemoteDeletionPending && (search.trimmed().isEmpty() || t.title.contains(search.trimmed(),Qt::CaseInsensitive) || t.notes.contains(search.trimmed(),Qt::CaseInsensitive))){result.append(TaskListModel::taskMap(t));if(result.size()>=200)break;}return result;
}
QString TaskWorkspace::calendarTitle() const
{
    if(m_view==QStringLiteral("day"))return m_calendarDate.toString(QStringLiteral("yyyy年M月d日 dddd"));
    if(m_view==QStringLiteral("year"))return m_calendarDate.toString(QStringLiteral("yyyy年"));
    if(m_view!=QStringLiteral("week"))return m_calendarDate.toString(QStringLiteral("yyyy年M月"));
    const auto days=CalendarService::days({},m_calendarDate,true,m_sundayFirst);
    return days.first().date.toString(Qt::ISODate)+QStringLiteral(" — ")+days.last().date.toString(Qt::ISODate);
}
QString TaskWorkspace::calendarTitleFor(const QString& anchor,bool week) const
{
    const auto date=QDate::fromString(anchor,Qt::ISODate);if(!date.isValid())return {};
    if(!week)return date.toString(QStringLiteral("yyyy年M月"));
    const auto days=CalendarService::days({},date,true,m_sundayFirst);
    return days.first().date.toString(Qt::ISODate)+QStringLiteral(" — ")+days.last().date.toString(Qt::ISODate);
}
QString TaskWorkspace::shiftCalendar(const QString& anchor,int offset,bool week) const
{
    const auto date=QDate::fromString(anchor,Qt::ISODate);if(!date.isValid())return {};
    return (offset==0?m_clock.localToday():(week?date.addDays(offset*7):date.addMonths(offset))).toString(Qt::ISODate);
}
int TaskWorkspace::recurrenceWeekdayMask(const QString& anchor) const
{
    const auto date=QDate::fromString(anchor,Qt::ISODate);
    return 1<<((date.isValid()?date:m_today).dayOfWeek()-1);
}
QString TaskWorkspace::recurrencePreview(const QVariantMap& values) const
{
    RecurrenceRule rule;rule.anchor=QDate::fromString(values.value("anchor").toString(),Qt::ISODate);if(!rule.anchor.isValid())return tr("先设置开始日期，再预览循环。");
    rule.frequency=values.value("frequency").toString();rule.interval=values.value("interval",1).toInt();rule.weekdays=values.value("weekdays").toInt();rule.monthDay=rule.anchor.day();rule.yearMonth=rule.anchor.month();rule.end=QDate::fromString(values.value("end").toString(),Qt::ISODate);
    if(rule.frequency==QStringLiteral("AFTER_DAYS"))return tr("每次完成后 %1 天生成下一项。").arg(rule.interval);
    const auto dates=RecurrenceService::nextDates(rule,m_today);QStringList labels;for(const auto& date:dates)labels.append(date.toString(QStringLiteral("yyyy-M-d")));
    return labels.isEmpty()?tr("当前规则没有后续日期，请检查星期或结束日期。"):tr("接下来：%1").arg(labels.join(QStringLiteral("、")));
}
QVariantList TaskWorkspace::recurrenceRules() const
{
    QVariantList result;
    for(const auto& r:m_rules)result.append(QVariantMap{{"id",r.id},{"title",r.title},{"notes",r.notes},{"frequency",r.frequency},
        {"anchor",r.anchor.toString(Qt::ISODate)},{"end",r.end.toString(Qt::ISODate)},{"interval",r.interval},{"weekdays",r.weekdays},
        {"monthDay",r.monthDay},{"yearMonth",r.yearMonth},{"active",r.active},{"rowVersion",r.rowVersion}});
    return result;
}
QVariantList TaskWorkspace::notifications() const
{
    QVariantList result;for(const auto& notification:m_taskNotifications){auto record=notification.toMap();const auto id=record.value(QStringLiteral("taskId")).toString();const auto found=m_taskIndex.constFind(id);if(found!=m_taskIndex.cend())record.insert(QStringLiteral("title"),found->title);result.append(record);}
    std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.toMap().value(QStringLiteral("firedAt")).toString()>b.toMap().value(QStringLiteral("firedAt")).toString();});while(result.size()>50)result.removeLast();return result;
}
QString TaskWorkspace::localReminder(const QString& utc) const
{return QDateTime::fromString(utc,Qt::ISODateWithMs).toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));}
QVariantMap TaskWorkspace::datePicker(const QString& anchor,int offset) const
{
    auto date=QDate::fromString(anchor,Qt::ISODate);if(!date.isValid())date=m_today;date=QDate(date.year(),date.month(),1).addMonths(qBound(-1,offset,1));
    QVariantList days;for(const auto& day:CalendarService::days({},date,false,m_sundayFirst))days.append(QVariantMap{{"date",day.date.toString(Qt::ISODate)},{"day",day.date.day()},{"inMonth",day.inMonth},{"today",day.date==m_today}});
    return {{"anchor",date.toString(Qt::ISODate)},{"title",date.toString(QStringLiteral("yyyy年M月"))},{"days",days}};
}

} // namespace daymark
