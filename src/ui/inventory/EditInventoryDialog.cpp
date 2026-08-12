#include "EditInventoryDialog.h"

#include "../../app/WorkspaceContext.h"

#include "../../models/Color.h"
#include "../../models/InventoryRecord.h"
#include "../../models/Part.h"
#include "../../models/StorageLocation.h"

#include "../../repositories/ColorRepository.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/StorageLocationRepository.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHash>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>

EditInventoryDialog::EditInventoryDialog(int inventoryRecordId,
                                         WorkspaceContext& workspaceContext,
                                         QWidget* parent)
    : QDialog(parent)
    , m_inventoryRecordId(inventoryRecordId)
    , m_workspaceContext(workspaceContext)
{
    setWindowTitle("Edit Inventory");

    resize(520, 320);

    auto* layout = new QFormLayout(this);

    m_partLabel = new QLabel(this);

    m_colorCombo = new QComboBox(this);

    m_storageCombo = new QComboBox(this);

    m_conditionCombo = new QComboBox(this);

    m_ownershipCombo = new QComboBox(this);

    m_quantitySpin = new QSpinBox(this);

    m_quantitySpin->setMinimum(0);
    m_quantitySpin->setMaximum(1000000);

    m_conditionCombo->addItem("Used");
    m_conditionCombo->addItem("New");

    m_ownershipCombo->addItem("Owned");

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);

    layout->addRow("Part:", m_partLabel);

    layout->addRow("Color:", m_colorCombo);

    layout->addRow("Storage:", m_storageCombo);

    layout->addRow("Condition:", m_conditionCombo);

    layout->addRow("Ownership:", m_ownershipCombo);

    layout->addRow("Quantity:", m_quantitySpin);

    layout->addRow(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &EditInventoryDialog::saveChanges);

    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadColors();
    loadStorageLocations();

    if (!loadInventoryRecord()) {
        if (QPushButton* saveButton = m_buttonBox->button(QDialogButtonBox::Save)) {
            saveButton->setEnabled(false);
        }
    }
}

void EditInventoryDialog::loadColors()
{
    ColorRepository repository;

    const QList<Color> colors = repository.getAll();

    for (const Color& color : colors) {
        m_colorCombo->addItem(color.name(), color.id());
    }
}

void EditInventoryDialog::loadStorageLocations()
{
    if (!m_workspaceContext.hasCurrentWorkspace())
        return;

    StorageLocationRepository repository;

    const QList<StorageLocation> locations = repository.getByWorkspace(
        m_workspaceContext.currentWorkspaceId());

    QHash<int, StorageLocation> locationById;

    for (const StorageLocation& location : locations) {
        locationById.insert(location.id(), location);
    }

    for (const StorageLocation& location : locations) {
        QStringList pathParts;

        pathParts.prepend(location.name());

        int parentId = location.parentLocationId();

        while (parentId > 0) {
            if (!locationById.contains(parentId))
                break;

            const StorageLocation parent = locationById.value(parentId);

            pathParts.prepend(parent.name());

            parentId = parent.parentLocationId();
        }

        m_storageCombo->addItem(pathParts.join(" / "), location.id());
    }
}

bool EditInventoryDialog::loadInventoryRecord()
{
    InventoryRecordRepository inventoryRepository;

    const std::optional<InventoryRecord> record = inventoryRepository.getById(m_inventoryRecordId);

    if (!record) {
        QMessageBox::warning(this, "BrickSuite", "Unable to load the selected inventory record.");

        return false;
    }

    m_partId = record->partId();

    PartRepository partRepository;

    const std::optional<Part> part = partRepository.getById(m_partId);

    if (part) {
        m_partLabel->setText(QString("%1 — %2").arg(part->partNumber()).arg(part->name()));
    }

    const int colorIndex = m_colorCombo->findData(record->colorId());

    if (colorIndex >= 0) {
        m_colorCombo->setCurrentIndex(colorIndex);
    }

    const int storageIndex = m_storageCombo->findData(record->storageLocationId());

    if (storageIndex >= 0) {
        m_storageCombo->setCurrentIndex(storageIndex);
    }

    const int conditionIndex = m_conditionCombo->findText(record->condition());

    if (conditionIndex >= 0) {
        m_conditionCombo->setCurrentIndex(conditionIndex);
    }

    const int ownershipIndex = m_ownershipCombo->findText(record->ownershipType());

    if (ownershipIndex >= 0) {
        m_ownershipCombo->setCurrentIndex(ownershipIndex);
    }

    m_quantitySpin->setValue(record->quantity());

    return true;
}

void EditInventoryDialog::saveChanges()
{
    InventoryRecordRepository repository;

    const std::optional<InventoryRecord> existing = repository.getById(m_inventoryRecordId);

    if (!existing) {
        QMessageBox::warning(this, "BrickSuite", "Unable to reload the inventory record.");

        return;
    }

    const int quantity = m_quantitySpin->value();

    if (quantity == 0) {
        QMessageBox::warning(this,
                             "BrickSuite",
                             "Quantity cannot be zero in this version.\n\n"
                             "A remove-inventory workflow will be added separately.");

        return;
    }

    InventoryRecord updated = *existing;

    updated.setColorId(m_colorCombo->currentData().toInt());

    updated.setStorageLocationId(m_storageCombo->currentData().toInt());

    updated.setCondition(m_conditionCombo->currentText().trimmed());

    updated.setOwnershipType(m_ownershipCombo->currentText().trimmed());

    updated.setQuantity(quantity);

    if (!repository.updateOrMerge(updated)) {
        QMessageBox::critical(this, "BrickSuite", "Unable to update the inventory record.");

        return;
    }

    accept();
}