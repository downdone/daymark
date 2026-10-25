#include "CatalogService.h"
#include <QSet>
namespace daymark {
Result<void> CatalogService::validate(CatalogItem& item) const
{
    item.title=item.title.trimmed();
    if(item.title.isEmpty() || item.title.toUcs4().size()>512)return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("titleInvalid"));
    if(item.kind==QStringLiteral("project")) {
        if((item.startOn && !item.startOn->isValid()) || (item.deadlineOn && !item.deadlineOn->isValid()))return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("dateInvalid"));
        if(item.startOn && item.deadlineOn && *item.startOn>*item.deadlineOn)return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("dateOrder"));
        if(item.planningState!=QStringLiteral("ACTIVE") && item.planningState!=QStringLiteral("SOMEDAY"))return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("planningInvalid"));
        if(!QStringList{QStringLiteral("OPEN"),QStringLiteral("COMPLETED"),QStringLiteral("CANCELLED")}.contains(item.status))return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("catalogInvalid"));
        if(!item.parentId.isEmpty()) {
            if(item.parentId==item.id)return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("categoryCycle"));
            const auto parent=m_repository.find(QStringLiteral("project"),item.parentId);
            if(parent.ok()) {
                if(!parent.value.parentId.isEmpty())return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("categoryDepth"));
                const auto all=m_repository.all();if(!all.ok())return {all.error};
                for(const auto& child:all.value)if(child.kind==QStringLiteral("project") && child.parentId==item.id && !item.id.isEmpty())return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("categoryHasChildren"));
            } else if(!m_repository.find(QStringLiteral("area"),item.parentId).ok())return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("ownershipInvalid"));
        }
    } else if(item.kind==QStringLiteral("heading")) {
        if(!m_repository.find(QStringLiteral("project"),item.parentId).ok())return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("ownershipInvalid"));
        if(!item.id.isEmpty()) {
            const auto current=m_repository.find(item.kind,item.id);
            if(!current.ok())return {current.error};
            if(current.value.parentId!=item.parentId)return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("ownershipInvalid"));
        }
    } else if(item.kind!=QStringLiteral("area") && item.kind!=QStringLiteral("tag"))return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("catalogInvalid"));
    if(!QStringList{QStringLiteral("neutral"),QStringLiteral("blue"),QStringLiteral("green"),QStringLiteral("orange"),QStringLiteral("purple")}.contains(item.colorKey))return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("catalogInvalid"));
    return {};
}
Result<CatalogItem> CatalogService::save(CatalogItem item)
{
    const auto valid=validate(item);if(!valid.ok())return {{},valid.error};
    return m_repository.save(item,m_clock.nowUtc().toUTC().toString(Qt::ISODateWithMs));
}
Result<void> CatalogService::drop(const QString& id,qint64 version,const QString& targetId,qint64 targetVersion,const QString& placement)
{
    if(id==targetId || !QStringList{QStringLiteral("before"),QStringLiteral("after"),QStringLiteral("inside")}.contains(placement))return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("orderInvalid"));
    auto source=m_repository.find(QStringLiteral("project"),id);const auto target=m_repository.find(QStringLiteral("project"),targetId);
    if(!source.ok())return {source.error};if(!target.ok())return {target.error};
    if(source.value.rowVersion!=version || target.value.rowVersion!=targetVersion)return Result<void>::failure(ErrorCode::Conflict,QStringLiteral("conflict"));
    if(placement==QStringLiteral("inside") && !target.value.parentId.isEmpty())return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("categoryDepth"));
    source.value.parentId=placement==QStringLiteral("inside")?targetId:target.value.parentId;
    const auto valid=validate(source.value);if(!valid.ok())return valid;
    return m_repository.relocate(source.value,targetId,targetVersion,placement,m_clock.nowUtc().toUTC().toString(Qt::ISODateWithMs));
}
Result<void> CatalogService::remove(const QString& kind,const QString& id,qint64 version)
{return m_repository.remove(kind,id,version,m_clock.nowUtc().toUTC().toString(Qt::ISODateWithMs));}
Result<void> CatalogService::validateTask(Task& task) const
{
    const auto invalid=[] {return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("ownershipInvalid"));};
    if(!task.projectId.isEmpty() && !task.areaId.isEmpty())return invalid();
    if(!task.projectId.isEmpty()) {
        const auto project=m_repository.find(QStringLiteral("project"),task.projectId);if(!project.ok())return invalid();
        task.projectPlanning=project.value.planningState;task.projectStatus=project.value.status;task.projectName=project.value.title;task.projectAreaId=project.value.parentId;task.projectDeleted=project.value.archived;
    } else {task.projectPlanning.clear();task.projectStatus.clear();task.projectName.clear();task.projectAreaId.clear();task.projectDeleted=false;}
    if(!task.areaId.isEmpty() && !m_repository.find(QStringLiteral("area"),task.areaId).ok())return invalid();
    const auto effectiveArea=task.areaId.isEmpty()?task.projectAreaId:task.areaId;
    task.areaArchived=false;task.areaName.clear();
    if(!effectiveArea.isEmpty()) {
        const auto category=m_repository.find(QStringLiteral("project"),effectiveArea);
        if(category.ok()){task.areaArchived=category.value.archived || category.value.status!=QStringLiteral("OPEN") || category.value.planningState==QStringLiteral("SOMEDAY");task.areaName=category.value.title;}
        else {const auto area=m_repository.find(QStringLiteral("area"),effectiveArea);if(!area.ok())return invalid();task.areaArchived=area.value.archived;task.areaName=area.value.title;}
    }
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
