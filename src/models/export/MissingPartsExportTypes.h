#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

enum class MissingPartsExportField
{
    Build,
    BuildReference,
    PartNumber,
    PartName,
    Category,
    Color,
    QuantityMissing,
    Manufacturer,
    LegoElementId,
    RebrickablePartId,
    RebrickableColorId,
    BrickLinkPartId,
    BrickLinkColorId,
    Required,
    Pulled,
    Remaining,
    Available
};

struct MissingPartsExportFieldDescriptor
{
    MissingPartsExportField field;
    QString id;
    QString displayName;
    QString csvHeader;
    bool defaultEnabled = false;
    int defaultOrder = 0;
    bool ambiguous = false;
    bool numeric = false;
};

struct MissingPartsExportRow
{
    QString buildName;
    QString buildReference;
    int partId = 0;
    int colorId = 0;
    QString partNumber;
    QString partName;
    QString category;
    QString colorName;
    int missing = 0;
    QString manufacturer;
    QStringList legoElementIds;
    QString rebrickablePartId;
    int rebrickableColorId = -1;
    QStringList brickLinkPartIds;
    QString brickLinkColorId;
    int required = 0;
    int pulled = 0;
    int remaining = 0;
    int available = 0;
};

struct MissingPartsExportConfiguration
{
    QStringList fieldOrder;
    QSet<QString> enabledFields;
};

struct MissingPartsExportProjection
{
    QList<MissingPartsExportFieldDescriptor> fields;
    QStringList headers;
    QList<QStringList> rows;
};
