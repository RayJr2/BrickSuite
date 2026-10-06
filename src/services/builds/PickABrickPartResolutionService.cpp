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

#include "PickABrickPartResolutionService.h"

#include "../../database/DatabaseManager.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/PartRepository.h"
#include "../parts/ElementIdentityService.h"
#include "PickABrickExportService.h"

PickABrickPartResolutionService::PickABrickPartResolutionService(QSqlDatabase database)
    : m_database(database.isValid() ? database : DatabaseManager::instance().database())
{
}

PickABrickPartResolution PickABrickPartResolutionService::resolveExact(
    const QString& partNumber, int rebrickableColorId) const
{
    PickABrickPartResolution result;
    const QString exactPartNumber = partNumber.trimmed();
    if (exactPartNumber.isEmpty())
        return result;

    const auto part = PartRepository(m_database).getByPartNumber(exactPartNumber);
    if (!part)
        return result;

    result.partFound = true;
    result.partNumber = part->partNumber();
    result.partName = part->name();
    const auto color = ColorRepository(m_database).getByRebrickableId(rebrickableColorId);
    if (color) {
        result.elementCandidates = PickABrickExportService::numericCandidateOrder(
            ElementIdentityService(m_database).forPartColor(part->id(), color->id()));
    }
    return result;
}
