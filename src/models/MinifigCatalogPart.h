#pragma once

#include <QDateTime>
#include <QString>

class MinifigCatalogPart
{
public:
    int id = 0;
    int minifigCatalogId = 0;
    int partId = 0;
    int colorId = 0;
    int quantityRequired = 0;
    bool isSpare = false;
    QString partNumber;
    QString partName;
    QString colorName;
    int rebrickableColorId = 0;
    QString provider;
    QString source;
    QDateTime createdUtc;
    QDateTime modifiedUtc;
};
