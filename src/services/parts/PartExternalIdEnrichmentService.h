/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "../../api/rebrickable/RebrickableService.h"

#include <QHash>
#include <QObject>
#include <QSet>

class RebrickableApiClient;

class PartExternalIdEnrichmentService : public QObject
{
    Q_OBJECT
public:
    enum class LookupOutcome
    {
        Loaded,
        Unavailable,
        RetryableFailure,
        PersistenceFailure
    };
    Q_ENUM(LookupOutcome)

    explicit PartExternalIdEnrichmentService(QObject* parent = nullptr,
                                              bool dispatchNetworkRequests = true);
    ~PartExternalIdEnrichmentService() override;

    static PartExternalIdEnrichmentService* instance();

    void ensureExternalIds(int partId);
    void ensureExternalIds(const QList<int>& partIds);
    void ensureExternalIdsForPartNumber(const QString& partNumber);
    // Uses the existing batched provider queue to obtain general image metadata
    // even when external identity enrichment was completed previously.
    void ensureGeneralImageMetadata(const QList<int>& partIds);
    bool isLookupPending(int partId) const;
    bool persistExternalIds(int partId,
                            const QHash<QString, QStringList>& externalIds);

    // Public result entry points keep provider I/O replaceable in tests.
    void handleBatchResult(const RebrickableService::PartImageUrlsResult& result);
    void handleDetailsResult(const RebrickableService::PartDetailsResult& result);

signals:
    void batchRequested(const QStringList& partNumbers);
    void detailsRequested(const QString& partNumber);
    void generalImageMetadataReady(const QString& partNumber, const QString& imageUrl);
    void externalIdsLookupFinished(int partId, LookupOutcome outcome);

private:
    void dispatchPending();
    void requestDirectDetails(const QString& partNumber);
    static bool isLikelyPrintedPartNumber(const QString& partNumber);

    RebrickableApiClient* m_apiClient = nullptr;
    QSet<int> m_queuedPartIds;
    QSet<int> m_activePartIds;
    QSet<int> m_imageOnlyPartIds;
    QSet<int> m_generalImageRequestedThisSession;
    QSet<QString> m_directPending;
    QHash<QString, int> m_partIdByRequestedNumber;
    bool m_dispatchScheduled = false;
    bool m_dispatchNetworkRequests = true;
    static PartExternalIdEnrichmentService* s_instance;
    static constexpr int BatchSize = 20;
};
