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
    explicit PartExternalIdEnrichmentService(QObject* parent = nullptr,
                                              bool dispatchNetworkRequests = true);
    ~PartExternalIdEnrichmentService() override;

    static PartExternalIdEnrichmentService* instance();

    void ensureExternalIds(int partId);
    void ensureExternalIds(const QList<int>& partIds);
    void ensureExternalIdsForPartNumber(const QString& partNumber);
    bool persistExternalIds(int partId,
                            const QHash<QString, QStringList>& externalIds);

    // Public result entry points keep provider I/O replaceable in tests.
    void handleBatchResult(const RebrickableService::PartImageUrlsResult& result);
    void handleDetailsResult(const RebrickableService::PartDetailsResult& result);

signals:
    void batchRequested(const QStringList& partNumbers);
    void detailsRequested(const QString& partNumber);
    void generalImageMetadataReady(const QString& partNumber, const QString& imageUrl);

private:
    void dispatchPending();
    void requestDirectDetails(const QString& partNumber);
    static bool isLikelyPrintedPartNumber(const QString& partNumber);

    RebrickableApiClient* m_apiClient = nullptr;
    QSet<int> m_queuedPartIds;
    QSet<int> m_activePartIds;
    QSet<QString> m_directPending;
    QHash<QString, int> m_partIdByRequestedNumber;
    bool m_dispatchScheduled = false;
    bool m_dispatchNetworkRequests = true;
    static PartExternalIdEnrichmentService* s_instance;
    static constexpr int BatchSize = 20;
};
