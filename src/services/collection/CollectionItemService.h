#pragma once

#include "../../models/CollectionItem.h"

#include <QString>

class CollectionItemService
{
public:
    enum class Error {
        None,
        InvalidInput,
        WorkspaceNotFound,
        CatalogItemNotFound,
        LocationInvalid,
        SourceBuildNotFound,
        SourceBuildIncomplete,
        SourceBuildMismatch,
        SourceBuildAlreadyUsed,
        ItemNotFound,
        DatabaseFailure
    };

    struct Result {
        bool success = false;
        Error error = Error::None;
        QString message;
        int collectionItemId = 0;
    };

    Result createSet(int workspaceId, int setCatalogId, CollectionItemState state,
                     int storageLocationId = 0, int sourceBuildId = 0,
                     const QString& nickname = {}, const QString& notes = {}) const;
    Result createMinifig(int workspaceId, int minifigCatalogId, CollectionItemState state,
                         int storageLocationId = 0, int sourceBuildId = 0,
                         const QString& nickname = {}, const QString& notes = {}) const;
    Result createMocFromBuild(int workspaceId, int sourceBuildId,
                              CollectionItemState state, int storageLocationId = 0,
                              const QString& nickname = {}, const QString& notes = {}) const;
    Result updateDetails(int itemId, CollectionItemState state, int storageLocationId,
                         const QString& nickname, const QString& notes,
                         bool allowPartsSource) const;
    Result setActive(int itemId, bool active) const;

private:
    Result create(CollectionItem item) const;
    Result validate(CollectionItem& item, bool creating) const;
};
