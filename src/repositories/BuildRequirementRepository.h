#pragma once

#include "../models/BuildRequirement.h"

#include <QList>
#include <optional>

class QSqlQuery;

class BuildRequirementRepository
{
public:
    bool create(BuildRequirement& requirement);

    std::optional<BuildRequirement> getById(int id) const;

    QList<BuildRequirement> getByBuild(int buildId) const;

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
