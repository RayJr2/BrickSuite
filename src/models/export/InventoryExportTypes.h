#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

struct InventoryExportRow
{
    qint64 inventoryRecordId = 0;
    qint64 storageLocationId = 0;
    int partId = 0;
    int colorId = 0;
    int manufacturerId = 0;
    QString partNumber;
    QString partName;
    QString category;
    QString color;
    int quantity = 0;
    QString storagePath;
    QString manufacturer;
    QString condition;
    QString ownership;
    QStringList legoElementIds;
    QString rebrickablePartId;
    int rebrickableColorId = -1;
    QStringList brickLinkPartIds;
    QString brickLinkColorId;
};

struct InventoryExportFieldDescriptor
{
    QString id;
    QString label;
    QString header;
    bool defaultEnabled = false;
    int defaultOrder = 0;
    bool numeric = false;
};

struct InventoryExportConfiguration
{
    QStringList fieldOrder;
    QSet<QString> enabledFields;
};

struct InventoryExportProjection
{
    QList<InventoryExportFieldDescriptor> fields;
    QStringList headers;
    QList<QStringList> rows;
};
