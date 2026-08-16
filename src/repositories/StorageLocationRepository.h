#pragma once

#include "../models/StorageLocation.h"

#include <QList>
#include <optional>

class QSqlQuery;

class StorageLocationRepository
{
public:
    bool create(StorageLocation& location);

    QList<StorageLocation> getByWorkspace(int workspaceId) const;

    QList<StorageLocation> getChildren(int workspaceId, int parentLocationId) const;

    std::optional<StorageLocation> getById(int id) const;

    bool update(StorageLocation& location);

    bool hasChildren(int locationId) const;

    bool hasInventory(int locationId) const;

    bool isDescendant(int locationId, int possibleDescendantId) const;

    bool deactivate(int locationId);

private:
    StorageLocation locationFromQuery(const QSqlQuery& query) const;
};
