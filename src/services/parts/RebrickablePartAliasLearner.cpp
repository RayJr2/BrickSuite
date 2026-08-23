/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "RebrickablePartAliasLearner.h"

#include "../../models/PartAlias.h"
#include "../../repositories/PartAliasRepository.h"
#include "../../repositories/ExternalPartIdentifierRepository.h"
#include "../../repositories/PartRepository.h"

#include <QDebug>
#include <QSet>

RebrickablePartAliasLearner::Result
RebrickablePartAliasLearner::learnFromLocalExternalId(
    const QString& requestedPartNumber) const
{
    Result result;
    result.requestedPartNumber = requestedPartNumber.trimmed();

    if (result.requestedPartNumber.isEmpty())
        return result;

    ExternalPartIdentifierRepository externalRepository;
    const QList<ExternalPartIdentifier> matches =
        externalRepository.findByExternalId(result.requestedPartNumber, true);

    QSet<int> partIds;
    for (const ExternalPartIdentifier& item : matches)
        partIds.insert(item.partId);

    if (partIds.isEmpty()) {
        result.message = QStringLiteral("No local external-ID mapping was found.");
        return result;
    }

    if (partIds.size() != 1) {
        result.ambiguous = true;
        result.message =
            QStringLiteral("External ID %1 maps to multiple BrickSuite parts; "
                           "BrickSuite did not guess.")
                .arg(result.requestedPartNumber);
        return result;
    }

    PartRepository partRepository;
    const int canonicalPartId = *partIds.constBegin();
    const auto canonicalPart = partRepository.getById(canonicalPartId);

    if (!canonicalPart) {
        result.message =
            QStringLiteral("The local external-ID mapping points to a missing part.");
        return result;
    }

    PartAlias alias;
    alias.partId = canonicalPart->id();
    alias.aliasPartNumber = result.requestedPartNumber;
    alias.aliasType = PartAliasType::PartNumberMapping;
    alias.source = QStringLiteral("Rebrickable");
    alias.isActive = true;
    alias.notes = QStringLiteral("Learned from locally cached Rebrickable external IDs.");

    PartAliasRepository aliasRepository;
    if (!aliasRepository.upsert(alias)) {
        result.message = QStringLiteral("Unable to save the learned alias.");
        return result;
    }

    result.learned = true;
    result.canonicalPartId = canonicalPart->id();
    result.canonicalPartNumber = canonicalPart->partNumber();
    result.message =
        QStringLiteral("Learned local external-ID alias %1 -> %2.")
            .arg(result.requestedPartNumber, result.canonicalPartNumber);

    qInfo() << "Part alias learned from local Rebrickable external IDs."
            << "Alias:" << result.requestedPartNumber
            << "CanonicalPart:" << result.canonicalPartNumber;

    return result;
}

RebrickablePartAliasLearner::Result
RebrickablePartAliasLearner::learn(
    const RebrickableService::PartDetailsResult& providerResult) const
{
    Result result;
    result.requestedPartNumber =
        providerResult.requestedPartNumber.trimmed();

    if (!providerResult.success
        || result.requestedPartNumber.isEmpty()) {
        result.message = providerResult.message;
        return result;
    }

    QStringList candidates;

    for (const QString& value : providerResult.rebrickablePartIds) {
        const QString candidate = value.trimmed();

        if (!candidate.isEmpty()
            && !candidates.contains(candidate, Qt::CaseInsensitive)) {
            candidates.append(candidate);
        }
    }

    // Some current API responses may directly return a different canonical
    // part_num even without the explicit mapping array.
    const QString returnedPartNumber =
        providerResult.part.partNumber.trimmed();

    if (candidates.isEmpty()
        && !returnedPartNumber.isEmpty()
        && returnedPartNumber.compare(result.requestedPartNumber,
                                      Qt::CaseInsensitive) != 0) {
        candidates.append(returnedPartNumber);
    }

    if (candidates.isEmpty()) {
        result.message =
            QStringLiteral("Rebrickable did not return a canonical Part Number Mapping.");
        return result;
    }

    if (candidates.size() != 1) {
        result.ambiguous = true;
        result.message =
            QStringLiteral("Rebrickable returned %1 possible canonical part numbers; "
                           "BrickSuite did not guess.")
                .arg(candidates.size());
        return result;
    }

    result.canonicalPartNumber = candidates.first();

    PartRepository partRepository;
    const std::optional<Part> canonicalPart =
        partRepository.getByPartNumber(result.canonicalPartNumber);

    if (!canonicalPart) {
        result.message =
            QStringLiteral("Rebrickable mapped %1 to %2, but %2 is not in the "
                           "BrickSuite Parts Catalog.")
                .arg(result.requestedPartNumber,
                     result.canonicalPartNumber);
        return result;
    }

    // Exact identities must never be converted into aliases.
    if (result.requestedPartNumber.compare(
            canonicalPart->partNumber(),
            Qt::CaseInsensitive) == 0) {
        result.message =
            QStringLiteral("Rebrickable returned the same canonical part number.");
        return result;
    }

    PartAlias alias;
    alias.partId = canonicalPart->id();
    alias.aliasPartNumber = result.requestedPartNumber;
    alias.aliasType = PartAliasType::PartNumberMapping;
    alias.source = QStringLiteral("Rebrickable");
    alias.isActive = true;
    alias.notes =
        QStringLiteral("Learned from Rebrickable Part Number Mapping.");

    PartAliasRepository aliasRepository;

    if (!aliasRepository.upsert(alias)) {
        result.message =
            QStringLiteral("Unable to save the learned Rebrickable alias.");
        return result;
    }

    result.learned = true;
    result.canonicalPartId = canonicalPart->id();
    result.message =
        QStringLiteral("Learned Rebrickable alias %1 -> %2.")
            .arg(result.requestedPartNumber,
                 result.canonicalPartNumber);

    qInfo() << "Part alias learned from Rebrickable."
            << "Alias:" << result.requestedPartNumber
            << "CanonicalPart:" << result.canonicalPartNumber
            << "PartId:" << result.canonicalPartId;

    return result;
}
