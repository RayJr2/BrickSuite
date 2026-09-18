#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtGlobal>

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
    QStringList pickABrickElementCandidates;
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

struct PickABrickExportSourceRow
{
    MissingPartsExportRow source;
    bool included = true;
    QStringList elementCandidates;
    QString selectedElementId;
    QString overridePartNumber;
    QString overridePartName;
    enum class OverrideState { None, Resolving, Ready, PartNotFound, NoElement, Unavailable, Failed };
    OverrideState overrideState = OverrideState::None;
    quint64 resolutionGeneration = 0;

    bool hasPartOverride() const { return !overridePartNumber.isEmpty(); }
};

struct PickABrickPartResolution
{
    bool serviceAvailable = true;
    bool partFound = false;
    QString partNumber;
    QString partName;
    QStringList elementCandidates;
};

struct PickABrickExportRow
{
    QString elementId;
    qint64 quantity = 0;
};

struct PickABrickExportProjection
{
    QList<PickABrickExportRow> rows;
    int includedSourceRows = 0;
    int unresolvedIncludedRows = 0;
    int excludedSourceRows = 0;
    qint64 includedPieces = 0;
    qint64 excludedPieces = 0;
    QString error;

    bool ready() const
    {
        return error.isEmpty() && includedSourceRows > 0 && unresolvedIncludedRows == 0;
    }
};
