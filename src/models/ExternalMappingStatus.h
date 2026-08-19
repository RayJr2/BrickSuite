/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include <QString>

enum class ExternalMappingStatus
{
    Unknown,
    Mapped,
    Unsupported
};

inline QString externalMappingStatusToString(ExternalMappingStatus status)
{
    switch (status) {
    case ExternalMappingStatus::Unknown:
        return QStringLiteral("Unknown");
    case ExternalMappingStatus::Mapped:
        return QStringLiteral("Mapped");
    case ExternalMappingStatus::Unsupported:
        return QStringLiteral("Unsupported");
    }

    return QStringLiteral("Unknown");
}

inline ExternalMappingStatus externalMappingStatusFromString(const QString& value)
{
    if (value.compare(QStringLiteral("Mapped"), Qt::CaseInsensitive) == 0)
        return ExternalMappingStatus::Mapped;

    if (value.compare(QStringLiteral("Unsupported"), Qt::CaseInsensitive) == 0)
        return ExternalMappingStatus::Unsupported;

    return ExternalMappingStatus::Unknown;
}
