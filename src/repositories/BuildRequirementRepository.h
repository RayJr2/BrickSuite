#pragma once

#include "RepositoryConnection.h"

#include "../models/BuildRequirement.h"

#include <QList>
#include <optional>

class QSqlQuery;

class BuildRequirementRepository : protected RepositoryConnection
{
public:
    BuildRequirementRepository() = default;
    explicit BuildRequirementRepository(const QSqlDatabase& database)
        : RepositoryConnection(database) {}

    bool create(BuildRequirement& requirement);
    std::optional<BuildRequirement> getById(int id) const;
    bool tryGetById(int id, std::optional<BuildRequirement>& requirement) const;
    QList<BuildRequirement> getByBuild(int buildId) const;
    bool tryGetByBuild(int buildId, QList<BuildRequirement>& requirements) const;
    bool update(BuildRequirement& requirement);
    bool remove(int requirementId);
    bool removeAllForBuild(int buildId);
    std::optional<BuildRequirement> getByBuildPartColor(int buildId,
                                                        int partId,
                                                        int colorId,
                                                        bool isSpare) const;

private:
    BuildRequirement requirementFromQuery(const QSqlQuery& query) const;
};
