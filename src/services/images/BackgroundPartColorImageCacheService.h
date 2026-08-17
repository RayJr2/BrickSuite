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

    static constexpr int BackgroundIntervalMs = 5000;
};
