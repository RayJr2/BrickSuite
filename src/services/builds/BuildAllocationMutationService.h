#pragma once

#include "../../models/BuildAllocation.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <functional>

class BuildAllocationMutationService
{
public:
    enum class Error { None, InvalidInput, NotFound, InvalidState, DatabaseFailure };
    struct Result {
        bool success = false;
        Error error = Error::None;
        QString message;
        QList<BuildAllocation> allocations;
        QList<int> affectedRequirementIds;
        QList<int> affectedInventoryIds;
        int piecesAdded = 0;
        int allocationsCreated = 0;
        int allocationsUpdated = 0;
        int preferredPiecesAdded = 0;
        bool changed = false;
    };
    BuildAllocationMutationService();
    explicit BuildAllocationMutationService(const QSqlDatabase& database);
    Result replaceForRequirement(int requirementId, const QList<BuildAllocation>& allocations) const;
    Result replaceForRequirementInCurrentTransaction(int requirementId,
                                                      const QList<BuildAllocation>& allocations) const;
    Result allocateAvailable(int buildId, int preferredStorageLocationId = 0) const;
    Result allocateAvailableInCurrentTransaction(int buildId,
                                                 int preferredStorageLocationId = 0) const;
private:
    QSqlDatabase database() const;
    Result inTransaction(const std::function<Result()>& operation) const;
    QString m_connectionName;
};
