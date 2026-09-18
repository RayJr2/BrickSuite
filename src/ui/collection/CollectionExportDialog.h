#pragma once
#include "../../models/export/CollectionExportTypes.h"
#include <QDialog>
class QLabel;class QListWidget;class QPushButton;class QTableWidget;
class CollectionExportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CollectionExportDialog(QList<CollectionExportRow>,QString,QWidget* parent=nullptr);
private:
    void apply(const CollectionExportConfiguration&);
    CollectionExportConfiguration configuration() const;
    void updatePreview();
    void moveField(int);
    void exportCsv();
    QList<CollectionExportRow> m_rows;
    QString m_filterSummary;
    QListWidget* m_fields=nullptr;
    QTableWidget* m_preview=nullptr;
    QLabel* m_status=nullptr;
    QPushButton* m_export=nullptr;
};
