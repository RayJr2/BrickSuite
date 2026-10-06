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

#include "Part.h"
#include "PartAlias.h"
#include "PartRelationship.h"

#include <QList>
#include <QString>

enum class PartResolutionStatus
{
    ExactMatch,
    AliasMatch,
    RelationshipCandidate,
    NotFound,
    Ambiguous
};

struct PartResolutionCandidate
{
    Part part;
    PartRelationshipType relationshipType = PartRelationshipType::Unknown;
    QString sourceRelationshipType;
    QString source;
};

struct PartResolutionResult
{
    QString inputPartNumber;

    PartResolutionStatus status = PartResolutionStatus::NotFound;

    Part part;

    bool hasResolvedPart = false;

    PartAlias alias;

    bool matchedAlias = false;

    QList<PartResolutionCandidate> relationshipCandidates;

    QString message;
};

inline QString partResolutionStatusText(PartResolutionStatus status)
{
    switch (status) {
    case PartResolutionStatus::ExactMatch:
        return QStringLiteral("Exact Match");
    case PartResolutionStatus::AliasMatch:
        return QStringLiteral("Alias Match");
    case PartResolutionStatus::RelationshipCandidate:
        return QStringLiteral("Relationship Candidate");
    case PartResolutionStatus::Ambiguous:
        return QStringLiteral("Ambiguous");
    case PartResolutionStatus::NotFound:
    default:
        return QStringLiteral("Not Found");
    }
}
