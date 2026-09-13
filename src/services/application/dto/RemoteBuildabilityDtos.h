#pragma once

#include "RemoteReadDtos.h"

#include <QJsonObject>
#include <QList>
#include <QString>

namespace RemoteBuildabilityDto {

constexpr int MaximumResults = 250;
constexpr int MaximumRequirementPageSize = 500;
constexpr int MaximumCollectionSources = 500;

struct SearchRequest {
    qint64 workspaceId = 0;
    QString text;
    int minimumPercent = 75;
    int minimumSetParts = 25;
    int yearFrom = 0;
    int yearTo = 0;
    int rebrickableThemeId = 0;
    bool fullyBuildableOnly = false;
    bool includeCollection = true;
    bool fullyBuildableFirst = true;
    int maximumResults = 250;
};

struct CompactResult {
    QString setNumber;
    QString name;
    int year = 0;
    int rebrickableThemeId = 0;
    QString qualifiedThemeName;
    QString imageUrl;
    int catalogPartCount = 0;
    int totalRequiredPieces = 0;
    int totalRequirements = 0;
    int looseSatisfiedPieces = 0;
    int looseSatisfiedRequirements = 0;
    int advisorySatisfiedPieces = 0;
    int advisorySatisfiedRequirements = 0;
    int missingPieces = 0;
    bool usesCollection = false;
    int collectionSourceCount = 0;
};

struct SearchResponse {
    QList<CompactResult> rows;
    int qualifyingCount = 0;
    int returnedCount = 0;
    bool capReached = false;
    int eligibleCollectionSourceCount = 0;
    int dormantCollectionSourceCount = 0;
};

struct DetailsRequest {
    qint64 workspaceId = 0;
    QString setNumber;
    bool includeCollection = true;
    RemoteReadDto::PageRequest paging{1, 250};
};

struct Requirement {
    QString partNumber;
    QString partDescription;
    int rebrickableColorId = -1;
    QString colorName;
    int requiredQuantity = 0;
    int looseAvailableQuantity = 0;
    int looseUsedQuantity = 0;
    int collectionUsedQuantity = 0;
    int missingQuantity = 0;
};

struct CollectionSource {
    qint64 collectionItemId = 0;
    QString displayLabel;
    QString state;
    int piecesUsed = 0;
};

struct DetailsResponse {
    CompactResult candidate;
    QList<Requirement> requirements;
    QList<CollectionSource> sources;
    int page = 1;
    int pageSize = 250;
    int totalCount = 0;
    int returnedCount = 0;
};

QJsonObject toJson(const SearchRequest& value);
bool fromJson(const QJsonObject& object, SearchRequest* value, QString* error = nullptr);
QJsonObject toJson(const CompactResult& value);
bool fromJson(const QJsonObject& object, CompactResult* value, QString* error = nullptr);
QJsonObject toJson(const SearchResponse& value);
bool fromJson(const QJsonObject& object, SearchResponse* value, QString* error = nullptr);
QJsonObject toJson(const DetailsRequest& value);
bool fromJson(const QJsonObject& object, DetailsRequest* value, QString* error = nullptr);
QJsonObject toJson(const Requirement& value);
bool fromJson(const QJsonObject& object, Requirement* value, QString* error = nullptr);
QJsonObject toJson(const CollectionSource& value);
bool fromJson(const QJsonObject& object, CollectionSource* value, QString* error = nullptr);
QJsonObject toJson(const DetailsResponse& value);
bool fromJson(const QJsonObject& object, DetailsResponse* value, QString* error = nullptr);

} // namespace RemoteBuildabilityDto
