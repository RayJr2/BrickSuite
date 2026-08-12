#pragma once

#include <QDialog>

#include "../../import/RebrickableInventoryImportPreview.h"

class QLabel;
class QTableWidget;
class QDialogButtonBox;

class InventoryImportPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit InventoryImportPreviewDialog(const RebrickableInventoryImportPreview& preview,
                                          QWidget* parent = nullptr);

private:
    void populateSummary();
    void populateTable();

    RebrickableInventoryImportPreview m_preview;

    QLabel* m_summaryLabel = nullptr;
    QTableWidget* m_table = nullptr;
    QDialogButtonBox* m_buttonBox = nullptr;
};
