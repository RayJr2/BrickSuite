#pragma once

#include <QDateTime>
#include <QString>

enum class CollectionItemType { Set, Minifig, Moc, Invalid };
enum class CollectionItemState { Assembled, Unassembled, PartiallyAssembled, Sealed, Invalid };

QString collectionItemTypeToString(CollectionItemType type);
CollectionItemType collectionItemTypeFromString(const QString& value);
QString collectionItemStateToString(CollectionItemState state);
CollectionItemState collectionItemStateFromString(const QString& value);

class CollectionItem
{
public:
    int id = 0;
    int workspaceId = 0;
    CollectionItemType type = CollectionItemType::Invalid;
    int setCatalogId = 0;
    int minifigCatalogId = 0;
    CollectionItemState state = CollectionItemState::Invalid;
    int storageLocationId = 0;
    int sourceBuildId = 0;
    QString nickname;
    QString notes;
    bool allowPartsSource = false;
    bool isActive = true;
    QDateTime createdUtc;
    QDateTime modifiedUtc;
};
