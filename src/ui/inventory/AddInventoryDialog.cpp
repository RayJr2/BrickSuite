#include "AddInventoryDialog.h"

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

AddInventoryDialog::AddInventoryDialog(int partId,
                                       WorkspaceContext& workspaceContext,
                                       QWidget* parent)
    : QDialog(parent)
    , m_partId(partId)
    , m_workspaceContext(workspaceContext)
{
    setWindowTitle("Add to Inventory");

    resize(500, 320);

    auto* layout = new QFormLayout(this);

    m_partLabel = new QLabel(this);

    m_colorCombo = new QComboBox(this);

    m_storageCombo = new QComboBox(this);

    m_conditionCombo = new QComboBox(this);

    m_ownershipCombo = new QComboBox(this);

    m_quantitySpin = new QSpinBox(this);

    m_quantitySpin->setMinimum(1);
    m_quantitySpin->setMaximum(1000000);
    m_quantitySpin->setValue(1);

    m_conditionCombo->addItem("Used");
    m_conditionCombo->addItem("New");

    m_ownershipCombo->addItem("Owned");

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    if (QPushButton* okButton = m_buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setText("Add");
    }

    layout->addRow("Part:", m_partLabel);

    layout->addRow("Color:", m_colorCombo);

    layout->addRow("Storage:", m_storageCombo);

    layout->addRow("Condition:", m_conditionCombo);

    layout->addRow("Ownership:", m_ownershipCombo);

    layout->addRow("Quantity:", m_quantitySpin);

    layout->addRow(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &AddInventoryDialog::addInventory);

    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadPart();
    loadColors();
    loadStorageLocations();
}

void AddInventoryDialog::loadPart()
{
    PartRepository repository;

    const std::optional<Part> part = repository.getById(m_partId);

    if (!part) {
        m_partLabel->setText("Unable to load part.");

        if (QPushButton* okButton = m_buttonBox->button(QDialogButtonBox::Ok)) {
            okButton->setEnabled(false);
        }

        return;
    }

    m_partLabel->setText(QString("%1 — %2").arg(part->partNumber()).arg(part->name()));
}

void AddInventoryDialog::loadColors()
{
    m_colorCombo->clear();

    ColorRepository repository;

    const QList<Color> colors = repository.getAll();

    for (const Color& color : colors) {
        m_colorCombo->addItem(color.name(), color.id());
    }
}

void AddInventoryDialog::loadStorageLocations()
{
    m_storageCombo->clear();

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

void AddInventoryDialog::addInventory()
{
    if (!m_workspaceContext.hasCurrentWorkspace()) {
        QMessageBox::warning(this, "BrickSuite", "Select a workspace before adding inventory.");

        return;
    }

    if (m_partId <= 0) {
        QMessageBox::warning(this, "BrickSuite", "No valid part is selected.");

        return;
    }

    const int colorId = m_colorCombo->currentData().toInt();

    const int storageLocationId = m_storageCombo->currentData().toInt();

    if (colorId <= 0) {
        QMessageBox::warning(this, "BrickSuite", "Select a color.");

        return;
    }

    if (storageLocationId <= 0) {
        QMessageBox::warning(this, "BrickSuite", "Select a storage location.");

        return;
    }

    InventoryRecord record;

    record.setWorkspaceId(m_workspaceContext.currentWorkspaceId());

    record.setPartId(m_partId);

    record.setColorId(colorId);

    record.setStorageLocationId(storageLocationId);

    record.setCondition(m_conditionCombo->currentText().trimmed());

    record.setOwnershipType(m_ownershipCombo->currentText().trimmed());

    record.setQuantity(m_quantitySpin->value());

    InventoryRecordRepository repository;

    if (!repository.addOrIncreaseQuantity(record)) {
        QMessageBox::critical(this, "BrickSuite", "Unable to add the part to inventory.");

        return;
    }

    accept();
}
