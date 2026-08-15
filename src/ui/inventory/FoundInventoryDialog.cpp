#include "FoundInventoryDialog.h"

#include "../../models/LostInventoryItem.h"
#include "../../models/StorageLocation.h"

#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/LostInventoryRepository.h"
#include "../../repositories/StorageLocationRepository.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHash>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>

FoundInventoryDialog::FoundInventoryDialog(int workspaceId, int partId, int colorId, QWidget* parent)
    : QDialog(parent)
    , m_workspaceId(workspaceId)
    , m_partId(partId)
    , m_colorId(colorId)
{
    setWindowTitle("Found / Return Inventory");

    resize(525, 340);

    auto* layout = new QFormLayout(this);

    m_partLabel = new QLabel(this);

    m_colorLabel = new QLabel(this);

    m_outstandingLabel = new QLabel(this);

    m_quantityFoundSpin = new QSpinBox(this);

    m_storageCombo = new QComboBox(this);

    m_conditionCombo = new QComboBox(this);

    m_ownershipCombo = new QComboBox(this);

    m_notesEdit = new QTextEdit(this);

    m_notesEdit->setMaximumHeight(90);

    m_notesEdit->setPlaceholderText("Optional note about where the part was found.");

    m_conditionCombo->addItem("Used");

    m_conditionCombo->addItem("New");

    m_ownershipCombo->addItem("Owned");

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    if (QPushButton* okButton = m_buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setText("Return to Inventory");
    }

    layout->addRow("Part:", m_partLabel);

    layout->addRow("Color:", m_colorLabel);

    layout->addRow("Outstanding Lost:", m_outstandingLabel);

    layout->addRow("Quantity Found:", m_quantityFoundSpin);

    layout->addRow("Return To:", m_storageCombo);

    layout->addRow("Condition:", m_conditionCombo);

    layout->addRow("Ownership:", m_ownershipCombo);

    layout->addRow("Notes:", m_notesEdit);

    layout->addRow(m_buttonBox);

    loadStorageLocations();

    if (!loadLostInventory()) {
        if (QPushButton* okButton = m_buttonBox->button(QDialogButtonBox::Ok)) {
            okButton->setEnabled(false);
        }
    }

    connect(m_buttonBox,
            &QDialogButtonBox::accepted,
            this,
            &FoundInventoryDialog::returnFoundInventory);

    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

bool FoundInventoryDialog::loadLostInventory()
{
    LostInventoryRepository repository;

    const std::optional<LostInventoryItem> item
        = repository.getOutstandingForPartColor(m_workspaceId, m_partId, m_colorId);

    if (!item) {
        QMessageBox::information(this,
                                 "Found Inventory",
                                 "There is no outstanding lost quantity "
                                 "for this Part / Color.");

        return false;
    }

    m_outstandingQuantity = item->outstandingQuantity;

    m_lastStorageLocationId = item->lastStorageLocationId;

    m_partLabel->setText(QString("%1 — %2").arg(item->partNumber, item->partName));

    m_colorLabel->setText(item->colorName);

    m_outstandingLabel->setText(QString::number(m_outstandingQuantity));

    m_quantityFoundSpin->setRange(1, m_outstandingQuantity);

    m_quantityFoundSpin->setValue(1);

    //
    // Default Condition / Ownership from the
    // most recent Lost event.
    //
    const int conditionIndex = m_conditionCombo->findText(item->condition, Qt::MatchFixedString);

    if (conditionIndex >= 0) {
        m_conditionCombo->setCurrentIndex(conditionIndex);
    }

    const int ownershipIndex = m_ownershipCombo->findText(item->ownershipType, Qt::MatchFixedString);

    if (ownershipIndex >= 0) {
        m_ownershipCombo->setCurrentIndex(ownershipIndex);
    }

    //
    // Default Return To to the last known location,
    // when that location is still active.
    //
    const int locationIndex = m_storageCombo->findData(m_lastStorageLocationId);

    if (locationIndex >= 0) {
        m_storageCombo->setCurrentIndex(locationIndex);
    }

    return true;
}

void FoundInventoryDialog::loadStorageLocations()
{
    m_storageCombo->clear();

    StorageLocationRepository repository;

    const QList<StorageLocation> locations = repository.getByWorkspace(m_workspaceId);

    QHash<int, StorageLocation> locationById;

    for (const StorageLocation& location : locations) {
        locationById.insert(location.id(), location);
    }

    for (const StorageLocation& location : locations) {
        QStringList pathParts;

        pathParts.prepend(location.name());

        int parentId = location.parentLocationId();

        while (parentId > 0) {
            if (!locationById.contains(parentId)) {
                break;
            }

            const StorageLocation parent = locationById.value(parentId);

            pathParts.prepend(parent.name());

            parentId = parent.parentLocationId();
        }

        m_storageCombo->addItem(pathParts.join(" / "), location.id());
    }
}

void FoundInventoryDialog::returnFoundInventory()
{
    if (m_outstandingQuantity <= 0)
        return;

    const int quantityFound = m_quantityFoundSpin->value();

    const int storageLocationId = m_storageCombo->currentData().toInt();

    if (quantityFound <= 0 || quantityFound > m_outstandingQuantity) {
        QMessageBox::warning(this, "Found Inventory", "Enter a valid Quantity Found.");

        return;
    }

    if (storageLocationId <= 0) {
        QMessageBox::warning(this, "Found Inventory", "Select a Return To storage location.");

        return;
    }

    const int remainingLost = m_outstandingQuantity - quantityFound;

    const QMessageBox::StandardButton response
        = QMessageBox::question(this,
                                "Return Found Inventory",
                                QString("Return %1 piece(s) to My Loose Inventory?\n\n"
                                        "Outstanding Lost: %2\n"
                                        "Quantity Found: %3\n"
                                        "Still Lost After Return: %4\n\n"
                                        "Return To:\n%5")
                                    .arg(quantityFound)
                                    .arg(m_outstandingQuantity)
                                    .arg(quantityFound)
                                    .arg(remainingLost)
                                    .arg(m_storageCombo->currentText()),
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::Yes);

    if (response != QMessageBox::Yes)
        return;

    InventoryRecordRepository repository;

    if (!repository.markFound(m_workspaceId,
                              m_partId,
                              m_colorId,
                              quantityFound,
                              storageLocationId,
                              m_conditionCombo->currentText().trimmed(),
                              m_ownershipCombo->currentText().trimmed(),
                              m_notesEdit->toPlainText().trimmed())) {
        QMessageBox::critical(this,
                              "Found Inventory",
                              "Unable to return the found inventory.\n\n"
                              "No changes were saved.");

        return;
    }

    QMessageBox::information(this,
                             "Found Inventory",
                             QString("%1 piece(s) returned to "
                                     "My Loose Inventory.")
                                 .arg(quantityFound));

    accept();
}