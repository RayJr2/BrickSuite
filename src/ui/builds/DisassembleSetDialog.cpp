#include "DisassembleSetDialog.h"

#include "../../database/DatabaseManager.h"

#include "../../models/Build.h"
#include "../../models/BuildRequirement.h"
#include "../../models/Color.h"
#include "../../models/InventoryRecord.h"
#include "../../models/Part.h"
#include "../../models/StorageLocation.h"

#include "../../repositories/BuildRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/StorageLocationRepository.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <algorithm>

DisassembleSetDialog::DisassembleSetDialog(int buildId, QWidget* parent)
    : QDialog(parent)
    , m_buildId(buildId)
{
    setWindowTitle("Disassemble Complete Set");

    resize(1100, 700);

    auto* mainLayout = new QVBoxLayout(this);

    m_buildLabel = new QLabel(this);

    QFont buildFont = m_buildLabel->font();

    buildFont.setBold(true);

    m_buildLabel->setFont(buildFont);

    mainLayout->addWidget(m_buildLabel);

    //
    // Default destination lets the user assign a
    // common sorting/storage location quickly.
    //
    auto* destinationLayout = new QHBoxLayout();

    destinationLayout->addWidget(new QLabel("Default Destination:", this));

    m_defaultDestinationCombo = new QComboBox(this);

    destinationLayout->addWidget(m_defaultDestinationCombo, 1);

    m_applyDefaultButton = new QPushButton("Apply to All", this);

    destinationLayout->addWidget(m_applyDefaultButton);

    mainLayout->addLayout(destinationLayout);

    m_table = new QTableWidget(this);

    m_table->setColumnCount(7);

    m_table->setHorizontalHeaderLabels(QStringList() << "Part #"
                                                     << "Name"
                                                     << "Color"
                                                     << "Set Qty"
                                                     << "Spare"
                                                     << "Qty Returned"
                                                     << "Move To");

    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_table->setSelectionMode(QAbstractItemView::SingleSelection);

    m_table->verticalHeader()->setVisible(false);

    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);

    mainLayout->addWidget(m_table, 1);

    m_summaryLabel = new QLabel(this);

    m_summaryLabel->setWordWrap(true);

    mainLayout->addWidget(m_summaryLabel);

    m_statusLabel = new QLabel(this);

    m_statusLabel->setWordWrap(true);

    mainLayout->addWidget(m_statusLabel);

    auto* buttonBox = new QDialogButtonBox(this);

    m_disassembleButton = buttonBox->addButton("Disassemble Set", QDialogButtonBox::AcceptRole);

    m_cancelButton = buttonBox->addButton("Cancel", QDialogButtonBox::RejectRole);

    m_disassembleButton->setEnabled(false);

    mainLayout->addWidget(buttonBox);

    connect(m_applyDefaultButton,
            &QPushButton::clicked,
            this,
            &DisassembleSetDialog::applyDefaultDestination);

    connect(m_disassembleButton, &QPushButton::clicked, this, &DisassembleSetDialog::disassembleSet);

    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    if (!loadBuild())
        return;

    if (!loadStorageLocations())
        return;

    if (!loadRequirements())
        return;

    updateSummary();
}

bool DisassembleSetDialog::loadBuild()
{
    BuildRepository repository;

    const std::optional<Build> build = repository.getById(m_buildId);

    if (!build) {
        QMessageBox::critical(this, "Disassemble Set", "Unable to load the selected Build.");

        return false;
    }

    if (build->buildType() != "Set" || build->inventoryMode() != "CompleteSet") {
        QMessageBox::warning(this,
                             "Disassemble Set",
                             "Only Complete Set builds can be "
                             "disassembled into loose inventory.");

        return false;
    }

    if (build->status() == "Disassembled") {
        QMessageBox::information(this, "Disassemble Set", "This Set has already been disassembled.");

        return false;
    }

    m_workspaceId = build->workspaceId();

    m_buildName = build->name();

    m_setNumber = build->setNumber();

    m_buildLabel->setText(QString("%1 — %2").arg(m_setNumber, m_buildName));

    return true;
}

