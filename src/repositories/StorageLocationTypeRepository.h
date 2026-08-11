#pragma once

#include "../models/StorageLocationType.h"

#include <QList>
#include <optional>

class QSqlQuery;

class StorageLocationTypeRepository
{
public:
    QList<StorageLocationType> getAll() const;

    QList<StorageLocationType> getActive() const;

    std::optional<StorageLocationType> getById(int id) const;

private:
    StorageLocationType locationTypeFromQuery(const QSqlQuery& query) const;
};
