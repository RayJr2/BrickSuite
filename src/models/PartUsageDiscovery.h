#pragma once

#include <QList>
#include <QString>

struct PartUsageCriterion
{
    int partId = 0;
    QString partNumber;
    QString partName;
    int colorId = 0; // Zero means Any Color.
    QString colorName;
    int quantity = 1;
};

enum class PartUsageMatchMode { All, Any };

struct PartUsageSearch
{
    QList<PartUsageCriterion> criteria;
    QString text;
    PartUsageMatchMode matchMode = PartUsageMatchMode::All;
    int maximumResults = 250;
};

struct PartUsageSetResult
{
    int setCatalogId = 0;
    QString setNumber;
    QString name;
    int year = 0;
    int themeId = 0;
    QString themeName;
    QString imageUrl;
    int catalogPartCount = 0;
    int matchedCriteria = 0;
    int totalCriteria = 0;
};

struct PartUsageSearchResult
{
    bool success = false;
    QString errorMessage;
    QList<PartUsageSetResult> sets;
    bool capReached = false;
    int qualifyingCount = 0;
};
