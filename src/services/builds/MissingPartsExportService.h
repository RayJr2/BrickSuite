#pragma once

#include "../../models/export/MissingPartsExportTypes.h"
#include "../application/dto/RemoteReadDtos.h"
#include "MissingPartsService.h"

#include <QSqlDatabase>

class Build;

class MissingPartsExportService
{
public:
    explicit MissingPartsExportService(const QSqlDatabase& database);

    QList<MissingPartsExportRow> createRows(
        const Build& build,
        const QList<MissingPartsService::MissingPart>& missingParts) const;

    static QList<MissingPartsExportRow> createRemoteRows(
        const RemoteReadDto::BuildDetail& build,
        const QList<RemoteReadDto::MissingPart>& missingParts);

    static QList<MissingPartsExportFieldDescriptor> fieldDescriptors();
    static MissingPartsExportConfiguration defaultConfiguration();
    static MissingPartsExportConfiguration normalizeConfiguration(
        const QStringList& savedOrder,
        const QStringList& savedEnabled);
    static MissingPartsExportProjection project(
        const QList<MissingPartsExportRow>& rows,
        const MissingPartsExportConfiguration& configuration);
    static QString value(const MissingPartsExportRow& row,
                         MissingPartsExportField field);

private:
    QSqlDatabase m_database;
};
