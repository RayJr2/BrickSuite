/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "BrickLinkPartResolver.h"

#include "../../api/ApiProvider.h"
#include "../../models/ExternalMappingStatus.h"
#include "../../repositories/ExternalPartIdentifierRepository.h"
#include "../../repositories/ExternalPartMappingRepository.h"

BrickLinkPartResolver::Result BrickLinkPartResolver::resolve(
    int partId,
    const QString& partNumber) const
{
    Result result;
    result.partId = partId;
    result.sourcePartNumber = partNumber.trimmed();

    if (partId <= 0 || result.sourcePartNumber.isEmpty()) {
        result.status = ResolutionStatus::NeedsReview;
        result.canExport = false;
        result.message =
            QStringLiteral("Part identity is incomplete and cannot be exported.");
        return result;
    }

    ExternalPartMappingRepository mappingRepository;

    const auto mapping =
        mappingRepository.getByPartAndProvider(
            partId,
            apiProviderName(ApiProvider::BrickLink));

    if (mapping && mapping->status == ExternalMappingStatus::Mapped
        && !mapping->externalId.trimmed().isEmpty()) {
        result.itemId = mapping->externalId.trimmed();

        if (mapping->source.compare(QStringLiteral("User"),
                                    Qt::CaseInsensitive) == 0) {
            result.status = ResolutionStatus::UserOverride;
            result.message =
                QStringLiteral("Using a user-confirmed BrickLink part-number override.");
        } else if (mapping->source.compare(QStringLiteral("Rebrickable"),
                                           Qt::CaseInsensitive) == 0) {
            result.status = ResolutionStatus::ExternalId;
            result.message =
                QStringLiteral("Using the BrickLink external ID supplied by Rebrickable.");
        } else {
            result.status = ResolutionStatus::MappedOverride;
            result.message =
                QStringLiteral("Using a stored BrickLink part-number mapping.");
        }

        result.canExport = true;
        return result;
    }

    if (!mapping) {
        const QList<ExternalPartIdentifier> identifiers =
            ExternalPartIdentifierRepository().findByPartAndProvider(
                partId,
                apiProviderName(ApiProvider::BrickLink));

        if (identifiers.size() == 1
            && !identifiers.first().externalId.trimmed().isEmpty()) {
            result.itemId = identifiers.first().externalId.trimmed();
            result.status = ResolutionStatus::ExternalId;
            result.canExport = true;
            result.message =
                QStringLiteral("Using the authoritative BrickLink external ID supplied by %1.")
                    .arg(identifiers.first().source);
            return result;
        }

        const auto lookupStatus =
            ExternalPartIdentifierRepository().lookupStatus(
                partId,
                QStringLiteral("Rebrickable"));
        const bool lookupComplete =
            lookupStatus == ExternalPartIdentifierRepository::LookupStatus::Loaded
            || lookupStatus == ExternalPartIdentifierRepository::LookupStatus::Unavailable;
        result.status = identifiers.size() > 1
                            ? ResolutionStatus::Ambiguous
                            : lookupComplete
                                  ? ResolutionStatus::Unavailable
                                  : ResolutionStatus::NotResolved;
        result.canExport = false;
        result.message = identifiers.size() > 1
                             ? QStringLiteral("Multiple BrickLink ITEMIDs require review.")
                             : QStringLiteral("No authoritative BrickLink ITEMID is available.");
        return result;
    }

    result.status = ResolutionStatus::NeedsReview;
    result.canExport = false;

    if (mapping->status == ExternalMappingStatus::Unsupported) {
        result.message =
            mapping->notes.trimmed().isEmpty()
                ? QStringLiteral("This part is marked unsupported for BrickLink export.")
                : mapping->notes.trimmed();
    } else if (mapping->status == ExternalMappingStatus::Unknown) {
        result.message =
            mapping->notes.trimmed().isEmpty()
                ? QStringLiteral("This part requires a BrickLink ITEMID review.")
                : mapping->notes.trimmed();
    } else {
        result.message =
            QStringLiteral("The stored BrickLink mapping is incomplete.");
    }

    return result;
}

QString BrickLinkPartResolver::statusText(ResolutionStatus status)
{
    switch (status) {
    case ResolutionStatus::ExternalId:
        return QStringLiteral("External ID");
    case ResolutionStatus::UserOverride:
        return QStringLiteral("User Override");
    case ResolutionStatus::MappedOverride:
        return QStringLiteral("Mapped Override");
    case ResolutionStatus::NotResolved:
        return QStringLiteral("Not Resolved");
    case ResolutionStatus::Unavailable:
        return QStringLiteral("Unavailable — Needs Review");
    case ResolutionStatus::Ambiguous:
        return QStringLiteral("Multiple IDs — Needs Review");
    case ResolutionStatus::NeedsReview:
        return QStringLiteral("Needs Review");
    }

    return QStringLiteral("Needs Review");
}
