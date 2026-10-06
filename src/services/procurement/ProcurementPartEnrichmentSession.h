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
