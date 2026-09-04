/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */
#pragma once

#include "../models/PartRelationship.h"

#include <QList>
#include <QString>

class PartRelationshipRepository
{
public:
    QList<PartRelationship> getByParentPartId(int parentPartId) const;
    QList<PartRelationship> getByChildPartId(int childPartId) const;
    QList<PartRelationship> getByPartId(int partId) const;
    QList<int> getActiveDecoratedChildPartIdsByParentPartId(
        int parentPartId,
        int limit) const;

    bool upsert(const PartRelationship& relationship) const;

    bool setActive(int relationshipId, bool active) const;
    bool setAllActiveForSource(const QString& source, bool active) const;
};
