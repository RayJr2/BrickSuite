#pragma once

#include "../models/Workspace.h"

#include <QList>
#include <optional>

class WorkspaceRepository
{
public:
    bool create(Workspace& workspace);

    QList<Workspace> getAll() const;

    std::optional<Workspace> getById(int id) const;

    bool update(const Workspace& workspace);

private:
    Workspace workspaceFromQuery(const class QSqlQuery& query) const;
};