bool DisassembleSetDialog::loadStorageLocations()
{
    StorageLocationRepository repository;

    const QList<StorageLocation> locations = repository.getByWorkspace(m_workspaceId);

    if (locations.isEmpty()) {
        QMessageBox::warning(this,
                             "Disassemble Set",
                             "No active storage locations are available.\n\n"
                             "Create a storage location before "
                             "disassembling this Set.");

        return false;
    }

    m_locations.clear();

    for (const StorageLocation& location : locations) {
        LocationChoice choice;

        choice.id = location.id();

        choice.path = storagePath(location.id());

        m_locations.append(choice);
    }

    //
    // Sort by the full visible hierarchy path.
    //
    std::sort(m_locations.begin(),
              m_locations.end(),
              [](const LocationChoice& left, const LocationChoice& right) {
                  return left.path.compare(right.path, Qt::CaseInsensitive) < 0;
              });

    populateLocationCombo(m_defaultDestinationCombo);

    return true;
}

void DisassembleSetDialog::populateLocationCombo(QComboBox* combo)
{
    combo->clear();

    combo->addItem("Select storage location...", 0);

    for (const LocationChoice& location : m_locations) {
        combo->addItem(location.path, location.id);
    }
}

QString DisassembleSetDialog::storagePath(int storageLocationId) const
{
    StorageLocationRepository repository;

    QStringList parts;

    int currentId = storageLocationId;

    int safetyCount = 0;

    while (currentId > 0 && safetyCount < 100) {
        const std::optional<StorageLocation> location = repository.getById(currentId);

        if (!location)
            break;

        parts.prepend(location->name());

        currentId = location->parentLocationId();

        ++safetyCount;
    }

    return parts.join(" / ");
}

bool DisassembleSetDialog::loadRequirements()
{
    BuildRequirementRepository requirementRepository;

    const QList<BuildRequirement> requirements = requirementRepository.getByBuild(m_buildId);

    if (requirements.isEmpty()) {
        QMessageBox::warning(this,
                             "Disassemble Set",
                             "This Complete Set does not have a parts list.\n\n"
                             "Use Load Set from Rebrickable first, then "
                             "disassemble the Set.");

        return false;
    }

    PartRepository partRepository;
    ColorRepository colorRepository;

    m_table->setRowCount(0);

    m_rows.clear();

    int tableRow = 0;

    for (const BuildRequirement& requirement : requirements) {
        const std::optional<Part> part = partRepository.getById(requirement.partId());

        const std::optional<Color> color = colorRepository.getById(requirement.colorId());

        m_table->insertRow(tableRow);

        auto* partNumberItem = new QTableWidgetItem(part ? part->partNumber()
                                                         : QString::number(requirement.partId()));

        auto* nameItem = new QTableWidgetItem(part ? part->name() : QString());

        auto* colorItem = new QTableWidgetItem(color ? color->name()
                                                     : QString::number(requirement.colorId()));

        auto* setQuantityItem = new QTableWidgetItem(
            QString::number(requirement.quantityRequired()));

        auto* spareItem = new QTableWidgetItem(requirement.isSpare() ? "Yes" : "No");

        auto* quantitySpin = new QSpinBox(m_table);

        quantitySpin->setRange(0, requirement.quantityRequired());

        //
        // Default assumption: the physical Set
        // contains the published quantity.
        //
        quantitySpin->setValue(requirement.quantityRequired());

        quantitySpin->setAlignment(Qt::AlignCenter);

        auto* destinationCombo = new QComboBox(m_table);

        populateLocationCombo(destinationCombo);

        setQuantityItem->setTextAlignment(Qt::AlignCenter);

        spareItem->setTextAlignment(Qt::AlignCenter);

        m_table->setItem(tableRow, 0, partNumberItem);

        m_table->setItem(tableRow, 1, nameItem);

        m_table->setItem(tableRow, 2, colorItem);

        m_table->setItem(tableRow, 3, setQuantityItem);

        m_table->setItem(tableRow, 4, spareItem);

        m_table->setCellWidget(tableRow, 5, quantitySpin);

        m_table->setCellWidget(tableRow, 6, destinationCombo);

        RowData row;

        row.requirementId = requirement.id();

        row.partId = requirement.partId();

        row.colorId = requirement.colorId();

        row.setQuantity = requirement.quantityRequired();

        row.isSpare = requirement.isSpare();

        row.quantitySpin = quantitySpin;

        row.destinationCombo = destinationCombo;

        m_rows.append(row);

        connect(quantitySpin, &QSpinBox::valueChanged, this, [this]() { updateSummary(); });

        connect(destinationCombo, &QComboBox::currentIndexChanged, this, [this]() {
            updateSummary();
        });

        ++tableRow;
    }

    return true;
}

