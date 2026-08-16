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

#include "../../services/RebrickableApiClient.h"
#include "../../settings/UserSettings.h"

#include "../helpers/ColorComboHelper.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHash>
#include <QSet>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QTimer>

AddInventoryDialog::AddInventoryDialog(int partId,
                                       WorkspaceContext& workspaceContext,
                                       QWidget* parent)
    : QDialog(parent)
    , m_partId(partId)
    , m_workspaceContext(workspaceContext)
{
    m_quickEntryMode = false;

    initializeUi();

    loadPart();

    loadKnownColors();

    loadStorageLocations();

    updateAddButtonState();
}

AddInventoryDialog::AddInventoryDialog(WorkspaceContext& workspaceContext, QWidget* parent)
    : QDialog(parent)
    , m_workspaceContext(workspaceContext)
{
    m_quickEntryMode = true;

    initializeUi();

    loadStorageLocations();

    clearPartSelection();

    updateAddButtonState();

    m_partSearchEdit->setFocus();
}

void AddInventoryDialog::initializeUi()
{
    setWindowTitle(m_quickEntryMode ? "Add Part to Inventory" : "Add to Inventory");

    resize(m_quickEntryMode ? 650 : 500, 340);

    auto* layout = new QFormLayout(this);

    //
    // Part control.
    //
    if (m_quickEntryMode) {
        m_partSearchEdit = new QLineEdit(this);

        m_partSearchEdit->setPlaceholderText("Search by Part Number or Name");

        m_partSearchModel = new QStandardItemModel(this);

        m_partCompleter = new QCompleter(m_partSearchModel, this);

        m_partCompleter->setCaseSensitivity(Qt::CaseInsensitive);

        m_partCompleter->setCompletionMode(QCompleter::UnfilteredPopupCompletion);

        m_partCompleter->setMaxVisibleItems(12);

        m_partSearchEdit->setCompleter(m_partCompleter);

        layout->addRow("Part:", m_partSearchEdit);

        m_partSearchTimer = new QTimer(this);

        m_partSearchTimer->setSingleShot(true);

        m_partSearchTimer->setInterval(200);
    } else {
        m_partLabel = new QLabel(this);

        layout->addRow("Part:", m_partLabel);
    }

    m_colorCombo = new QComboBox(this);

    m_showAllColorsCheck = new QCheckBox("Show all colors", this);

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

    m_keepOpenCheck = new QCheckBox("Keep Open", this);

    m_keepOpenCheck->setVisible(m_quickEntryMode);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    if (QPushButton* okButton = m_buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setText("Add");
    }

    layout->addRow("Color:", m_colorCombo);

    layout->addRow(QString(), m_showAllColorsCheck);

    layout->addRow("Storage:", m_storageCombo);

    layout->addRow("Condition:", m_conditionCombo);

    layout->addRow("Ownership:", m_ownershipCombo);

    layout->addRow("Quantity:", m_quantitySpin);

    if (m_quickEntryMode) {
        layout->addRow(QString(), m_keepOpenCheck);
    }

    layout->addRow(m_buttonBox);

    m_rebrickableApiClient = new RebrickableApiClient(this);

    connect(m_showAllColorsCheck,
            &QCheckBox::toggled,
            this,
            &AddInventoryDialog::showAllColorsToggled);

    connect(m_rebrickableApiClient,
            &RebrickableApiClient::partColorsFinished,
            this,
            [this](const RebrickableApiClient::PartColorsResult& result) {
                //
                // Ignore an old asynchronous response belonging
                // to a Part that is no longer selected.
                //
                if (result.partNumber.trimmed().compare(m_partNumber, Qt::CaseInsensitive) != 0) {
                    return;
                }

                //
                // Preserve the user's current selection in case
                // the color list is rebuilt underneath them.
                //
                const int previouslySelectedColorId = m_colorCombo->currentData().toInt();

                if (!result.success) {
                    //
                    // If the user has already selected Show All
                    // Colors, don't rebuild the combo again.
                    //
                    if (!m_showAllColorsCheck->isChecked()) {
                        loadAllColors();

                        QSignalBlocker blocker(m_showAllColorsCheck);

                        m_showAllColorsCheck->setChecked(true);
                    }

                    updateAddButtonState();

                    return;
                }

                m_knownRebrickableColorIds.clear();

                for (const auto& color : result.colors) {
                    m_knownRebrickableColorIds.append(color.rebrickableColorId);
                }

                //
                // If Show All Colors is currently selected,
                // remember the known-color information but do
                // not replace the user's current list.
                //
                if (m_showAllColorsCheck->isChecked()) {
                    updateAddButtonState();

                    return;
                }

                applyKnownColors(previouslySelectedColorId);

                updateAddButtonState();
            });

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &AddInventoryDialog::addInventory);

    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (m_quickEntryMode) {
        connect(m_partSearchEdit, &QLineEdit::textEdited, this, [this]() {
            //
            // Editing invalidates any previously
            // selected catalog Part.
            //
            m_partId = 0;
            m_partNumber.clear();

            m_colorCombo->clear();
            m_colorCombo->setEnabled(false);

            updateAddButtonState();

            m_partSearchTimer->start();
        });

        connect(m_partSearchTimer, &QTimer::timeout, this, &AddInventoryDialog::updatePartSearch);

        connect(m_partCompleter,
                QOverload<const QModelIndex&>::of(&QCompleter::activated),
                this,
                &AddInventoryDialog::selectSearchResult);
    }
}

