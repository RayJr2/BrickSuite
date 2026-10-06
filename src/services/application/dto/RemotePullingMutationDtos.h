/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

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
