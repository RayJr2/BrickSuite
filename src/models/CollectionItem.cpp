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

QString collectionItemConditionToString(CollectionItemCondition condition)
{
    switch (condition) {
    case CollectionItemCondition::New: return QStringLiteral("New");
    case CollectionItemCondition::Used: return QStringLiteral("Used");
    case CollectionItemCondition::Invalid: break;
    }
    return {};
}

CollectionItemCondition collectionItemConditionFromString(const QString& value)
{
    if (value == QStringLiteral("New")) return CollectionItemCondition::New;
    if (value == QStringLiteral("Used")) return CollectionItemCondition::Used;
    return CollectionItemCondition::Invalid;
}

QString collectionItemCompletenessToString(CollectionItemCompleteness completeness)
{
    switch (completeness) {
    case CollectionItemCompleteness::Unknown: return QStringLiteral("Unknown");
    case CollectionItemCompleteness::Complete: return QStringLiteral("Complete");
    case CollectionItemCompleteness::Incomplete: return QStringLiteral("Incomplete");
    case CollectionItemCompleteness::Invalid: break;
    }
    return {};
}

CollectionItemCompleteness collectionItemCompletenessFromString(const QString& value)
{
    if (value == QStringLiteral("Unknown")) return CollectionItemCompleteness::Unknown;
    if (value == QStringLiteral("Complete")) return CollectionItemCompleteness::Complete;
    if (value == QStringLiteral("Incomplete")) return CollectionItemCompleteness::Incomplete;
    return CollectionItemCompleteness::Invalid;
}
