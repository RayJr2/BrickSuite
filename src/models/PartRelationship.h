/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include <QString>

enum class PartRelationshipType
{
    Unknown,
    Alternate,
    Mold,
    Print,
    Pattern,
    Subpart,
    Related
};

inline QString partRelationshipTypeToString(PartRelationshipType type)
{
    switch (type) {
    case PartRelationshipType::Alternate:
        return QStringLiteral("Alternate");
    case PartRelationshipType::Mold:
        return QStringLiteral("Mold");
    case PartRelationshipType::Print:
        return QStringLiteral("Print");
    case PartRelationshipType::Pattern:
        return QStringLiteral("Pattern");
    case PartRelationshipType::Subpart:
        return QStringLiteral("Subpart");
    case PartRelationshipType::Related:
        return QStringLiteral("Related");
    case PartRelationshipType::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

inline PartRelationshipType partRelationshipTypeFromString(const QString& value)
{
    if (value.compare(QStringLiteral("Alternate"), Qt::CaseInsensitive) == 0)
        return PartRelationshipType::Alternate;
    if (value.compare(QStringLiteral("Mold"), Qt::CaseInsensitive) == 0)
        return PartRelationshipType::Mold;
    if (value.compare(QStringLiteral("Print"), Qt::CaseInsensitive) == 0)
        return PartRelationshipType::Print;
    if (value.compare(QStringLiteral("Pattern"), Qt::CaseInsensitive) == 0)
        return PartRelationshipType::Pattern;
    if (value.compare(QStringLiteral("Subpart"), Qt::CaseInsensitive) == 0)
        return PartRelationshipType::Subpart;
    if (value.compare(QStringLiteral("Related"), Qt::CaseInsensitive) == 0)
        return PartRelationshipType::Related;

    return PartRelationshipType::Unknown;
}

struct PartRelationship
{
    int id = 0;

    int parentPartId = 0;
    int childPartId = 0;

    PartRelationshipType relationshipType = PartRelationshipType::Unknown;

    // Provider-native relationship code, e.g. Rebrickable A/M/P/R/B/T.
    QString sourceRelationshipType;

    QString source;
    bool isActive = true;

    QString createdUtc;
    QString modifiedUtc;
};
