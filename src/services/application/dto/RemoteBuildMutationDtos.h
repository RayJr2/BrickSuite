#pragma once

#include "RemoteMutationDtos.h"

#include <QList>
#include <QJsonArray>

namespace RemoteBuildMutationDto {

struct ExpectedState {
    QString modifiedUtc;
    QString buildType;
    QString reference;
    QString inventoryMode;
    QString manufacturer;
    QString name;
    QString status;
    QString notes;
    bool active = true;
};

struct ReturnRow {
    qint64 requirementId = 0;
    QString manufacturer;
    qint64 storageId = 0;
    int quantity = 0;
    bool spare = false;
};

struct RequirementExpectedState {
    qint64 requirementId = 0;
    qint64 buildId = 0;
    QString modifiedUtc;
    QString partNumber;
    int rebrickableColorId = 0;
    QString substitutePartNumber;
    int substituteRebrickableColorId = -1;
    int quantityRequired = 0;
    int quantityPulled = 0;
    int quantityReleased = 0;
    bool spare = false;
};

struct AllocationRow {
    qint64 allocationId = 0;
    qint64 inventoryRecordId = 0;
    int quantity = 0;
    int expectedQuantity = 0;
    QString modifiedUtc;
    QString inventoryModifiedUtc;
    int inventoryQuantity = 0;
};

struct Request {
    qint64 workspaceId = 0;
    qint64 buildId = 0;
    QString mutationId;
    QString buildType;
    QString reference;
    QString inventoryMode;
    QString manufacturer;
    QString initialStatus;
    QString name;
    QString notes;
    bool desiredActive = true;
    QString linkedCollectionState = QStringLiteral("Unassembled");
    ExpectedState expected;
    QList<ReturnRow> returns;
    qint64 requirementId = 0;
    QString partNumber;
    int rebrickableColorId = 0;
    QString substitutePartNumber;
    int substituteRebrickableColorId = -1;
    int quantityRequired = 0;
    bool spare = false;
    qint64 preferredStorageId = 0;
    int quantity = 0;
    RequirementExpectedState expectedRequirement;
    QList<AllocationRow> allocations;
};

struct Result {
    QString mutationId;
    QString operation;
    bool replayed = false;
    QJsonObject build;
    QJsonObject effects;
    QJsonObject requirement;
    QJsonArray allocations;
};

RemoteMutationDto::Metadata toMetadata(const QString& operation, const Request& request);
bool fromMetadata(const QString& operation, const RemoteMutationDto::Metadata& metadata,
                  Request* request, RemoteMutationDto::Error* error = nullptr);
bool resultFromMutation(const RemoteMutationDto::Result& source, Result* result,
                        RemoteMutationDto::Error* error = nullptr);

} // namespace RemoteBuildMutationDto
