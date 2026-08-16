#pragma once

#include "../models/Build.h"

#include <QList>
#include <optional>

class QSqlQuery;

class BuildRepository
{
public:
    bool create(Build& build);

    std::optional<Build> getById(int id) const;

    QList<Build> getByWorkspace(int workspaceId, bool includeArchived = false) const;

    bool setActive(int buildId, bool active);

    bool update(Build& build);

private:
    Build buildFromQuery(const QSqlQuery& query) const;
};
