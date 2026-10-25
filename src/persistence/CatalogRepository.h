#pragma once
#include "domain/Task.h"
#include <QSqlDatabase>
namespace daymark {
class CatalogRepository final {
public:
    explicit CatalogRepository(QSqlDatabase db):m_db(std::move(db)){}
    Result<QList<CatalogItem>> all() const;
    Result<CatalogItem> find(const QString& kind,const QString& id) const;
    Result<CatalogItem> save(CatalogItem item,const QString& now);
    Result<void> remove(const QString& kind,const QString& id,qint64 version,const QString& now);
    Result<void> relocate(const CatalogItem& item,const QString& targetId,qint64 targetVersion,const QString& placement,const QString& now);
    Result<void> move(const QString& kind,const QString& id,qint64 version,int direction,const QString& now);
private:
    QSqlDatabase m_db;
};
}
