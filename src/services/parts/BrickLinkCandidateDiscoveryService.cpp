#include "BrickLinkCandidateDiscoveryService.h"

#include "../../repositories/PartRelationshipRepository.h"
#include "../../repositories/PartRepository.h"

#include <QHash>
#include <QRegularExpression>

#include <algorithm>

QString BrickLinkCandidateDiscoveryService::baseHint(const QString& brickLinkId)
{
    // Initial support is intentionally limited to explicit BrickLink pb
    // decoration suffixes. The suffix number is never interpreted as a
    // Rebrickable pr/pat number.
    static const QRegularExpression pattern(
        QStringLiteral("^([A-Z0-9][A-Z0-9._-]*?)PB[0-9]+[A-Z]?(?:C[0-9]+)?$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = pattern.match(brickLinkId.trimmed());
    return match.hasMatch() ? match.captured(1) : QString();
}

BrickLinkCandidateDiscoveryService::Result
BrickLinkCandidateDiscoveryService::discover(const QString& brickLinkId) const
{
    Result result;
    result.brickLinkId = brickLinkId.trimmed();
    result.baseHint = baseHint(result.brickLinkId);
    if (result.baseHint.isEmpty())
        return result;

    constexpr int QueryLimit = MaximumCandidates + 1;
    PartRepository partRepository;
    QHash<int, Part> candidatesById;

    const auto basePart = partRepository.getByPartNumber(result.baseHint);
    if (basePart && basePart->isActive()) {
        const QList<int> childPartIds =
            PartRelationshipRepository().getActiveDecoratedChildPartIdsByParentPartId(
                basePart->id(), QueryLimit);
        if (childPartIds.size() > MaximumCandidates) {
            result.status = Status::TooBroad;
            return result;
        }
        for (int childPartId : childPartIds) {
            const auto child = partRepository.getById(childPartId);
            if (child && child->isActive())
                candidatesById.insert(child->id(), *child);
        }
    }

    const QList<Part> prefixCandidates =
        partRepository.findActiveDecoratedByBasePrefix(result.baseHint, QueryLimit);
    if (prefixCandidates.size() > MaximumCandidates) {
        result.status = Status::TooBroad;
        return result;
    }
    for (const Part& candidate : prefixCandidates)
        candidatesById.insert(candidate.id(), candidate);

    if (candidatesById.size() > MaximumCandidates) {
        result.status = Status::TooBroad;
        return result;
    }

    result.candidates = candidatesById.values();
    std::sort(result.candidates.begin(), result.candidates.end(),
              [](const Part& left, const Part& right) {
                  return left.partNumber().compare(right.partNumber(), Qt::CaseInsensitive) < 0;
              });
    result.status = result.candidates.isEmpty() ? Status::NoCandidates : Status::Candidates;
    return result;
}

void BrickLinkCandidateLookupSession::begin(
    const QString& brickLinkId,
    const QList<int>& candidatePartIds)
{
    m_brickLinkId = brickLinkId.trimmed();
    m_pendingPartIds = QSet<int>(candidatePartIds.begin(), candidatePartIds.end());
}

void BrickLinkCandidateLookupSession::clear()
{
    m_brickLinkId.clear();
    m_pendingPartIds.clear();
}

bool BrickLinkCandidateLookupSession::accepts(
    const QString& currentInput,
    int partId) const
{
    return !m_brickLinkId.isEmpty()
        && currentInput.trimmed().compare(m_brickLinkId, Qt::CaseInsensitive) == 0
        && m_pendingPartIds.contains(partId);
}

bool BrickLinkCandidateLookupSession::finish(int partId)
{
    m_pendingPartIds.remove(partId);
    return m_pendingPartIds.isEmpty();
}

bool BrickLinkCandidateLookupSession::isActive() const
{
    return !m_brickLinkId.isEmpty() && !m_pendingPartIds.isEmpty();
}

QString BrickLinkCandidateLookupSession::brickLinkId() const
{
    return m_brickLinkId;
}
