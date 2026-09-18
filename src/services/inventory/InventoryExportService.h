#pragma once

#include "../../models/InventorySearchResult.h"
#include "../../models/export/InventoryExportTypes.h"

#include <QSqlDatabase>

namespace RemoteReadDto { struct InventoryExportRow; }

class InventoryExportService
{
public:
    explicit InventoryExportService(const QSqlDatabase& database = QSqlDatabase());

    QList<InventoryExportRow> createRows(const QList<InventorySearchResult>& source,
                                         const QSet<QString>& enabledFields = {}) const;
    void enrichRows(QList<InventoryExportRow>& rows,
                    const QSet<QString>& enabledFields) const;
    static QList<InventoryExportRow> createRemoteRows(
        const QList<RemoteReadDto::InventoryExportRow>& source);
    static QList<InventoryExportFieldDescriptor> fieldDescriptors();
    static QSet<QString> remoteEnrichmentFieldIds();
    static InventoryExportConfiguration defaultConfiguration();
    static InventoryExportConfiguration normalizeConfiguration(
        const QStringList& order, const QStringList& enabled);
    static InventoryExportProjection project(const QList<InventoryExportRow>& rows,
                                             const InventoryExportConfiguration& configuration,
                                             int maximumRows = -1);

private:
    QSqlDatabase m_database;
};
