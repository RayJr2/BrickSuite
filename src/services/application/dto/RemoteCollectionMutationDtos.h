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
#include <QList>

namespace RemoteCollectionMutationDto {
struct ExpectedState { QString modifiedUtc,type,setNumber,minifigNumber,state,condition,completeness,nickname,notes; qint64 storageId=0,sourceBuildId=0; bool active=true; bool allowPartsSource=false; };
struct DisassemblyReturn { qint64 rowIndex=0,storageId=0; int quantity=0; };
struct Request { qint64 workspaceId=0,collectionItemId=0,storageId=0,buildId=0; QString mutationId,sourceType,setNumber,minifigNumber,state,condition,completeness,nickname,notes,planId; bool desiredActive=true; bool allowPartsSource=false; ExpectedState expected; QList<DisassemblyReturn> returns; };
struct Result { QString mutationId,operation; bool replayed=false; QJsonObject item; };
RemoteMutationDto::Metadata toMetadata(const QString&,const Request&);
bool fromMetadata(const QString&,const RemoteMutationDto::Metadata&,Request*,RemoteMutationDto::Error* = nullptr);
bool resultFromMutation(const RemoteMutationDto::Result&,Result*,RemoteMutationDto::Error* = nullptr);
}
