#pragma once

#include "dto/RemoteReadDtos.h"

#include <QSqlDatabase>
#include <QString>

class CollectionDisassemblyPlanService
{
public:
    struct Result {
        bool success = false;
        QString errorCode;
        QString message;
        RemoteReadDto::CollectionDisassemblyPlan plan;
    };

    explicit CollectionDisassemblyPlanService(const QSqlDatabase& database);
    Result preview(int workspaceId, int collectionItemId) const;

private:
    QString m_connectionName;
};
