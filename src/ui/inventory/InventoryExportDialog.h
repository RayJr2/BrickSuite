#pragma once

#include "../../models/export/InventoryExportTypes.h"
#include <QDialog>
#include <functional>

class QLabel; class QListWidget; class QPushButton; class QTableWidget;

class InventoryExportDialog : public QDialog
{
    Q_OBJECT
public:
    using EnrichmentCompletion=std::function<void(bool,QList<InventoryExportRow>,QString)>;
    using Enricher=std::function<void(QList<InventoryExportRow>,const QSet<QString>&,
        QObject*,EnrichmentCompletion)>;
    explicit InventoryExportDialog(QList<InventoryExportRow> rows, QString filterSummary,
                                   QSet<QString> enrichedFields, Enricher enricher,
                                   QWidget* parent=nullptr);
private:
    void apply(const InventoryExportConfiguration&);
    InventoryExportConfiguration configuration() const;
    void updatePreview(); void moveField(int offset); void exportCsv();
    QList<InventoryExportRow> m_rows; QString m_filterSummary;
    QListWidget* m_fields=nullptr; QTableWidget* m_preview=nullptr;
    QLabel* m_status=nullptr; QPushButton* m_export=nullptr;
    QSet<QString> m_enrichedFields; Enricher m_enricher; bool m_enrichmentPending=false;
};
