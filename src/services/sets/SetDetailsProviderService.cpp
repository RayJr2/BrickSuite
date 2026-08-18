/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "SetDetailsProviderService.h"

#include "../../services/RebrickableApiClient.h"
#include "../../settings/UserSettings.h"

#include <QDebug>

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
                result.usedFallback = m_bricksetAttempted;
                result.setNumber = m_setNumber;

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
    m_bricksetAttempted = false;

    if (m_setNumber.isEmpty()) {
        Result result;
        result.message = QStringLiteral("Set number is empty.");
        emit detailsReady(result);
        return;
    }

    UserSettings& settings = UserSettings::instance();

    const QString bricksetApiKey = settings.bricksetApiKey().trimmed();

    // A previously verified Brickset credential makes Brickset the preferred
    // enrichment provider. The request itself still determines whether the
    // provider is reachable in this session; any failure falls back cleanly.
    if (!bricksetApiKey.isEmpty()
        && settings.bricksetConnectionPreviouslyVerified()) {
        m_bricksetAttempted = true;
        m_bricksetService->getSetDetails(m_setNumber, bricksetApiKey);
        return;
    }

    requestRebrickable(false);
}

void SetDetailsProviderService::requestRebrickable(bool usedFallback,
                                                   const QString& fallbackReason)
{
    UserSettings& settings = UserSettings::instance();
    const QString rebrickableApiKey = settings.rebrickableApiKey().trimmed();

    if (rebrickableApiKey.isEmpty()) {
        Result result;
        result.bricksetAttempted = m_bricksetAttempted;
        result.usedFallback = usedFallback;
        result.setNumber = m_setNumber;
        result.source = Source::None;

        if (usedFallback && !fallbackReason.isEmpty()) {
            result.message =
                QStringLiteral("Brickset enrichment was unavailable and no "
                               "Rebrickable API key is configured. %1")
                    .arg(fallbackReason);
        } else {
            result.message = QStringLiteral("No set-details provider is configured.");
        }

        emit detailsReady(result);
        return;
    }

    m_rebrickableApiClient->getSetDetails(m_setNumber, rebrickableApiKey);
}
