#pragma once

#include <QList>
#include <QSqlDatabase>
#include <QString>

struct SetInventoryRevision
{
    int id = 0;
    QString provider;
    QString externalInventoryId;
    int setCatalogId = 0;
    int version = 0;
    bool active = false;
    bool preferred = false;
};

struct SetInventoryPart
{
    int partId = 0;
    int colorId = 0;
    qint64 quantity = 0;
    bool spare = false;
    QString imageUrl;
};

struct SetInventoryMinifig
{
    int minifigCatalogId = 0;
    qint64 quantity = 0;
};

struct SetInventoryContainedSet
{
    int setCatalogId = 0;
    qint64 quantity = 0;
};

class SetInventoryRevisionRepository
{
public:
    explicit SetInventoryRevisionRepository(
        QSqlDatabase database = QSqlDatabase());
    QList<SetInventoryRevision> revisionsForSet(int setCatalogId,
                                                const QString& provider = {}) const;
    SetInventoryRevision preferredRevisionForSet(int setCatalogId,
                                                  const QString& provider) const;
    QList<SetInventoryPart> partsForRevision(int revisionId,
                                             bool includeSpares = true) const;
    QList<SetInventoryMinifig> minifigsForRevision(int revisionId) const;
    QList<SetInventoryContainedSet> containedSetsForRevision(int revisionId) const;
    QList<int> revisionIdsContainingPart(int partId, int colorId) const;
    QList<int> revisionIdsContainingMinifig(int minifigCatalogId) const;
    QList<int> revisionIdsContainingSet(int setCatalogId) const;

private:
    QSqlDatabase m_database;
};
