#include "PartExternalIdEnrichmentService.h"

#include "../RebrickableApiClient.h"
#include "../mappings/BrickLinkMappingService.h"
#include "../../repositories/ExternalPartIdentifierRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../settings/UserSettings.h"

#include <QDebug>
#include <QRegularExpression>
#include <QTimer>

PartExternalIdEnrichmentService* PartExternalIdEnrichmentService::s_instance = nullptr;

PartExternalIdEnrichmentService::PartExternalIdEnrichmentService(QObject* parent,
                                                                 bool dispatchNetworkRequests)
    : QObject(parent), m_dispatchNetworkRequests(dispatchNetworkRequests)
{
    s_instance = this;
    if (m_dispatchNetworkRequests) {
        m_apiClient = new RebrickableApiClient(this);
        connect(m_apiClient, &RebrickableApiClient::partImageUrlsFinished,
                this, &PartExternalIdEnrichmentService::handleBatchResult);
        connect(m_apiClient, &RebrickableApiClient::partDetailsFinished,
                this, &PartExternalIdEnrichmentService::handleDetailsResult);
    }
}

PartExternalIdEnrichmentService::~PartExternalIdEnrichmentService()
{
    if (s_instance == this) s_instance = nullptr;
}

PartExternalIdEnrichmentService* PartExternalIdEnrichmentService::instance() { return s_instance; }

void PartExternalIdEnrichmentService::ensureExternalIdsForPartNumber(const QString& partNumber)
{
    const auto part = PartRepository().getByPartNumber(partNumber.trimmed());
    if (part) ensureExternalIds(part->id());
}

void PartExternalIdEnrichmentService::ensureExternalIds(int partId)
{
    if (partId <= 0 || m_queuedPartIds.contains(partId) || m_activePartIds.contains(partId)) return;
    if (ExternalPartIdentifierRepository().isLookupComplete(partId, QStringLiteral("Rebrickable"))) return;
    const auto part = PartRepository().getById(partId);
    if (!part || m_directPending.contains(part->partNumber().trimmed().toLower())) return;
    m_queuedPartIds.insert(partId);
    if (!m_dispatchScheduled) {
        m_dispatchScheduled = true;
        QTimer::singleShot(0, this, &PartExternalIdEnrichmentService::dispatchPending);
    }
}

void PartExternalIdEnrichmentService::ensureExternalIds(const QList<int>& partIds)
{
    for (int partId : partIds) ensureExternalIds(partId);
}

void PartExternalIdEnrichmentService::dispatchPending()
{
    m_dispatchScheduled = false;
    if (m_queuedPartIds.isEmpty()) return;
    QStringList numbers;
    PartRepository repository;
    while (!m_queuedPartIds.isEmpty() && numbers.size() < BatchSize) {
        const int id = *m_queuedPartIds.begin();
        m_queuedPartIds.remove(id);
        const auto part = repository.getById(id);
        if (!part || !part->isActive() || part->partNumber().trimmed().isEmpty()) continue;
        const QString number = part->partNumber().trimmed();
        m_activePartIds.insert(id);
        m_partIdByRequestedNumber.insert(number.toLower(), id);
        numbers.append(number);
    }
    if (!m_queuedPartIds.isEmpty()) {
        m_dispatchScheduled = true;
        QTimer::singleShot(0, this, &PartExternalIdEnrichmentService::dispatchPending);
    }
    if (numbers.isEmpty()) return;
    emit batchRequested(numbers);
    if (m_apiClient) {
        const QString key = UserSettings::instance().rebrickableApiKey().trimmed();
        if (!key.isEmpty() && !RebrickableApiClient::isSessionBlocked())
            m_apiClient->getPartImageUrls(numbers, key, RebrickableApiClient::RequestPriority::Background);
        else
            for (const QString& number : numbers) m_activePartIds.remove(m_partIdByRequestedNumber.take(number.toLower()));
    }
}

