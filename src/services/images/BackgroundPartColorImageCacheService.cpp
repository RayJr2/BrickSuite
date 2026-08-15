#include "BackgroundPartColorImageCacheService.h"

#include "../../app/WorkspaceContext.h"

#include "../../models/Color.h"
#include "../../models/InventoryRecord.h"
#include "../../models/Part.h"

#include "../../repositories/ColorRepository.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/PartRepository.h"

#include "../../settings/UserSettings.h"

#include "../RebrickableApiClient.h"
#include "PartImageService.h"

#include <QMetaObject>
#include <QTimer>
#include <memory>

BackgroundPartColorImageCacheService::BackgroundPartColorImageCacheService(
    WorkspaceContext& workspaceContext, QObject* parent)
    : QObject(parent)
    , m_workspaceContext(workspaceContext)
{
    m_partImageService = new PartImageService(this);

    m_apiClient = new RebrickableApiClient(this);

    m_timer = new QTimer(this);

    m_timer->setSingleShot(true);

    connect(m_timer, &QTimer::timeout, this, &BackgroundPartColorImageCacheService::processNext);

    connect(&m_workspaceContext, &WorkspaceContext::currentWorkspaceChanged, this, [this](int) {
        rebuildQueue();
    });
}

void BackgroundPartColorImageCacheService::start()
{
    if (m_started)
        return;

    m_started = true;

    rebuildQueue();
}

void BackgroundPartColorImageCacheService::stop()
{
    m_started = false;

    m_timer->stop();

    m_workItems.clear();

    m_skippedThisRun.clear();

    ++m_generation;
}

QString BackgroundPartColorImageCacheService::workKey(const QString& partNumber,
                                                      int rebrickableColorId) const
{
    return QString("%1|%2").arg(partNumber.trimmed()).arg(rebrickableColorId);
}

void BackgroundPartColorImageCacheService::rebuildQueue()
{
    ++m_generation;

    m_timer->stop();

    m_workItems.clear();

    m_skippedThisRun.clear();

    //
    // An existing request cannot be cancelled safely here.
    // Its completion callback will notice the changed
    // generation and will not continue the old queue.
    //
    if (!m_started || !m_workspaceContext.hasCurrentWorkspace()) {
        return;
    }

    if (UserSettings::instance().rebrickableApiKey().trimmed().isEmpty()) {
        return;
    }

    if (RebrickableApiClient::isSessionBlocked())
        return;

    InventoryRecordRepository inventoryRepository;

    PartRepository partRepository;
    ColorRepository colorRepository;

    const QList<InventoryRecord> records = inventoryRepository.getByWorkspace(
        m_workspaceContext.currentWorkspaceId());

    QSet<QString> addedKeys;

    for (const InventoryRecord& record : records) {
        if (record.quantity() <= 0)
            continue;

        const std::optional<Part> part = partRepository.getById(record.partId());

        if (!part)
            continue;

        const std::optional<Color> color = colorRepository.getById(record.colorId());

        if (!color)
            continue;

        const QString partNumber = part->partNumber().trimmed();

        const int rebrickableColorId = color->rebrickableId();

        if (partNumber.isEmpty() || rebrickableColorId < 0) {
            continue;
        }

        if (m_partImageService->hasCachedPartColorImage(partNumber, rebrickableColorId)) {
            continue;
        }

        const QString key = workKey(partNumber, rebrickableColorId);

        if (addedKeys.contains(key))
            continue;

        addedKeys.insert(key);

        WorkItem item;

        item.partNumber = partNumber;

        item.rebrickableColorId = rebrickableColorId;

        m_workItems.append(item);
    }

    //
    // If an old request is still completing, let it
    // finish. Its generation check prevents it from
    // advancing this new queue incorrectly.
    //
    if (!m_requestInProgress && !m_workItems.isEmpty()) {
        m_timer->start(BackgroundIntervalMs);
    }
}

