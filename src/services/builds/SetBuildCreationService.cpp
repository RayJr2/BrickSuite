#include "SetBuildCreationService.h"

#include "../../database/DatabaseManager.h"
#include "../../models/Build.h"
#include "../../models/BuildRequirement.h"
#include "../../repositories/BuildRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/EffectiveSetCompositionRepository.h"
#include "../../repositories/SetCatalogRepository.h"
#include "../../repositories/WorkspaceRepository.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>

SetBuildCreationService::SetBuildCreationService()
    : SetBuildCreationService(DatabaseManager::instance().database()) {}

SetBuildCreationService::SetBuildCreationService(const QSqlDatabase& database)
    : m_connectionName(database.connectionName()) {}

QSqlDatabase SetBuildCreationService::database() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

SetBuildCreationService::Result SetBuildCreationService::create(
    int workspaceId, int setCatalogId, const QString& buildName) const
{
    QSqlDatabase db = database();
    if (!db.transaction()) {
        Result result;
        result.message = QStringLiteral("Unable to begin the Set Build creation transaction.");
        return result;
    }
    Result result = createInCurrentTransaction(workspaceId, setCatalogId, buildName);
    if (!result.success) {
        db.rollback();
        return result;
    }
    if (!db.commit()) {
        db.rollback();
        result.success = false;
        result.message = QStringLiteral("Unable to commit the Set Build creation transaction.");
    }
    return result;
}

SetBuildCreationService::Result SetBuildCreationService::createInCurrentTransaction(
    int workspaceId, int setCatalogId, const QString& buildName) const
{
    Result result;
    const QString name = buildName.trimmed();
    if (name.isEmpty()) {
        result.message = QStringLiteral("Enter a name for the Set Build.");
        return result;
    }

    QSqlDatabase db = database();
    if (workspaceId <= 0 || !WorkspaceRepository(db).getById(workspaceId)) {
        result.message = QStringLiteral("Select a valid workspace before creating the Set Build.");
        return result;
    }
    const auto set = SetCatalogRepository(db).getById(setCatalogId);
    if (setCatalogId <= 0 || !set) {
        result.message = QStringLiteral("The selected Set catalog record is unavailable.");
        return result;
    }

    QList<EffectiveSetCompositionPart> requiredParts;
    const auto composition = EffectiveSetCompositionRepository(db).forSet(setCatalogId, true);
    if (!composition.success) {
        result.message = QStringLiteral("Unable to load effective Set composition: ") + composition.message;
        return result;
    }
    for (const EffectiveSetCompositionPart& part : composition.parts) {
        if (part.spare)
            result.excludedSparePieces += part.quantity;
        else
            requiredParts.append(part);
    }
    if (requiredParts.isEmpty()) {
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
    if (!BuildRepository(db).create(build)) {
        result.message = QStringLiteral("Unable to create the Set Build.");
        return result;
    }

    BuildRequirementRepository requirements(db);
    // Both persisted composition sources guarantee one logical row per exact
    // Part+Color+spare identity, so no cross-row aggregation is necessary.
    for (const EffectiveSetCompositionPart& part : requiredParts) {
        BuildRequirement requirement;
        requirement.setBuildId(build.id());
        requirement.setPartId(part.partId);
        requirement.setColorId(part.colorId);
        requirement.setSubstitutePartId(0);
        requirement.setSubstituteColorId(0);
        requirement.setQuantityRequired(part.quantity);
        requirement.setQuantityPulled(0);
        requirement.setQuantityReleased(0);
        requirement.setIsSpare(false);
        if (!requirements.create(requirement)) {
            const QString error = db.lastError().text();
            result.message = error.isEmpty()
                ? QStringLiteral("Unable to create a Set Build requirement.")
                : QStringLiteral("Unable to create a Set Build requirement: ") + error;
            return result;
        }
        ++result.requirementRows;
        result.requiredPieces += part.quantity;
    }

    result.success = true;
    result.buildId = build.id();
    result.message = QStringLiteral("Set Build created.");
    qInfo() << "Set Build created from catalog composition."
            << "BuildId:" << build.id() << "SetCatalogId:" << setCatalogId
            << "RequirementRows:" << result.requirementRows
            << "Source:" << (composition.source==EffectiveSetCompositionSource::PreferredRebrickableRevision
                                ? QStringLiteral("PreferredRebrickableRevision")
                                : QStringLiteral("LegacyCatalogFallback"))
            << "Revision:" << composition.revisionId << composition.revisionVersion;
    return result;
}
