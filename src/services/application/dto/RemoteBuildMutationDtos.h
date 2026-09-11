#pragma once

#include "RemoteMutationDtos.h"

#include <QList>

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
};

struct Result {
    QString mutationId;
    QString operation;
    bool replayed = false;
    QJsonObject build;
    QJsonObject effects;
};

RemoteMutationDto::Metadata toMetadata(const QString& operation, const Request& request);
bool fromMetadata(const QString& operation, const RemoteMutationDto::Metadata& metadata,
                  Request* request, RemoteMutationDto::Error* error = nullptr);
bool resultFromMutation(const RemoteMutationDto::Result& source, Result* result,
                        RemoteMutationDto::Error* error = nullptr);

} // namespace RemoteBuildMutationDto
