#include "TaskTimeRecognition.h"
#include <QRegularExpression>

namespace daymark {
namespace {
int numeral(const QString& value) {
    bool ok=false;const int digit=value.toInt(&ok);if(ok)return digit;
    const QString numbers=QStringLiteral("零一二三四五六七八九");
    const auto ten=value.indexOf(QChar(0x5341));
    auto one=[&](const QString& text){return text.size()==1?int(numbers.indexOf(text.at(0))):-1;};
    if(ten<0)return one(value);
    const int head=ten==0?1:one(value.left(ten));
    const int tail=ten==value.size()-1?0:one(value.mid(ten+1));
    return head>0 && tail>=0?head*10+tail:-1;
}
QDateTime wallClock(const QDate& date,const QTime& time,const QTimeZone& zone) {
    return QDateTime(date,time,zone,QDateTime::TransitionResolution::Reject);
}
RecognizedTaskTime choice(QString reason) {
    RecognizedTaskTime result;result.state=RecognizedTaskTime::State::Choice;result.reason=std::move(reason);return result;
}
}
RecognizedTaskTime TaskTimeRecognition::parse(const QString& text,const QDateTime& nowUtc,const QTimeZone& zone,const TimeRecognitionOptions& options) {
    RecognizedTaskTime result;
    if(!options.enabled || text.isEmpty())return result;
    if(text.size()>1024)return choice(QStringLiteral("timeInputTooLong"));
    if(!nowUtc.isValid() || !zone.isValid())return choice(QStringLiteral("timeContextInvalid"));
    static const QRegularExpression complex(QStringLiteral(R"(每天|每周|每月|每年|每隔|明早|明晚|今晚|昨天|昨日|前天|下下|下个月|下月|周末|月底|月末|天后|天前|周后|月后|年后|\b(?:tomorrow|yesterday|tonight|am|pm)\b)"),QRegularExpression::CaseInsensitiveOption);
    if(complex.match(text).hasMatch() || QRegularExpression(QStringLiteral(R"((?:日|号|点|时)\s*(?:至|到|[-~～])\s*[0-9一二三四五六七八九十])")).match(text).hasMatch())return choice(QStringLiteral("timeComplex"));
    const auto localNow=nowUtc.toTimeZone(zone);const auto today=localNow.date();
    static const QRegularExpression dates(QStringLiteral(R"((?<![0-9A-Za-z])(?:(\d{4})-(\d{1,2})-(\d{1,2})(?!\d)|(?:(\d{4})年)?(\d{1,2})月(\d{1,2})[日号]|(今天|明天|后天)|(下|本|这)?(?:周|星期)([一二三四五六日天])) )").trimmed());
    auto matches=dates.globalMatch(text);int dateCount=0;
    while(matches.hasNext()) {
        const auto match=matches.next();if(++dateCount>1)return choice(QStringLiteral("timeMultiple"));
        result.spans.append({int(match.capturedStart()),int(match.capturedLength())});
        if(!match.captured(7).isEmpty())result.date=today.addDays(match.captured(7)==QStringLiteral("今天")?0:match.captured(7)==QStringLiteral("明天")?1:2);
        else if(!match.captured(9).isEmpty()) {
            const int weekday=int(QStringLiteral("一二三四五六日天").indexOf(match.captured(9)))+1;
            int offset=(weekday==8?7:weekday)-today.dayOfWeek();
            if(match.captured(8)==QStringLiteral("下"))offset+=7;
            else if(match.captured(8).isEmpty() && offset<0)offset+=7;
            result.date=today.addDays(offset);
        } else {
            const bool iso=!match.captured(1).isEmpty();
            const int month=match.captured(iso?2:5).toInt(),day=match.captured(iso?3:6).toInt();
            const auto yearText=match.captured(iso?1:4);
            if(!yearText.isEmpty())result.date=QDate(yearText.toInt(),month,day);
            else for(int year=today.year();year<=today.year()+8;++year) {
                const QDate candidate(year,month,day);if(candidate.isValid() && candidate>=today){result.date=candidate;break;}
            }
        }
        if(!result.date.isValid())return choice(QStringLiteral("timeDateInvalid"));
        const auto suffix=text.mid(match.capturedEnd(),2);
        if(suffix.startsWith(QStringLiteral("前")) || suffix==QStringLiteral("之前") || text.left(match.capturedStart()).endsWith(QStringLiteral("截止")))result.target=QStringLiteral("deadline");
    }
    static const QRegularExpression relative(QStringLiteral(R"((?<![0-9A-Za-z])(\d{1,4})\s*(分钟|小时)后)"));
    auto intervals=relative.globalMatch(text);int relativeCount=0;
    while(intervals.hasNext()) {
        const auto match=intervals.next();if(++relativeCount>1 || dateCount)return choice(QStringLiteral("timeMultiple"));
        const int amount=match.captured(1).toInt();if(amount<1 || amount>1440)return choice(QStringLiteral("timeRelativeInvalid"));
        result.reminderUtc=nowUtc.toUTC().addSecs(amount*(match.captured(2)==QStringLiteral("小时")?3600:60));
        const auto local=result.reminderUtc.toTimeZone(zone);result.date=local.date();result.time=local.time();
        result.spans.append({int(match.capturedStart()),int(match.capturedLength())});
    }
    static const QRegularExpression times(QStringLiteral(R"((?<![0-9A-Za-z])(?:(上午|下午|晚上|中午|早上|凌晨)\s*)?([0-9]{1,2}|[零一二三四五六七八九十]{1,3})(?:[:：]([0-9]{2})|[点时](?:([0-9]{1,2}|[零一二三四五六七八九十]{1,3})分?|(半))?)(?![0-9A-Za-z式]))"));
    auto clock=times.globalMatch(text);int timeCount=0;
    while(clock.hasNext()) {
        const auto match=clock.next();if(++timeCount>1 || relativeCount)return choice(QStringLiteral("timeMultiple"));
        if(text.mid(match.capturedEnd()).startsWith(QStringLiteral("前")))return choice(QStringLiteral("timeComplex"));
        int hour=numeral(match.captured(2));const int minute=!match.captured(5).isEmpty()?30:!match.captured(3).isEmpty()?match.captured(3).toInt():!match.captured(4).isEmpty()?numeral(match.captured(4)):0;
        const auto period=match.captured(1);
        if(!period.isEmpty()) {
            if(hour<1 || hour>12)return choice(QStringLiteral("timeClockInvalid"));
            if(period==QStringLiteral("下午") || period==QStringLiteral("晚上") || period==QStringLiteral("中午")){if(hour!=12)hour+=12;}
            else if(hour==12){if(period!=QStringLiteral("凌晨"))return choice(QStringLiteral("timeClockUnclear"));hour=0;}
        }
        result.time=QTime(hour,minute);if(!result.time.isValid())return choice(QStringLiteral("timeClockInvalid"));
        result.spans.append({int(match.capturedStart()),int(match.capturedLength())});
        if(!result.date.isValid())result.date=today;
    }
    if(!dateCount && !timeCount && !relativeCount)return result;
    if(!timeCount && !relativeCount && QRegularExpression(QStringLiteral("上午|下午|晚上|早上|中午|凌晨|早晨")).match(text).hasMatch())return choice(QStringLiteral("timeClockUnclear"));
    const bool noReminder=text.contains(QStringLiteral("不要提醒")) || text.contains(QStringLiteral("不提醒"));
    if(result.target==QStringLiteral("deadline")){result.time={};result.reminderUtc={};}
    else if(!noReminder) {
        if(!result.time.isValid() && options.dateOnlyReminder)result.time=options.dateOnlyTime;
        if(result.time.isValid() && !result.reminderUtc.isValid()) {
            const auto instant=wallClock(result.date,result.time,zone);
            if(!instant.isValid())return choice(QStringLiteral("timeZoneAmbiguous"));
            if(instant<=nowUtc)return choice(QStringLiteral("timePast"));
            result.reminderUtc=instant.toUTC();
        }
    } else {result.reminderUtc={};result.time={};}
    result.state=RecognizedTaskTime::State::Ready;
    return result;
}
Result<RecognizedTaskTime> TaskTimeRecognition::manual(const TaskTimeOverride& input,const QDateTime& nowUtc,const QTimeZone& zone) {
    if(!input.date.isValid() || !zone.isValid() || (input.reminder && !input.time.isValid()) || (input.target!=QStringLiteral("planned") && input.target!=QStringLiteral("deadline")) || (input.target==QStringLiteral("deadline") && input.reminder))return Result<RecognizedTaskTime>::failure(ErrorCode::ValidationError,QStringLiteral("timeOverrideInvalid"));
    RecognizedTaskTime result;result.state=RecognizedTaskTime::State::Ready;result.date=input.date;result.time=input.reminder?input.time:QTime();result.target=input.target;
    if(input.reminder) {
        const auto instant=wallClock(input.date,input.time,zone);
        if(!instant.isValid() || instant<=nowUtc)return Result<RecognizedTaskTime>::failure(ErrorCode::ValidationError,QStringLiteral("timeReminderInvalid"));
        result.reminderUtc=instant.toUTC();
    }
    return {result,{}};
}
}
