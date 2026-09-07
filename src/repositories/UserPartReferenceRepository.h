/* BrickSuite - The Digital Twin Platform for Your Brick Workshop */
#pragma once

#include "../models/UserPartReferenceEntry.h"
#include <QList>

class UserPartReferenceRepository
{
public:
    QList<UserPartReferenceEntry> getAll(bool* ok = nullptr) const;
    bool create(UserPartReferenceEntry& entry, QString* errorMessage = nullptr) const;
    bool remove(int id, QString* errorMessage = nullptr) const;
};
