#pragma once

#include "../../models/export/MissingPartsExportTypes.h"

class PickABrickExportService
{
public:
    static QList<PickABrickExportSourceRow> createSourceRows(
        const QList<MissingPartsExportRow>& rows);
    static QStringList numericCandidateOrder(const QStringList& candidates);
    static QString suggestedElementId(const QStringList& candidates);
    static bool isValidElementId(const QString& elementId);
    static PickABrickExportProjection project(
        const QList<PickABrickExportSourceRow>& rows);
};
