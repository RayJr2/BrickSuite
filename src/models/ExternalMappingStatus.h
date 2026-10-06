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
