#pragma once

#include "../../models/export/MissingPartsExportTypes.h"

#include <QDialog>

class QLabel;
class QListWidget;
class QPushButton;
class QTableWidget;

class MissingPartsExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MissingPartsExportDialog(QList<MissingPartsExportRow> rows,
                                      QString defaultFileName,
                                      QWidget* parent = nullptr);

private:
    void loadConfiguration();
    void applyConfiguration(const MissingPartsExportConfiguration& configuration);
    MissingPartsExportConfiguration configuration() const;
    void persistConfiguration();
    void updatePreview();
    void moveCurrentField(int offset);
    void exportCsv();

    QList<MissingPartsExportRow> m_rows;
    QString m_defaultFileName;
    QListWidget* m_fields = nullptr;
    QPushButton* m_moveUp = nullptr;
    QPushButton* m_moveDown = nullptr;
    QTableWidget* m_preview = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_export = nullptr;
};