bool PartExternalIdEnrichmentService::persistExternalIds(
    int partId, const QHash<QString, QStringList>& externalIds)
{
    QHash<QString, QStringList> validIds;
    for (auto it = externalIds.constBegin(); it != externalIds.constEnd(); ++it) {
        const QString provider = it.key().trimmed();
        if (provider.isEmpty()) continue;
        QStringList values;
        for (const QString& raw : it.value()) {
            const QString value = raw.trimmed();
            if (!value.isEmpty() && !values.contains(value, Qt::CaseInsensitive)) values.append(value);
        }
        if (!values.isEmpty()) validIds.insert(provider, values);
    }
    ExternalPartIdentifierRepository repository;
    if (!repository.replaceProviderIds(partId, validIds, QStringLiteral("Rebrickable"))) {
        qWarning() << "Unable to persist Rebrickable Part external IDs." << "PartId:" << partId;
        return false;
    }
    QStringList brickLinkIds;
    for (auto it = validIds.constBegin(); it != validIds.constEnd(); ++it) {
        if (it.key().compare(QStringLiteral("BrickLink"), Qt::CaseInsensitive) == 0)
            brickLinkIds.append(it.value());
    }
    brickLinkIds.removeDuplicates();
    if (brickLinkIds.size() == 1
        && !BrickLinkMappingService().storePartExternalIds(partId, validIds)) {
        // A unique BrickLink ID has a deterministic legacy projection. Do not
        // declare the lookup complete when that required local write failed.
        return false;
    }
    if (!repository.setLookupStatus(partId, QStringLiteral("Rebrickable"), QStringLiteral("Loaded"))) {
        qWarning() << "Unable to mark Rebrickable Part external IDs loaded." << "PartId:" << partId;
        return false;
    }
    return true;
}

void PartExternalIdEnrichmentService::handleBatchResult(
    const RebrickableService::PartImageUrlsResult& result)
{
    QSet<QString> returned;
    for (const auto& item : result.parts) {
        const QString key = item.partNumber.trimmed().toLower();
        returned.insert(key);
        const int partId = m_partIdByRequestedNumber.value(key);
        if (result.success && partId > 0) {
            const bool persisted = persistExternalIds(partId, item.externalIds);
            bool hasBrickLink = false;
            for (auto it = item.externalIds.constBegin(); it != item.externalIds.constEnd(); ++it)
                if (it.key().compare(QStringLiteral("BrickLink"), Qt::CaseInsensitive) == 0
                    && !it.value().isEmpty()) hasBrickLink = true;
            if (persisted && isLikelyPrintedPartNumber(item.partNumber) && !hasBrickLink)
                requestDirectDetails(item.partNumber);
        }
        if (!item.partImageUrl.trimmed().isEmpty()) emit generalImageMetadataReady(item.partNumber, item.partImageUrl);
        m_activePartIds.remove(partId); m_partIdByRequestedNumber.remove(key);
    }
    for (const QString& requested : result.requestedPartNumbers) {
        const QString key = requested.trimmed().toLower();
        if (returned.contains(key)) continue;
        const int partId = m_partIdByRequestedNumber.take(key);
        m_activePartIds.remove(partId);
        if (!result.success || partId <= 0) continue; // transient/provider failures remain retryable
        if (isLikelyPrintedPartNumber(requested)) requestDirectDetails(requested);
        else ExternalPartIdentifierRepository().setLookupStatus(
            partId, QStringLiteral("Rebrickable"), QStringLiteral("Unavailable"));
    }
}

void PartExternalIdEnrichmentService::requestDirectDetails(const QString& partNumber)
{
    const QString key = partNumber.trimmed().toLower();
    if (key.isEmpty() || m_directPending.contains(key)) return;
    m_directPending.insert(key);
    emit detailsRequested(partNumber);
    if (m_apiClient) {
        const QString apiKey = UserSettings::instance().rebrickableApiKey().trimmed();
        if (!apiKey.isEmpty()) m_apiClient->getPartDetails(
            partNumber, apiKey, RebrickableApiClient::RequestPriority::Background);
    }
}

void PartExternalIdEnrichmentService::handleDetailsResult(
    const RebrickableService::PartDetailsResult& result)
{
    const QString key = result.requestedPartNumber.trimmed().toLower();
    if (!m_directPending.remove(key)) return;
    const auto part = PartRepository().getByPartNumber(result.requestedPartNumber);
    if (!part) return;
    if (result.success) {
        persistExternalIds(part->id(), result.part.externalIds);
        if (!result.part.partImageUrl.trimmed().isEmpty())
            emit generalImageMetadataReady(result.part.partNumber, result.part.partImageUrl);
    } else if (result.httpStatusCode == 404) {
        ExternalPartIdentifierRepository().setLookupStatus(
            part->id(), QStringLiteral("Rebrickable"), QStringLiteral("Unavailable"));
    }
}

bool PartExternalIdEnrichmentService::isLikelyPrintedPartNumber(const QString& partNumber)
{
    static const QRegularExpression pattern(QStringLiteral("p(?:r|b)\\d"),
                                             QRegularExpression::CaseInsensitiveOption);
    return pattern.match(partNumber).hasMatch();
}
