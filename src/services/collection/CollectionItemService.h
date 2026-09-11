#pragma once

#include "../../models/CollectionItem.h"

#include <QString>
#include <QSqlDatabase>
#include <functional>

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
        CatalogMatchNotFound,
        CatalogMatchAmbiguous,
        SourceBuildAlreadyLinked,
        ItemNotFound,
        DatabaseFailure
    };

    struct Result {
        bool success = false;
        Error error = Error::None;
        QString message;
        int collectionItemId = 0;
        CollectionItem item;
        bool changed = false;
    };

    CollectionItemService();
    explicit CollectionItemService(const QSqlDatabase& database);

    struct SetLinkPreview {
        Result result;
        int setCatalogId = 0;
        QString buildName;
        QString buildReference;
        QString catalogName;
        QString catalogReference;
    };

    Result createSet(int workspaceId, int setCatalogId, CollectionItemState state,
                     int storageLocationId = 0, int sourceBuildId = 0,
                     const QString& nickname = {}, const QString& notes = {},
                     CollectionItemCondition condition = CollectionItemCondition::Used,
                     CollectionItemCompleteness completeness = CollectionItemCompleteness::Unknown) const;
    Result createMinifig(int workspaceId, int minifigCatalogId, CollectionItemState state,
                         int storageLocationId = 0, int sourceBuildId = 0,
                         const QString& nickname = {}, const QString& notes = {},
                         CollectionItemCondition condition = CollectionItemCondition::Used,
                         CollectionItemCompleteness completeness = CollectionItemCompleteness::Unknown) const;
    Result createMocFromBuild(int workspaceId, int sourceBuildId,
                              CollectionItemState state, int storageLocationId = 0,
                              const QString& nickname = {}, const QString& notes = {},
                              CollectionItemCondition condition = CollectionItemCondition::Used,
                              CollectionItemCompleteness completeness = CollectionItemCompleteness::Complete) const;
    Result createFromBuild(int workspaceId, int sourceBuildId, CollectionItemState state,
                           int storageLocationId = 0, const QString& nickname = {},
                           const QString& notes = {},
                           CollectionItemCondition condition = CollectionItemCondition::Used,
                           CollectionItemCompleteness completeness = CollectionItemCompleteness::Complete) const;
    Result updateDetails(int itemId, CollectionItemState state, int storageLocationId,
                         const QString& nickname, const QString& notes,
                         bool allowPartsSource, CollectionItemCondition condition,
                         CollectionItemCompleteness completeness) const;
    Result updateStateForDisassembly(int sourceBuildId, CollectionItemState state) const;
    SetLinkPreview previewLegacySetBuildLink(int buildId) const;
    Result linkLegacySetBuild(int buildId, int expectedSetCatalogId) const;
    Result setActive(int itemId, bool active) const;

    Result createSetInCurrentTransaction(int workspaceId, int setCatalogId,
        CollectionItemState state, int storageLocationId = 0, int sourceBuildId = 0,
        const QString& nickname = {}, const QString& notes = {},
        CollectionItemCondition condition = CollectionItemCondition::Used,
        CollectionItemCompleteness completeness = CollectionItemCompleteness::Unknown) const;
    Result createMinifigInCurrentTransaction(int workspaceId, int minifigCatalogId,
        CollectionItemState state, int storageLocationId = 0, int sourceBuildId = 0,
        const QString& nickname = {}, const QString& notes = {},
        CollectionItemCondition condition = CollectionItemCondition::Used,
        CollectionItemCompleteness completeness = CollectionItemCompleteness::Unknown) const;
    Result createMocFromBuildInCurrentTransaction(int workspaceId, int sourceBuildId,
        CollectionItemState state, int storageLocationId = 0,
        const QString& nickname = {}, const QString& notes = {},
        CollectionItemCondition condition = CollectionItemCondition::Used,
        CollectionItemCompleteness completeness = CollectionItemCompleteness::Complete) const;
    Result createFromBuildInCurrentTransaction(int workspaceId, int sourceBuildId,
        CollectionItemState state, int storageLocationId = 0,
        const QString& nickname = {}, const QString& notes = {},
        CollectionItemCondition condition = CollectionItemCondition::Used,
        CollectionItemCompleteness completeness = CollectionItemCompleteness::Complete) const;
    Result updateDetailsInCurrentTransaction(int itemId, CollectionItemState state,
        int storageLocationId, const QString& nickname, const QString& notes,
        bool allowPartsSource, CollectionItemCondition condition,
        CollectionItemCompleteness completeness) const;
    Result updateStateForDisassemblyInCurrentTransaction(
        int sourceBuildId, CollectionItemState state) const;
    Result linkLegacySetBuildInCurrentTransaction(
        int buildId, int expectedSetCatalogId) const;
    Result setActiveInCurrentTransaction(int itemId, bool active) const;

private:
    QSqlDatabase database() const;
    Result inTransaction(const std::function<Result()>& operation) const;
    Result createInCurrentTransaction(CollectionItem item) const;
    Result validate(CollectionItem& item, bool creating,
                    int preservedLocationId = 0) const;

    QString m_connectionName;
};
