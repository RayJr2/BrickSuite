#include "MinifigBuildCreationService.h"

#include "../../database/DatabaseManager.h"
#include "../../models/Build.h"
#include "../../models/BuildRequirement.h"
#include "../../repositories/BuildRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/MinifigCatalogPartRepository.h"
#include "../../repositories/MinifigCatalogRepository.h"
#include "../../repositories/WorkspaceRepository.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>

MinifigBuildCreationService::Result MinifigBuildCreationService::create(
    int workspaceId, int minifigCatalogId, const QString& buildName) const
{
    Result result;
    const QString name = buildName.trimmed();
    if (workspaceId <= 0 || !WorkspaceRepository().getById(workspaceId)) {
        result.message = "Select a valid workspace before creating the Minifig Build.";
        return result;
    }
    if (minifigCatalogId <= 0 || !MinifigCatalogRepository().getById(minifigCatalogId)) {
        result.message = "The selected Minifig catalog record is unavailable.";
        return result;
    }
    if (name.isEmpty()) {
        result.message = "Enter a name for the Minifig Build.";
        return result;
    }

    QList<MinifigCatalogPart> requiredParts;
    const QList<MinifigCatalogPart> composition =
        MinifigCatalogPartRepository().listForMinifig(minifigCatalogId);
    for (const MinifigCatalogPart& part : composition) {
        if (!part.isSpare)
            requiredParts.append(part);
    }
    if (requiredParts.isEmpty()) {
        result.message = "Import a Minifig parts list before creating a Build from Stock.";
        return result;
    }

    QSqlDatabase database = DatabaseManager::instance().database();
    if (!database.transaction()) {
        result.message = "Unable to begin the Minifig Build creation transaction.";
        return result;
    }

    Build build;
    build.setWorkspaceId(workspaceId);
    build.setBuildType("Minifig");
    build.setMinifigCatalogId(minifigCatalogId);
    build.setName(name);
    build.setInventoryMode("Stock");
    build.setStatus("Planned");
    BuildRepository buildRepository;
    if (!buildRepository.create(build)) {
        database.rollback();
        result.message = "Unable to create the Minifig Build.";
        return result;
    }

    BuildRequirementRepository requirementRepository;
    for (const MinifigCatalogPart& part : requiredParts) {
        BuildRequirement requirement;
        requirement.setBuildId(build.id());
        requirement.setPartId(part.partId);
        requirement.setColorId(part.colorId);
        requirement.setQuantityRequired(part.quantityRequired);
        requirement.setQuantityPulled(0);
        requirement.setQuantityReleased(0);
        requirement.setIsSpare(false);
        if (!requirementRepository.create(requirement)) {
            const QString error = database.lastError().text();
            database.rollback();
            result.message = error.isEmpty() ? "Unable to create a Minifig Build requirement."
                                             : "Unable to create a Minifig Build requirement: " + error;
            return result;
        }
        ++result.requirementRows;
        result.requiredPieces += part.quantityRequired;
    }

    if (!database.commit()) {
        database.rollback();
        result.message = "Unable to commit the Minifig Build creation transaction.";
        return result;
    }
    result.success = true;
    result.buildId = build.id();
    result.message = "Minifig Build created.";
    qInfo() << "Minifig Build created." << "BuildId:" << build.id()
            << "MinifigCatalogId:" << minifigCatalogId
            << "RequirementRows:" << result.requirementRows;
    return result;
}
