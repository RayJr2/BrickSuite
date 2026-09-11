#pragma once

#include "RemoteMutationDtos.h"
#include "../../parts/PartReferenceCustomizationService.h"

namespace RemotePartReferenceMutationDto {

struct ExpectedState {
    QString modifiedUtc;
    QString partNumber;
    QString catalog;
    QString section;
    PartReferencePlacement placement = PartReferencePlacement::Append;
    QString anchorPartNumber;
};

struct Request {
    qint64 workspaceId = 0; // Mutation-envelope context only; customizations are Host-global.
    QString mutationId;
    qint64 customizationId = 0;
    QString partNumber;
    QString catalog;
    QString section;
    PartReferencePlacement placement = PartReferencePlacement::Append;
    QString anchorPartNumber;
    ExpectedState expected;
};

struct Result {
    QString mutationId;
    QString operation;
    bool replayed = false;
    qint64 customizationId = 0;
    QString partNumber;
    QString catalog;
    QString section;
    PartReferencePlacement placement = PartReferencePlacement::Append;
    QString anchorPartNumber;
    QString createdUtc;
    QString modifiedUtc;
};

RemoteMutationDto::Metadata toMetadata(const QString& operation, const Request& request);
bool fromMetadata(const QString& operation, const RemoteMutationDto::Metadata& metadata,
                  Request* result, RemoteMutationDto::Error* error = nullptr);
bool resultFromMutation(const RemoteMutationDto::Result& source, Result* result,
                        RemoteMutationDto::Error* error = nullptr);
QString placementName(PartReferencePlacement placement);

} // namespace RemotePartReferenceMutationDto
