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

#include "BuildMutationService.h"

#include "../../database/DatabaseManager.h"
#include "../application/HostOperationalGate.h"
#include "../../repositories/BuildAllocationRepository.h"
#include "../../repositories/BuildRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/ManufacturerRepository.h"
#include "../../repositories/WorkspaceRepository.h"

#include <QSqlError>

namespace {
BuildMutationService::Result failure(BuildMutationService::Error error, const QString& message)
{
    BuildMutationService::Result result;
    result.error = error;
    result.message = message;
    return result;
}
}

BuildMutationService::BuildMutationService()
    : BuildMutationService(DatabaseManager::instance().database()) {}

BuildMutationService::BuildMutationService(const QSqlDatabase& database)
    : m_connectionName(database.connectionName()) {}

QSqlDatabase BuildMutationService::database() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

BuildMutationService::Result BuildMutationService::inTransaction(
    const std::function<Result()>& operation) const
{
    if (database().connectionName() == QStringLiteral("qt_sql_default_connection")
        && !HostOperationalGate::localWritesAllowed())
        return failure(Error::InvalidState, QStringLiteral("Host Maintenance prevents operational changes."));
    QSqlDatabase db = database();
    if (!db.isValid() || !db.isOpen() || !db.transaction())
        return failure(Error::DatabaseFailure, QStringLiteral("Unable to begin the Build transaction."));
    Result result = operation();
    if (!result.success) {
        db.rollback();
        return result;
    }
    if (!db.commit()) {
        db.rollback();
        return failure(Error::DatabaseFailure, QStringLiteral("Unable to commit the Build transaction: %1").arg(db.lastError().text()));
    }
    return result;
}

BuildMutationService::Result BuildMutationService::create(Build build) const
{
    return inTransaction([&] { return createInCurrentTransaction(build); });
}

BuildMutationService::Result BuildMutationService::createInCurrentTransaction(Build build) const
{
    QSqlDatabase db = database();
    const QString name = build.name().trimmed();
    const QString type = build.buildType().trimmed();
    if (build.workspaceId() <= 0 || name.isEmpty()
        || (type != QStringLiteral("Set") && type != QStringLiteral("MOC")))
        return failure(Error::InvalidInput, QStringLiteral("Enter valid Build details."));
    std::optional<Workspace> workspace;
    if (!WorkspaceRepository(db).tryGetById(build.workspaceId(), workspace))
        return failure(Error::DatabaseFailure, QStringLiteral("Unable to validate the Build workspace."));
    if (!workspace)
        return failure(Error::NotFound, QStringLiteral("The selected workspace is unavailable."));
    if (build.manufacturerId() > 0 && !ManufacturerRepository(db).getById(build.manufacturerId()))
        return failure(Error::NotFound, QStringLiteral("The selected manufacturer is unavailable."));
    build.setName(name);
    build.setNotes(build.notes().trimmed());
    if (!BuildRepository(db).create(build))
        return failure(Error::DatabaseFailure, QStringLiteral("Unable to create the Build: %1").arg(db.lastError().text()));
    Result result;
    result.success = result.changed = true;
    result.build = build;
    result.message = QStringLiteral("Build created.");
    return result;
}

BuildMutationService::Result BuildMutationService::updateMetadata(
    int buildId, const QString& name, int manufacturerId, const QString& notes) const
{
    return inTransaction([&] { return updateMetadataInCurrentTransaction(buildId, name, manufacturerId, notes); });
}

