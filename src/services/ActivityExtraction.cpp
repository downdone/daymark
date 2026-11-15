#include "ActivityExtraction.h"
#include <QRegularExpression>
#include <algorithm>

namespace daymark {
Result<QVariantList> ActivityExtraction::preview(const QString& text,const QDateTime& now,const QTimeZone& zone)
{
    if(text.trimmed().isEmpty() || text.size()>8192)
        return Result<QVariantList>::failure(ErrorCode::ValidationError,QStringLiteral("taskInputInvalid"));
    const auto parts=text.split(QRegularExpression(QStringLiteral("[。；;\\n]+")),Qt::SkipEmptyParts);
    if(parts.size()>32)return Result<QVariantList>::failure(ErrorCode::ValidationError,QStringLiteral("taskInputInvalid"));
    QVariantList candidates;
    static const QRegularExpression range(QStringLiteral(R"((\d{1,2}[:：]\d{2})\s*[—–\-~～至到]\s*(\d{1,2}[:：]\d{2}))"));
    static const QRegularExpression location(QStringLiteral(R"((?:在|地点[:：]\s*)([^，,。；\n]{1,80}?)(?:召开|举行|进行|开会|，|,))"));
    static const QRegularExpression explicitLocation(QStringLiteral(R"(地点[:：]\s*([^，,。；\n]{1,80})(?:$|，|,))"));
    for(const auto& part:parts){
        const auto source=part.trimmed();if(source.isEmpty())continue;
        QString input=source;const auto times=range.match(source);
        if(times.hasMatch())input.replace(times.capturedStart(),times.capturedLength(),times.captured(1));
        const auto recognized=TaskTimeRecognition::parse(input,now,zone,{true,false,{9,0}});
        QVariantMap item{{"title",source.left(512)},{"notes",source},{"source",source},{"selected",true},
            {"startOn",QString()},{"deadlineOn",QString()},{"endOn",QString()},
            {"startTime",QString()},{"endTime",QString()},{"place",QString()},
            {"issue",QString()},{"reminder",false}};
        if(recognized.state==RecognizedTaskTime::State::Choice)item["issue"]=QStringLiteral("时间需要确认，未自动安排");
        else if(recognized.state==RecognizedTaskTime::State::Ready){
            item[recognized.target==QStringLiteral("deadline")?"deadlineOn":"startOn"]=recognized.date.toString(Qt::ISODate);
            if(!times.hasMatch() && recognized.time.isValid() && recognized.target!=QStringLiteral("deadline")){
                item["startTime"]=recognized.time.toString(QStringLiteral("HH:mm"));
                item["issue"]=QStringLiteral("已识别开始时刻，请补充结束时刻或清除时间安排");
            }
            if(times.hasMatch() && recognized.target!=QStringLiteral("deadline")){
                const auto endParts=QString(times.captured(2)).replace(QChar(0xff1a),QLatin1Char(':')).split(QLatin1Char(':'));
                const QTime end(endParts[0].toInt(),endParts[1].toInt());
                const auto begin=QDateTime(recognized.date,recognized.time,zone,QDateTime::TransitionResolution::Reject);
                const auto finish=QDateTime(recognized.date,end,zone,QDateTime::TransitionResolution::Reject);
                if(begin.isValid() && finish.isValid() && finish>begin){
                    item["startTime"]=recognized.time.toString(QStringLiteral("HH:mm"));item["endTime"]=end.toString(QStringLiteral("HH:mm"));item["endOn"]=recognized.date.toString(Qt::ISODate);
                }else item["issue"]=QStringLiteral("时段结束不明确，请手动确认");
            }
        }
        auto place=location.match(source);if(!place.hasMatch())place=explicitLocation.match(source);
        if(place.hasMatch()){
            bool overlapsDate=false;
            for(const auto& span:recognized.spans)overlapsDate|=span.first<place.capturedEnd(1) && span.first+span.second>place.capturedStart(1);
            if(!overlapsDate)item["place"]=place.captured(1).trimmed();
        }
        if(recognized.state==RecognizedTaskTime::State::Ready){
            QString title=input;auto spans=recognized.spans;
            std::sort(spans.begin(),spans.end(),[](const auto& a,const auto& b){return a.first>b.first;});
            for(const auto& span:spans)title.remove(span.first,span.second);
            const auto titlePlace=location.match(title);
            if(titlePlace.hasMatch())title.remove(titlePlace.capturedStart(),titlePlace.capturedEnd(1)-titlePlace.capturedStart());
            title.remove(QRegularExpression(QStringLiteral(R"(^[\s，,：:]*(?:请)?(?:在)?(?:截止)?(?:之前|前)?[\s，,：:]*)")));
            if(!title.trimmed().isEmpty()){
                const auto points=title.trimmed().toUcs4();
                item["title"]=QString::fromUcs4(points.constData(),qMin(qsizetype(512),points.size()));
            }
        }
        candidates.append(item);
    }
    return {candidates,{}};
}
}
