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
QJsonObject toJson(const RemoteReadDto::InventoryRow&);
bool fromJson(const QJsonObject&, RemoteReadDto::InventoryRow*, DecodeError* = nullptr);
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
QJsonObject toJson(const RemoteReadDto::PullingRow&);
bool fromJson(const QJsonObject&, RemoteReadDto::PullingRow*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::CollectionSummary&);
bool fromJson(const QJsonObject&, RemoteReadDto::CollectionSummary*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::CollectionDetail&);
bool fromJson(const QJsonObject&, RemoteReadDto::CollectionDetail*, DecodeError* = nullptr);
QJsonObject toJson(const RemoteReadDto::PartReferenceCustomization&);
bool fromJson(const QJsonObject&, RemoteReadDto::PartReferenceCustomization*, DecodeError* = nullptr);
QString utc(const QDateTime& value);
bool parseUtc(const QJsonValue& value, QDateTime* result);

} // namespace RemoteReadJson
