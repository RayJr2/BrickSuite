#pragma once

#include <QList>
#include <QSqlDatabase>
#include <QString>

// All writes belong to the caller's transaction. No Collection/Build lifecycle.
class CompositionToInventoryService
{
public:
    struct Row { int partId = 0; int colorId = 0; int storageId = 0; qint64 quantity = 0; };
    struct Context {
        int workspaceId = 0;
        int manufacturerId = 0;
        QString condition;
        QString ownership = QStringLiteral("Owned");
        QString movementType, referenceType, referenceId, notes;
    };
    struct Result {
        bool success = false;
        QString message;
        qint64 totalPieces = 0;
        QList<int> inventoryIds;
    };
    explicit CompositionToInventoryService(QSqlDatabase database) : m_database(database) {}
    Result validate(const Context& context, const QList<Row>& rows) const;
    Result addInCurrentTransaction(const Context& context, const QList<Row>& rows) const;
private:
    QSqlDatabase m_database;
};
