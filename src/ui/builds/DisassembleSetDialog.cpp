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

#include "DisassembleSetDialog.h"

#include "../../database/DatabaseManager.h"

#include "../../models/Build.h"
#include "../../models/BuildRequirement.h"
#include "../../models/Color.h"
#include "../../models/InventoryRecord.h"
#include "../../models/Part.h"
#include "../../models/StorageLocation.h"

#include "../../repositories/BuildRepository.h"
#include "../../repositories/BuildAllocationRepository.h"
#include "../../repositories/ManufacturerRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/CollectionRepository.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/StorageLocationRepository.h"
#include "../../services/storage/SessionStorageSelectionService.h"
#include "../../services/collection/CollectionItemService.h"
#include "../../services/builds/BuildLifecycleService.h"

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

DisassembleSetDialog::DisassembleSetDialog(
    int buildId, SessionStorageSelectionService& sessionStorageSelectionService,
    QWidget* parent, bool collectOnly)
    : QDialog(parent)
    , m_buildId(buildId)
    , m_sessionStorageSelectionService(sessionStorageSelectionService)
    , m_collectOnly(collectOnly)
{
    setWindowTitle("Disassemble Complete Set");

    resize(1100, 700);

    auto* mainLayout = new QVBoxLayout(this);

    m_buildLabel = new QLabel(this);

    QFont buildFont = m_buildLabel->font();

    buildFont.setBold(true);

    m_buildLabel->setFont(buildFont);

    mainLayout->addWidget(m_buildLabel);

    auto* collectionLayout = new QHBoxLayout;
    m_collectionStateLabel = new QLabel(
        "This Build is represented in My Collection. Resulting Collection State:", this);
    m_collectionStateCombo = new QComboBox(this);
    for (const auto state : {CollectionItemState::Assembled,
                             CollectionItemState::Unassembled,
                             CollectionItemState::PartiallyAssembled,
                             CollectionItemState::Sealed})
        m_collectionStateCombo->addItem(collectionItemStateToString(state),
                                        static_cast<int>(state));
    m_collectionStateCombo->setCurrentIndex(m_collectionStateCombo->findData(
        static_cast<int>(CollectionItemState::Unassembled)));
    m_collectionStateLabel->hide();
    m_collectionStateCombo->hide();
    collectionLayout->addWidget(m_collectionStateLabel);
    collectionLayout->addWidget(m_collectionStateCombo);
    collectionLayout->addStretch();
    mainLayout->addLayout(collectionLayout);

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

    m_table->setColumnCount(8);

    m_table->setHorizontalHeaderLabels(QStringList() << "Part #"
                                                     << "Name"
                                                     << "Color"
                                                     << "Manufacturer"
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

    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Stretch);

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

    if (m_buildId <= 0)
        return;

    if (!loadBuild())
        return;

    if (m_collectOnly) {
        setWindowTitle("Return Pulled Pieces");
        m_disassembleButton->setText("Continue");
    }

    if (!loadStorageLocations())
        return;

    if (!loadRequirements())
        return;

    updateSummary();
}

DisassembleSetDialog::DisassembleSetDialog(
    int workspaceId, const QString& buildName, const QString& reference,
    const QString& inventoryMode,
    const QList<RemoteReadDto::BuildCancellationReturnRow>& rows,
    const QList<RemoteReadDto::StorageSummary>& storage,
    SessionStorageSelectionService& sessionStorageSelectionService, QWidget* parent)
    : DisassembleSetDialog(0, sessionStorageSelectionService, parent, true)
{
    m_workspaceId=workspaceId;m_buildName=buildName;m_setNumber=reference;
    m_inventoryMode=inventoryMode;m_disassemblyLabel=inventoryMode==QStringLiteral("CompleteSet")?QStringLiteral("Complete Set"):QStringLiteral("Build");
    setWindowTitle(inventoryMode==QStringLiteral("CompleteSet")?QStringLiteral("Disassemble Complete Set"):QStringLiteral("Return Pulled Pieces"));
    m_disassembleButton->setText(QStringLiteral("Continue"));
    m_buildLabel->setText(reference.isEmpty()?buildName:QString("%1 — %2").arg(reference,buildName));
    loadRemoteRows(rows,storage);updateSummary();
}

