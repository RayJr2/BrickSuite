#pragma once

#include "../models/PartCategory.h"

#include <QList>
#include <optional>

class QSqlQuery;

class PartCategoryRepository
{
public:
    QList<PartCategory> getAll() const;

    std::optional<PartCategory> getById(int id) const;

    std::optional<PartCategory> getByRebrickableId(int rebrickableId) const;

private:
    PartCategory partCategoryFromQuery(const QSqlQuery& query) const;
};
