#pragma once
#include "persistence/CatalogRepository.h"
namespace daymark {
class CatalogService final {
public:
    CatalogService(CatalogRepository& repository,const IClock& clock):m_repository(repository),m_clock(clock){}
    Result<CatalogItem> save(CatalogItem item);
    Result<void> remove(const QString& kind,const QString& id,qint64 version);
    Result<void> validateTask(Task& task) const;
private:
    CatalogRepository& m_repository;
    const IClock& m_clock;
};
}
