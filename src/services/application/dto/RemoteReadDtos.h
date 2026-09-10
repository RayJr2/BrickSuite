#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

namespace RemoteReadDto {

constexpr int MaximumPageSize = 500;
constexpr int MaximumStorageLocations = 10000;
constexpr int MaximumTextLength = 512;

struct PageRequest { int page = 1; int pageSize = 250; };
template <typename T> struct Page {
    QList<T> rows; int page = 1; int pageSize = 250; int totalRows = 0;
    bool resourceFound = true; // Host scope result; never serialized as data.
};

struct WorkspaceSummary { qint64 workspaceId = 0; QString name; };
struct StorageSummary {
    qint64 storageId = 0; qint64 parentStorageId = 0; QString name;
    QString displayPath; QString typeName; int sortOrder = 0;
    bool active = true; bool allowsInventory = true; bool allowsCollection = false;
};

struct InventorySearchRequest {
    qint64 workspaceId = 0; QString text; qint64 storageId = 0;
    int rebrickableCategoryId = -1; int rebrickableColorId = -1; PageRequest paging;
};
struct InventoryRow {
    qint64 inventoryRecordId = 0; qint64 workspaceId = 0;
    QString partNumber; QString partNameFallback; int rebrickableCategoryId = -1;
    int rebrickableColorId = -1; QString colorNameFallback;
    int quantity = 0; qint64 storageId = 0; QString storagePath;
    QString manufacturerDisplay; QString condition; QString ownershipType;
};
struct InventoryDetail : InventoryRow { QDateTime createdUtc; QDateTime modifiedUtc; };
struct InventoryHistoryRow {
    qint64 movementId = 0; QString movementType; int quantityChange = 0;
    qint64 fromStorageId = 0; QString fromStoragePath;
    qint64 toStorageId = 0; QString toStoragePath;
    QString condition; QString ownershipType; QString referenceType;
    QString referenceId; QString notes; QDateTime createdUtc;
};

struct BuildSummary {
    qint64 buildId = 0; qint64 workspaceId = 0; QString buildType;
    QString name; QString setNumber; QString minifigNumber;
    QString inventoryMode; QString manufacturerDisplay; QString status;
    QString notes; bool active = true;
};
using BuildDetail = BuildSummary;
struct BuildRequirement {
    qint64 requirementId = 0; qint64 buildId = 0;
    QString partNumber; QString partNameFallback;
    int rebrickableColorId = -1; QString colorNameFallback;
      QString substitutePartNumber; int substituteRebrickableColorId = -1;
      int quantityRequired = 0; int quantityPulled = 0; bool spare = false;
      int owned = -1; int thisRequirementAllocated = -1; int otherAllocated = -1;
      int available = -1; int missing = -1;
};
struct MissingPart {
    QString partNumber; QString partNameFallback; int rebrickableColorId = -1;
    QString colorNameFallback; int required = 0; int pulled = 0;
    int remaining = 0; int owned = 0; int thisBuildAllocated = 0;
    int otherBuildsAllocated = 0; int available = 0; int missing = 0;
};
struct PullingRow {
    qint64 requirementId = 0; qint64 allocationId = 0; qint64 inventoryRecordId = 0;
    qint64 storageId = 0; QString storagePath; QString partNumber;
    QString partNameFallback; int rebrickableColorId = -1; QString colorNameFallback;
      int quantityRequired = 0; int quantityPulled = 0; int quantityAllocated = 0;
      int inventoryQuantity = 0;
    bool substitution = false;
};

struct CollectionSearchRequest {
    qint64 workspaceId = 0; QString text; QString type; QString state;
    QString condition; QString completeness; qint64 storageId = 0;
    int activeState = 1; PageRequest paging{1, 100};
};
struct CollectionSummary {
    qint64 collectionItemId = 0; qint64 workspaceId = 0; QString type;
    QString setNumber; QString minifigNumber; QString referenceFallback; QString titleFallback;
    QString state; QString condition; QString completeness; qint64 storageId = 0;
    QString storagePath; QString nickname; bool active = true;
};
struct CollectionDetail : CollectionSummary { QString notes; QDateTime createdUtc; QDateTime modifiedUtc; };

struct PartReferenceCustomization {
    qint64 customizationId = 0; QString partNumber; QString partNameFallback;
    QString catalog; QString section; int displayOrder = 0;
    QString representativeFor; QString notes;
};

} // namespace RemoteReadDto
