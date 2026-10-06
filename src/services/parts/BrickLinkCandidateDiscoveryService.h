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

#include "../../models/Part.h"

#include <QList>
#include <QSet>
#include <QString>

class BrickLinkCandidateDiscoveryService
{
public:
    static constexpr int MaximumCandidates = 20;

    enum class Status
    {
        Unsupported,
        NoCandidates,
        Candidates,
        TooBroad
    };

    struct Result
    {
        Status status = Status::Unsupported;
        QString brickLinkId;
        QString baseHint;
        QList<Part> candidates;
    };

    Result discover(const QString& brickLinkId) const;
    static QString baseHint(const QString& brickLinkId);
};

// Small value object used by Add Part to reject stale asynchronous results.
class BrickLinkCandidateLookupSession
{
public:
    void begin(const QString& brickLinkId, const QList<int>& candidatePartIds);
    void clear();
    bool accepts(const QString& currentInput, int partId) const;
    bool finish(int partId);
    bool isActive() const;
    QString brickLinkId() const;

private:
    QString m_brickLinkId;
    QSet<int> m_pendingPartIds;
};
