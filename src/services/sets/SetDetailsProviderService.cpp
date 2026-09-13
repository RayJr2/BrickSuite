/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "SetDetailsProviderService.h"

#include "../../services/RebrickableApiClient.h"
#include "../../settings/UserSettings.h"
#include "../../api/ApiProviderStatusRegistry.h"
#include "../../api/brickset/BricksetUsagePolicy.h"

#include <QDebug>
#include <QPointer>

SetDetailsProviderService::SetDetailsProviderService(QObject* parent)
    : QObject(parent)
    , m_bricksetService(new BricksetService(this))
    , m_rebrickableApiClient(new RebrickableApiClient(this))
{
    connect(m_bricksetService,
            &BricksetService::setDetailsFinished,
            this,
            [this](const BricksetService::SetDetailsResult& bricksetResult) {
                if (bricksetResult.requestedSetNumber != m_setNumber)
                    return;

                if (bricksetResult.success) {
                    Result result;
                    result.hasEnrichment = true;
                    result.bricksetAttempted = true;
                    result.source = Source::Brickset;
                    result.setNumber = m_setNumber;
                    result.message = bricksetResult.message;
                    result.brickset = bricksetResult.set;

                    emit detailsReady(result);
                    return;
                }

                const bool ordinaryNotFound =
                    bricksetResult.matches == 0
                    && bricksetResult.message.contains(QStringLiteral("not found"),
                                                       Qt::CaseInsensitive);

                if (!ordinaryNotFound) {
                    qWarning() << "Brickset Set Details enrichment unavailable; "
                                  "falling back to Rebrickable."
                               << "Set:" << m_setNumber
                               << "ErrorType:" << static_cast<int>(bricksetResult.error.type)
                               << "HTTP:" << bricksetResult.httpStatusCode
                               << "Message:" << bricksetResult.message;
                }

                requestRebrickable(true, bricksetResult.message);
            });

    connect(m_rebrickableApiClient,
            &RebrickableApiClient::setDetailsFinished,
            this,
            [this](const RebrickableApiClient::SetDetailsResult& rebrickableResult) {
                Result result;
                result.bricksetAttempted = m_bricksetAttempted;
                result.usedFallback = m_usedFallback;
                result.setNumber = m_setNumber;
                result.fallbackReason = m_fallbackReason;

                if (rebrickableResult.success) {
                    result.hasEnrichment = true;
                    result.source = Source::Rebrickable;
                    result.message = rebrickableResult.message;
                    result.rebrickable = rebrickableResult.set;
                } else {
                    result.source = Source::None;
                    result.message = rebrickableResult.message;

                    qWarning() << "Set Details provider enrichment failed."
                               << "Set:" << m_setNumber
                               << "HTTP:" << rebrickableResult.httpStatusCode
                               << "Message:" << rebrickableResult.message;
                }

                emit detailsReady(result);
            });
}

void SetDetailsProviderService::requestDetails(const QString& setNumber)
{
    m_setNumber = setNumber.trimmed();
    m_fallbackReason.clear();
    m_bricksetAttempted = false;

    if (m_setNumber.isEmpty()) {
        Result result;
        result.message = QStringLiteral("Set number is empty.");
        emit detailsReady(result);
        return;
    }

    m_usedFallback = false;

    requestPreferredProvider();
}

void SetDetailsProviderService::requestPreferredProvider()
{
    UserSettings& settings = UserSettings::instance();

    const QString bricksetApiKey = settings.bricksetApiKey().trimmed();

    const ApiProviderStatusRegistry& providerStatus =
        ApiProviderStatusRegistry::instance();

    if (!bricksetApiKey.isEmpty()
        && providerStatus.isConnected(ApiProvider::Brickset)) {
        const int threshold = settings.bricksetDailyGetSetsThreshold();
        QPointer<SetDetailsProviderService> self(this);
        BricksetUsagePolicy::instance().requestAdmission(
            threshold,
            [self, bricksetApiKey, threshold](BricksetUsagePolicy::Decision decision) {
                if (!self) return;
                if (decision == BricksetUsagePolicy::Decision::Admitted) {
                    self->m_bricksetAttempted = true;
                    self->m_bricksetService->getSetDetails(self->m_setNumber, bricksetApiKey);
                    return;
                }
                const auto usage = BricksetUsagePolicy::instance().state(threshold);
                const QString reason = decision == BricksetUsagePolicy::Decision::ThresholdReached
                    ? QStringLiteral("Brickset daily getSets threshold reached (%1 / %2).")
                          .arg(usage.effectiveCount).arg(threshold)
                    : QStringLiteral("Brickset getSets usage could not be verified safely.");
                self->requestRebrickable(true, reason);
            },
            [self, bricksetApiKey]() {
                if (self) self->m_bricksetService->getKeyUsageStats(bricksetApiKey);
            });
        return;
    }

    requestRebrickable(false);
}

void SetDetailsProviderService::requestRebrickable(bool usedFallback,
                                                   const QString& fallbackReason)
{
    m_usedFallback = usedFallback;
    m_fallbackReason = fallbackReason;

    UserSettings& settings = UserSettings::instance();
    const QString rebrickableApiKey = settings.rebrickableApiKey().trimmed();

    const bool rebrickableConnected =
        ApiProviderStatusRegistry::instance().isConnected(ApiProvider::Rebrickable);

    if (rebrickableApiKey.isEmpty() || !rebrickableConnected) {
        Result result;
        result.bricksetAttempted = m_bricksetAttempted;
        result.usedFallback = usedFallback;
        result.setNumber = m_setNumber;
        result.source = Source::None;

        if (usedFallback && !fallbackReason.isEmpty()) {
            result.message =
                QStringLiteral("Brickset enrichment was unavailable and "
                               "Rebrickable is not currently connected. %1")
                    .arg(fallbackReason);
        } else if (rebrickableApiKey.isEmpty()) {
            result.message =
                QStringLiteral("No connected set-details provider is available.");
        } else {
            result.message =
                QStringLiteral("Rebrickable is not currently connected and "
                               "Brickset enrichment is unavailable.");
        }

        emit detailsReady(result);
        return;
    }

    m_rebrickableApiClient->getSetDetails(m_setNumber, rebrickableApiKey);
}
