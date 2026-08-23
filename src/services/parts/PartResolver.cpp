/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "PartResolver.h"

#include "../../repositories/PartAliasRepository.h"
#include "../../repositories/PartRelationshipRepository.h"
#include "../../repositories/PartRepository.h"

#include <QHash>
#include <QSet>

namespace
{
bool isResolverRelationshipType(PartRelationshipType type)
{
    return type == PartRelationshipType::Alternate
        || type == PartRelationshipType::Mold;
}

QString candidateKey(int partId,
                     PartRelationshipType relationshipType)
{
    return QStringLiteral("%1|%2")
        .arg(partId)
        .arg(partRelationshipTypeToString(relationshipType));
}
}

PartResolutionResult PartResolver::resolve(
    const QString& partNumber) const
{
    PartResolutionResult result;
    result.inputPartNumber = partNumber.trimmed();

    if (result.inputPartNumber.isEmpty()) {
        result.status = PartResolutionStatus::NotFound;
        result.message =
            QStringLiteral("Enter a part number.");
        return result;
    }

    PartRepository partRepository;

    //
    // Resolution rule #1:
    // an exact BrickSuite catalog identity always wins.
    //
    const std::optional<Part> exact =
        partRepository.getByPartNumber(result.inputPartNumber);

    if (exact) {
        result.status = PartResolutionStatus::ExactMatch;
        result.part = *exact;
        result.hasResolvedPart = true;

        //
        // A/M relationships are returned as advisory information only.
        // They never replace a valid exact catalog identity.
        //
        result.relationshipCandidates =
            relationshipCandidatesForPart(exact->id());

        if (result.relationshipCandidates.isEmpty()) {
            result.message =
                QStringLiteral("The part number matches the BrickSuite catalog exactly.");
        } else {
            result.message =
                QStringLiteral("The part number matches the BrickSuite catalog exactly. "
                               "%1 alternate/mold relationship candidate(s) are also known.")
                    .arg(result.relationshipCandidates.size());
        }

        return result;
    }

    //
    // Resolution rule #2:
    // a trusted active alias can map the entered identity directly to one
    // canonical BrickSuite part. Alias numbers are unique case-insensitively.
    //
    PartAliasRepository aliasRepository;

    const std::optional<PartAlias> alias =
        aliasRepository.getByAliasPartNumber(
            result.inputPartNumber,
            true);

    if (alias) {
        const std::optional<Part> canonical =
            partRepository.getById(alias->partId);

        if (!canonical) {
            result.status = PartResolutionStatus::Ambiguous;
            result.alias = *alias;
            result.matchedAlias = true;
            result.message =
                QStringLiteral("The stored alias points to a BrickSuite part "
                               "that is no longer available.");
            return result;
        }

        result.status = PartResolutionStatus::AliasMatch;
        result.part = *canonical;
        result.hasResolvedPart = true;
        result.alias = *alias;
        result.matchedAlias = true;
        result.relationshipCandidates =
            relationshipCandidatesForPart(canonical->id());

        result.message =
            QStringLiteral("%1 resolves to BrickSuite part %2.")
                .arg(result.inputPartNumber,
                     canonical->partNumber());

        return result;
    }

    //
    // Relationships are intentionally not searched by raw input number here.
    // part_relationship contains relationships between canonical BrickSuite
    // part IDs, so a participant that matches the entered number would have
    // already been returned by the exact-match rule above. This prevents
    // Alternate/Mold relationships from silently becoming aliases.
    //
    result.status = PartResolutionStatus::NotFound;
    result.message =
        QStringLiteral("The part number was not found in the BrickSuite catalog "
                       "or active alias table.");

    return result;
}

QList<PartResolutionCandidate>
PartResolver::relationshipCandidatesForPart(
    int partId) const
{
    QList<PartResolutionCandidate> candidates;

    if (partId <= 0)
        return candidates;

    PartRelationshipRepository relationshipRepository;
    PartRepository partRepository;

    const QList<PartRelationship> relationships =
        relationshipRepository.getByPartId(partId);

    QSet<QString> seen;

    for (const PartRelationship& relationship : relationships) {
        if (!relationship.isActive
            || !isResolverRelationshipType(relationship.relationshipType)) {
            continue;
        }

        const int candidatePartId =
            relationship.parentPartId == partId
                ? relationship.childPartId
                : relationship.parentPartId;

        if (candidatePartId <= 0
            || candidatePartId == partId) {
            continue;
        }

        const QString key =
            candidateKey(candidatePartId,
                         relationship.relationshipType);

        if (seen.contains(key))
            continue;

        const std::optional<Part> candidatePart =
            partRepository.getById(candidatePartId);

        if (!candidatePart)
            continue;

        PartResolutionCandidate candidate;
        candidate.part = *candidatePart;
        candidate.relationshipType =
            relationship.relationshipType;
        candidate.sourceRelationshipType =
            relationship.sourceRelationshipType;
        candidate.source =
            relationship.source;

        candidates.append(candidate);
        seen.insert(key);
    }

    return candidates;
}