void DisassembleSetDialog::applyDefaultDestination()
{
    const int locationId = m_defaultDestinationCombo->currentData().toInt();

    if (locationId <= 0) {
        QMessageBox::information(this, "Disassemble Set", "Select a Default Destination first.");

        return;
    }

    for (RowData& row : m_rows) {
        const int index = row.destinationCombo->findData(locationId);

        if (index >= 0) {
            row.destinationCombo->setCurrentIndex(index);
        }
    }

    updateSummary();
}

void DisassembleSetDialog::updateSummary()
{
    int regularPieces = 0;
    int sparePieces = 0;

    int returnedPieces = 0;

    int rowsWithoutDestination = 0;

    for (const RowData& row : m_rows) {
        const int quantity = row.quantitySpin->value();

        if (row.isSpare)
            sparePieces += quantity;
        else
            regularPieces += quantity;

        returnedPieces += quantity;

        //
        // Qty 0 means this piece is not being
        // returned to loose inventory, so it does
        // not need a destination.
        //
        if (quantity > 0 && row.destinationCombo->currentData().toInt() <= 0) {
            ++rowsWithoutDestination;
        }
    }

    m_summaryLabel->setText(QString("Regular pieces returned: %1     "
                                    "Spare pieces returned: %2     "
                                    "Total loose pieces: %3")
                                .arg(regularPieces)
                                .arg(sparePieces)
                                .arg(returnedPieces));

    if (rowsWithoutDestination > 0) {
        m_statusLabel->setText(QString("%1 row(s) with returned pieces still "
                                       "need a storage destination.")
                                   .arg(rowsWithoutDestination));

        m_disassembleButton->setEnabled(false);

        return;
    }

    if (returnedPieces <= 0) {
        m_statusLabel->setText("At least one piece must be returned "
                               "to loose inventory.");

        m_disassembleButton->setEnabled(false);

        return;
    }

    m_statusLabel->setText("Ready to disassemble. No inventory changes "
                           "have been made yet.");

    m_disassembleButton->setEnabled(true);
}

