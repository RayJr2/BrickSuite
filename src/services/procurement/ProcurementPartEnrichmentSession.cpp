/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#include "ProcurementPartEnrichmentSession.h"

#include <algorithm>

void ProcurementPartEnrichmentSession::addEligibleRow(int row, int partId)
{
    if (row < 0 || partId <= 0)
        return;

    QList<int>& rows = m_rowsByPartId[partId];
    if (!rows.contains(row))
        rows.append(row);
    if (!m_stateByPartId.contains(partId))
        m_stateByPartId.insert(partId, State::Eligible);
}

QList<int> ProcurementPartEnrichmentSession::eligiblePartIds() const
{
    QList<int> result;
    for (auto it = m_stateByPartId.constBegin(); it != m_stateByPartId.constEnd(); ++it) {
        if (it.value() == State::Eligible)
            result.append(it.key());
    }
    std::sort(result.begin(), result.end());
    return result;
}

QList<int> ProcurementPartEnrichmentSession::retryablePartIds() const
{
    QList<int> result;
    for (auto it = m_stateByPartId.constBegin(); it != m_stateByPartId.constEnd(); ++it) {
        if (it.value() == State::RetryableFailure)
            result.append(it.key());
    }
    std::sort(result.begin(), result.end());
    return result;
}

QList<int> ProcurementPartEnrichmentSession::rowsForPart(int partId) const
{
    return m_rowsByPartId.value(partId);
}

bool ProcurementPartEnrichmentSession::contains(int partId) const
{
    return m_stateByPartId.contains(partId);
}

bool ProcurementPartEnrichmentSession::isPending(int partId) const
{
    return m_stateByPartId.value(partId) == State::Pending;
}

bool ProcurementPartEnrichmentSession::isRetryable(int partId) const
{
    return m_stateByPartId.value(partId) == State::RetryableFailure;
}

void ProcurementPartEnrichmentSession::markPending(const QList<int>& partIds)
{
    for (int partId : partIds) {
        if (m_stateByPartId.contains(partId))
            m_stateByPartId[partId] = State::Pending;
    }
}

void ProcurementPartEnrichmentSession::markComplete(int partId, bool retryableFailure)
{
    if (!m_stateByPartId.contains(partId))
        return;
    m_stateByPartId[partId] = retryableFailure
                                      ? State::RetryableFailure
                                      : State::Complete;
}

int ProcurementPartEnrichmentSession::totalPartCount() const
{
    return m_stateByPartId.size();
}

int ProcurementPartEnrichmentSession::completedPartCount() const
{
    int count = 0;
    for (State state : m_stateByPartId) {
        if (state == State::Complete || state == State::RetryableFailure)
            ++count;
    }
    return count;
}

int ProcurementPartEnrichmentSession::pendingPartCount() const
{
    int count = 0;
    for (State state : m_stateByPartId) {
        if (state == State::Pending)
            ++count;
    }
    return count;
}
