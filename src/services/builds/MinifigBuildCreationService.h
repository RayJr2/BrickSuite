#pragma once

#include <QString>
#include <QSqlDatabase>

class MinifigBuildCreationService
{
public:
    struct Result
    {
        bool success = false;
        int buildId = 0;
        int requirementRows = 0;
        int requiredPieces = 0;
        QString message;
    };

    MinifigBuildCreationService();
    explicit MinifigBuildCreationService(const QSqlDatabase& database);
    Result create(int workspaceId, int minifigCatalogId, const QString& buildName) const;
    Result createInCurrentTransaction(int workspaceId, int minifigCatalogId,
                                      const QString& buildName) const;
private:
    QSqlDatabase database() const;
    QString m_connectionName;
};
