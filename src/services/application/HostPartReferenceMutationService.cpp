#include "HostPartReferenceMutationService.h"

#include "dto/RemotePartReferenceMutationDtos.h"
#include "../parts/PartReferenceManifest.h"
#include "../parts/PartReferenceCustomizationService.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/UserPartReferenceRepository.h"

namespace {
QString normalized(const QString& value) { return value.trimmed().toLower(); }

HostWriteExecutor::MutationOutcome failure(const QString& code, const QString& message,
                                           bool refresh = false)
{
    HostWriteExecutor::MutationOutcome result;
    result.error={code,message,false};
    if (refresh) result.error.conflict={{"refreshRequired",true}};
    return result;
}

QJsonObject authoritative(const UserPartReferenceEntry& entry, const QString& partNumber)
{
    return {{"customizationId",entry.id},{"partNumber",partNumber},
            {"catalog",entry.catalog},{"section",entry.section},
            {"placement",RemotePartReferenceMutationDto::placementName(entry.placement)},
            {"anchorPartNumber",entry.anchorPartNumber},
            {"createdUtc",entry.createdUtc.toUTC().toString(Qt::ISODateWithMs)},
            {"modifiedUtc",entry.modifiedUtc.toUTC().toString(Qt::ISODateWithMs)}};
}
}

HostWriteExecutor::Mutation HostPartReferenceMutationService::createMutation(
    const QString& operation, const RemoteMutationDto::Metadata& metadata,
    RemoteMutationDto::Error* error)
{
    RemotePartReferenceMutationDto::Request request;
    if (!RemotePartReferenceMutationDto::fromMetadata(operation,metadata,&request,error)) return {};
    return [operation,request](const QSqlDatabase& database) {
        PartReferenceManifest manifest;
        QString manifestError;
        if (!manifest.load(&manifestError))
            return failure(QStringLiteral("INTERNAL_ERROR"),
                           QStringLiteral("The Host Part Reference definition is unavailable."));
        PartRepository parts(database);
        UserPartReferenceRepository users(database);
        if (operation==QStringLiteral("partReference.customizations.add")) {
            bool overlayLoaded=false; users.getAll(&overlayLoaded);
            if(!overlayLoaded)
                return failure(QStringLiteral("INTERNAL_ERROR"),
                               QStringLiteral("The Host could not validate existing customizations."));
            const auto part=parts.getByPartNumber(request.partNumber);
            if (!part || !part->isActive())
                return failure(QStringLiteral("NOT_FOUND"),
                               QStringLiteral("The requested Part is not available on the Host."));
            const auto added=PartReferenceCustomizationService(manifest,database).add(
                part->id(),request.catalog,request.section,request.placement,request.anchorPartNumber);
            if (!added.success) {
                const bool duplicate=added.message.contains(QStringLiteral("already in Part Reference"));
                return failure(duplicate?QStringLiteral("CONFLICT"):QStringLiteral("INVALID_ARGUMENT"),
                               added.message,duplicate);
            }
            bool loaded=false;
            const auto stored=users.getById(added.userEntryId,&loaded);
            if (!loaded || !stored)
                return failure(QStringLiteral("INTERNAL_ERROR"),
                               QStringLiteral("The Host could not reload the saved customization."));
            HostWriteExecutor::MutationOutcome outcome; outcome.success=true;
            outcome.authoritative=authoritative(*stored,part->partNumber());
            outcome.publicationWorkflow=HostMutationPublicationService::Workflow::PartReferenceCustomization;
            outcome.publicationScope.partNumber=part->partNumber(); return outcome;
        }

        bool loaded=false;
        const auto stored=users.getById(int(request.customizationId),&loaded);
        if (!loaded)
            return failure(QStringLiteral("INTERNAL_ERROR"),
                           QStringLiteral("The Host could not load the customization."));
        if (!stored)
            return failure(QStringLiteral("NOT_FOUND"),
                           QStringLiteral("The user customization no longer exists."));
        const auto part=parts.getById(stored->partId);
        if (!part)
            return failure(QStringLiteral("INTERNAL_ERROR"),
                           QStringLiteral("The Host customization has no valid Part."));
        const auto& expected=request.expected;
        const bool matches=stored->modifiedUtc.toUTC().toString(Qt::ISODateWithMs)==expected.modifiedUtc
            && normalized(part->partNumber())==normalized(expected.partNumber)
            && normalized(stored->catalog)==normalized(expected.catalog)
            && normalized(stored->section)==normalized(expected.section)
            && stored->placement==expected.placement
            && normalized(stored->anchorPartNumber)==normalized(expected.anchorPartNumber);
        if (!matches)
            return failure(QStringLiteral("STALE_VERSION"),
                           QStringLiteral("The Part Reference customization changed. Refresh and try again."),true);
        const QJsonObject snapshot=authoritative(*stored,part->partNumber());
        const auto removed=PartReferenceCustomizationService(manifest,database).remove(stored->id);
        if (!removed.success)
            return failure(QStringLiteral("INTERNAL_ERROR"),removed.message);
        HostWriteExecutor::MutationOutcome outcome; outcome.success=true;
        outcome.authoritative=snapshot;
        outcome.publicationWorkflow=HostMutationPublicationService::Workflow::PartReferenceCustomization;
        outcome.publicationScope.partNumber=part->partNumber(); return outcome;
    };
}
