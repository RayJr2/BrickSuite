/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "BrickLinkMappingService.h"

#include "../../api/ApiProvider.h"
#include "../../models/Color.h"
#include "../../models/ExternalColorMapping.h"
#include "../../models/ExternalPartMapping.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/ExternalColorMappingRepository.h"
#include "../../repositories/ExternalPartMappingRepository.h"

#include <QDebug>
#include <QHash>

namespace
{
QStringList providerIds(
    const QHash<QString, QStringList>& externalIds,
    const QString& provider)
{
    for (auto it = externalIds.constBegin(); it != externalIds.constEnd(); ++it) {
        if (it.key().compare(provider, Qt::CaseInsensitive) == 0)
            return it.value();
    }

    return {};
}
}

BrickLinkMappingService::BrickLinkMappingService(QObject* parent)
    : QObject(parent)
    , m_rebrickableService(new RebrickableService(this))
{
    connect(m_rebrickableService,
            &RebrickableService::catalogColorsFinished,
            this,
            &BrickLinkMappingService::applyCatalogColors);
}

// BrickLink API access is intentionally not used here. BrickSuite obtains
// BrickLink provider IDs from Rebrickable's color reference data and stores
// those mappings locally for later XML generation.
void BrickLinkMappingService::refreshColorMappings(
    const QString& rebrickableApiKey)
{
    const QString apiKey = rebrickableApiKey.trimmed();

    if (apiKey.isEmpty()) {
        ColorRefreshResult result;
        result.message = QStringLiteral("Rebrickable API key is not configured.");
        emit colorMappingsRefreshed(result);
        return;
    }

    m_rebrickableService->getCatalogColors(apiKey);
}

void BrickLinkMappingService::ensureColorMappings(
    const QString& rebrickableApiKey)
{
    const QString apiKey = rebrickableApiKey.trimmed();

    ColorRefreshResult result;

    if (apiKey.isEmpty()) {
        result.message =
            QStringLiteral("Rebrickable API key is not configured.");
        emit colorMappingsRefreshed(result);
        return;
    }

    ColorRepository colorRepository;
    ExternalColorMappingRepository mappingRepository;

    const QList<Color> brickSuiteColors = colorRepository.getAll();
    const QString provider = apiProviderName(ApiProvider::BrickLink);

    const QList<ExternalColorMapping> existingMappings =
        mappingRepository.getByProvider(provider);

    //
    // A completed refresh creates exactly one provider mapping row for each
    // BrickSuite color, including explicit Unknown/Unsupported entries.
    // If coverage is already complete, avoid another provider request.
    //
    if (!brickSuiteColors.isEmpty()
        && existingMappings.size() >= brickSuiteColors.size()) {
        result.success = true;
        result.message =
            QStringLiteral("BrickLink color mappings are already initialized.");
        result.brickSuiteColors = brickSuiteColors.size();

        result.mapped =
            mappingRepository.countByProviderAndStatus(
                provider,
                ExternalMappingStatus::Mapped);

        result.unsupported =
            mappingRepository.countByProviderAndStatus(
                provider,
                ExternalMappingStatus::Unsupported);

        result.unknown =
            mappingRepository.countByProviderAndStatus(
                provider,
                ExternalMappingStatus::Unknown);

        emit colorMappingsRefreshed(result);
        return;
    }

    refreshColorMappings(apiKey);
}

bool BrickLinkMappingService::storePartExternalIds(
    int partId,
    const QHash<QString, QStringList>& externalIds) const
{
    if (partId <= 0)
        return false;

    QStringList brickLinkIds =
        providerIds(externalIds, QStringLiteral("BrickLink"));

    for (QString& id : brickLinkIds)
        id = id.trimmed();

    brickLinkIds.removeAll(QString());
    brickLinkIds.removeDuplicates();

    // Do not guess when Rebrickable supplies zero or multiple BrickLink IDs.
    if (brickLinkIds.size() != 1)
        return false;

    ExternalPartMappingRepository repository;

    const QString provider =
        apiProviderName(ApiProvider::BrickLink);

    const auto existing =
        repository.getByPartAndProvider(partId, provider);

    // A user-confirmed mapping always has precedence over provider data.
    if (existing
        && existing->source.compare(QStringLiteral("User"),
                                    Qt::CaseInsensitive) == 0) {
        return true;
    }

    // Reopening Part Details should not rewrite an identical provider
    // mapping or advance modified_utc when nothing actually changed.
    if (existing
        && existing->status == ExternalMappingStatus::Mapped
        && existing->source.compare(QStringLiteral("Rebrickable"),
                                    Qt::CaseInsensitive) == 0
        && existing->externalId.trimmed() == brickLinkIds.first()) {
        return true;
    }

    ExternalPartMapping mapping;
    mapping.partId = partId;
    mapping.provider = provider;
    mapping.externalId = brickLinkIds.first();
    mapping.status = ExternalMappingStatus::Mapped;
    mapping.source = QStringLiteral("Rebrickable");
    mapping.notes =
        QStringLiteral("Resolved from Rebrickable BrickLink external part ID.");

    if (!repository.upsert(mapping)) {
        qWarning() << "Unable to store Rebrickable BrickLink external part ID."
                   << "PartId:" << partId;
        return false;
    }

    return true;
}

