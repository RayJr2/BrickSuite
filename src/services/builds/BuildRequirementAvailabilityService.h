#pragma once

#include <QString>

class BuildRequirement;
class QSqlDatabase;
class QThread;

class BuildRequirementAvailabilityService
{
public:
    struct Projection {
        int owned = 0;
        int thisRequirementAllocated = 0;
        int otherAllocated = 0;
        int available = 0;
        int missing = 0;
    };

    BuildRequirementAvailabilityService();
    explicit BuildRequirementAvailabilityService(const QSqlDatabase& database);
    Projection project(int workspaceId, const BuildRequirement& requirement) const;

private:
    QSqlDatabase serviceDatabase() const;
    QString m_connectionName;
    QThread* m_ownerThread = nullptr;
};
