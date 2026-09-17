#pragma once

#include "../../models/CollectionItem.h"

#include <QDateTime>
#include <QList>
#include <QSqlDatabase>
#include <QString>

class CollectionDisassemblyService
{
public:
    struct Row {
        int partId = 0;
        int colorId = 0;
        int quantity = 0;
        QString partNumber;
        QString partName;
        QString colorName;
    };
    enum class Error {
        None, InvalidInput, NotFound, Ineligible, Unsupported,
        CompositionUnavailable, InvalidDestination, Stale, DatabaseFailure
    };
    struct Plan {
        bool success = false;
        Error error = Error::None;
        QString message;
        CollectionItem item;
        QString reference;
        QString name;
        QList<Row> rows;
        int totalPieces = 0;
        int excludedSpareRows = 0;
        int excludedSparePieces = 0;
    };
    struct Result {
        bool success = false;
        Error error = Error::None;
        QString message;
        int collectionItemId = 0;
        int distinctRows = 0;
        int totalPieces = 0;
        QList<int> affectedInventoryIds;
    };
    struct DestinationAssignment {
        int partId = 0;
        int colorId = 0;
        int quantity = 0;
        int storageLocationId = 0;
    };

    CollectionDisassemblyService();
    explicit CollectionDisassemblyService(const QSqlDatabase& database);

    Plan preview(int collectionItemId) const;
    Result disassemble(int collectionItemId, const QDateTime& expectedModifiedUtc,
                       const QList<DestinationAssignment>& assignments) const;

private:
    Plan buildPlan(int collectionItemId) const;
    QSqlDatabase database() const;
    QString m_connectionName;
};
