#pragma once

#include <QList>
#include <QString>

struct RebrickableInventoryImportPreviewRow
{
    QString partNumber;
    QString partName;

    int partId = 0;

    int rebrickableColorId = 0;
    int colorId = 0;
    QString colorName;

    int csvQuantity = 0;
    int currentQuantity = 0;
    int resultingQuantity = 0;

    QString status;
    QString errorMessage;
};

struct RebrickableInventoryImportPreview
{
    QString sourceFilePath;
    QString sourceFileName;

    int rowsProcessed = 0;
    int validRows = 0;
    int failedRows = 0;

    int totalCsvQuantity = 0;

    QList<RebrickableInventoryImportPreviewRow> rows;
};
