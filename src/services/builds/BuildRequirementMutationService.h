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

#include "../../models/BuildRequirement.h"

#include <QSqlDatabase>
#include <QString>
#include <functional>

class BuildRequirementMutationService
{
public:
    enum class Error { None, InvalidInput, NotFound, InvalidState, DatabaseFailure };
    struct Result { bool success=false; Error error=Error::None; QString message; BuildRequirement requirement; bool changed=false; };
    BuildRequirementMutationService();
    explicit BuildRequirementMutationService(const QSqlDatabase& database);
    Result add(BuildRequirement requirement) const;
    Result addInCurrentTransaction(BuildRequirement requirement) const;
    Result edit(int id, int substitutePartId, int substituteColorId, int quantity, bool spare) const;
    Result editInCurrentTransaction(int id, int substitutePartId, int substituteColorId, int quantity, bool spare) const;
    Result remove(int id) const;
    Result removeInCurrentTransaction(int id) const;
private:
    QSqlDatabase database() const;
    Result inTransaction(const std::function<Result()>& operation) const;
    QString m_connectionName;
};
