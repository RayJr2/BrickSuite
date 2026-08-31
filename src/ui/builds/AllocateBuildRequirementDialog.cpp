/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

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
    m_pulledLabel = new QLabel(this);
    m_remainingLabel = new QLabel(this);

    formLayout->addRow("Part:", m_partLabel);
    formLayout->addRow("Color:", m_colorLabel);
    formLayout->addRow("Required:", m_requiredLabel);
    formLayout->addRow("Pulled:", m_pulledLabel);
    formLayout->addRow("Remaining:", m_remainingLabel);

    mainLayout->addLayout(formLayout);

    m_table = new QTableWidget(this);

    m_table->setColumnCount(6);

    m_table->setHorizontalHeaderLabels(QStringList() << "Storage Location"
                                                     << "Owned"
                                                     << "Other Alloc."
                                                     << "This Requirement"
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

    //
    // Allocation works against the effective fulfillment
    // identity. If no substitute is selected these resolve
    // to the original requirement Part / Color.
    //
    m_partId = requirement->effectivePartId();
    m_colorId = requirement->effectiveColorId();

    m_quantityRequired = requirement->quantityRequired();
    m_quantityPulled = requirement->quantityPulled();
    m_quantityRemaining = qMax(m_quantityRequired - m_quantityPulled, 0);
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
    m_pulledLabel->setText(QString::number(m_quantityPulled));
    m_remainingLabel->setText(QString::number(m_quantityRemaining));

    if (m_quantityRemaining <= 0) {
        QMessageBox::information(this,
                                 "Allocate Requirement",
                                 "This Build Requirement has already "
                                 "been completely pulled.");
        return false;
    }

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
        const int totalAllocated =
            allocationRepository.totalAllocatedForInventoryRecord(record.id());

        //
        // The current requirement gets only its own reservation
        // back while editing. Allocations for other requirements
        // in this same Build remain unavailable.
        //
        int currentRequirementAllocated = 0;

        const QList<BuildAllocation> recordAllocations =
            allocationRepository.getByInventoryRecord(record.id());

        for (const BuildAllocation& allocation : recordAllocations) {
            if (allocation.buildRequirementId() == m_requirementId) {
                currentRequirementAllocated += allocation.quantityAllocated();
            }
        }

        const int otherAllocated = qMax(totalAllocated - currentRequirementAllocated, 0);

        const int maximumForThisRequirement = qMax(record.quantity() - otherAllocated, 0);

        const int currentlyUnallocated = qMax(record.quantity() - totalAllocated, 0);

        m_table->insertRow(tableRow);

        auto* locationItem = new QTableWidgetItem(storageLocationName(record.storageLocationId()));
        auto* ownedItem = new QTableWidgetItem(QString::number(record.quantity()));
        auto* otherBuildsItem = new QTableWidgetItem(QString::number(otherAllocated));
        auto* thisBuildItem = new QTableWidgetItem(QString::number(currentRequirementAllocated));
        auto* availableItem = new QTableWidgetItem(QString::number(currentlyUnallocated));

        auto* allocationSpin = new QSpinBox(m_table);
        allocationSpin->setRange(0, maximumForThisRequirement);
        allocationSpin->setValue(currentRequirementAllocated);
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
        row.otherBuildsAllocated = otherAllocated;
        row.currentBuildAllocated = currentRequirementAllocated;
        row.allocationSpin = allocationSpin;

        m_rows.append(row);

        connect(allocationSpin, &QSpinBox::valueChanged, this, [this]() { updateSummary(); });

        ++tableRow;
    }

    if (records.isEmpty()) {
        m_summaryLabel->setText("No matching inventory records are "
                                "available for this requirement's effective Part and Color.");
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

    const int stillNeeded = qMax(m_quantityRemaining - proposedAllocation, 0);
    const bool overAllocated = proposedAllocation > m_quantityRemaining;

    m_summaryLabel->setText(QString("Required: %1     "
                                    "Pulled: %2     "
                                    "Remaining: %3     "
                                    "This Requirement Allocation: %4     "
                                    "Still Needed: %5")
                                .arg(m_quantityRequired)
                                .arg(m_quantityPulled)
                                .arg(m_quantityRemaining)
                                .arg(proposedAllocation)
                                .arg(stillNeeded));

    m_saveButton->setEnabled(!overAllocated);
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

    if (proposedTotal > m_quantityRemaining) {
        QMessageBox::warning(this,
                             "Allocate Requirement",
                             "The proposed allocation exceeds "
                             "the remaining quantity required.");
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

        const QList<BuildAllocation> allocations =
            allocationRepository.getByInventoryRecord(row.inventoryRecordId);

        std::optional<BuildAllocation> existingAllocation;

        for (const BuildAllocation& allocation : allocations) {
            if (allocation.buildId() == m_buildId
                && allocation.buildRequirementId() == m_requirementId) {
                existingAllocation = allocation;
                break;
            }
        }

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

        if (proposedQuantity == 0)
            continue;

        const std::optional<InventoryRecord> inventoryRecord =
            InventoryRecordRepository().getById(row.inventoryRecordId);

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
        allocation.setBuildRequirementId(m_requirementId);
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
