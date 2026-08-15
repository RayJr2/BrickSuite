#pragma once

#include <QDateTime>
#include <QString>

struct LostInventoryItem
{
    int workspaceId = 0;

    int partId = 0;
    int colorId = 0;

    QString partNumber;
    QString partName;
    QString colorName;

    int outstandingQuantity = 0;

    int lastStorageLocationId = 0;

    QString condition;
    QString ownershipType;

    QDateTime lastLostUtc;
};