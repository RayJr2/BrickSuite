#pragma once

#include "../../models/export/MissingPartsExportTypes.h"

#include <QDialog>

#include <functional>

class QLabel;
class QListWidget;
class QPushButton;
class QTableWidget;
class QComboBox;
class QWidget;

class MissingPartsExportDialog : public QDialog
{
    Q_OBJECT

public:
    using PartOverrideResolver = std::function<void(
        const QString&, int, QObject*,
        std::function<void(const PickABrickPartResolution&)>)>;

    explicit MissingPartsExportDialog(QList<MissingPartsExportRow> rows,
                                      QString defaultFileName,
                                      QWidget* parent = nullptr,
                                      PartOverrideResolver partOverrideResolver = {});

private:
    void loadConfiguration();
    void applyConfiguration(const MissingPartsExportConfiguration& configuration);
    MissingPartsExportConfiguration configuration() const;
    void persistConfiguration();
    void updatePreview();
    void updateGeneralPreview();
    void updatePickABrickPreview();
    void updatePickABrickSummary();
    void resolvePartOverride(int rowIndex, const QString& partNumber);
    QString pickABrickRowStatus(const PickABrickExportSourceRow& row) const;
    void moveCurrentField(int offset);
    void exportCsv();
    bool pickABrickSelected() const;

    QList<MissingPartsExportRow> m_rows;
    QList<PickABrickExportSourceRow> m_pickABrickRows;
    QString m_defaultFileName;
    PartOverrideResolver m_partOverrideResolver;
    QComboBox* m_preset = nullptr;
    QWidget* m_generalControls = nullptr;
    QLabel* m_description = nullptr;
    QListWidget* m_fields = nullptr;
    QPushButton* m_moveUp = nullptr;
    QPushButton* m_moveDown = nullptr;
    QTableWidget* m_preview = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_export = nullptr;
};
