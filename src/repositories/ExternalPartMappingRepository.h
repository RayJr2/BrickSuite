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

#include "../models/ExternalPartMapping.h"
#include "RepositoryConnection.h"

#include <optional>
#include <QList>
#include <QString>

class ExternalPartMappingRepository : protected RepositoryConnection
{
public:
    ExternalPartMappingRepository() = default;
    explicit ExternalPartMappingRepository(const QSqlDatabase& database)
        : RepositoryConnection(database) {}
    std::optional<ExternalPartMapping> getByPartAndProvider(
        int partId,
        const QString& provider) const;

    QList<ExternalPartMapping> findByProviderAndExternalId(
        const QString& provider,
        const QString& externalId) const;

    bool upsert(const ExternalPartMapping& mapping) const;

    bool remove(int partId, const QString& provider) const;
};
