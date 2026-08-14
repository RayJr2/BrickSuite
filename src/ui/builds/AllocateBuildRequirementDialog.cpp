#include "AllocateBuildRequirementDialog.h"

#include "../../database/DatabaseManager.h"

#include "../../models/BuildAllocation.h"
#include "../../models/BuildRequirement.h"
#include "../../models/Color.h"
#include "../../models/InventoryRecord.h"
#include "../../models/Part.h"

#include "../../repositories/BuildAllocationRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/PartRepository.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

AllocateBuildRequirementDialog::AllocateBuildRequirementDialog(int workspaceId,
                                                               int buildId,
                                                               int requirementId,
                                                               QWidget* parent)
    : QDialog(parent)
    , m_workspaceId(workspaceId)
    , m_buildId(buildId)
    , m_requirementId(requirementId)
{
    setWindowTitle("Allocate Build Requirement");

    resize(850, 500);

    auto* mainLayout = new QVBoxLayout(this);

    auto* formLayout = new QFormLayout();

    m_partLabel = new QLabel(this);

    m_colorLabel = new QLabel(this);

    m_requiredLabel = new QLabel(this);

    formLayout->addRow("Part:", m_partLabel);

    formLayout->addRow("Color:", m_colorLabel);

    formLayout->addRow("Required:", m_requiredLabel);

    mainLayout->addLayout(formLayout);

    m_table = new QTableWidget(this);

    m_table->setColumnCount(6);

    m_table->setHorizontalHeaderLabels(QStringList() << "Storage Location"
                                                     << "Owned"
                                                     << "Other Builds"
                                                     << "This Build"
                                                     << "Available"
                                                     << "Allocate");

    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_table->verticalHeader()->setVisible(false);

    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    for (int column = 1; column <= 5; ++column) {
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }

    mainLayout->addWidget(m_table, 1);

    m_summaryLabel = new QLabel(this);

    m_summaryLabel->setWordWrap(true);

    mainLayout->addWidget(m_summaryLabel);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);

    m_saveButton = buttonBox->button(QDialogButtonBox::Save);

    m_cancelButton = buttonBox->button(QDialogButtonBox::Cancel);

    mainLayout->addWidget(buttonBox);

    connect(buttonBox,
            &QDialogButtonBox::accepted,
            this,
            &AllocateBuildRequirementDialog::saveAllocations);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (!loadRequirement()) {
        m_saveButton->setEnabled(false);

        return;
    }

    loadInventory();
}

bool AllocateBuildRequirementDialog::loadRequirement()
{
    BuildRequirementRepository requirementRepository;

    const QList<BuildRequirement> requirements = requirementRepository.getByBuild(m_buildId);

    std::optional<BuildRequirement> requirement;

    for (const BuildRequirement& item : requirements) {
        if (item.id() == m_requirementId) {
            requirement = item;

            break;
        }
    }

    if (!requirement) {
        QMessageBox::critical(this,
                              "Allocate Requirement",
                              "Unable to load the selected "
                              "Build Requirement.");

        return false;
    }

    if (requirement->buildId() != m_buildId) {
        QMessageBox::critical(this,
                              "Allocate Requirement",
                              "The selected requirement does not "
                              "belong to this Build.");

        return false;
    }

    m_partId = requirement->partId();

    m_colorId = requirement->colorId();

    m_quantityRequired = requirement->quantityRequired();

    m_isSpare = requirement->isSpare();

    if (m_isSpare) {
        QMessageBox::information(this,
                                 "Allocate Requirement",
                                 "Spare requirements are optional and "
                                 "are not allocated from workshop "
                                 "inventory by default.");

        return false;
    }

    PartRepository partRepository;
    ColorRepository colorRepository;

    const std::optional<Part> part = partRepository.getById(m_partId);

    const std::optional<Color> color = colorRepository.getById(m_colorId);

    if (part) {
        m_partLabel->setText(QString("%1 — %2").arg(part->partNumber(), part->name()));
    } else {
        m_partLabel->setText(QString::number(m_partId));
    }

    if (color) {
        m_colorLabel->setText(color->name());
    } else {
        m_colorLabel->setText(QString::number(m_colorId));
    }

    m_requiredLabel->setText(QString::number(m_quantityRequired));

    return true;
}