void BackgroundPartColorImageCacheService::processNext()
{
    if (!m_started || m_requestInProgress) {
        return;
    }

    if (!m_workspaceContext.hasCurrentWorkspace())
        return;

    if (RebrickableApiClient::isSessionBlocked())
        return;

    const QString apiKey = UserSettings::instance().rebrickableApiKey().trimmed();

    if (apiKey.isEmpty())
        return;

    //
    // Discard anything that became cached while it
    // waited in the queue.
    //
    while (!m_workItems.isEmpty()) {
        const WorkItem item = m_workItems.first();

        const QString key = workKey(item.partNumber, item.rebrickableColorId);

        if (m_skippedThisRun.contains(key)
            || m_partImageService->hasCachedPartColorImage(item.partNumber,
                                                           item.rebrickableColorId)) {
            m_workItems.removeFirst();

            continue;
        }

        break;
    }

    if (m_workItems.isEmpty())
        return;

    const WorkItem item = m_workItems.takeFirst();

    const QString key = workKey(item.partNumber, item.rebrickableColorId);

    const int requestGeneration = m_generation;

    m_requestInProgress = true;

    //
    // One-shot connection for this background item.
    //
    auto connection = std::make_shared<QMetaObject::Connection>();

    *connection
        = connect(m_apiClient,
                  &RebrickableApiClient::partColorDetailsFinished,
                  this,
                  [this, item, key, requestGeneration, connection](
                      const RebrickableApiClient::PartColorDetailsResult& result) {
                      //
                      // Ignore signals for another Part/Color
                      // request that might be using this same
                      // application-scoped client.
                      //
                      if (result.partColor.partNumber != item.partNumber
                          || result.partColor.rebrickableColorId != item.rebrickableColorId) {
                          return;
                      }

                      disconnect(*connection);

                      m_requestInProgress = false;

                      if (requestGeneration != m_generation) {
                          if (m_started && !m_workItems.isEmpty()) {
                              scheduleNext();
                          }

                          return;
                      }

                      if (!result.success || result.partColor.partImageUrl.trimmed().isEmpty()) {
                          m_skippedThisRun.insert(key);

                          if (!RebrickableApiClient::isSessionBlocked()) {
                              scheduleNext();
                          }

                          return;
                      }

                      //
                      // Downloading the CDN image itself does
                      // not consume another Rebrickable API
                      // request.
                      //
                      auto imageConnection = std::make_shared<QMetaObject::Connection>();

                      auto failureConnection = std::make_shared<QMetaObject::Connection>();

                      *imageConnection = connect(m_partImageService,
                                                 &PartImageService::partColorImageReady,
                                                 this,
                                                 [this,
                                                  item,
                                                  requestGeneration,
                                                  imageConnection,
                                                  failureConnection](const QString& partNumber,
                                                                     int colorId,
                                                                     const QString& imagePath) {
                                                     if (partNumber != item.partNumber
                                                         || colorId != item.rebrickableColorId) {
                                                         return;
                                                     }

                                                     disconnect(*imageConnection);

                                                     disconnect(*failureConnection);

                                                     if (requestGeneration != m_generation) {
                                                         return;
                                                     }

                                                     emit partColorImageCached(partNumber,
                                                                               colorId,
                                                                               imagePath);

                                                     scheduleNext();
                                                 });

                      *failureConnection = connect(m_partImageService,
                                                   &PartImageService::partColorImageFailed,
                                                   this,
                                                   [this,
                                                    item,
                                                    key,
                                                    requestGeneration,
                                                    imageConnection,
                                                    failureConnection](const QString& partNumber,
                                                                       int colorId,
                                                                       const QString&) {
                                                       if (partNumber != item.partNumber
                                                           || colorId != item.rebrickableColorId) {
                                                           return;
                                                       }

                                                       disconnect(*imageConnection);

                                                       disconnect(*failureConnection);

                                                       if (requestGeneration != m_generation) {
                                                           return;
                                                       }

                                                       m_skippedThisRun.insert(key);

                                                       scheduleNext();
                                                   });

                      m_partImageService->requestPartColorImage(result.partColor.partNumber,
                                                                result.partColor.rebrickableColorId,
                                                                result.partColor.partImageUrl);
                  });

    m_apiClient->getPartColorDetails(item.partNumber,
                                     item.rebrickableColorId,
                                     apiKey,
                                     RebrickableApiClient::RequestPriority::Background);
}

void BackgroundPartColorImageCacheService::scheduleNext()
{
    if (!m_started || m_workItems.isEmpty() || RebrickableApiClient::isSessionBlocked()) {
        return;
    }

    m_timer->start(BackgroundIntervalMs);
}