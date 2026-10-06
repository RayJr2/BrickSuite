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

#include "../../models/Manufacturer.h"
#include "../../repositories/ManufacturerRepository.h"

#include <QString>

class ManufacturerManagementService
{
public:
    enum class Error { None, DuplicateCode, DuplicateName, InvalidInput,
                       ProtectedOperation, NotFound, DatabaseFailure };
    struct Result { bool success = false; Error error = Error::None; QString message; int manufacturerId = 0; };
    struct UsageResult { bool success = false; QString message; ManufacturerUsage usage; };

    Result create(Manufacturer manufacturer) const;
    Result edit(Manufacturer manufacturer) const;
    Result setActive(int manufacturerId, bool active) const;
    UsageResult usage(int manufacturerId) const;

private:
    Result validateIdentity(const Manufacturer& manufacturer, int excludeId) const;
};
