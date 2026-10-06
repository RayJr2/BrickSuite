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
#include "../models/ExternalPartIdentifier.h"
#include "RepositoryConnection.h"
#include <QList>
#include <QHash>
#include <QString>

class ExternalPartIdentifierRepository : protected RepositoryConnection
{
public:
    ExternalPartIdentifierRepository() = default;
    explicit ExternalPartIdentifierRepository(const QSqlDatabase& database)
        : RepositoryConnection(database) {}
    enum class LookupStatus
    {
        NotRequested,
        Unknown,
        Loaded,
        Unavailable
    };

    bool replaceProviderIds(
        int partId,
        const QHash<QString, QStringList>& externalIds,
        const QString& source) const;

    QList<ExternalPartIdentifier> findByExternalId(
        const QString& externalId,
        bool activeOnly = true) const;

    QList<ExternalPartIdentifier> findByProviderAndExternalId(
        const QString& provider,
        const QString& externalId,
        bool activeOnly = true) const;

    QList<ExternalPartIdentifier> findByPartAndProvider(
        int partId,
        const QString& provider,
        bool activeOnly = true) const;

    // Background enrichment status is tracked separately from identifier
    // rows because a successful provider lookup may legitimately return no
    // external IDs. Without a terminal status BrickSuite would repeatedly
    // request the same part forever.
    bool isLookupComplete(int partId, const QString& source) const;
    LookupStatus lookupStatus(int partId, const QString& source) const;
    bool setLookupStatus(int partId,
                         const QString& source,
                         const QString& status) const;
};
