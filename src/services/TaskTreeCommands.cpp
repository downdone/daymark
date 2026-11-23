#include "TaskService.h"
#include "TaskCycleService.h"
#include <QHash>
#include <QUuid>

namespace daymark {
namespace {
QList<Task> subtree(const QList<Task>& tasks,const QString& root)
{
    QHash<QString,QList<Task>> children;QHash<QString,Task> byId;
    for(const auto& task:tasks){byId.insert(task.id,task);children[task.parentTaskId].append(task);}
    QList<Task> result;if(!byId.contains(root))return result;
    QList<Task> pending{byId.value(root)};QSet<QString> seen;
    for(qsizetype i=0;i<pending.size();++i){const auto task=pending[i];if(seen.contains(task.id))continue;seen.insert(task.id);result.append(task);pending.append(children.value(task.id));}
    return result;
}
}

Result<Task> TaskService::edit(const QString& id,qint64 version,const TaskPatch& patch)
{
    Result<Task> result;
    const auto transaction=m_repository.atomically([&]() -> Result<void> {
        const auto before=m_repository.find(id);if(!before.ok())return {before.error};
        auto command=patch;
        if(command.cycle && command.cycle->rule.active && !before.value.startOn && command.startOn.kind!=FieldPatch<QDate>::Kind::Set){
            command.startOn=FieldPatch<QDate>::set(m_clock.localToday());
            command.planningState=FieldPatch<QString>::set(QStringLiteral("ACTIVE"));
        }
        if(command.parentTaskId.kind==FieldPatch<QString>::Kind::Set && !command.parentTaskId.value.isEmpty() && command.parentTaskId.value!=before.value.parentTaskId){
            const auto parent=m_repository.find(command.parentTaskId.value);if(!parent.ok())return {parent.error};
            command.projectId=parent.value.projectId.isEmpty()?FieldPatch<QString>::clear():FieldPatch<QString>::set(parent.value.projectId);
            command.areaId=parent.value.areaId.isEmpty()?FieldPatch<QString>::clear():FieldPatch<QString>::set(parent.value.areaId);
            command.headingId=parent.value.headingId.isEmpty()?FieldPatch<QString>::clear():FieldPatch<QString>::set(parent.value.headingId);
        } else if(!before.value.parentTaskId.isEmpty()) {
            const auto destination=[](const QString& current,const FieldPatch<QString>& value){return value.kind==FieldPatch<QString>::Kind::Unspecified?current:value.kind==FieldPatch<QString>::Kind::Clear?QString():value.value;};
            if(destination(before.value.projectId,command.projectId)!=before.value.projectId || destination(before.value.areaId,command.areaId)!=before.value.areaId)command.parentTaskId=FieldPatch<QString>::clear();
        }
        result=editOne(id,version,command);if(!result.ok())return {result.error};
        if(command.cycle){
            if(!m_cycles)return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("cycleInvalid"));
            if(command.convertLegacyCycleVersion){
                if(!command.cycle->rule.active)return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("cycleInvalid"));
                const auto converted=m_cycles->convertLegacy(result.value,*command.convertLegacyCycleVersion);if(!converted.ok())return converted;
                result=m_repository.find(id);if(!result.ok())return {result.error};
            }
            const auto cycle=command.cycle->rule.active?m_cycles->configure(result.value,*command.cycle):m_cycles->stop(result.value,command.cycle->rowVersion);
            if(!cycle.ok())return cycle;
            result=m_repository.find(id);if(!result.ok())return {result.error};
        }else if(m_cycles && result.value.cycle && result.value.cycle->rule.active){
            if(!result.value.parentTaskId.isEmpty())return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("cycleInvalid"));
            if(result.value.startOn!=before.value.startOn || result.value.deadlineOn!=before.value.deadlineOn || result.value.schedule!=before.value.schedule){
                if(before.value.status!=QStringLiteral("OPEN"))return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("cycleWaitingEdit"));
                const auto configured=m_cycles->configure(result.value,*result.value.cycle);if(!configured.ok())return configured;
                result=m_repository.find(id);if(!result.ok())return {result.error};
            }
        }
        if(result.value.projectId!=before.value.projectId || result.value.areaId!=before.value.areaId || result.value.headingId!=before.value.headingId){
            const auto all=m_repository.all();if(!all.ok())return {all.error};const auto nested=subtree(all.value,id);
            for(auto child:nested){if(child.id==id || !child.deletedAtUtc.isEmpty())continue;child.projectId=result.value.projectId;child.areaId=result.value.areaId;child.headingId=result.value.headingId;child.updatedAtUtc=result.value.updatedAtUtc;if(!child.recurrenceId.isEmpty())child.instanceOverridden=true;
                const auto saved=m_repository.update(child,child.rowVersion);if(!saved.ok())return {saved.error};}
            result=m_repository.find(id);if(!result.ok())return {result.error};
        }
        return {};
    });
    if(!transaction.ok())return {{},transaction.error};return result;
}