void AllocateBuildRequirementDialog::loadInventory()
{
    m_table->setRowCount(0);

    m_rows.clear();

    InventoryRecordRepository inventoryRepository;

    BuildAllocationRepository allocationRepository;

    const QList<InventoryRecord> records = inventoryRepository.getByPartColor(m_workspaceId,
                                                                              m_partId,
                                                                              m_colorId);

    int tableRow = 0;

    for (const InventoryRecord& record : records) {
        const int totalAllocated = allocationRepository.totalAllocatedForInventoryRecord(
            record.id());

        const int thisBuildAllocated
            = allocationRepository.totalAllocatedForInventoryRecordForBuild(record.id(), m_buildId);

        const int otherBuildsAllocated = qMax(totalAllocated - thisBuildAllocated, 0);

        //
        // When editing an existing allocation,
        // this Build's own allocation is available
        // to it again.
        //
        const int maximumForThisBuild = qMax(record.quantity() - otherBuildsAllocated, 0);

        const int currentlyUnallocated = qMax(record.quantity() - totalAllocated, 0);

        m_table->insertRow(tableRow);

        auto* locationItem = new QTableWidgetItem(storageLocationName(record.storageLocationId()));

        auto* ownedItem = new QTableWidgetItem(QString::number(record.quantity()));

        auto* otherBuildsItem = new QTableWidgetItem(QString::number(otherBuildsAllocated));

        auto* thisBuildItem = new QTableWidgetItem(QString::number(thisBuildAllocated));

        auto* availableItem = new QTableWidgetItem(QString::number(currentlyUnallocated));

        auto* allocationSpin = new QSpinBox(m_table);

        allocationSpin->setRange(0, maximumForThisBuild);

        allocationSpin->setValue(thisBuildAllocated);

        allocationSpin->setAlignment(Qt::AlignCenter);

        ownedItem->setTextAlignment(Qt::AlignCenter);

        otherBuildsItem->setTextAlignment(Qt::AlignCenter);

        thisBuildItem->setTextAlignment(Qt::AlignCenter);

        availableItem->setTextAlignment(Qt::AlignCenter);

        m_table->setItem(tableRow, 0, locationItem);

        m_table->setItem(tableRow, 1, ownedItem);

        m_table->setItem(tableRow, 2, otherBuildsItem);

        m_table->setItem(tableRow, 3, thisBuildItem);

        m_table->setItem(tableRow, 4, availableItem);

        m_table->setCellWidget(tableRow, 5, allocationSpin);

        AllocationRow row;

        row.inventoryRecordId = record.id();

        row.storageLocationId = record.storageLocationId();

        row.quantityOwned = record.quantity();

        row.otherBuildsAllocated = otherBuildsAllocated;

        row.currentBuildAllocated = thisBuildAllocated;

        row.allocationSpin = allocationSpin;

        m_rows.append(row);

        connect(allocationSpin, &QSpinBox::valueChanged, this, [this]() { updateSummary(); });

        ++tableRow;
    }

    if (records.isEmpty()) {
        m_summaryLabel->setText("No matching inventory records are "
                                "available for this Part and Color.");

        m_saveButton->setEnabled(false);

        return;
    }

    updateSummary();
}

void AllocateBuildRequirementDialog::updateSummary()
{
    int proposedAllocation = 0;

    for (const AllocationRow& row : m_rows) {
        proposedAllocation += row.allocationSpin->value();
    }

    const int remaining = qMax(m_quantityRequired - proposedAllocation, 0);

    const int overAllocated = qMax(proposedAllocation - m_quantityRequired, 0);

    m_summaryLabel->setText(QString("Required: %1     "
                                    "This Build Allocation: %2     "
                                    "Remaining Missing: %3")
                                .arg(m_quantityRequired)
                                .arg(proposedAllocation)
                                .arg(remaining));

    //
    // We do not permit a Build to reserve more
    // than this requirement actually needs.
    //
    m_saveButton->setEnabled(overAllocated == 0);
}