void DisassembleSetDialog::loadRemoteRows(
    const QList<RemoteReadDto::BuildCancellationReturnRow>& rows,
    const QList<RemoteReadDto::StorageSummary>& storage)
{
    m_locations.clear();QSet<qint64> parents;
    for(const auto&location:storage)if(location.active&&location.parentStorageId>0)parents.insert(location.parentStorageId);
    for(const auto&location:storage)if(location.active&&location.allowsInventory&&!parents.contains(location.storageId))
        m_locations.append({int(location.storageId),location.displayPath});
    std::sort(m_locations.begin(),m_locations.end(),[](const auto&a,const auto&b){return a.path.compare(b.path,Qt::CaseInsensitive)<0;});
    populateLocationCombo(m_defaultDestinationCombo);
    const int remembered=m_sessionStorageSelectionService.rememberedDestination(m_workspaceId);
    const int defaultIndex=m_defaultDestinationCombo->findData(remembered);
    if(defaultIndex>=0)m_defaultDestinationCombo->setCurrentIndex(defaultIndex);
    m_table->setHorizontalHeaderItem(4,new QTableWidgetItem(m_inventoryMode==QStringLiteral("CompleteSet")?QStringLiteral("Set Qty"):QStringLiteral("Pulled Qty")));
    m_table->setRowCount(0);m_rows.clear();int tableRow=0;
    for(const auto&source:rows){
        if(source.quantityPulled<=0)continue;m_table->insertRow(tableRow);
        m_table->setItem(tableRow,0,new QTableWidgetItem(source.partNumber));
        m_table->setItem(tableRow,1,new QTableWidgetItem(source.partNameFallback));
        m_table->setItem(tableRow,2,new QTableWidgetItem(source.colorNameFallback));
        m_table->setItem(tableRow,3,new QTableWidgetItem(source.manufacturerDisplay));
        m_table->setItem(tableRow,4,new QTableWidgetItem(QString::number(source.quantityPulled)));
        m_table->setItem(tableRow,5,new QTableWidgetItem(source.spare?QStringLiteral("Yes"):QStringLiteral("No")));
        auto*quantity=new QSpinBox(m_table);quantity->setRange(source.quantityPulled,source.quantityPulled);quantity->setValue(source.quantityPulled);
        auto*destination=new QComboBox(m_table);populateLocationCombo(destination);
        m_table->setCellWidget(tableRow,6,quantity);m_table->setCellWidget(tableRow,7,destination);
        RowData row;row.requirementId=int(source.requirementId);row.manufacturerName=source.manufacturerDisplay;
        row.sourceQuantity=source.quantityPulled;row.isSpare=source.spare;row.quantitySpin=quantity;row.destinationCombo=destination;m_rows.append(row);
        connect(destination,&QComboBox::currentIndexChanged,this,[this]{updateSummary();});++tableRow;
    }
    if(m_rows.isEmpty())m_statusLabel->setText(QStringLiteral("No pulled pieces require a return plan."));
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

    m_buildManufacturerId = build->manufacturerId();

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

    std::optional<CollectionItem> collection;
    if (!CollectionRepository().tryGetBySourceBuild(m_buildId, collection)) {
        QMessageBox::critical(this, "Disassemble Build",
                              "Unable to check for a linked Collection item.");
        return false;
    }
    if (collection) {
        m_linkedCollectionItemId = collection->id;
        m_collectionStateLabel->show();
        m_collectionStateCombo->show();
    }

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

    const int remembered = m_sessionStorageSelectionService.rememberedDestination(m_workspaceId);
    const int rememberedIndex = m_defaultDestinationCombo->findData(remembered);
    if (rememberedIndex >= 0)
        m_defaultDestinationCombo->setCurrentIndex(rememberedIndex);

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
    BuildAllocationRepository allocationRepository;
    ManufacturerRepository manufacturerRepository;

    const QList<BuildRequirement> requirements =
        requirementRepository.getByBuild(m_buildId);

    if (requirements.isEmpty()) {
        QMessageBox::warning(this,
                             "Disassemble Build",
                             "This Build does not have a parts list.");
        return false;
    }

    if (m_inventoryMode == "CompleteSet") {
        m_table->setHorizontalHeaderItem(4,
                                         new QTableWidgetItem("Set Qty"));
    } else {
        m_table->setHorizontalHeaderItem(4,
                                         new QTableWidgetItem("Pulled Qty"));
    }

    PartRepository partRepository;
    ColorRepository colorRepository;

    m_table->setRowCount(0);
    m_rows.clear();

    int tableRow = 0;

    for (const BuildRequirement& requirement : requirements) {
        int pulledOrSetQuantity = 0;

        if (m_inventoryMode == "CompleteSet") {
            //
            // Regular Complete Set requirements represent assembled pieces and
            // return in full during disassembly.
            //
            // Spare requirements are different: M18.3 may already have moved
            // some/all boxed spares into My Loose Inventory. Only the
            // unreleased remainder still belongs to the Complete Set and is
            // eligible to be returned here.
            //
            pulledOrSetQuantity =
                requirement.isSpare()
                    ? qMax(requirement.quantityRequired()
                               - requirement.quantityReleased(),
                           0)
                    : requirement.quantityRequired();
        } else {
            pulledOrSetQuantity = requirement.quantityPulled();
        }

        if (pulledOrSetQuantity <= 0)
            continue;

        const int returnedPartId=m_inventoryMode==QStringLiteral("Stock")
            ?requirement.effectivePartId():requirement.partId();
        const int returnedColorId=m_inventoryMode==QStringLiteral("Stock")
            ?requirement.effectiveColorId():requirement.colorId();
        const std::optional<Part> part =
            partRepository.getById(returnedPartId);

        const std::optional<Color> color =
            colorRepository.getById(returnedColorId);

        struct ManufacturerSlice
        {
            int manufacturerId = 0;
            int quantity = 0;
        };

        QList<ManufacturerSlice> slices;

        if (m_inventoryMode == "CompleteSet") {
            ManufacturerSlice slice;
            slice.manufacturerId = m_buildManufacturerId;
            slice.quantity = pulledOrSetQuantity;
            slices.append(slice);
        } else {
            const QList<BuildPartManufacturerProvenance> provenance =
                allocationRepository.pulledManufacturerProvenance(
                    m_buildId,
                    returnedPartId,
                    returnedColorId);

            int provenanceQuantity = 0;

            for (const BuildPartManufacturerProvenance& item : provenance) {
                ManufacturerSlice slice;
                slice.manufacturerId = item.manufacturerId;
                slice.quantity = item.quantityPulled;
                slices.append(slice);

                provenanceQuantity += item.quantityPulled;
            }

            //
            // Never guess the manufacturer of stock-built pieces. Older
            // pulled Builds may predate the provenance table; those Builds
            // require an explicit recovery/migration decision rather than
            // silently returning everything as LEGO.
            //
            if (provenanceQuantity != pulledOrSetQuantity) {
                QMessageBox::critical(
                    this,
                    "Disassemble Build",
                    QString("Manufacturer provenance is incomplete for "
                            "Part %1 / Color %2.\n\n"
                            "Pulled quantity: %3\n"
                            "Manufacturer-tracked quantity: %4\n\n"
                            "BrickSuite will not guess the manufacturer. "
                            "No inventory changes have been made.")
                        .arg(part ? part->partNumber()
                                  : QString::number(returnedPartId))
                        .arg(color ? color->name()
                                   : QString::number(returnedColorId))
                        .arg(pulledOrSetQuantity)
                        .arg(provenanceQuantity));

                m_table->setRowCount(0);
                m_rows.clear();
                return false;
            }
        }

        for (const ManufacturerSlice& slice : slices) {
            if (slice.quantity <= 0)
                continue;

            const std::optional<Manufacturer> manufacturer =
                manufacturerRepository.getById(slice.manufacturerId);

            if (!manufacturer) {
                QMessageBox::critical(
                    this,
                    "Disassemble Build",
                    QString("Unable to resolve Manufacturer ID %1 for "
                            "Part %2.\n\nNo inventory changes have been made.")
                        .arg(slice.manufacturerId)
                        .arg(part ? part->partNumber()
                                  : QString::number(returnedPartId)));

                m_table->setRowCount(0);
                m_rows.clear();
                return false;
            }

            m_table->insertRow(tableRow);

            auto* partNumberItem =
                new QTableWidgetItem(part ? part->partNumber()
                                          : QString::number(returnedPartId));

            auto* nameItem =
                new QTableWidgetItem(part ? part->name() : QString());

            auto* colorItem =
                new QTableWidgetItem(color ? color->name()
                                           : QString::number(returnedColorId));

            auto* manufacturerItem =
                new QTableWidgetItem(manufacturer->name());

            auto* sourceQuantityItem =
                new QTableWidgetItem(QString::number(slice.quantity));

            auto* spareItem =
                new QTableWidgetItem(requirement.isSpare() ? "Yes" : "No");

            auto* quantitySpin = new QSpinBox(m_table);
            quantitySpin->setRange(0, slice.quantity);
            quantitySpin->setValue(slice.quantity);
            quantitySpin->setAlignment(Qt::AlignCenter);

            auto* destinationCombo = new QComboBox(m_table);
            populateLocationCombo(destinationCombo);

            sourceQuantityItem->setTextAlignment(Qt::AlignCenter);
            spareItem->setTextAlignment(Qt::AlignCenter);

            m_table->setItem(tableRow, 0, partNumberItem);
            m_table->setItem(tableRow, 1, nameItem);
            m_table->setItem(tableRow, 2, colorItem);
            m_table->setItem(tableRow, 3, manufacturerItem);
            m_table->setItem(tableRow, 4, sourceQuantityItem);
            m_table->setItem(tableRow, 5, spareItem);
            m_table->setCellWidget(tableRow, 6, quantitySpin);
            m_table->setCellWidget(tableRow, 7, destinationCombo);

            RowData row;
            row.requirementId = requirement.id();
            row.partId = returnedPartId;
            row.colorId = returnedColorId;
            row.manufacturerId = slice.manufacturerId;
            row.sourceQuantity = slice.quantity;
            row.isSpare = requirement.isSpare();
            row.quantitySpin = quantitySpin;
            row.destinationCombo = destinationCombo;

            m_rows.append(row);

            connect(quantitySpin,
                    &QSpinBox::valueChanged,
                    this,
                    [this]() { updateSummary(); });

            connect(destinationCombo,
                    &QComboBox::currentIndexChanged,
                    this,
                    [this]() { updateSummary(); });

            ++tableRow;
        }
    }

    if (m_rows.isEmpty()) {
        QMessageBox::information(
            this,
            "Disassemble Build",
            m_inventoryMode == "CompleteSet"
                ? "This Complete Set does not contain any requirements to disassemble."
                : "This completed Build does not contain any pulled pieces "
                  "to return to loose inventory.");

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
    int commonDestinationId = 0;

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

        if (commonDestinationId == 0)
            commonDestinationId = storageLocationId;
        else if (commonDestinationId != storageLocationId)
            commonDestinationId = -1;

        ++rowsReturned;
    }

    const QString operationName = m_inventoryMode == "CompleteSet"
                                      ? "Complete Set"
                                      : (m_inventoryMode == "Stock"
                                                 && !m_setNumber.trimmed().isEmpty()
                                             ? "Build"
                                             : "Build");

    const QMessageBox::StandardButton response = m_collectOnly
        ? QMessageBox::Yes
        : QMessageBox::question(this,
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
                                        "be changed to Disassembled.%6")
                                    .arg(operationName)
                                    .arg(m_buildName)
                                    .arg(m_setNumber.trimmed().isEmpty()
                                             ? QString()
                                             : QString("\nReference: %1").arg(m_setNumber))
                                    .arg(rowsReturned)
                                    .arg(totalReturned)
                                    .arg(m_linkedCollectionItemId > 0
                                        ? QString("\n\nThe linked My Collection item will change to %1. "
                                                  "Its Condition, Completeness, and Collection "
                                                  "Location will not change.")
                                              .arg(m_collectionStateCombo->currentText())
                                        : QString()),
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::No);

    if (response != QMessageBox::Yes)
        return;

    QList<BuildLifecycleService::DisassemblyReturn> returns;
    m_returnSelections.clear();
    for (const RowData& row : m_rows) {
        const int quantity = row.quantitySpin->value();
        if (quantity <= 0)
            continue;
        BuildLifecycleService::DisassemblyReturn value;
        value.requirementId = row.requirementId;
        value.partId = row.partId;
        value.colorId = row.colorId;
        value.manufacturerId = row.manufacturerId;
        value.storageLocationId = row.destinationCombo->currentData().toInt();
        value.quantity = quantity;
        value.spare = row.isSpare;
        returns.append(value);
        m_returnSelections.append({value.requirementId,
                                   value.partId,
                                   value.colorId,
                                   value.manufacturerId,
                                   row.manufacturerName,
                                   value.storageLocationId,
                                   value.quantity,
                                   value.spare});
    }
    const auto collectionState = m_linkedCollectionItemId > 0
        ? static_cast<CollectionItemState>(m_collectionStateCombo->currentData().toInt())
        : CollectionItemState::Unassembled;
    if (!m_collectOnly) {
        const auto result = BuildLifecycleService().disassemble(
            m_buildId, returns, collectionState);
        if (!result.success) {
            QMessageBox::critical(this, "Disassemble Build",
                                  result.message + "\n\nNo changes were saved.");
            return;
        }
    }

    if (commonDestinationId > 0) {
        m_sessionStorageSelectionService.rememberDestination(
            m_workspaceId, commonDestinationId);
    }

    if (!m_collectOnly) {
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
    }

    accept();
}

QList<DisassembleSetDialog::ReturnSelection> DisassembleSetDialog::returnSelections() const
{
    return m_returnSelections;
}

int DisassembleSetDialog::linkedCollectionState() const
{
    return m_linkedCollectionItemId > 0 ? m_collectionStateCombo->currentData().toInt() : 0;
}
