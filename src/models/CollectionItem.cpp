#include "CollectionItem.h"

QString collectionItemTypeToString(CollectionItemType type)
{
    switch (type) {
    case CollectionItemType::Set: return QStringLiteral("Set");
    case CollectionItemType::Minifig: return QStringLiteral("Minifig");
    case CollectionItemType::Moc: return QStringLiteral("MOC");
    case CollectionItemType::Invalid: break;
    }
    return {};
}

CollectionItemType collectionItemTypeFromString(const QString& value)
{
    if (value == QStringLiteral("Set")) return CollectionItemType::Set;
    if (value == QStringLiteral("Minifig")) return CollectionItemType::Minifig;
    if (value == QStringLiteral("MOC")) return CollectionItemType::Moc;
    return CollectionItemType::Invalid;
}

QString collectionItemStateToString(CollectionItemState state)
{
    switch (state) {
    case CollectionItemState::Assembled: return QStringLiteral("Assembled");
    case CollectionItemState::Unassembled: return QStringLiteral("Unassembled");
    case CollectionItemState::PartiallyAssembled: return QStringLiteral("PartiallyAssembled");
    case CollectionItemState::Sealed: return QStringLiteral("Sealed");
    case CollectionItemState::Invalid: break;
    }
    return {};
}

CollectionItemState collectionItemStateFromString(const QString& value)
{
    if (value == QStringLiteral("Assembled")) return CollectionItemState::Assembled;
    if (value == QStringLiteral("Unassembled")) return CollectionItemState::Unassembled;
    if (value == QStringLiteral("PartiallyAssembled"))
        return CollectionItemState::PartiallyAssembled;
    if (value == QStringLiteral("Sealed")) return CollectionItemState::Sealed;
    return CollectionItemState::Invalid;
}
