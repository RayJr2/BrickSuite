#pragma once

#include <QDateTime>
#include <QString>

struct InventoryHistoryResult
{
    int movementId = 0;

    QString movementType;
    int quantityChange = 0;

    int fromStorageLocationId = 0;
    QString fromStoragePath;

    int toStorageLocationId = 0;
    QString toStoragePath;

    QString condition;
    QString ownershipType;

    QString referenceType;
    QString referenceId;

    QString notes;

    QDateTime createdUtc;
};
