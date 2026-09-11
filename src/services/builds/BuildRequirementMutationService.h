#pragma once

#include "../../models/BuildRequirement.h"

#include <QSqlDatabase>
#include <QString>
#include <functional>

class BuildRequirementMutationService
{
public:
    enum class Error { None, InvalidInput, NotFound, InvalidState, DatabaseFailure };
    struct Result { bool success=false; Error error=Error::None; QString message; BuildRequirement requirement; bool changed=false; };
    BuildRequirementMutationService();
    explicit BuildRequirementMutationService(const QSqlDatabase& database);
    Result add(BuildRequirement requirement) const;
    Result addInCurrentTransaction(BuildRequirement requirement) const;
    Result edit(int id, int substitutePartId, int substituteColorId, int quantity, bool spare) const;
    Result editInCurrentTransaction(int id, int substitutePartId, int substituteColorId, int quantity, bool spare) const;
    Result remove(int id) const;
    Result removeInCurrentTransaction(int id) const;
private:
    QSqlDatabase database() const;
    Result inTransaction(const std::function<Result()>& operation) const;
    QString m_connectionName;
};
