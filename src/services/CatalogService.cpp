#include "CatalogService.h"
#include <QSet>
namespace daymark {
Result<CatalogItem> CatalogService::save(CatalogItem item)
{
    item.title=item.title.trimmed();
    if(item.title.isEmpty() || item.title.toUcs4().size()>512)return Result<CatalogItem>::failure(ErrorCode::ValidationError,QStringLiteral("titleInvalid"));
    if(item.kind==QStringLiteral("project")) {
        if((item.startOn && !item.startOn->isValid()) || (item.deadlineOn && !item.deadlineOn->isValid()))return Result<CatalogItem>::failure(ErrorCode::ValidationError,QStringLiteral("dateInvalid"));
        if(item.startOn && item.deadlineOn && *item.startOn>*item.deadlineOn)return Result<CatalogItem>::failure(ErrorCode::ValidationError,QStringLiteral("dateOrder"));
        if(item.planningState!=QStringLiteral("ACTIVE") && item.planningState!=QStringLiteral("SOMEDAY"))return Result<CatalogItem>::failure(ErrorCode::ValidationError,QStringLiteral("planningInvalid"));
        if(!QStringList{QStringLiteral("OPEN"),QStringLiteral("COMPLETED"),QStringLiteral("CANCELLED")}.contains(item.status))return Result<CatalogItem>::failure(ErrorCode::ValidationError,QStringLiteral("catalogInvalid"));
        if(!item.parentId.isEmpty() && !m_repository.find(QStringLiteral("area"),item.parentId).ok())return Result<CatalogItem>::failure(ErrorCode::ValidationError,QStringLiteral("ownershipInvalid"));
    } else if(item.kind==QStringLiteral("heading")) {
        if(!m_repository.find(QStringLiteral("project"),item.parentId).ok())return Result<CatalogItem>::failure(ErrorCode::ValidationError,QStringLiteral("ownershipInvalid"));
        if(!item.id.isEmpty()) {
            const auto current=m_repository.find(item.kind,item.id);
            if(!current.ok())return current;
            if(current.value.parentId!=item.parentId)return Result<CatalogItem>::failure(ErrorCode::ValidationError,QStringLiteral("ownershipInvalid"));
        }
    } else if(item.kind!=QStringLiteral("area") && item.kind!=QStringLiteral("tag"))return Result<CatalogItem>::failure(ErrorCode::ValidationError,QStringLiteral("catalogInvalid"));
    if(!QStringList{QStringLiteral("neutral"),QStringLiteral("blue"),QStringLiteral("green"),QStringLiteral("orange"),QStringLiteral("purple")}.contains(item.colorKey))return Result<CatalogItem>::failure(ErrorCode::ValidationError,QStringLiteral("catalogInvalid"));
    return m_repository.save(item,m_clock.nowUtc().toUTC().toString(Qt::ISODateWithMs));
}
Result<void> CatalogService::remove(const QString& kind,const QString& id,qint64 version)
{return m_repository.remove(kind,id,version,m_clock.nowUtc().toUTC().toString(Qt::ISODateWithMs));}
Result<void> CatalogService::validateTask(Task& task) const
{
    const auto invalid=[] {return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("ownershipInvalid"));};
    if(!task.projectId.isEmpty() && !task.areaId.isEmpty())return invalid();
    if(!task.projectId.isEmpty()) {
        const auto project=m_repository.find(QStringLiteral("project"),task.projectId);if(!project.ok())return invalid();
        task.projectPlanning=project.value.planningState;task.projectStatus=project.value.status;task.projectName=project.value.title;task.projectAreaId=project.value.parentId;task.projectDeleted=false;
    } else {task.projectPlanning.clear();task.projectStatus.clear();task.projectName.clear();task.projectAreaId.clear();task.projectDeleted=false;}
    if(!task.areaId.isEmpty() && !m_repository.find(QStringLiteral("area"),task.areaId).ok())return invalid();
    if(!task.headingId.isEmpty()) {
        const auto heading=m_repository.find(QStringLiteral("heading"),task.headingId);
        if(!heading.ok() || heading.value.parentId!=task.projectId)return invalid();
    }
    QSet<QString> tags;
    for(const auto& id:task.tagIds) {
        if(tags.contains(id) || !m_repository.find(QStringLiteral("tag"),id).ok())return invalid();tags.insert(id);
    }
    return {};
}
}
