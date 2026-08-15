#pragma once

#include <QObject>
#include <QSet>
#include <QString>

class WorkspaceContext;
class PartImageService;
class RebrickableApiClient;
class QTimer;

class BackgroundPartColorImageCacheService : public QObject
{
    Q_OBJECT

public:
    explicit BackgroundPartColorImageCacheService(WorkspaceContext& workspaceContext,
                                                  QObject* parent = nullptr);

    void start();
    void stop();

    void rebuildQueue();

signals:
    void partColorImageCached(const QString& partNumber,
                              int rebrickableColorId,
                              const QString& imagePath);

private:
    struct WorkItem
    {
        QString partNumber;
        int rebrickableColorId = -1;
    };

    void processNext();

    void scheduleNext();

    QString workKey(const QString& partNumber, int rebrickableColorId) const;

    WorkspaceContext& m_workspaceContext;

    PartImageService* m_partImageService = nullptr;
    RebrickableApiClient* m_apiClient = nullptr;
    QTimer* m_timer = nullptr;

    QList<WorkItem> m_workItems;

    QSet<QString> m_skippedThisRun;

    bool m_started = false;
    bool m_requestInProgress = false;

    int m_generation = 0;

    static constexpr int BackgroundIntervalMs = 10000;
};
