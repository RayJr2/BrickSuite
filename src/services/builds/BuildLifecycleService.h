#pragma once

#include "../../models/Build.h"
#include "../../models/CollectionItem.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <functional>

class BuildLifecycleService
{
public:
    struct DisassemblyReturn {
        int requirementId = 0;
        int partId = 0;
        int colorId = 0;
        int manufacturerId = 0;
        int storageLocationId = 0;
        int quantity = 0;
        bool spare = false;
    };
    enum class Error { None, InvalidInput, NotFound, InvalidState, DatabaseFailure };
    struct Result {
        bool success = false;
        Error error = Error::None;
        QString message;
        Build build;
        QList<int> affectedRequirementIds;
        QList<int> affectedInventoryIds;
        QList<int> affectedAllocationIds;
        int returnedPieces = 0;
        bool collectionChanged = false;
    };
    struct ReturnPlanResult {
        bool success = false;
        Error error = Error::None;
        QString message;
        Build build;
        QList<DisassemblyReturn> rows;
    };
    BuildLifecycleService();
    explicit BuildLifecycleService(const QSqlDatabase& database);
    Result disassemble(int buildId, const QList<DisassemblyReturn>& returns,
                       CollectionItemState linkedCollectionState) const;
    Result disassembleInCurrentTransaction(int buildId,
        const QList<DisassemblyReturn>& returns,
        CollectionItemState linkedCollectionState) const;
    Result cancel(int buildId, const QList<DisassemblyReturn>& returns,
                  CollectionItemState linkedCollectionState) const;
    Result cancelInCurrentTransaction(int buildId,
        const QList<DisassemblyReturn>& returns,
        CollectionItemState linkedCollectionState) const;
    Result storeCompleteSetSpare(int buildId, int requirementId,
                                 int storageLocationId, int quantity) const;
    Result storeCompleteSetSpareInCurrentTransaction(int buildId, int requirementId,
        int storageLocationId, int quantity) const;
    ReturnPlanResult disassemblyReturnPlan(int buildId) const;
private:
    QSqlDatabase database() const;
    Result inTransaction(const std::function<Result()>& operation) const;
    Result returnPiecesInCurrentTransaction(Build& build,
        const QList<DisassemblyReturn>& returns) const;
    QString m_connectionName;
};