QString AllocateBuildRequirementDialog::storageLocationName(int storageLocationId) const
{
    QSqlDatabase database = DatabaseManager::instance().database();

    QSqlQuery query(database);

    query.prepare(R"(
        SELECT name
        FROM storage_location
        WHERE id = :id
    )");

    query.bindValue(":id", storageLocationId);

    if (!query.exec() || !query.next()) {
        return QString("Location %1").arg(storageLocationId);
    }

    return query.value(0).toString();
}

void AllocateBuildRequirementDialog::saveAllocations()
{
    int proposedTotal = 0;

    for (const AllocationRow& row : m_rows) {
        proposedTotal += row.allocationSpin->value();
    }

    if (proposedTotal > m_quantityRequired) {
        QMessageBox::warning(this,
                             "Allocate Requirement",
                             "The proposed allocation exceeds "
                             "the quantity required.");

        return;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    if (!database.transaction()) {
        QMessageBox::critical(this,
                              "Allocate Requirement",
                              "Unable to start the allocation "
                              "transaction.");

        return;
    }

    BuildAllocationRepository allocationRepository;

    for (const AllocationRow& row : m_rows) {
        const int proposedQuantity = row.allocationSpin->value();

        //
        // Find this Build's existing allocation
        // against this exact inventory record.
        //
        const QList<BuildAllocation> allocations = allocationRepository.getByInventoryRecord(
            row.inventoryRecordId);

        std::optional<BuildAllocation> existingAllocation;

        for (const BuildAllocation& allocation : allocations) {
            if (allocation.buildId() == m_buildId) {
                existingAllocation = allocation;

                break;
            }
        }

        //
        // Existing allocation -> zero:
        // remove the reservation.
        //
        if (existingAllocation && proposedQuantity == 0) {
            if (!allocationRepository.remove(existingAllocation->id())) {
                database.rollback();

                QMessageBox::critical(this,
                                      "Allocate Requirement",
                                      "Unable to remove an existing "
                                      "Build allocation.");

                return;
            }

            continue;
        }

        //
        // Existing allocation -> different quantity:
        // update it.
        //
        if (existingAllocation) {
            if (existingAllocation->quantityAllocated() == proposedQuantity) {
                continue;
            }

            existingAllocation->setQuantityAllocated(proposedQuantity);

            if (!allocationRepository.update(*existingAllocation)) {
                database.rollback();

                QMessageBox::critical(this,
                                      "Allocate Requirement",
                                      "Unable to update an existing "
                                      "Build allocation.");

                return;
            }

            continue;
        }

        //
        // Nothing allocated and still zero:
        // nothing to do.
        //
        if (proposedQuantity == 0)
            continue;

        //
        // Create a new allocation tied to this
        // exact inventory/storage record.
        //
        const std::optional<InventoryRecord> inventoryRecord = InventoryRecordRepository().getById(
            row.inventoryRecordId);

        if (!inventoryRecord) {
            database.rollback();

            QMessageBox::critical(this,
                                  "Allocate Requirement",
                                  "Unable to reload the inventory "
                                  "record being allocated.");

            return;
        }

        BuildAllocation allocation;

        allocation.setBuildId(m_buildId);

        allocation.setInventoryRecordId(row.inventoryRecordId);

        allocation.setPartId(m_partId);

        allocation.setColorId(m_colorId);

        allocation.setStorageLocationId(inventoryRecord->storageLocationId());

        allocation.setQuantityAllocated(proposedQuantity);

        if (!allocationRepository.create(allocation)) {
            database.rollback();

            QMessageBox::critical(this,
                                  "Allocate Requirement",
                                  "Unable to create the Build "
                                  "allocation.");

            return;
        }
    }

    if (!database.commit()) {
        database.rollback();

        QMessageBox::critical(this,
                              "Allocate Requirement",
                              "Unable to commit the Build "
                              "allocation.");

        return;
    }

    accept();
}