void DisassembleSetDialog::disassembleSet()
{
    int totalReturned = 0;
    int rowsReturned = 0;

    for (const RowData& row : m_rows) {
        const int quantity = row.quantitySpin->value();

        if (quantity <= 0)
            continue;

        const int storageLocationId = row.destinationCombo->currentData().toInt();

        if (storageLocationId <= 0) {
            QMessageBox::warning(this,
                                 "Disassemble Set",
                                 "Every returned Part must have a "
                                 "storage destination.");

            return;
        }

        totalReturned += quantity;

        ++rowsReturned;
    }

    const QMessageBox::StandardButton response
        = QMessageBox::question(this,
                                "Disassemble Complete Set",
                                QString("Disassemble this Complete Set?\n\n"
                                        "%1\n"
                                        "Set: %2\n\n"
                                        "Part/Color rows returned: %3\n"
                                        "Loose pieces added: %4\n\n"
                                        "The selected quantities will be added "
                                        "to My Loose Inventory at their chosen "
                                        "storage locations.\n\n"
                                        "Inventory movement history will be "
                                        "recorded and the Build Status will "
                                        "be changed to Disassembled.")
                                    .arg(m_buildName)
                                    .arg(m_setNumber)
                                    .arg(rowsReturned)
                                    .arg(totalReturned),
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::No);

    if (response != QMessageBox::Yes)
        return;

    QSqlDatabase database = DatabaseManager::instance().database();

    if (!database.transaction()) {
        QMessageBox::critical(this,
                              "Disassemble Set",
                              "Unable to start the disassembly "
                              "transaction.");

        return;
    }

    InventoryRecordRepository inventoryRepository;

    for (const RowData& row : m_rows) {
        const int quantity = row.quantitySpin->value();

        if (quantity <= 0)
            continue;

        const int storageLocationId = row.destinationCombo->currentData().toInt();

        //
        // Revalidate the destination inside the
        // transaction.
        //
        StorageLocationRepository storageRepository;

        const std::optional<StorageLocation> destination = storageRepository.getById(
            storageLocationId);

        if (!destination || !destination->isActive()
            || destination->workspaceId() != m_workspaceId) {
            database.rollback();

            QMessageBox::critical(this,
                                  "Disassemble Set",
                                  "A selected storage location is no "
                                  "longer valid.\n\n"
                                  "No changes were saved.");

            return;
        }

        InventoryRecord record;

        record.setWorkspaceId(m_workspaceId);

        record.setPartId(row.partId);

        record.setColorId(row.colorId);

        record.setStorageLocationId(storageLocationId);

        //
        // Once an assembled Set has been disassembled,
        // these pieces are loose workshop pieces.
        //
        record.setCondition("Used");

        record.setOwnershipType("Owned");

        record.setQuantity(quantity);

        const QString requirementType = row.isSpare ? "spare" : "regular";

        const QString notes = QString("Disassembled from %1 (%2), "
                                      "%3 Set requirement.")
                                  .arg(m_buildName)
                                  .arg(m_setNumber)
                                  .arg(requirementType);

        //
        // manageTransaction=false because this entire
        // Set operation owns the outer transaction.
        //
        // addOrIncreaseQuantity() will still create
        // the InventoryMovement record.
        //
        if (!inventoryRepository.addOrIncreaseQuantity(record,
                                                       "SetDisassembly",
                                                       "Build",
                                                       QString::number(m_buildId),
                                                       notes,
                                                       false)) {
            database.rollback();

            QMessageBox::critical(this,
                                  "Disassemble Set",
                                  "Unable to add a Set Part to loose "
                                  "inventory.\n\n"
                                  "No changes were saved.");

            return;
        }
    }

    //
    // Only mark the Build Disassembled after every
    // inventory row and movement record succeeds.
    //
    BuildRepository buildRepository;

    std::optional<Build> build = buildRepository.getById(m_buildId);

    if (!build) {
        database.rollback();

        QMessageBox::critical(this,
                              "Disassemble Set",
                              "Unable to reload the Build.\n\n"
                              "No changes were saved.");

        return;
    }

    build->setStatus("Disassembled");

    if (!buildRepository.update(*build)) {
        database.rollback();

        QMessageBox::critical(this,
                              "Disassemble Set",
                              "Unable to update the Build Status.\n\n"
                              "No changes were saved.");

        return;
    }

    if (!database.commit()) {
        database.rollback();

        QMessageBox::critical(this,
                              "Disassemble Set",
                              "Unable to commit the Set disassembly.\n\n"
                              "No changes were saved.");

        return;
    }

    QMessageBox::information(this,
                             "Disassemble Set",
                             QString("Set disassembled successfully.\n\n"
                                     "%1 loose pieces were added to "
                                     "My Loose Inventory.")
                                 .arg(totalReturned));

    accept();
}