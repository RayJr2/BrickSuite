/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
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
