#include "RemotePullingMutationDtos.h"

#include <QJsonArray>
#include <QSet>
#include <cmath>
#include <limits>

namespace {
bool exactPositive(const QJsonValue& value, qint64* output)
{
    if (!value.isDouble()) return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < 1.0
        || number > double(std::numeric_limits<int>::max())) return false;
    const qint64 integer = static_cast<qint64>(number);
    if (number != static_cast<double>(integer)) return false;
    *output = integer;
    return true;
}
void invalid(RemoteMutationDto::Error* error, const QString& message)
{
    if (error) *error = {QStringLiteral("INVALID_REQUEST"), message, false};
}
}

namespace RemotePullingMutationDto {
RemoteMutationDto::Metadata toMetadata(const Request& request)
{
    QJsonArray rows;
    for (const Row& row : request.rows) {
        rows.append(QJsonObject{{"allocationId", row.allocationId}, {"quantity", row.quantity},
            {"expectedAllocatedQuantity", row.expectedAllocatedQuantity},
            {"expectedInventoryQuantity", row.expectedInventoryQuantity},
            {"expectedRequirementPulledQuantity", row.expectedRequirementPulledQuantity}});
    }
    return {request.workspaceId, request.mutationId,
        QJsonObject{{"buildStatus", request.expectedBuildStatus}},
        QJsonObject{{"buildId", request.buildId}, {"rows", rows}}};
}

bool fromMetadata(const RemoteMutationDto::Metadata& metadata, Request* request,
                  RemoteMutationDto::Error* error)
{
    if (!request) return false;
    qint64 buildId = 0;
    if (!exactPositive(metadata.mutation.value("buildId"), &buildId)) {
        invalid(error, QStringLiteral("A valid Build ID is required.")); return false;
    }
    const QJsonValue rowsValue = metadata.mutation.value("rows");
    if (!rowsValue.isArray() || rowsValue.toArray().isEmpty()
        || rowsValue.toArray().size() > MaximumRows) {
        invalid(error, QStringLiteral("Supply between 1 and %1 Pulling rows.").arg(MaximumRows));
        return false;
    }
    Request parsed{metadata.workspaceId, metadata.mutationId, buildId,
                   metadata.expected.value("buildStatus").toString(), {}};
    QSet<qint64> allocations;
    for (const QJsonValue& value : rowsValue.toArray()) {
        if (!value.isObject()) { invalid(error, QStringLiteral("A Pulling row is malformed.")); return false; }
        const QJsonObject object = value.toObject();
        qint64 allocationId = 0, quantity = 0, allocated = 0, inventory = 0, pulled = 0;
        if (!exactPositive(object.value("allocationId"), &allocationId)
            || !exactPositive(object.value("quantity"), &quantity)
            || !exactPositive(object.value("expectedAllocatedQuantity"), &allocated)
            || !exactPositive(object.value("expectedInventoryQuantity"), &inventory)
            || !object.value("expectedRequirementPulledQuantity").isDouble()) {
            invalid(error, QStringLiteral("A Pulling row contains an invalid quantity or ID.")); return false;
        }
        const double pulledNumber = object.value("expectedRequirementPulledQuantity").toDouble();
        if (!std::isfinite(pulledNumber) || pulledNumber < 0.0
            || pulledNumber > double(std::numeric_limits<int>::max())) {
            invalid(error, QStringLiteral("A Pulling row contains an invalid expected quantity.")); return false;
        }
        pulled = static_cast<qint64>(pulledNumber);
        if (pulledNumber != static_cast<double>(pulled)) {
            invalid(error, QStringLiteral("A Pulling row contains an invalid expected quantity.")); return false;
        }
        if (allocations.contains(allocationId)) {
            invalid(error, QStringLiteral("Duplicate allocation IDs are not permitted.")); return false;
        }
        allocations.insert(allocationId);
        parsed.rows.append({allocationId, int(quantity), int(allocated), int(inventory), int(pulled)});
    }
    *request = parsed;
    return true;
}

bool resultFromMutation(const RemoteMutationDto::Result& source, Result* result,
                        RemoteMutationDto::Error* error)
{
    if (!result) return false;
    qint64 buildId = 0;
    if (!exactPositive(source.authoritative.value("buildId"), &buildId)) {
        invalid(error, QStringLiteral("The Host returned an invalid Pulling result.")); return false;
    }
    Result parsed;
    parsed.mutationId = source.mutationId;
    parsed.replayed = source.replayed;
    parsed.buildId = buildId;
    parsed.submittedRows = source.authoritative.value("submittedRows").toInt(-1);
    parsed.piecesRecorded = source.authoritative.value("piecesRecorded").toInt(-1);
    parsed.buildStatus = source.authoritative.value("buildStatus").toString();
    parsed.requirementPulledTotals = source.authoritative.value("requirementPulledTotals").toObject();
    for (const auto& value : source.authoritative.value("allocationIds").toArray()) {
        qint64 id = 0;
        if (!exactPositive(value, &id)) { invalid(error, QStringLiteral("The Host returned an invalid allocation ID.")); return false; }
        parsed.allocationIds.append(id);
    }
    if (parsed.submittedRows < 0 || parsed.piecesRecorded < 0) {
        invalid(error, QStringLiteral("The Host returned incomplete Pulling totals.")); return false;
    }
    *result = parsed;
    return true;
}
} // namespace RemotePullingMutationDto
