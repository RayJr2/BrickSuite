#pragma once

#include "RemoteMutationDtos.h"

#include <QJsonObject>
#include <QList>

namespace RemotePullingMutationDto {

constexpr int MaximumRows = 500;

struct Row {
    qint64 allocationId = 0;
    int quantity = 0;
    int expectedAllocatedQuantity = 0;
    int expectedInventoryQuantity = 0;
    int expectedRequirementPulledQuantity = 0;
};

struct Request {
    qint64 workspaceId = 0;
    QString mutationId;
    qint64 buildId = 0;
    QString expectedBuildStatus;
    QList<Row> rows;
};

struct Result {
    QString mutationId;
    bool replayed = false;
    qint64 buildId = 0;
    int submittedRows = 0;
    int piecesRecorded = 0;
    QList<qint64> allocationIds;
    QJsonObject requirementPulledTotals;
    QString buildStatus;
};

RemoteMutationDto::Metadata toMetadata(const Request& request);
bool fromMetadata(const RemoteMutationDto::Metadata& metadata, Request* request,
                  RemoteMutationDto::Error* error = nullptr);
bool resultFromMutation(const RemoteMutationDto::Result& source, Result* result,
                        RemoteMutationDto::Error* error = nullptr);

} // namespace RemotePullingMutationDto
