#pragma once

#include "../../models/Build.h"

#include <QSqlDatabase>
#include <QString>
#include <functional>

class BuildMutationService
{
public:
    enum class Error { None, InvalidInput, NotFound, InvalidState, DatabaseFailure };
    struct Result {
        bool success = false;
        Error error = Error::None;
        QString message;
        Build build;
        bool changed = false;
    };

    BuildMutationService();
    explicit BuildMutationService(const QSqlDatabase& database);

    Result create(Build build) const;
    Result createInCurrentTransaction(Build build) const;
    Result updateMetadata(int buildId, const QString& name, int manufacturerId,
                          const QString& notes) const;
    Result updateMetadataInCurrentTransaction(int buildId, const QString& name,
                                               int manufacturerId,
                                               const QString& notes) const;
    Result setActive(int buildId, bool active) const;
    Result setActiveInCurrentTransaction(int buildId, bool active) const;
    Result complete(int buildId) const;
    Result completeInCurrentTransaction(int buildId) const;

private:
    QSqlDatabase database() const;
    Result inTransaction(const std::function<Result()>& operation) const;
    QString m_connectionName;
};
