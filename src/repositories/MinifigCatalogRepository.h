#pragma once

#include "../models/MinifigCatalogItem.h"
#include "../models/MinifigCatalogSearchCriteria.h"
#include "../models/MinifigCatalogSearchResult.h"
#include "../models/MinifigExternalIdentifier.h"

#include <QList>
#include <optional>

class QSqlQuery;

class MinifigCatalogRepository
{
public:
    std::optional<MinifigCatalogItem> getById(int id) const;
    std::optional<MinifigCatalogItem> getByExternalIdentifier(
        const QString& provider,
        const QString& externalId) const;
    QList<MinifigExternalIdentifier> identifiersForMinifig(int minifigCatalogId,
                                                           bool activeOnly = true) const;
    QList<MinifigCatalogSearchResult> search(
        const MinifigCatalogSearchCriteria& criteria) const;
    int count(const MinifigCatalogSearchCriteria& criteria) const;
    int count(bool includeInactive = false) const;

private:
    static MinifigCatalogItem itemFromQuery(const QSqlQuery& query);
};
