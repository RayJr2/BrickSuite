/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "BrickLinkPartResolver.h"

#include "../../api/ApiProvider.h"
#include "../../models/ExternalMappingStatus.h"
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

    // Sparse-exception model:
    // no provider mapping row means the normal LEGO/Rebrickable part number
    // is used directly as BrickLink ITEMID.
    if (!mapping) {
        result.itemId = result.sourcePartNumber;
        result.status = ResolutionStatus::Direct;
        result.canExport = true;
        result.message =
            QStringLiteral("Using the BrickSuite/Rebrickable part number directly.");
        return result;
    }

    if (mapping->status == ExternalMappingStatus::Mapped
        && !mapping->externalId.trimmed().isEmpty()) {
        result.itemId = mapping->externalId.trimmed();
        result.status = ResolutionStatus::MappedOverride;
        result.canExport = true;
        result.message =
            QStringLiteral("Using a stored BrickLink part-number override.");
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
    case ResolutionStatus::Direct:
        return QStringLiteral("Direct");
    case ResolutionStatus::MappedOverride:
        return QStringLiteral("Mapped Override");
    case ResolutionStatus::NeedsReview:
        return QStringLiteral("Needs Review");
    }

    return QStringLiteral("Needs Review");
}