BuildMutationService::Result BuildMutationService::updateMetadataInCurrentTransaction(
    int buildId, const QString& name, int manufacturerId, const QString& notes) const
{
    QSqlDatabase db = database();
    if (name.trimmed().isEmpty())
        return failure(Error::InvalidInput, QStringLiteral("Enter a name for the Build."));
    std::optional<Build> build;
    if (!BuildRepository(db).tryGetById(buildId, build))
        return failure(Error::DatabaseFailure, QStringLiteral("Unable to load the Build."));
    if (!build)
        return failure(Error::NotFound, QStringLiteral("The Build was not found."));
    if (manufacturerId > 0 && !ManufacturerRepository(db).getById(manufacturerId))
        return failure(Error::NotFound, QStringLiteral("The selected manufacturer is unavailable."));
    const bool changed = build->name() != name.trimmed()
                         || build->manufacturerId() != manufacturerId
                         || build->notes() != notes.trimmed();
    build->setName(name.trimmed());
    build->setManufacturerId(manufacturerId);
    build->setNotes(notes.trimmed());
    if (changed && !BuildRepository(db).update(*build))
        return failure(Error::DatabaseFailure, QStringLiteral("Unable to update the Build."));
    Result result;
    result.success = true; result.changed = changed; result.build = *build;
    result.message = changed ? QStringLiteral("Build updated.") : QStringLiteral("No changes.");
    return result;
}


BuildMutationService::Result BuildMutationService::setActive(int buildId, bool active) const
{
    return inTransaction([&] { return setActiveInCurrentTransaction(buildId, active); });
}

BuildMutationService::Result BuildMutationService::setActiveInCurrentTransaction(int buildId, bool active) const
{
    QSqlDatabase db = database();
    std::optional<Build> build;
    if (!BuildRepository(db).tryGetById(buildId, build))
        return failure(Error::DatabaseFailure, QStringLiteral("Unable to load the Build."));
    if (!build) return failure(Error::NotFound, QStringLiteral("The Build was not found."));
    if (!active && build->status() != QStringLiteral("Cancelled")
        && build->status() != QStringLiteral("Disassembled"))
        return failure(Error::InvalidState, QStringLiteral("Only a Cancelled or Disassembled Build can be archived."));
    if (build->isActive() == active) { Result r; r.success = true; r.build = *build; r.message = QStringLiteral("No changes."); return r; }
    if (active && build->status() == QStringLiteral("Cancelled"))
        build->setStatus(QStringLiteral("Planned"));
    build->setIsActive(active);
    if (!BuildRepository(db).update(*build))
        return failure(Error::DatabaseFailure, QStringLiteral("Unable to update the Build active state."));
    Result result; result.success = result.changed = true; result.build = *build; result.message = QStringLiteral("Build active state updated."); return result;
}

BuildMutationService::Result BuildMutationService::complete(int buildId) const
{
    return inTransaction([&] { return completeInCurrentTransaction(buildId); });
}

BuildMutationService::Result BuildMutationService::completeInCurrentTransaction(int buildId) const
{
    QSqlDatabase db = database();
    std::optional<Build> build;
    if (!BuildRepository(db).tryGetById(buildId, build)) return failure(Error::DatabaseFailure, QStringLiteral("Unable to load the Build."));
    if (!build) return failure(Error::NotFound, QStringLiteral("The Build was not found."));
    QList<BuildRequirement> requirements;
    if (!BuildRequirementRepository(db).tryGetByBuild(buildId, requirements))
        return failure(Error::DatabaseFailure, QStringLiteral("Unable to validate Build requirements."));
    if (requirements.isEmpty()) return failure(Error::InvalidState, QStringLiteral("This Build does not have any requirements."));
    if (build->inventoryMode() == QStringLiteral("Stock")) {
        for (const BuildRequirement& requirement : requirements) {
            if (!requirement.isSpare() && requirement.quantityPulled() < requirement.quantityRequired())
                return failure(Error::InvalidState, QStringLiteral("All required pieces must be pulled before completion."));
        }
    }
    build->setStatus(QStringLiteral("Complete"));
    if (!BuildRepository(db).update(*build)) return failure(Error::DatabaseFailure, QStringLiteral("Unable to complete the Build."));
    Result result; result.success = result.changed = true; result.build = *build; result.message = QStringLiteral("Build completed."); return result;
}
