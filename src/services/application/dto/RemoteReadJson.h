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

#include "RemoteReadDtos.h"
#include <QJsonObject>

namespace RemoteReadJson {

struct DecodeError { QString message; };
bool pageRequest(const QJsonObject&, RemoteReadDto::PageRequest*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::WorkspaceSummary&);
bool fromJson(const QJsonObject&, RemoteReadDto::WorkspaceSummary*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::StorageSummary&);
bool fromJson(const QJsonObject&, RemoteReadDto::StorageSummary*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::StorageDetail&);
bool fromJson(const QJsonObject&, RemoteReadDto::StorageDetail*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::StorageType&);
bool fromJson(const QJsonObject&, RemoteReadDto::StorageType*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::InventoryRow&);
bool fromJson(const QJsonObject&, RemoteReadDto::InventoryRow*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::InventoryExportRow&);
bool fromJson(const QJsonObject&, RemoteReadDto::InventoryExportRow*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::InventoryDetail&);
bool fromJson(const QJsonObject&, RemoteReadDto::InventoryDetail*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::InventoryHistoryRow&);
bool fromJson(const QJsonObject&, RemoteReadDto::InventoryHistoryRow*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::LostInventoryRow&);
bool fromJson(const QJsonObject&, RemoteReadDto::LostInventoryRow*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::BuildSummary&);
bool fromJson(const QJsonObject&, RemoteReadDto::BuildSummary*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::BuildRequirement&);
bool fromJson(const QJsonObject&, RemoteReadDto::BuildRequirement*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::MissingPart&);
bool fromJson(const QJsonObject&, RemoteReadDto::MissingPart*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::PickABrickPartResolution&);
bool fromJson(const QJsonObject&, RemoteReadDto::PickABrickPartResolution*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::PullingRow&);
bool fromJson(const QJsonObject&, RemoteReadDto::PullingRow*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::BuildCancellationReturnRow&);
bool fromJson(const QJsonObject&, RemoteReadDto::BuildCancellationReturnRow*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::CollectionSummary&);
QJsonObject toJson(const RemoteReadDto::CollectionSummary&, bool includePartsSource);
bool fromJson(const QJsonObject&, RemoteReadDto::CollectionSummary*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::CollectionExportRow&);
bool fromJson(const QJsonObject&, RemoteReadDto::CollectionExportRow*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::CollectionDetail&);
QJsonObject toJson(const RemoteReadDto::CollectionDetail&, bool includePartsSource);
bool fromJson(const QJsonObject&, RemoteReadDto::CollectionDetail*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::CollectionDisassemblyPlanRow&);
bool fromJson(const QJsonObject&, RemoteReadDto::CollectionDisassemblyPlanRow*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::CollectionDisassemblyPlan&);
bool fromJson(const QJsonObject&, RemoteReadDto::CollectionDisassemblyPlan*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::PartReferenceCustomization&);
bool fromJson(const QJsonObject&, RemoteReadDto::PartReferenceCustomization*, DecodeError* = nullptr);
QString utc(const QDateTime& value);
bool parseUtc(const QJsonValue& value, QDateTime* result);

} // namespace RemoteReadJson
