/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "ProcurementDraftService.h"

#include "../../api/ApiProvider.h"
#include "../../models/Build.h"
#include "../../models/ExternalColorMapping.h"
#include "../../repositories/BuildRepository.h"
#include "../../repositories/ExternalColorMappingRepository.h"
#include "../../services/builds/MissingPartsService.h"
#include "../../services/mappings/BrickLinkPartResolver.h"

#include <QHash>

namespace
{
QString procurementKey(int partId, int colorId)
{
    return QStringLiteral("%1:%2").arg(partId).arg(colorId);
}
}

ProcurementDraftService::Result
ProcurementDraftService::createBrickLinkDraft(int workspaceId,
                                               int buildId) const
{
    Result result;

    if (workspaceId <= 0 || buildId <= 0) {
        result.message = QStringLiteral("Workspace or Build is not selected.");
        return result;
    }

    BuildRepository buildRepository;
    const auto build = buildRepository.getById(buildId);

    if (!build) {
        result.message = QStringLiteral("Unable to load the selected Build.");
        return result;
    }

    if (build->inventoryMode() != QStringLiteral("Stock")) {
        result.message =
            QStringLiteral("Missing Parts procurement is available only for Build from Stock.");
        return result;
    }

    result.draft.workspaceId = workspaceId;
    result.draft.buildId = buildId;
    result.draft.buildName = build->name();
    result.draft.buildType = build->buildType();
    result.draft.buildNumber = build->setNumber();

    MissingPartsService missingPartsService;
    const QList<MissingPartsService::MissingPart> missing =
        missingPartsService.getMissingParts(workspaceId, buildId);

    if (missing.isEmpty()) {
        result.success = true;
        result.message =
            QStringLiteral("This Build currently has no missing non-spare parts.");
        return result;
    }

    // Procurement operates on unique Part/Color combinations. If multiple
    // Build Requirement rows describe the same combination, sum only the
    // authoritative MissingPartsService::missing quantity here.
    QHash<QString, MissingPartsService::MissingPart> aggregated;

    for (const MissingPartsService::MissingPart& source : missing) {
        const QString key = procurementKey(source.partId, source.colorId);

        if (!aggregated.contains(key)) {
            aggregated.insert(key, source);
        } else {
            MissingPartsService::MissingPart item = aggregated.value(key);
            item.missing += source.missing;
            aggregated.insert(key, item);
        }
    }

    BrickLinkPartResolver partResolver;
    ExternalColorMappingRepository colorMappingRepository;

    const QString provider = apiProviderName(ApiProvider::BrickLink);

    for (const MissingPartsService::MissingPart& source : aggregated) {
        ProcurementItem item;

        item.partId = source.partId;
        item.colorId = source.colorId;
        item.partNumber = source.partNumber;
        item.partName = source.partName;
        item.colorName = source.colorName;
        item.quantityNeeded = source.missing;

        const BrickLinkPartResolver::Result partResult =
            partResolver.resolve(source.partId, source.partNumber);

        item.resolvedItemId = partResult.itemId;
        item.resolvedItemStatus =
            BrickLinkPartResolver::statusText(partResult.status);
        item.resolvedItemReady = partResult.canExport;

        const auto colorMapping =
            colorMappingRepository.getByColorAndProvider(source.colorId, provider);

        if (colorMapping
            && colorMapping->status == ExternalMappingStatus::Mapped
            && !colorMapping->externalId.trimmed().isEmpty()) {
            item.resolvedColorId = colorMapping->externalId.trimmed();
            item.resolvedColorStatus = QStringLiteral("Mapped");
            item.resolvedColorReady = true;
        } else if (colorMapping
                   && colorMapping->status == ExternalMappingStatus::Unsupported) {
            item.resolvedColorStatus = QStringLiteral("Unsupported");
        } else {
            item.resolvedColorStatus = QStringLiteral("Unknown");
        }

        result.draft.items.append(item);
    }

    result.success = true;
    result.message =
        QStringLiteral("Procurement draft prepared for BrickLink Wanted List preview.");

    return result;
}
