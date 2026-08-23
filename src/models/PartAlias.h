/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include <QString>

enum class PartAliasType
{
    Unknown,
    PartNumberMapping,
    RebrickableAlternate,
    RebrickableMold,
    UserConfirmed,
    Legacy,
    Molded
};

inline QString partAliasTypeToString(PartAliasType type)
{
    switch (type) {
    case PartAliasType::PartNumberMapping:
        return QStringLiteral("PartNumberMapping");
    case PartAliasType::RebrickableAlternate:
        return QStringLiteral("RebrickableAlternate");
    case PartAliasType::RebrickableMold:
        return QStringLiteral("RebrickableMold");
    case PartAliasType::UserConfirmed:
        return QStringLiteral("UserConfirmed");
    case PartAliasType::Legacy:
        return QStringLiteral("Legacy");
    case PartAliasType::Molded:
        return QStringLiteral("Molded");
    case PartAliasType::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

inline PartAliasType partAliasTypeFromString(const QString& value)
{
    if (value.compare(QStringLiteral("PartNumberMapping"), Qt::CaseInsensitive) == 0)
        return PartAliasType::PartNumberMapping;
    if (value.compare(QStringLiteral("RebrickableAlternate"), Qt::CaseInsensitive) == 0)
        return PartAliasType::RebrickableAlternate;
    if (value.compare(QStringLiteral("RebrickableMold"), Qt::CaseInsensitive) == 0)
        return PartAliasType::RebrickableMold;
    if (value.compare(QStringLiteral("UserConfirmed"), Qt::CaseInsensitive) == 0)
        return PartAliasType::UserConfirmed;
    if (value.compare(QStringLiteral("Legacy"), Qt::CaseInsensitive) == 0)
        return PartAliasType::Legacy;
    if (value.compare(QStringLiteral("Molded"), Qt::CaseInsensitive) == 0)
        return PartAliasType::Molded;

    return PartAliasType::Unknown;
}

struct PartAlias
{
    int id = 0;
    int partId = 0;

    QString aliasPartNumber;
    PartAliasType aliasType = PartAliasType::Unknown;

    QString source;
    bool isActive = true;
    QString notes;

    QString createdUtc;
    QString modifiedUtc;
};
