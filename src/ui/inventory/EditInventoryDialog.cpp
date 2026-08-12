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

#include "../../services/RebrickableApiClient.h"
#include "../../settings/UserSettings.h"

#include <QCheckBox>
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

    m_showAllColorsCheck = new QCheckBox("Show all colors", this);

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
    layout->addRow(QString(), m_showAllColorsCheck);

    layout->addRow("Condition:", m_conditionCombo);

    layout->addRow("Ownership:", m_ownershipCombo);

    layout->addRow("Quantity:", m_quantitySpin);

    layout->addRow(m_buttonBox);

    m_rebrickableApiClient = new RebrickableApiClient(this);

    connect(m_rebrickableApiClient,
            &RebrickableApiClient::partColorsFinished,
            this,
            [this](const RebrickableApiClient::PartColorsResult& result) {
                if (!result.success) {
                    loadAllColors();

                    const int index = m_colorCombo->findData(m_originalColorId);

                    if (index >= 0) {
                        m_colorCombo->setCurrentIndex(index);
                    }

                    m_showAllColorsCheck->setChecked(true);

                    return;
                }

                m_knownRebrickableColorIds.clear();

                for (const auto& color : result.colors) {
                    m_knownRebrickableColorIds.append(color.rebrickableColorId);
                }

                applyKnownColors();
            });

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &EditInventoryDialog::saveChanges);

    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadAllColors();

    if (!loadInventoryRecord()) {
        if (QPushButton* saveButton = m_buttonBox->button(QDialogButtonBox::Save)) {
            saveButton->setEnabled(false);
        }
        return;
    }

    loadKnownColors();
}

void EditInventoryDialog::loadAllColors()
{
    m_colorCombo->clear();

    ColorRepository repository;

    const QList<Color> colors = repository.getAll();

    for (const Color& color : colors) {
        m_colorCombo->addItem(color.name(), color.id());
    }

    m_colorCombo->setEnabled(true);
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

    m_originalColorId = record->colorId();

    // Preserve the current storage location.
    // Physical relocation is handled through
    // the dedicated Move Inventory workflow.
    m_storageLocationId = record->storageLocationId();

    PartRepository partRepository;

    const std::optional<Part> part = partRepository.getById(m_partId);

    if (part) {
        m_partNumber = part->partNumber();

        m_partLabel->setText(QString("%1 — %2").arg(part->partNumber()).arg(part->name()));
    }

    const int colorIndex = m_colorCombo->findData(record->colorId());

    if (colorIndex >= 0) {
        m_colorCombo->setCurrentIndex(colorIndex);
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

    updated.setStorageLocationId(m_storageLocationId);

    updated.setCondition(m_conditionCombo->currentText().trimmed());

    updated.setOwnershipType(m_ownershipCombo->currentText().trimmed());

    updated.setQuantity(quantity);

    if (!repository.updateOrMerge(updated)) {
        QMessageBox::critical(this, "BrickSuite", "Unable to update the inventory record.");

        return;
    }

    accept();
}

void EditInventoryDialog::loadKnownColors()
{
    m_knownRebrickableColorIds.clear();

    const QString apiKey = UserSettings::instance().rebrickableApiKey();

    if (m_partNumber.isEmpty() || apiKey.isEmpty()) {
        loadAllColors();

        const int index = m_colorCombo->findData(m_originalColorId);

        if (index >= 0) {
            m_colorCombo->setCurrentIndex(index);
        }

        m_showAllColorsCheck->setChecked(true);

        return;
    }

    m_colorCombo->clear();

    m_colorCombo->addItem("Loading known colors...");

    m_colorCombo->setEnabled(false);

    m_rebrickableApiClient->getPartColors(m_partNumber, apiKey);
}

void EditInventoryDialog::applyKnownColors()
{
    m_colorCombo->clear();

    ColorRepository repository;

    const QList<Color> colors = repository.getAll();

    bool originalColorAdded = false;

    for (const Color& color : colors) {
        const bool knownColor = m_knownRebrickableColorIds.contains(color.rebrickableId());

        const bool currentColor = color.id() == m_originalColorId;

        if (!knownColor && !currentColor) {
            continue;
        }

        m_colorCombo->addItem(color.name(), color.id());

        if (currentColor) {
            originalColorAdded = true;
        }
    }

    m_colorCombo->setEnabled(true);

    if (m_colorCombo->count() == 0) {
        loadAllColors();

        m_showAllColorsCheck->setChecked(true);

        return;
    }

    const int originalIndex = m_colorCombo->findData(m_originalColorId);

    if (originalIndex >= 0) {
        m_colorCombo->setCurrentIndex(originalIndex);
    }
}

void EditInventoryDialog::showAllColorsToggled(bool checked)
{
    const int currentColorId = m_colorCombo->currentData().toInt();

    if (checked) {
        loadAllColors();

        int index = m_colorCombo->findData(currentColorId);

        if (index < 0) {
            index = m_colorCombo->findData(m_originalColorId);
        }

        if (index >= 0) {
            m_colorCombo->setCurrentIndex(index);
        }

        return;
    }

    if (!m_knownRebrickableColorIds.isEmpty()) {
        applyKnownColors();
    } else {
        loadKnownColors();
    }
}