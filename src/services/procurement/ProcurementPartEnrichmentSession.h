/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include <QHash>
#include <QList>

class ProcurementPartEnrichmentSession
{
public:
    enum class State
    {
        Eligible,
        Pending,
        Complete,
        RetryableFailure
    };

    void addEligibleRow(int row, int partId);

    QList<int> eligiblePartIds() const;
    QList<int> retryablePartIds() const;
    QList<int> rowsForPart(int partId) const;

    bool contains(int partId) const;
    bool isPending(int partId) const;
    bool isRetryable(int partId) const;

    void markPending(const QList<int>& partIds);
    void markComplete(int partId, bool retryableFailure);

    int totalPartCount() const;
    int completedPartCount() const;
    int pendingPartCount() const;

private:
    QHash<int, QList<int>> m_rowsByPartId;
    QHash<int, State> m_stateByPartId;
};
