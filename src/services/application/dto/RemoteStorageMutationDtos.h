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
#include "RemoteReadDtos.h"

namespace RemoteStorageMutationDto {

struct ExpectedState {
    QString modifiedUtc;
    qint64 parentStorageId = 0;
    qint64 storageTypeId = 0;
    QString name;
    QString description;
    int sortOrder = 0;
    bool active = true;
    bool allowsInventory = true;
    bool allowsCollection = false;
};
struct Request {
    qint64 workspaceId = 0;
    qint64 storageId = 0;
    QString mutationId;
    QString name;
    QString description;
    qint64 parentStorageId = 0;
    qint64 storageTypeId = 0;
    bool allowsInventory = true;
    bool allowsCollection = false;
    bool active = true;
    ExpectedState expected;
};
struct Result {
    QString mutationId;
    QString operation;
    bool replayed = false;
    bool created = false;
    RemoteReadDto::StorageDetail storage;
};

RemoteMutationDto::Metadata toMetadata(const QString& operation,const Request& request);
bool fromMetadata(const QString& operation,const RemoteMutationDto::Metadata& metadata,
                  Request* request,RemoteMutationDto::Error* error=nullptr);
bool resultFromMutation(const RemoteMutationDto::Result& source,Result* result,
                        RemoteMutationDto::Error* error=nullptr);
}
