#include "CatalogRepository.h"
#include <QSqlQuery>
#include <QVariant>
#include <QUuid>

namespace daymark {
namespace {
QString tableFor(const QString& kind)
{
    if(kind==QStringLiteral("area"))return QStringLiteral("areas");
    if(kind==QStringLiteral("project"))return QStringLiteral("projects");
    if(kind==QStringLiteral("heading"))return QStringLiteral("headings");
    if(kind==QStringLiteral("tag"))return QStringLiteral("tags");
    return {};
}
}
Result<QList<CatalogItem>> CatalogRepository::all() const
{
    QList<CatalogItem> items;
    for(const auto& kind:{QStringLiteral("area"),QStringLiteral("project"),QStringLiteral("heading"),QStringLiteral("tag")}) {
        const auto name=(kind==QStringLiteral("area") || kind==QStringLiteral("tag"))?QStringLiteral("name"):QStringLiteral("title");
        const auto parent=kind==QStringLiteral("project")?QStringLiteral("COALESCE(parent_id,area_id)"):kind==QStringLiteral("heading")?QStringLiteral("project_id"):QStringLiteral("NULL");
        const auto planning=kind==QStringLiteral("project")?QStringLiteral("planning_state"):QStringLiteral("'ACTIVE'");
        const auto status=kind==QStringLiteral("project")?QStringLiteral("status"):QStringLiteral("'OPEN'");
        const auto extra=(kind==QStringLiteral("project")?QStringLiteral("description,'neutral',start_on,deadline_on"):(kind==QStringLiteral("area") || kind==QStringLiteral("tag"))?QStringLiteral("'',color_key,NULL,NULL"):QStringLiteral("'','neutral',NULL,NULL"))+((kind==QStringLiteral("area") || kind==QStringLiteral("project"))?QStringLiteral(",archived"):QStringLiteral(",0"));
        QSqlQuery q(m_db);
        if(!q.exec(QStringLiteral("SELECT id,%1,%2,%3,%4,row_version,%6 FROM %5 WHERE deleted_at_utc IS NULL ORDER BY sort_order,id").arg(name,parent,planning,status,tableFor(kind),extra)))
            return Result<QList<CatalogItem>>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
        while(q.next()) {
            CatalogItem item;item.id=q.value(0).toString();item.kind=kind;item.title=q.value(1).toString();item.parentId=q.value(2).toString();item.planningState=q.value(3).toString();item.status=q.value(4).toString();item.rowVersion=q.value(5).toLongLong();
            item.description=q.value(6).toString();item.colorKey=q.value(7).toString();
            if(!q.value(8).isNull())item.startOn=QDate::fromString(q.value(8).toString(),Qt::ISODate);
            if(!q.value(9).isNull())item.deadlineOn=QDate::fromString(q.value(9).toString(),Qt::ISODate);
            item.archived=q.value(10).toBool();
            items.append(item);
        }
    }
    return {items,{}};
}
Result<CatalogItem> CatalogRepository::find(const QString& kind,const QString& id) const
{
    const auto items=all();if(!items.ok())return {{},items.error};
    for(const auto& item:items.value)if(item.id==id && item.kind==kind)return {item,{}};
    return Result<CatalogItem>::failure(ErrorCode::NotFound,QStringLiteral("notFound"));
}
Result<void> CatalogRepository::move(const QString& kind,const QString& id,qint64 version,int direction,const QString& now)
{
    const auto table=tableFor(kind);
    if(table.isEmpty() || (direction!=-1 && direction!=1))return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("orderInvalid"));
    const auto current=find(kind,id);if(!current.ok())return {current.error};
    if(current.value.rowVersion!=version)return Result<void>::failure(ErrorCode::Conflict,QStringLiteral("conflict"));
    const auto allItems=all();if(!allItems.ok())return {allItems.error};
    QList<CatalogItem> siblings;int index=-1;
    for(const auto& item:allItems.value)if(item.kind==kind && item.parentId==current.value.parentId){if(item.id==id)index=siblings.size();siblings.append(item);}
    if(index+direction<0 || index+direction>=siblings.size())return {};
    siblings.swapItemsAt(index,index+direction);
    if(!m_db.transaction())return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    int order=0;
    for(const auto& item:siblings) {
        QSqlQuery query(m_db);query.prepare(QStringLiteral("UPDATE %1 SET sort_order=?,updated_at_utc=?,row_version=row_version+1 WHERE id=? AND row_version=? AND deleted_at_utc IS NULL").arg(table));
        query.addBindValue(order++);query.addBindValue(now);query.addBindValue(item.id);query.addBindValue(item.rowVersion);
        if(!query.exec() || query.numRowsAffected()!=1){m_db.rollback();return Result<void>::failure(ErrorCode::Conflict,QStringLiteral("conflict"));}
    }
    if(!m_db.commit()){m_db.rollback();return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));}return {};
}
Result<CatalogItem> CatalogRepository::save(CatalogItem item,const QString& now)
{
    const auto table=tableFor(item.kind);if(table.isEmpty())return Result<CatalogItem>::failure(ErrorCode::ValidationError,QStringLiteral("catalogInvalid"));
    const bool create=item.id.isEmpty();if(create)item.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    const auto titleColumn=(item.kind==QStringLiteral("area") || item.kind==QStringLiteral("tag"))?QStringLiteral("name"):QStringLiteral("title");
    QStringList columns{titleColumn,QStringLiteral("updated_at_utc")};QList<QVariant> values{item.title,now};
    if(item.kind==QStringLiteral("tag")){columns<<QStringLiteral("normalized_name");values<<item.title.toCaseFolded();}
    if(item.kind==QStringLiteral("area") || item.kind==QStringLiteral("tag")){columns<<QStringLiteral("color_key");values<<item.colorKey;}
    if(item.kind==QStringLiteral("area") || item.kind==QStringLiteral("project")){columns<<QStringLiteral("archived");values<<item.archived;}
    if(item.kind==QStringLiteral("project")) {
        const bool categoryParent=!item.parentId.isEmpty() && find(QStringLiteral("project"),item.parentId).ok();
        columns<<QStringLiteral("parent_id");values<<(categoryParent?QVariant(item.parentId):QVariant());
        columns<<QStringLiteral("area_id")<<QStringLiteral("planning_state")<<QStringLiteral("status")<<QStringLiteral("completed_at_utc");
        values<<(item.parentId.isEmpty() || categoryParent?QVariant():QVariant(item.parentId))<<item.planningState<<item.status<<(item.status==QStringLiteral("OPEN")?QVariant():QVariant(now));
        columns<<QStringLiteral("description")<<QStringLiteral("start_on")<<QStringLiteral("deadline_on");
        values<<(item.description.isNull()?QStringLiteral(""):item.description)<<(item.startOn?QVariant(item.startOn->toString(Qt::ISODate)):QVariant())<<(item.deadlineOn?QVariant(item.deadlineOn->toString(Qt::ISODate)):QVariant());
    }
    if(item.kind==QStringLiteral("heading")){columns<<QStringLiteral("project_id");values<<item.parentId;}
    QSqlQuery query(m_db);
    if(create) {
        columns<<QStringLiteral("id")<<QStringLiteral("created_at_utc");values<<item.id<<now;
        QStringList placeholders;for(int i=0;i<columns.size();++i)placeholders<<QStringLiteral("?");
        query.prepare(QStringLiteral("INSERT INTO %1(%2) VALUES(%3)").arg(table,columns.join(','),placeholders.join(',')));
    } else {
        QStringList assignments;for(const auto& c:columns)assignments<<c+QStringLiteral("=?");
        query.prepare(QStringLiteral("UPDATE %1 SET %2,row_version=row_version+1 WHERE id=? AND row_version=? AND deleted_at_utc IS NULL").arg(table,assignments.join(',')));
        values<<item.id<<item.rowVersion;
    }
    for(const auto& value:values)query.addBindValue(value);
    if(!query.exec())return Result<CatalogItem>::failure(ErrorCode::PersistenceError,item.kind==QStringLiteral("tag")?QStringLiteral("tagConflict"):QStringLiteral("databaseWrite"));
    if(query.numRowsAffected()!=1)return Result<CatalogItem>::failure(ErrorCode::Conflict,QStringLiteral("conflict"));
    return find(item.kind,item.id);
}
Result<void> CatalogRepository::relocate(const CatalogItem& item,const QString& targetId,qint64 targetVersion,const QString& placement,const QString& now)
{
    if(!m_db.transaction())return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    auto rollback=[&](const Error& error){m_db.rollback();return Result<void>{error};};
    const auto target=find(QStringLiteral("project"),targetId);if(!target.ok())return rollback(target.error);
    if(target.value.rowVersion!=targetVersion)return rollback({ErrorCode::Conflict,QStringLiteral("conflict")});
    const auto saved=save(item,now);if(!saved.ok())return rollback(saved.error);
    const auto entries=all();if(!entries.ok())return rollback(entries.error);
    QList<CatalogItem> siblings;int insertion=-1;
    for(const auto& current:entries.value)if(current.kind==QStringLiteral("project") && current.parentId==item.parentId && current.id!=item.id){if(current.id==targetId)insertion=siblings.size();siblings.append(current);}
    if(placement==QStringLiteral("inside"))insertion=siblings.size();else if(insertion<0)return rollback({ErrorCode::ValidationError,QStringLiteral("orderInvalid")});else if(placement==QStringLiteral("after"))++insertion;
    siblings.insert(insertion,saved.value);int order=0;
    for(const auto& current:siblings){QSqlQuery query(m_db);query.prepare(QStringLiteral("UPDATE projects SET sort_order=?,updated_at_utc=?,row_version=row_version+1 WHERE id=? AND row_version=? AND deleted_at_utc IS NULL"));query.addBindValue(order++);query.addBindValue(now);query.addBindValue(current.id);query.addBindValue(current.rowVersion);
        if(!query.exec())return rollback({ErrorCode::PersistenceError,QStringLiteral("databaseWrite")});if(query.numRowsAffected()!=1)return rollback({ErrorCode::Conflict,QStringLiteral("conflict")});}
    if(!m_db.commit())return rollback({ErrorCode::PersistenceError,QStringLiteral("databaseWrite")});return {};
}
Result<void> CatalogRepository::remove(const QString& kind,const QString& id,qint64 version,const QString& now)
{
    const auto table=tableFor(kind);if(table.isEmpty())return Result<void>::failure(ErrorCode::ValidationError,QStringLiteral("catalogInvalid"));
    if(!m_db.transaction())return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    auto rollback=[&](ErrorCode code,const QString& key){m_db.rollback();return Result<void>::failure(code,key);};
    QSqlQuery query(m_db);query.prepare(QStringLiteral("UPDATE %1 SET deleted_at_utc=?,updated_at_utc=?,row_version=row_version+1 WHERE id=? AND row_version=? AND deleted_at_utc IS NULL").arg(table));
    query.addBindValue(now);query.addBindValue(now);query.addBindValue(id);query.addBindValue(version);
    if(!query.exec())return rollback(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    if(query.numRowsAffected()!=1)return rollback(ErrorCode::Conflict,QStringLiteral("conflict"));
    QStringList statements;
    if(kind==QStringLiteral("project")) {
        statements<<QStringLiteral("UPDATE projects SET parent_id=NULL,updated_at_utc=?,row_version=row_version+1 WHERE parent_id=?")
                  <<QStringLiteral("UPDATE tasks SET project_id=NULL,heading_id=NULL,planning_state='ACTIVE',updated_at_utc=?,row_version=row_version+1 WHERE project_id=?")
                  <<QStringLiteral("UPDATE headings SET deleted_at_utc=?,updated_at_utc=?,row_version=row_version+1 WHERE project_id=? AND deleted_at_utc IS NULL");
    } else if(kind==QStringLiteral("area")) {
        statements<<QStringLiteral("UPDATE projects SET area_id=NULL,updated_at_utc=?,row_version=row_version+1 WHERE area_id=?")
                  <<QStringLiteral("UPDATE tasks SET direct_area_id=NULL,updated_at_utc=?,row_version=row_version+1 WHERE direct_area_id=?");
    } else if(kind==QStringLiteral("heading"))statements<<QStringLiteral("UPDATE tasks SET heading_id=NULL,updated_at_utc=?,row_version=row_version+1 WHERE heading_id=?");
    else if(kind==QStringLiteral("tag")) {
        query.prepare(QStringLiteral("DELETE FROM task_tags WHERE tag_id=?"));query.addBindValue(id);
        if(!query.exec())return rollback(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    }
    for(const auto& sql:statements) {
        query.prepare(sql);query.addBindValue(now);if(sql.contains(QStringLiteral("SET deleted_at_utc")))query.addBindValue(now);query.addBindValue(id);
        if(!query.exec())return rollback(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    }
    if(!m_db.commit())return rollback(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
    return {};
}
}
