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

#include <QDebug>
#include <QAbstractItemView>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QSqlError>
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
        QMessageBox::critical(this, "Disassemble Build", "Unable to load the selected Build.");

        return false;
    }

    if (build->status() != "Complete") {
        QMessageBox::warning(this,
                             "Disassemble Build",
                             "Only completed Builds can be disassembled.");

        return false;
    }

    if (build->inventoryMode() != "CompleteSet" && build->inventoryMode() != "Stock") {
        QMessageBox::warning(this,
                             "Disassemble Build",
                             "This Build has an unsupported Inventory Mode.");

        return false;
    }

    m_workspaceId = build->workspaceId();

    m_buildName = build->name();

    m_setNumber = build->setNumber();

    m_inventoryMode = build->inventoryMode();

    if (m_inventoryMode == "CompleteSet") {
        m_disassemblyLabel = "Complete Set";

        setWindowTitle("Disassemble Complete Set");

        m_disassembleButton->setText("Disassemble Set");
    } else {
        m_disassemblyLabel = build->buildType() == "MOC" ? "MOC Build" : "Build from Stock";

        setWindowTitle(build->buildType() == "MOC" ? "Disassemble MOC" : "Disassemble Build");

        m_disassembleButton->setText(build->buildType() == "MOC" ? "Disassemble MOC"
                                                                 : "Disassemble Build");
    }

    QString buildText;

    if (!m_setNumber.trimmed().isEmpty()) {
        buildText = QString("%1 — %2").arg(m_setNumber, m_buildName);
    } else {
        buildText = m_buildName;
    }

    m_buildLabel->setText(buildText);

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

    QSet<int> activeParentIds;

    for (const StorageLocation& location : locations) {
        if (location.parentLocationId() > 0) {
            activeParentIds.insert(location.parentLocationId());
        }
    }

    for (const StorageLocation& location : locations) {
        // Only active leaf locations are valid disassembly destinations.
        if (activeParentIds.contains(location.id())) {
            continue;
        }

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
        QMessageBox::warning(this, "Disassemble Build", "This Build does not have a parts list.");

        return false;
    }

    //
    // Column 3 represents the source quantity being
    // returned to loose inventory.
    //
    // Complete Set:
    //     use the published Set requirement quantity.
    //
    // Build from Stock / MOC:
    //     use only the quantity actually pulled into
    //     the Build.
    //
    if (m_inventoryMode == "CompleteSet") {
        m_table->setHorizontalHeaderItem(3, new QTableWidgetItem("Set Qty"));
    } else {
        m_table->setHorizontalHeaderItem(3, new QTableWidgetItem("Pulled Qty"));
    }

    PartRepository partRepository;

    ColorRepository colorRepository;

    m_table->setRowCount(0);

    m_rows.clear();

    int tableRow = 0;

    for (const BuildRequirement& requirement : requirements) {
        //
        // Determine how many pieces physically belong
        // to the source being disassembled.
        //
        const int sourceQuantity = m_inventoryMode == "CompleteSet" ? requirement.quantityRequired()
                                                                    : requirement.quantityPulled();

        //
        // A Build-from-Stock row that was never pulled
        // does not physically exist in the completed
        // Build, so there is nothing to return.
        //
        if (m_inventoryMode == "Stock" && sourceQuantity <= 0) {
            continue;
        }

        const std::optional<Part> part = partRepository.getById(requirement.partId());

        const std::optional<Color> color = colorRepository.getById(requirement.colorId());

        m_table->insertRow(tableRow);

        auto* partNumberItem = new QTableWidgetItem(part ? part->partNumber()
                                                         : QString::number(requirement.partId()));

        auto* nameItem = new QTableWidgetItem(part ? part->name() : QString());

        auto* colorItem = new QTableWidgetItem(color ? color->name()
                                                     : QString::number(requirement.colorId()));

        auto* sourceQuantityItem = new QTableWidgetItem(QString::number(sourceQuantity));

        auto* spareItem = new QTableWidgetItem(requirement.isSpare() ? "Yes" : "No");

        auto* quantitySpin = new QSpinBox(m_table);

        quantitySpin->setRange(0, sourceQuantity);

        //
        // Default assumption:
        // everything physically present in the
        // Build/Set is returned to loose inventory.
        //
        quantitySpin->setValue(sourceQuantity);

        quantitySpin->setAlignment(Qt::AlignCenter);

        auto* destinationCombo = new QComboBox(m_table);

        populateLocationCombo(destinationCombo);

        sourceQuantityItem->setTextAlignment(Qt::AlignCenter);

        spareItem->setTextAlignment(Qt::AlignCenter);

        m_table->setItem(tableRow, 0, partNumberItem);

        m_table->setItem(tableRow, 1, nameItem);

        m_table->setItem(tableRow, 2, colorItem);

        m_table->setItem(tableRow, 3, sourceQuantityItem);

        m_table->setItem(tableRow, 4, spareItem);

        m_table->setCellWidget(tableRow, 5, quantitySpin);

        m_table->setCellWidget(tableRow, 6, destinationCombo);

        RowData row;

        row.requirementId = requirement.id();

        row.partId = requirement.partId();

        row.colorId = requirement.colorId();

        row.sourceQuantity = sourceQuantity;

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

    //
    // A completed Build-from-Stock should normally
    // contain pulled pieces. If it does not, there
    // is nothing for this dialog to return.
    //
    if (m_rows.isEmpty()) {
        QMessageBox::information(this,
                                 "Disassemble Build",
                                 m_inventoryMode == "CompleteSet"
                                     ? "This Complete Set does not contain "
                                       "any requirements to disassemble."
                                     : "This completed Build does not contain "
                                       "any pulled pieces to return to loose inventory.");

        return false;
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

    const QString operationName = m_inventoryMode == "CompleteSet"
                                      ? "Complete Set"
                                      : (m_inventoryMode == "Stock"
                                                 && !m_setNumber.trimmed().isEmpty()
                                             ? "Build"
                                             : "Build");

    const QMessageBox::StandardButton response
        = QMessageBox::question(this,
                                "Disassemble Build",
                                QString("Disassemble this %1?\n\n"
                                        "%2%3\n\n"
                                        "Part/Color rows returned: %4\n"
                                        "Loose pieces added: %5\n\n"
                                        "The selected quantities will be added "
                                        "to My Loose Inventory at their chosen "
                                        "storage locations.\n\n"
                                        "Inventory movement history will be "
                                        "recorded and the Build Status will "
                                        "be changed to Disassembled.")
                                    .arg(operationName)
                                    .arg(m_buildName)
                                    .arg(m_setNumber.trimmed().isEmpty()
                                             ? QString()
                                             : QString("\nReference: %1").arg(m_setNumber))
                                    .arg(rowsReturned)
                                    .arg(totalReturned),
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::No);

    if (response != QMessageBox::Yes)
        return;

    QSqlDatabase database = DatabaseManager::instance().database();

    if (!database.transaction()) {
        qCritical() << "Unable to start Build disassembly transaction."
                    << "BuildId:" << m_buildId
                    << "DatabaseError:" << database.lastError().text();
        QMessageBox::critical(this,
                              "Disassemble Set",
                              "Unable to start the disassembly "
                              "transaction.");

        return;
    }

    InventoryRecordRepository inventoryRepository;
    BuildRequirementRepository requirementRepository;

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

        QString notes;

        if (m_inventoryMode == "CompleteSet") {
            notes = QString("Disassembled from %1 (%2), "
                            "%3 Complete Set requirement.")
                        .arg(m_buildName)
                        .arg(m_setNumber)
                        .arg(requirementType);
        } else {
            notes = QString("Returned from completed Build %1%2.")
                        .arg(m_buildName)
                        .arg(m_setNumber.trimmed().isEmpty() ? QString()
                                                             : QString(" (%1)").arg(m_setNumber));
        }

        //
        // manageTransaction=false because this entire
        // Set operation owns the outer transaction.
        //
        // addOrIncreaseQuantity() will still create
        // the InventoryMovement record.
        //
        const QString movementType = m_inventoryMode == "CompleteSet" ? "SetDisassembly"
                                                                      : "BuildDisassembly";

        if (!inventoryRepository.addOrIncreaseQuantity(record,
                                                       movementType,
                                                       "Build",
                                                       QString::number(m_buildId),
                                                       notes,
                                                       false)) {
            qCritical() << "Build disassembly failed returning inventory."
                        << "BuildId:" << m_buildId
                        << "PartId:" << row.partId
                        << "ColorId:" << row.colorId
                        << "Quantity:" << quantity
                        << "StorageLocationId:" << storageLocationId;
            database.rollback();

            QMessageBox::critical(this,
                                  "Disassemble Set",
                                  "Unable to add a Set Part to loose "
                                  "inventory.\n\n"
                                  "No changes were saved.");

            return;
        }

        if (m_inventoryMode == "Stock") {
            const std::optional<BuildRequirement> requirement = requirementRepository.getById(
                row.requirementId);

            if (!requirement) {
                database.rollback();

                QMessageBox::critical(this,
                                      "Disassemble Build",
                                      "Unable to reload the Build Requirement.\n\n"
                                      "No changes were saved.");

                return;
            }

            //
            // Reduce Pulled by the quantity actually returned
            // to loose inventory.
            //
            // Normally a complete disassembly will return the
            // entire Pulled quantity, resulting in Pulled = 0.
            //
            // If the user returns fewer pieces, the difference
            // remains associated with the Build.
            //
            const int remainingPulled = qMax(requirement->quantityPulled()
                                                 - row.quantitySpin->value(),
                                             0);

            BuildRequirement updatedRequirement = *requirement;

            updatedRequirement.setQuantityPulled(remainingPulled);

            if (!requirementRepository.update(updatedRequirement)) {
                qCritical() << "Build disassembly failed updating pulled quantity."
                            << "BuildId:" << m_buildId
                            << "RequirementId:" << row.requirementId
                            << "RemainingPulled:" << remainingPulled;
                database.rollback();

                QMessageBox::critical(this,
                                      "Disassemble Build",
                                      "Unable to update the pulled quantity.\n\n"
                                      "No changes were saved.");

                return;
            }
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
        qCritical() << "Build disassembly failed updating Build status."
                    << "BuildId:" << m_buildId;
        database.rollback();

        QMessageBox::critical(this,
                              "Disassemble Set",
                              "Unable to update the Build Status.\n\n"
                              "No changes were saved.");

        return;
    }

    if (!database.commit()) {
        qCritical() << "Unable to commit Build disassembly."
                    << "BuildId:" << m_buildId
                    << "DatabaseError:" << database.lastError().text();
        database.rollback();

        QMessageBox::critical(this,
                              "Disassemble Set",
                              "Unable to commit the Set disassembly.\n\n"
                              "No changes were saved.");

        return;
    }

    qInfo() << "Build disassembled."
            << "BuildId:" << m_buildId
            << "Name:" << m_buildName
            << "InventoryMode:" << m_inventoryMode
            << "RowsReturned:" << rowsReturned
            << "PiecesReturned:" << totalReturned;

    QMessageBox::information(this,
                             "Disassemble Build",
                             QString("%1 disassembled successfully.\n\n"
                                     "%2 loose pieces were added to "
                                     "My Loose Inventory.")
                                 .arg(m_disassemblyLabel)
                                 .arg(totalReturned));

    accept();
}