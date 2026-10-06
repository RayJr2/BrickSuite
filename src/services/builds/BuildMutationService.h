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

#include "../../models/Build.h"

#include <QSqlDatabase>
#include <QString>
#include <functional>

class BuildMutationService
{
public:
    enum class Error { None, InvalidInput, NotFound, InvalidState, DatabaseFailure };
    struct Result {
        bool success = false;
        Error error = Error::None;
        QString message;
        Build build;
        bool changed = false;
    };

    BuildMutationService();
    explicit BuildMutationService(const QSqlDatabase& database);

    Result create(Build build) const;
    Result createInCurrentTransaction(Build build) const;
    Result updateMetadata(int buildId, const QString& name, int manufacturerId,
                          const QString& notes) const;
    Result updateMetadataInCurrentTransaction(int buildId, const QString& name,
                                               int manufacturerId,
                                               const QString& notes) const;
    Result setActive(int buildId, bool active) const;
    Result setActiveInCurrentTransaction(int buildId, bool active) const;
    Result complete(int buildId) const;
    Result completeInCurrentTransaction(int buildId) const;

private:
    QSqlDatabase database() const;
    Result inTransaction(const std::function<Result()>& operation) const;
    QString m_connectionName;
};
