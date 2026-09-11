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

MinifigBuildCreationService::MinifigBuildCreationService()
    : MinifigBuildCreationService(DatabaseManager::instance().database()) {}
MinifigBuildCreationService::MinifigBuildCreationService(const QSqlDatabase& database)
    : m_connectionName(database.connectionName()) {}
QSqlDatabase MinifigBuildCreationService::database() const
{ return QSqlDatabase::database(m_connectionName, false); }

MinifigBuildCreationService::Result MinifigBuildCreationService::create(
    int workspaceId, int minifigCatalogId, const QString& buildName) const
{
    QSqlDatabase db = database();
    if (!db.transaction()) { Result r; r.message = "Unable to begin the Minifig Build creation transaction."; return r; }
    Result result = createInCurrentTransaction(workspaceId, minifigCatalogId, buildName);
    if (!result.success) { db.rollback(); return result; }
    if (!db.commit()) { db.rollback(); result.success = false; result.message = "Unable to commit the Minifig Build creation transaction."; }
    return result;
}

MinifigBuildCreationService::Result MinifigBuildCreationService::createInCurrentTransaction(
    int workspaceId, int minifigCatalogId, const QString& buildName) const
{
    Result result;
    const QString name = buildName.trimmed();
    QSqlDatabase db = database();
    if (workspaceId <= 0 || !WorkspaceRepository(db).getById(workspaceId)) {
        result.message = "Select a valid workspace before creating the Minifig Build.";
        return result;
    }
    if (minifigCatalogId <= 0 || !MinifigCatalogRepository(db).getById(minifigCatalogId)) {
        result.message = "The selected Minifig catalog record is unavailable.";
        return result;
    }
    if (name.isEmpty()) {
        result.message = "Enter a name for the Minifig Build.";
        return result;
    }

    QList<MinifigCatalogPart> requiredParts;
    const QList<MinifigCatalogPart> composition =
        MinifigCatalogPartRepository(db).listForMinifig(minifigCatalogId);
    for (const MinifigCatalogPart& part : composition) {
        if (!part.isSpare)
            requiredParts.append(part);
    }
    if (requiredParts.isEmpty()) {
        result.message = "Import a Minifig parts list before creating a Build from Stock.";
        return result;
    }

    Build build;
    build.setWorkspaceId(workspaceId);
    build.setBuildType("Minifig");
    build.setMinifigCatalogId(minifigCatalogId);
    build.setName(name);
    build.setInventoryMode("Stock");
    build.setStatus("Planned");
    BuildRepository buildRepository(db);
    if (!buildRepository.create(build)) {
        result.message = "Unable to create the Minifig Build.";
        return result;
    }

    BuildRequirementRepository requirementRepository(db);
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
            const QString error = db.lastError().text();
            result.message = error.isEmpty() ? "Unable to create a Minifig Build requirement."
                                             : "Unable to create a Minifig Build requirement: " + error;
            return result;
        }
        ++result.requirementRows;
        result.requiredPieces += part.quantityRequired;
    }

    result.success = true;
    result.buildId = build.id();
    result.message = "Minifig Build created.";
    qInfo() << "Minifig Build created." << "BuildId:" << build.id()
            << "MinifigCatalogId:" << minifigCatalogId
            << "RequirementRows:" << result.requirementRows;
    return result;
}
