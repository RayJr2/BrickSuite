#include "RemoteInventoryMutationDtos.h"

#include <cmath>
#include <limits>

namespace {
bool integer(const QJsonValue& value, qint64 minimum, qint64 maximum, qint64* result)
{
    if (!value.isDouble()) return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < double(minimum) || number > double(maximum)) return false;
    const qint64 exact = qint64(number);
    if (number != double(exact)) return false;
    *result = exact;
    return true;
}
bool bounded(const QString& value) { return value.size() <= RemoteInventoryMutationDto::MaximumTextLength; }
void invalid(RemoteMutationDto::Error* error, const QString& message)
{
    if (error) *error = {QStringLiteral("INVALID_ARGUMENT"), message, false};
}
}

namespace RemoteInventoryMutationDto {
RemoteMutationDto::Metadata toMetadata(const Request& request)
{
    QJsonObject expected{{"inventoryRecordId", request.expected.inventoryRecordId},
        {"quantity", request.expected.quantity}, {"storageLocationId", request.expected.storageLocationId},
        {"modifiedUtc", request.expected.modifiedUtc}, {"partNumber", request.expected.partNumber},
        {"colorExternalId", request.expected.colorExternalId},
        {"manufacturerName", request.expected.manufacturerName}, {"condition", request.expected.condition},
        {"ownershipType", request.expected.ownershipType}};
    QJsonObject mutation{{"inventoryRecordId", request.inventoryRecordId}, {"partNumber", request.partNumber},
        {"colorExternalId", request.colorExternalId}, {"manufacturerName", request.manufacturerName},
        {"storageLocationId", request.storageLocationId},
        {"destinationStorageLocationId", request.destinationStorageLocationId}, {"quantity", request.quantity},
        {"condition", request.condition}, {"ownershipType", request.ownershipType}, {"notes", request.notes}};
    return {request.workspaceId, request.mutationId, expected, mutation};
}

bool fromMetadata(const QString& operation, const RemoteMutationDto::Metadata& metadata,
                  Request* request, RemoteMutationDto::Error* error)
{
    if (!request) return false;
    Request parsed;
    parsed.workspaceId = metadata.workspaceId;
    parsed.mutationId = metadata.mutationId;
    qint64 id = 0, quantity = 0, storage = 0;
    if (integer(metadata.expected.value("inventoryRecordId"), 0, INT_MAX, &id)) parsed.expected.inventoryRecordId=id;
    if (integer(metadata.expected.value("quantity"), -1, MaximumQuantity, &quantity)) parsed.expected.quantity=int(quantity);
    if (integer(metadata.expected.value("storageLocationId"), 0, INT_MAX, &storage)) parsed.expected.storageLocationId=storage;
    parsed.expected.modifiedUtc=metadata.expected.value("modifiedUtc").toString();
    parsed.expected.partNumber=metadata.expected.value("partNumber").toString().trimmed();
    parsed.expected.colorExternalId=metadata.expected.value("colorExternalId").toInt(-1);
    parsed.expected.manufacturerName=metadata.expected.value("manufacturerName").toString().trimmed();
    parsed.expected.condition=metadata.expected.value("condition").toString().trimmed();
    parsed.expected.ownershipType=metadata.expected.value("ownershipType").toString().trimmed();
    if (integer(metadata.mutation.value("inventoryRecordId"), 0, INT_MAX, &id)) parsed.inventoryRecordId=id;
    parsed.partNumber=metadata.mutation.value("partNumber").toString().trimmed();
    parsed.colorExternalId=metadata.mutation.value("colorExternalId").toInt(-1);
    parsed.manufacturerName=metadata.mutation.value("manufacturerName").toString().trimmed();
    if (integer(metadata.mutation.value("storageLocationId"), 0, INT_MAX, &storage)) parsed.storageLocationId=storage;
    if (integer(metadata.mutation.value("destinationStorageLocationId"), 0, INT_MAX, &storage)) parsed.destinationStorageLocationId=storage;
    if (integer(metadata.mutation.value("quantity"), 0, MaximumQuantity, &quantity)) parsed.quantity=int(quantity);
    parsed.condition=metadata.mutation.value("condition").toString().trimmed();
    parsed.ownershipType=metadata.mutation.value("ownershipType").toString().trimmed();
    parsed.notes=metadata.mutation.value("notes").toString().trimmed();
    const bool recordOperation = operation != QStringLiteral("inventory.add")
                                 && operation != QStringLiteral("inventory.markFound");
    if ((recordOperation && (parsed.inventoryRecordId <= 0 || parsed.expected.inventoryRecordId != parsed.inventoryRecordId))
        || parsed.quantity <= 0 || !bounded(parsed.partNumber) || !bounded(parsed.manufacturerName)
        || !bounded(parsed.condition) || !bounded(parsed.ownershipType) || !bounded(parsed.notes)) {
        invalid(error, QStringLiteral("The Inventory mutation fields are invalid.")); return false;
    }
    *request=parsed; return true;
}

bool resultFromMutation(const RemoteMutationDto::Result& source, Result* result,
                        RemoteMutationDto::Error* error)
{
    if (!result) return false;
    Result parsed; parsed.mutationId=source.mutationId; parsed.operation=source.operation;
    parsed.replayed=source.replayed;
    parsed.sourceRecordId=qint64(source.authoritative.value("sourceRecordId").toDouble());
    parsed.destinationRecordId=qint64(source.authoritative.value("destinationRecordId").toDouble());
    parsed.survivingRecordId=qint64(source.authoritative.value("survivingRecordId").toDouble());
    parsed.resultingQuantity=source.authoritative.value("resultingQuantity").toInt();
    parsed.sourceQuantity=source.authoritative.value("sourceQuantity").toInt(-1);
    parsed.storageLocationId=qint64(source.authoritative.value("storageLocationId").toDouble());
    parsed.modifiedUtc=source.authoritative.value("modifiedUtc").toString();
    parsed.created=source.authoritative.value("created").toBool(); parsed.merged=source.authoritative.value("merged").toBool();
    if (parsed.survivingRecordId <= 0 && parsed.sourceRecordId <= 0) {
        invalid(error, QStringLiteral("The Host returned an invalid Inventory result.")); return false;
    }
    *result=parsed; return true;
}
} // namespace RemoteInventoryMutationDto
