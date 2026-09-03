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
    bool tryGetBySourceBuild(int buildId, std::optional<CollectionItem>& item) const;
    bool hasSourceBuild(int buildId) const;
    QList<CollectionSearchResult> search(const CollectionSearchCriteria& criteria) const;
    std::optional<CollectionSearchResult> displayById(int id) const;
    int count(const CollectionSearchCriteria& criteria) const;
    bool update(CollectionItem& item);
    bool updateStateForSourceBuild(int buildId, CollectionItemState state);
    bool setActive(int itemId, bool active);

private:
    static CollectionItem itemFromQuery(const QSqlQuery& query);
};
