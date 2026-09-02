#include "SetBuildCreationService.h"

#include "../../database/DatabaseManager.h"
#include "../../models/Build.h"
#include "../../models/BuildRequirement.h"
#include "../../repositories/BuildRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/SetCatalogPartRepository.h"
#include "../../repositories/SetCatalogRepository.h"
#include "../../repositories/WorkspaceRepository.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>

SetBuildCreationService::Result SetBuildCreationService::create(
    int workspaceId, int setCatalogId, const QString& buildName) const
{
    Result result;
    const QString name = buildName.trimmed();
    if (name.isEmpty()) {
        result.message = QStringLiteral("Enter a name for the Set Build.");
        return result;
    }

    QSqlDatabase database = DatabaseManager::instance().database();
    if (!database.transaction()) {
        result.message = QStringLiteral("Unable to begin the Set Build creation transaction.");
        return result;
    }
    if (workspaceId <= 0 || !WorkspaceRepository().getById(workspaceId)) {
        database.rollback();
        result.message = QStringLiteral("Select a valid workspace before creating the Set Build.");
        return result;
    }
    const auto set = SetCatalogRepository().getById(setCatalogId);
    if (setCatalogId <= 0 || !set) {
        database.rollback();
        result.message = QStringLiteral("The selected Set catalog record is unavailable.");
        return result;
    }

    QList<SetCatalogPart> requiredParts;
    const QList<SetCatalogPart> composition = SetCatalogPartRepository().listForSet(setCatalogId);
    for (const SetCatalogPart& part : composition) {
        if (part.isSpare)
            result.excludedSparePieces += part.quantityRequired;
        else
            requiredParts.append(part);
    }
    if (requiredParts.isEmpty()) {
        database.rollback();
        result.message = QStringLiteral("Get or import a Set parts list before creating a Build from Stock.");
        return result;
    }

    Build build;
    build.setWorkspaceId(workspaceId);
    build.setBuildType(QStringLiteral("Set"));
    build.setSetCatalogId(setCatalogId);
    build.setSetNumber(set->setNumber());
    build.setName(name);
    build.setInventoryMode(QStringLiteral("Stock"));
    build.setStatus(QStringLiteral("Planned"));
    if (!BuildRepository().create(build)) {
        database.rollback();
        result.message = QStringLiteral("Unable to create the Set Build.");
        return result;
    }

    BuildRequirementRepository requirements;
    for (const SetCatalogPart& part : requiredParts) {
        BuildRequirement requirement;
        requirement.setBuildId(build.id());
        requirement.setPartId(part.partId);
        requirement.setColorId(part.colorId);
        requirement.setSubstitutePartId(0);
        requirement.setSubstituteColorId(0);
        requirement.setQuantityRequired(part.quantityRequired);
        requirement.setQuantityPulled(0);
        requirement.setQuantityReleased(0);
        requirement.setIsSpare(false);
        if (!requirements.create(requirement)) {
            const QString error = database.lastError().text();
            database.rollback();
            result.message = error.isEmpty()
                ? QStringLiteral("Unable to create a Set Build requirement.")
                : QStringLiteral("Unable to create a Set Build requirement: ") + error;
            return result;
        }
        ++result.requirementRows;
        result.requiredPieces += part.quantityRequired;
    }

    if (!database.commit()) {
        database.rollback();
        result.message = QStringLiteral("Unable to commit the Set Build creation transaction.");
        return result;
    }
    result.success = true;
    result.buildId = build.id();
    result.message = QStringLiteral("Set Build created.");
    qInfo() << "Set Build created from catalog composition."
            << "BuildId:" << build.id() << "SetCatalogId:" << setCatalogId
            << "RequirementRows:" << result.requirementRows;
    return result;
}
