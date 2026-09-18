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
