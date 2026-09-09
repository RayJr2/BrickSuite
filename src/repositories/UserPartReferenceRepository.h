/* BrickSuite - The Digital Twin Platform for Your Brick Workshop */
#pragma once

#include "RepositoryConnection.h"

#include "../models/UserPartReferenceEntry.h"
#include <QList>

class UserPartReferenceRepository : protected RepositoryConnection
{
public:
    UserPartReferenceRepository() = default;
    explicit UserPartReferenceRepository(const QSqlDatabase& database)
        : RepositoryConnection(database) {}

    QList<UserPartReferenceEntry> getAll(bool* ok = nullptr) const;
    bool create(UserPartReferenceEntry& entry, QString* errorMessage = nullptr) const;
    bool remove(int id, QString* errorMessage = nullptr) const;
};
