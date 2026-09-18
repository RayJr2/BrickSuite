#pragma once
#include <QStringList>
#include <QSqlDatabase>
class LDrawIdentityService
{
public:
    LDrawIdentityService() = default;
    explicit LDrawIdentityService(const QSqlDatabase& database) : m_database(database) {}
    QStringList candidatesForPart(int partId) const;
private:
    QSqlDatabase m_database;
};