void BrickLinkMappingService::applyCatalogColors(
    const RebrickableService::CatalogColorsResult& apiResult)
{
    ColorRefreshResult result;

    if (!apiResult.success) {
        result.message = apiResult.message;
        emit colorMappingsRefreshed(result);
        return;
    }

    result.rebrickableColors = apiResult.colors.size();

    ColorRepository colorRepository;
    ExternalColorMappingRepository mappingRepository;

    const QList<Color> brickSuiteColors = colorRepository.getAll();
    result.brickSuiteColors = brickSuiteColors.size();

    QHash<int, RebrickableService::CatalogColor> apiColorsById;

    for (const RebrickableService::CatalogColor& color : apiResult.colors)
        apiColorsById.insert(color.rebrickableColorId, color);

    const QString provider = apiProviderName(ApiProvider::BrickLink);

    for (const Color& color : brickSuiteColors) {
        const auto existing =
            mappingRepository.getByColorAndProvider(color.id(), provider);

        if (!apiColorsById.contains(color.rebrickableId())) {
            if (!existing) {
                ExternalColorMapping mapping;
                mapping.colorId = color.id();
                mapping.provider = provider;
                mapping.status = ExternalMappingStatus::Unknown;
                mapping.source = QStringLiteral("Rebrickable");
                mapping.notes =
                    QStringLiteral("Rebrickable catalog color was not returned.");

                if (!mappingRepository.upsert(mapping)) {
                    result.message =
                        QStringLiteral("Unable to save BrickLink color mapping.");
                    emit colorMappingsRefreshed(result);
                    return;
                }
            }

            continue;
        }

        ++result.matchedBrickSuiteColors;

        const RebrickableService::CatalogColor apiColor =
            apiColorsById.value(color.rebrickableId());

        QStringList brickLinkIds =
            providerIds(apiColor.externalIds, QStringLiteral("BrickLink"));

        brickLinkIds.removeAll(QString());
        brickLinkIds.removeDuplicates();

        if (brickLinkIds.size() == 1) {
            bool numeric = false;
            brickLinkIds.first().toInt(&numeric);

            if (numeric) {
                ExternalColorMapping mapping;
                mapping.colorId = color.id();
                mapping.provider = provider;
                mapping.externalId = brickLinkIds.first();
                mapping.status = ExternalMappingStatus::Mapped;
                mapping.source = QStringLiteral("Rebrickable");
                mapping.notes =
                    QStringLiteral("Resolved from Rebrickable external color IDs.");

                if (!mappingRepository.upsert(mapping)) {
                    result.message =
                        QStringLiteral("Unable to save mapped BrickLink color.");
                    emit colorMappingsRefreshed(result);
                    return;
                }

                continue;
            }
        }

        // Never guess. Preserve a previously confirmed Mapped/Unsupported
        // record when the current provider payload cannot resolve it.
        if (existing
            && existing->status != ExternalMappingStatus::Unknown) {
            continue;
        }

        ExternalColorMapping mapping;
        mapping.colorId = color.id();
        mapping.provider = provider;
        mapping.status = ExternalMappingStatus::Unknown;
        mapping.source = QStringLiteral("Rebrickable");

        if (brickLinkIds.isEmpty()) {
            mapping.notes =
                QStringLiteral("No BrickLink external color ID returned by Rebrickable.");
        } else {
            mapping.notes =
                QStringLiteral("Multiple or invalid BrickLink color IDs returned: %1")
                    .arg(brickLinkIds.join(QStringLiteral(", ")));
        }

        if (!mappingRepository.upsert(mapping)) {
            result.message =
                QStringLiteral("Unable to save unresolved BrickLink color mapping.");
            emit colorMappingsRefreshed(result);
            return;
        }
    }

    result.mapped =
        mappingRepository.countByProviderAndStatus(
            provider,
            ExternalMappingStatus::Mapped);

    result.unsupported =
        mappingRepository.countByProviderAndStatus(
            provider,
            ExternalMappingStatus::Unsupported);

    result.unknown =
        mappingRepository.countByProviderAndStatus(
            provider,
            ExternalMappingStatus::Unknown);

    result.success = true;
    result.message =
        QStringLiteral("BrickLink color mapping refresh completed.");

    qInfo() << "BrickLink color mapping refresh completed."
            << "BrickSuiteColors:" << result.brickSuiteColors
            << "RebrickableColors:" << result.rebrickableColors
            << "Matched:" << result.matchedBrickSuiteColors
            << "Mapped:" << result.mapped
            << "Unsupported:" << result.unsupported
            << "Unknown:" << result.unknown;

    emit colorMappingsRefreshed(result);
}
