#pragma once

#include "../models/CollectionItem.h"
#include "../models/CollectionSearchCriteria.h"
#include "../models/CollectionSearchResult.h"

#include <QList>
#include <optional>

class QSqlQuery;

class CollectionRepository
{
public:
    bool create(CollectionItem& item);
    std::optional<CollectionItem> getById(int id) const;
    std::optional<CollectionItem> getBySourceBuild(int buildId) const;
    bool hasSourceBuild(int buildId) const;
    QList<CollectionSearchResult> search(const CollectionSearchCriteria& criteria) const;
    int count(const CollectionSearchCriteria& criteria) const;
    bool update(CollectionItem& item);
    bool setActive(int itemId, bool active);

private:
    static CollectionItem itemFromQuery(const QSqlQuery& query);
};
