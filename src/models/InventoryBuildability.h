#pragma once

#include <QList>
#include <QString>

struct InventoryBuildabilityRequirement
{
    int partId = 0;
    int colorId = 0;
    QString partNumber;
    QString partName;
    QString colorName;
    int required = 0;
    int looseAvailable = 0;
    int looseUsed = 0;
    int collectionUsed = 0;
    int missing = 0;
};

struct InventoryBuildabilitySource
{
    int collectionItemId = 0;
    QString label;
    QString state;
    int piecesUsed = 0;
};

struct InventoryBuildabilitySetResult
{
    int setCatalogId = 0;
    QString setNumber;
    QString name;
    int year = 0;
    int themeCatalogId = 0;
    QString themeName;
    QString imageUrl;
    int catalogPartCount = 0;
    int looseSatisfiedQuantity = 0;
    int totalQuantity = 0;
    int looseSatisfiedRequirements = 0;
    int totalRequirements = 0;
    int advisorySatisfiedQuantity = 0;
    int advisorySatisfiedRequirements = 0;
    int missingQuantity = 0;
    QList<InventoryBuildabilityRequirement> requirements;
    QList<InventoryBuildabilitySource> sources;

    int loosePercent() const;
    int advisoryPercent() const;
    bool usesCollection() const { return !sources.isEmpty(); }
};

struct InventoryBuildabilitySearch
{
    int workspaceId = 0;
    QString text;
    int minimumPercent = 75;
    int minimumSetParts = 25;
    int yearFrom = 0;
    int yearTo = 0;
    int themeCatalogId = 0;
    bool fullyBuildableOnly = false;
    bool includeCollection = true;
    bool fullyBuildableFirst = true;
    int maximumResults = 250;
};

struct InventoryBuildabilitySearchResult
{
    bool success = false;
    QString errorMessage;
    QList<InventoryBuildabilitySetResult> sets;
    bool capReached = false;
    int qualifyingCount = 0;
    int eligibleCollectionSources = 0;
    int dormantCollectionSources = 0;
    int candidateCountBeforeFilters = 0;
    int candidateCountAfterCatalogFilters = 0;
    int preciseEvaluationCount = 0;
};
