#include "RemotePartReferenceMutationDtos.h"

#include <QDateTime>
#include <climits>

namespace {
constexpr int MaximumIdentityLength = 200;
void invalid(RemoteMutationDto::Error* error, const QString& message)
{ if (error) *error = {QStringLiteral("INVALID_ARGUMENT"), message, false}; }
bool validText(const QJsonValue& value, QString* result, bool allowEmpty = false)
{
    if (!value.isString()) return false;
    *result = value.toString().trimmed();
    return result->size() <= MaximumIdentityLength && (allowEmpty || !result->isEmpty());
}
bool positiveInteger(const QJsonValue& value, qint64* result)
{
    if (!value.isDouble()) return false;
    const double number = value.toDouble();
    if (number < 1 || number > 9007199254740991.0 || number != qint64(number)) return false;
    *result = qint64(number); return true;
}
bool parsePlacement(const QJsonValue& value, PartReferencePlacement* result)
{
    if (!value.isString()) return false;
    const QString name = value.toString();
    if (name == QStringLiteral("Append")) *result = PartReferencePlacement::Append;
    else if (name == QStringLiteral("Before")) *result = PartReferencePlacement::Before;
    else if (name == QStringLiteral("After")) *result = PartReferencePlacement::After;
    else return false;
    return true;
}
QJsonObject stateJson(const RemotePartReferenceMutationDto::ExpectedState& value)
{
    return {{"modifiedUtc",value.modifiedUtc},{"partNumber",value.partNumber},
            {"catalog",value.catalog},{"section",value.section},
            {"placement",RemotePartReferenceMutationDto::placementName(value.placement)},
            {"anchorPartNumber",value.anchorPartNumber}};
}
}

namespace RemotePartReferenceMutationDto {
QString placementName(PartReferencePlacement placement)
{
    if (placement == PartReferencePlacement::Before) return QStringLiteral("Before");
    if (placement == PartReferencePlacement::After) return QStringLiteral("After");
    return QStringLiteral("Append");
}

RemoteMutationDto::Metadata toMetadata(const QString& operation, const Request& request)
{
    QJsonObject mutation;
    QJsonObject expected;
    if (operation == QStringLiteral("partReference.customizations.add")) {
        mutation={{"partNumber",request.partNumber},{"catalog",request.catalog},
                  {"section",request.section},{"placement",placementName(request.placement)},
                  {"anchorPartNumber",request.anchorPartNumber}};
    } else {
        mutation={{"customizationId",double(request.customizationId)}};
        expected=stateJson(request.expected);
    }
    return {request.workspaceId,request.mutationId,expected,mutation};
}

bool fromMetadata(const QString& operation, const RemoteMutationDto::Metadata& metadata,
                  Request* result, RemoteMutationDto::Error* error)
{
    if (!result) return false;
    const bool add=operation==QStringLiteral("partReference.customizations.add");
    const bool remove=operation==QStringLiteral("partReference.customizations.remove");
    if (!add && !remove) { invalid(error,QStringLiteral("Unsupported Part Reference mutation.")); return false; }
    Request value; value.workspaceId=metadata.workspaceId; value.mutationId=metadata.mutationId;
    if (add) {
        if (!metadata.expected.isEmpty() || metadata.mutation.size()!=5
            || !validText(metadata.mutation.value("partNumber"),&value.partNumber)
            || !validText(metadata.mutation.value("catalog"),&value.catalog)
            || !validText(metadata.mutation.value("section"),&value.section)
            || !parsePlacement(metadata.mutation.value("placement"),&value.placement)
            || !validText(metadata.mutation.value("anchorPartNumber"),&value.anchorPartNumber,true)
            || (value.placement!=PartReferencePlacement::Append && value.anchorPartNumber.isEmpty())
            || (value.placement==PartReferencePlacement::Append && !value.anchorPartNumber.isEmpty())) {
            invalid(error,QStringLiteral("The Part Reference Add request is invalid.")); return false;
        }
    } else {
        if (metadata.mutation.size()!=1 || metadata.expected.size()!=6
            || !positiveInteger(metadata.mutation.value("customizationId"),&value.customizationId)
            || !validText(metadata.expected.value("modifiedUtc"),&value.expected.modifiedUtc)
            || !validText(metadata.expected.value("partNumber"),&value.expected.partNumber)
            || !validText(metadata.expected.value("catalog"),&value.expected.catalog)
            || !validText(metadata.expected.value("section"),&value.expected.section)
            || !parsePlacement(metadata.expected.value("placement"),&value.expected.placement)
            || !validText(metadata.expected.value("anchorPartNumber"),&value.expected.anchorPartNumber,true)) {
            invalid(error,QStringLiteral("The Part Reference Remove request is invalid.")); return false;
        }
        const QDateTime modified=QDateTime::fromString(value.expected.modifiedUtc,Qt::ISODateWithMs);
        if(!modified.isValid()||modified.offsetFromUtc()!=0){
            invalid(error,QStringLiteral("The expected modification time must be UTC."));return false;
        }
    }
    *result=value; return true;
}

bool resultFromMutation(const RemoteMutationDto::Result& source, Result* result,
                        RemoteMutationDto::Error* error)
{
    if (!result) return false;
    Result value; value.mutationId=source.mutationId; value.operation=source.operation;
    value.replayed=source.replayed;
    if (!positiveInteger(source.authoritative.value("customizationId"),&value.customizationId)
        || !validText(source.authoritative.value("partNumber"),&value.partNumber)
        || !validText(source.authoritative.value("catalog"),&value.catalog)
        || !validText(source.authoritative.value("section"),&value.section)
        || !parsePlacement(source.authoritative.value("placement"),&value.placement)
        || !validText(source.authoritative.value("anchorPartNumber"),&value.anchorPartNumber,true)
        || !validText(source.authoritative.value("createdUtc"),&value.createdUtc)
        || !validText(source.authoritative.value("modifiedUtc"),&value.modifiedUtc)) {
        invalid(error,QStringLiteral("The Host returned an invalid Part Reference result.")); return false;
    }
    *result=value; return true;
}
} // namespace RemotePartReferenceMutationDto
