#pragma once

#include "../models/SetCatalogItem.h"
#include "../models/SetCatalogSearchCriteria.h"

#include <QList>
#include <optional>

class QSqlQuery;

class SetCatalogRepository
{
public:
    std::optional<SetCatalogItem> getById(int id) const;

    std::optional<SetCatalogItem> getBySetNumber(const QString& setNumber) const;

    QList<SetCatalogItem> search(const SetCatalogSearchCriteria& criteria) const;

    QList<int> getYears() const;

    int count() const;

private:
    SetCatalogItem setFromQuery(const QSqlQuery& query) const;
};
