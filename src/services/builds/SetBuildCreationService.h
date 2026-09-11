#pragma once

#include <QString>
#include <QSqlDatabase>

class SetBuildCreationService
{
public:
    struct Result
    {
        bool success = false;
        int buildId = 0;
        int requirementRows = 0;
        int requiredPieces = 0;
        int excludedSparePieces = 0;
        QString message;
    };

    SetBuildCreationService();
    explicit SetBuildCreationService(const QSqlDatabase& database);

    Result create(int workspaceId, int setCatalogId, const QString& buildName) const;
    Result createInCurrentTransaction(int workspaceId, int setCatalogId,
                                      const QString& buildName) const;

private:
    QSqlDatabase database() const;
    QString m_connectionName;
};
