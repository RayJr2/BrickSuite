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

namespace RemoteInventoryMutationDto {

constexpr int MaximumQuantity = 1000000;
constexpr int MaximumTextLength = 256;

struct ExpectedState {
    qint64 inventoryRecordId = 0;
    int quantity = -1;
    qint64 storageLocationId = 0;
    QString modifiedUtc;
    QString partNumber;
    int colorExternalId = -1;
    QString manufacturerName;
    QString condition;
    QString ownershipType;
};

struct Request {
    qint64 workspaceId = 0;
    QString mutationId;
    ExpectedState expected;
    qint64 inventoryRecordId = 0;
    QString partNumber;
    int colorExternalId = -1;
    QString manufacturerName;
    qint64 storageLocationId = 0;
    qint64 destinationStorageLocationId = 0;
    int quantity = 0;
    QString condition;
    QString ownershipType;
    QString notes;
};

struct Result {
    QString mutationId;
    QString operation;
    bool replayed = false;
    qint64 sourceRecordId = 0;
    qint64 destinationRecordId = 0;
    qint64 survivingRecordId = 0;
    int resultingQuantity = 0;
    int sourceQuantity = -1;
    qint64 storageLocationId = 0;
    QString modifiedUtc;
    bool created = false;
    bool merged = false;
};

RemoteMutationDto::Metadata toMetadata(const Request& request);
bool fromMetadata(const QString& operation, const RemoteMutationDto::Metadata& metadata,
                  Request* request, RemoteMutationDto::Error* error = nullptr);
bool resultFromMutation(const RemoteMutationDto::Result& source, Result* result,
                        RemoteMutationDto::Error* error = nullptr);

} // namespace RemoteInventoryMutationDto