Result<Task> TaskService::changeState(const QString& id,qint64 version,const QString& action)
{
    Result<Task> result;
    const auto transaction=m_repository.atomically([&]() -> Result<void> {
        const auto root=m_repository.find(id);if(!root.ok())return {root.error};
        if(root.value.rowVersion!=version)return {Error{ErrorCode::Conflict,QStringLiteral("conflict")}};
        if(!root.value.deletedAtUtc.isEmpty() && action!=QStringLiteral("restore"))return {Error{ErrorCode::NotFound,QStringLiteral("notFound")}};
        const bool skipped=action==QStringLiteral("skipOccurrence");
        if(skipped && (!m_cycles || !root.value.cycle || !root.value.cycle->rule.active || root.value.status!=QStringLiteral("OPEN")))
            return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("cycleInvalid"));
        const auto effectiveAction=skipped?QStringLiteral("complete"):action;
        if((effectiveAction==QStringLiteral("complete") && root.value.status==QStringLiteral("COMPLETED")) || (action==QStringLiteral("reopen") && root.value.status==QStringLiteral("OPEN")) || (action==QStringLiteral("restore") && root.value.deletedAtUtc.isEmpty())){result=root;return {};}
        if(m_cycles && root.value.cycle && action==QStringLiteral("reopen")){
            const auto revoked=m_cycles->undo(root.value);if(!revoked.ok())return revoked;
        }
        const auto all=m_repository.all();if(!all.ok())return {all.error};
        const auto group=QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto branch=subtree(all.value,id);
        QStringList members;
        for(const auto& task:branch){
            if(task.id!=id){
                if(action==QStringLiteral("restore")){if(task.deletedAtUtc.isEmpty() || root.value.deletionGroupId.isEmpty() || task.deletionGroupId!=root.value.deletionGroupId)continue;}
                else if(action==QStringLiteral("reopen")){if(!task.deletedAtUtc.isEmpty() || task.status!=QStringLiteral("COMPLETED") || root.value.completionGroupId.isEmpty() || task.completionGroupId!=root.value.completionGroupId)continue;}
                else if(action==QStringLiteral("delete")){if(!task.deletedAtUtc.isEmpty())continue;}
                else if(effectiveAction==QStringLiteral("complete")){if(!task.deletedAtUtc.isEmpty() || task.status!=QStringLiteral("OPEN"))continue;members.append(task.id);}
                else continue;
            }
            const auto saved=changeStateOne(task.id,task.rowVersion,effectiveAction,group);if(!saved.ok())return {saved.error};
        }
        result=m_repository.find(id);if(!result.ok())return {result.error};
        if(m_cycles && root.value.cycle && effectiveAction==QStringLiteral("complete")){
            const auto recorded=m_cycles->complete(root.value,result.value,members,skipped);if(!recorded.ok())return recorded;
            result=m_repository.find(id);
        }
        return {result.error};
    });
    if(!transaction.ok())return {{},transaction.error};return result;
}
} // namespace daymark
