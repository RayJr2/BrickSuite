#pragma once

#include <QList>
#include <QSqlDatabase>
#include <QStringList>

class CatalogSetPartOutService
{
public:
    static constexpr int MaximumCopies = 1000000;
    struct Request {
        int workspaceId = 0, setCatalogId = 0, copies = 1;
        bool includeSpares = true;
        QString condition = QStringLiteral("Used");
        bool createStorage = true;
        int storageId = 0, storageTypeId = 0, parentStorageId = 0;
        QString storageName;
        QString operationId;
    };
    struct Row {
        int partId = 0, colorId = 0;
        QString partNumber, partName, colorName;
        qint64 perSet = 0, total = 0;
        bool spare = false;
    };
    struct Plan {
        bool success = false;
        QString message, setNumber, setName, source, fingerprint, destination;
        QStringList warnings;
        QList<Row> rows;
        int manufacturerId = 0;
        qint64 requiredPieces = 0, sparePieces = 0, totalPieces = 0;
    };
    struct Result {
        bool success = false, replayed = false, storageCreated = false;
        QString message, destination;
        int workspaceId = 0, storageId = 0;
        qint64 totalPieces = 0;
    };
    explicit CatalogSetPartOutService(QSqlDatabase database) : m_database(database) {}
    Plan preview(const Request& request) const;
    Result execute(const Request& request, const QString& fingerprint) const;
    // Called on a worker thread: open/close a dedicated connection on that thread.
    static Plan previewFile(const QString& databasePath, const Request& request);
    static Result executeFile(const QString& databasePath, const Request& request, const QString& fingerprint);
private:
    QSqlDatabase m_database;
};
