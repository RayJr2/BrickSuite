/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include <QString>

class BrickLinkPartResolver
{
public:
    enum class ResolutionStatus
    {
        Direct,
        ExternalId,
        UserOverride,
        MappedOverride,
        NeedsReview
    };

    struct Result
    {
        int partId = 0;
        QString sourcePartNumber;
        QString itemId;

        ResolutionStatus status = ResolutionStatus::NeedsReview;

        bool canExport = false;
        QString message;
    };

    Result resolve(int partId, const QString& partNumber) const;

    static QString statusText(ResolutionStatus status);
};
