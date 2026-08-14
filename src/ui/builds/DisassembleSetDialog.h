#pragma once

#include <QDialog>
#include <QList>

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

class DisassembleSetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DisassembleSetDialog(int buildId, QWidget* parent = nullptr);

private:
    struct RowData
    {
        int requirementId = 0;
        int partId = 0;
        int colorId = 0;

        int setQuantity = 0;
        bool isSpare = false;

        QSpinBox* quantitySpin = nullptr;
        QComboBox* destinationCombo = nullptr;
    };

    bool loadBuild();
    bool loadStorageLocations();
    bool loadRequirements();

    void applyDefaultDestination();
    void updateSummary();
    void disassembleSet();

    QString storagePath(int storageLocationId) const;

    void populateLocationCombo(QComboBox* combo);

    int m_buildId = 0;
    int m_workspaceId = 0;

    QString m_buildName;
    QString m_setNumber;

    QLabel* m_buildLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_statusLabel = nullptr;

    QComboBox* m_defaultDestinationCombo = nullptr;
    QPushButton* m_applyDefaultButton = nullptr;

    QTableWidget* m_table = nullptr;

    QPushButton* m_disassembleButton = nullptr;
    QPushButton* m_cancelButton = nullptr;

    struct LocationChoice
    {
        int id = 0;
        QString path;
    };

    QList<LocationChoice> m_locations;
    QList<RowData> m_rows;
};
