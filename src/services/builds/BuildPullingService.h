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
 */

#pragma once

#include <QList>
#include <QString>

class BuildPullingService
{
public:
    struct PullingItem
    {
        int buildId = 0;
        int buildRequirementId = 0;
        int allocationId = 0;
        int inventoryRecordId = 0;
        int storageLocationId = 0;

        int partId = 0;
        int colorId = 0;

        QString partNumber;
        QString partName;
        QString colorName;
        QString storagePath;

        int quantityRequired = 0;
        int quantityPulledForRequirement = 0;
        int quantityAllocatedHere = 0;

        bool isSubstitution = false;
        int originalPartId = 0;
        int originalColorId = 0;
        QString originalPartNumber;
        QString originalColorName;
    };

    struct PullingSummary
    {
        int buildId = 0;
        QString buildName;
        QString setNumber;

        int totalRequired = 0;
        int totalPulled = 0;
        int remaining = 0;
        int locationsRemaining = 0;
    };

    struct PullingView
    {
        PullingSummary summary;
        QList<PullingItem> items;
        bool success = false;
        QString message;
    };

    struct PullRequest
    {
        int allocationId = 0;
        int quantity = 0;
    };

    struct PullResult
    {
        bool success = false;
        QString message;
        int rowsPulled = 0;
        int piecesPulled = 0;
    };

    PullingView getPullingView(int buildId) const;

    PullResult recordPull(int allocationId, int quantity) const;
    PullResult recordPulls(const QList<PullRequest>& requests) const;

private:
    bool applyPull(const PullRequest& request,
                   int& buildId,
                   int& piecesPulled,
                   QString& errorMessage) const;

    QString storagePath(int storageLocationId) const;
};