void AddInventoryDialog::loadPart()
{
    PartRepository repository;

    const std::optional<Part> part = repository.getById(m_partId);

    if (!part) {
        m_partNumber.clear();

        m_partLabel->setText("Unable to load part.");

        if (QPushButton* okButton = m_buttonBox->button(QDialogButtonBox::Ok)) {
            okButton->setEnabled(false);
        }

        return;
    }

    // Save the Rebrickable part number so
    // loadKnownColors() can request the correct
    // part-specific color list.
    m_partNumber = part->partNumber();

    m_partLabel->setText(QString("%1 — %2").arg(part->partNumber()).arg(part->name()));
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
    QSet<int> activeParentIds;

    for (const StorageLocation& location : locations) {
        locationById.insert(location.id(), location);

        if (location.parentLocationId() > 0) {
            activeParentIds.insert(location.parentLocationId());
        }
    }

    for (const StorageLocation& location : locations) {
        // Parent/container locations remain part of the hierarchy so
        // full paths can be built, but only active leaf locations
        // are offered for operational inventory selection.
        if (activeParentIds.contains(location.id())) {
            continue;
        }

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

    updateAddButtonState();
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

    m_inventoryWasAdded = true;

    if (m_quickEntryMode && m_keepOpenCheck->isChecked()) {
        //
        // Preserve the Color and Quantity for rapid entry.
        // Storage, Condition and Ownership already remain
        // unchanged.
        //
        m_quickEntryColorId = m_colorCombo->currentData().toInt();

        clearPartSelection();

        return;
    }

    accept();
}

void AddInventoryDialog::loadKnownColors()
{
    m_colorCombo->clear();

    m_knownRebrickableColorIds.clear();

    const QString apiKey = UserSettings::instance().rebrickableApiKey();

    if (m_partNumber.isEmpty() || apiKey.isEmpty()) {
        loadAllColors();

        m_showAllColorsCheck->setChecked(true);

        return;
    }

    // Give the user feedback while the API request
    // is in progress.
    m_colorCombo->addItem("Loading known colors...");

    m_colorCombo->setEnabled(false);

    m_rebrickableApiClient->getPartColors(m_partNumber, apiKey);
}

void AddInventoryDialog::loadAllColors()
{
    m_colorCombo->clear();

    ColorRepository repository;

    const QList<Color> colors = repository.getAll();

    for (const Color& color : colors) {
        ColorComboHelper::addColorItem(m_colorCombo, color.name(), color.id(), color.rgb());
    }

    m_colorCombo->setEnabled(true);

    updateAddButtonState();
}

void AddInventoryDialog::applyKnownColors(int preferredColorId)
{
    m_colorCombo->clear();

    ColorRepository repository;

    const QList<Color> colors = repository.getAll();

    for (const Color& color : colors) {
        if (!m_knownRebrickableColorIds.contains(color.rebrickableId())) {
            continue;
        }

        //
        // Use the same helper as the rest of BrickSuite.
        // The internal BrickSuite Color ID remains the
        // combo item's user data.
        //
        ColorComboHelper::addColorItem(m_colorCombo, color.name(), color.id(), color.rgb());
    }

    m_colorCombo->setEnabled(true);

    //
    // If the user had already chosen a color before
    // this list was rebuilt, restore that exact
    // BrickSuite Color ID.
    //
    if (preferredColorId > 0) {
        const int preferredIndex = m_colorCombo->findData(preferredColorId);

        if (preferredIndex >= 0) {
            m_colorCombo->setCurrentIndex(preferredIndex);
        }
    }

    //
    // Defensive fallback:
    // if none of the Rebrickable colors mapped into
    // BrickSuite's local Color Catalog, show all.
    //
    if (m_colorCombo->count() == 0) {
        loadAllColors();

        QSignalBlocker blocker(m_showAllColorsCheck);

        m_showAllColorsCheck->setChecked(true);
    }

    updateAddButtonState();
}

void AddInventoryDialog::showAllColorsToggled(bool checked)
{
    if (checked) {
        loadAllColors();

        return;
    }

    if (!m_knownRebrickableColorIds.isEmpty()) {
        applyKnownColors();
    } else {
        loadKnownColors();
    }
}

void AddInventoryDialog::updatePartSearch()
{
    if (!m_quickEntryMode)
        return;

    const QString searchText = m_partSearchEdit->text().trimmed();

    m_partSearchModel->clear();

    if (searchText.length() < 2) {
        m_partCompleter->popup()->hide();

        return;
    }

    PartRepository repository;

    const QList<Part> parts = repository.searchForInventoryEntry(searchText, 20);

    for (const Part& part : parts) {
        const QString displayText = QString("%1 — %2").arg(part.partNumber(), part.name());

        auto* item = new QStandardItem(displayText);

        item->setData(part.id(), Qt::UserRole);

        item->setData(part.partNumber(), Qt::UserRole + 1);

        m_partSearchModel->appendRow(item);
    }

    if (m_partSearchModel->rowCount() > 0) {
        m_partCompleter->complete();
    }
}

void AddInventoryDialog::selectSearchResult(const QModelIndex& index)
{
    if (!index.isValid())
        return;

    const int partId = index.data(Qt::UserRole).toInt();

    if (partId <= 0)
        return;

    PartRepository repository;

    const std::optional<Part> part = repository.getById(partId);

    if (!part)
        return;

    m_partId = part->id();

    m_partNumber = part->partNumber();

    //
    // Set the displayed text to the validated
    // catalog selection.
    //
    m_partSearchEdit->setText(QString("%1 — %2").arg(part->partNumber(), part->name()));

    //
    // During rapid Keep-Open entry, preserve the user's
    // working color across Parts.
    //
    if (m_quickEntryMode && m_keepOpenCheck->isChecked() && m_quickEntryColorId > 0) {
        //
        // The user is physically sorting by this color,
        // so make the complete local color catalog
        // available rather than allowing the Part's
        // Rebrickable known-color list to remove it.
        //
        {
            QSignalBlocker blocker(m_showAllColorsCheck);

            m_showAllColorsCheck->setChecked(true);
        }

        loadAllColors();

        const int colorIndex = m_colorCombo->findData(m_quickEntryColorId);

        if (colorIndex >= 0) {
            m_colorCombo->setCurrentIndex(colorIndex);
        }

        updateAddButtonState();
    } else {
        loadKnownColors();

        updateAddButtonState();
    }
}

void AddInventoryDialog::clearPartSelection()
{
    m_partId = 0;

    m_partNumber.clear();

    m_knownRebrickableColorIds.clear();

    if (m_partSearchTimer) {
        m_partSearchTimer->stop();
    }

    if (m_partSearchEdit) {
        m_partSearchEdit->clear();
    }

    if (m_partSearchModel) {
        m_partSearchModel->clear();
    }

    //
    // There is no valid Part selected yet, so Add
    // remains disabled. Do not reset Quantity.
    //
    m_colorCombo->setEnabled(false);

    updateAddButtonState();

    if (m_partSearchEdit) {
        m_partSearchEdit->setFocus();
    }
}

void AddInventoryDialog::updateAddButtonState()
{
    QPushButton* addButton = m_buttonBox->button(QDialogButtonBox::Ok);

    if (!addButton)
        return;

    const bool validPart = m_partId > 0;

    const bool validColor = m_colorCombo->isEnabled() && m_colorCombo->currentData().toInt() > 0;

    const bool validStorage = m_storageCombo->currentData().toInt() > 0;

    addButton->setEnabled(validPart && validColor && validStorage);
}

bool AddInventoryDialog::inventoryWasAdded() const
{
    return m_inventoryWasAdded;
}
