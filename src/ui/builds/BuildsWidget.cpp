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

#include "BuildsWidget.h"

#include "../../settings/UserSettings.h"

#include "../../app/WorkspaceContext.h"
#include "../../database/DatabaseManager.h"
#include "AllocateBuildRequirementDialog.h"
#include "DisassembleSetDialog.h"
#include "EditBuildDialog.h"
#include "EditBuildRequirementDialog.h"
#include "ImportPullListDialog.h"
#include "InteractiveBuildPullingDialog.h"
#include "SetImportPreviewDialog.h"

#include "../../models/Build.h"
#include "../../models/BuildRequirement.h"
#include "../../models/Color.h"
#include "../../models/InventoryRecord.h"
#include "../../models/Part.h"
#include "../../models/SetCatalogItem.h"
#include "../../models/StorageLocation.h"

#include "../../repositories/BuildAllocationRepository.h"
#include "../../repositories/BuildRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/ManufacturerRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/SetCatalogRepository.h"
#include "../../repositories/StorageLocationRepository.h"

#include "../../import/RebrickableMocCsvImporter.h"
#include "../../services/builds/MissingPartsService.h"
#include "../../services/procurement/ProcurementDraftService.h"
#include "../../ui/procurement/ProcurementPreviewDialog.h"

#include "../../ui/helpers/ColorComboHelper.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QShowEvent>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>

namespace {

struct MocFileMetadata
{
    bool recognized = false;
    QString mocNumber;
    QString sourceSetNumber;
};

MocFileMetadata parseRebrickableMocFileName(const QString& fileName)
{
    MocFileMetadata metadata;

    const QFileInfo fileInfo(fileName);
    const QString baseName = fileInfo.completeBaseName();

    const QRegularExpression expression(R"(^rebrickable_parts_(moc-\d+)-(\d+)(?:-|$))",
                                        QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch match = expression.match(baseName);

    if (!match.hasMatch())
        return metadata;

    metadata.mocNumber = match.captured(1).toUpper();

    const QString sourceSetBase = match.captured(2);

    if (!sourceSetBase.isEmpty())
        metadata.sourceSetNumber = sourceSetBase + "-1";

    metadata.recognized = true;

    return metadata;
}

} // namespace

BuildsWidget::BuildsWidget(WorkspaceContext& workspaceContext, QWidget* parent)
    : QWidget(parent)
    , m_workspaceContext(workspaceContext)
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* titleLabel = new QLabel("Builds", this);
    mainLayout->addWidget(titleLabel);

    m_newBuildGroup = new QGroupBox("New Build", this);
    m_newBuildGroup->setCheckable(true);
    m_newBuildGroup->setChecked(true);

    auto* newBuildGroupLayout = new QVBoxLayout(m_newBuildGroup);

    m_newBuildContent = new QWidget(m_newBuildGroup);

    auto* formLayout = new QFormLayout(m_newBuildContent);

    m_typeCombo = new QComboBox(m_newBuildContent);
    m_typeCombo->addItem("Set", "Set");
    m_typeCombo->addItem("MOC", "MOC");

    m_setNumberEdit = new QLineEdit(m_newBuildContent);
    m_setNumberEdit->setPlaceholderText("Example: 1234-1");

    m_inventoryModeCombo = new QComboBox(m_newBuildContent);
    m_inventoryModeCombo->addItem("Build from Stock", "Stock");
    m_inventoryModeCombo->addItem("Complete Set", "CompleteSet");

    m_manufacturerCombo = new QComboBox(m_newBuildContent);

    m_nameEdit = new QLineEdit(m_newBuildContent);

    m_statusCombo = new QComboBox(m_newBuildContent);
    m_statusCombo->addItem("Planned", "Planned");
    m_statusCombo->addItem("Pulling", "Pulling");
    m_statusCombo->addItem("Complete", "Complete");

    m_notesEdit = new QTextEdit(m_newBuildContent);
    m_notesEdit->setMaximumHeight(70);

    m_addButton = new QPushButton("Add Build", m_newBuildContent);

    formLayout->addRow("Type:", m_typeCombo);

    auto* numberLabel = new QLabel("Set Number:", m_newBuildContent);
    formLayout->addRow(numberLabel, m_setNumberEdit);

    formLayout->addRow("Inventory Mode:", m_inventoryModeCombo);
    formLayout->addRow("Manufacturer:", m_manufacturerCombo);
    formLayout->addRow("Name:", m_nameEdit);
    formLayout->addRow("Status:", m_statusCombo);
    formLayout->addRow("Notes:", m_notesEdit);
    formLayout->addRow(QString(), m_addButton);

    newBuildGroupLayout->addWidget(m_newBuildContent);
    mainLayout->addWidget(m_newBuildGroup);

    connect(m_newBuildGroup, &QGroupBox::toggled, m_newBuildContent, &QWidget::setVisible);

    auto* existingGroup = new QGroupBox("Builds", this);
    auto* existingLayout = new QVBoxLayout(existingGroup);
    auto* existingHeaderLayout = new QHBoxLayout();

    existingHeaderLayout->addStretch(1);

    m_showArchivedBuildsCheck = new QCheckBox("Show Archived", existingGroup);
    m_showArchivedBuildsCheck->setChecked(UserSettings::instance().showArchivedBuilds());

    existingHeaderLayout->addWidget(m_showArchivedBuildsCheck);
    existingLayout->addLayout(existingHeaderLayout);

    m_buildsTable = new QTableWidget(existingGroup);
    m_buildsTable->setColumnCount(8);

    m_buildsTable->setHorizontalHeaderLabels(QStringList() << "Type"
                                                           << "Reference"
                                                           << "Inventory Mode"
                                                           << "Manufacturer"
                                                           << "Name"
                                                           << "Status"
                                                           << "Notes"
                                                           << "Action");

    m_buildsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_buildsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_buildsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_buildsTable->verticalHeader()->setVisible(false);

    m_buildsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_buildsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_buildsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_buildsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_buildsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_buildsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_buildsTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    m_buildsTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);

    existingLayout->addWidget(m_buildsTable);

    auto* requirementsGroup = new QGroupBox("Build Requirements", this);
    auto* requirementsLayout = new QVBoxLayout(requirementsGroup);
    auto* requirementsHeaderLayout = new QHBoxLayout();

    m_requirementsLabel = new QLabel("Select a build to view its requirements.", requirementsGroup);
    m_loadSetFromRebrickableButton = new QPushButton("Load Set from Rebrickable", requirementsGroup);
    m_importMocPartsButton = new QPushButton("Import MOC Parts CSV", requirementsGroup);
    m_allocateAvailableButton = new QPushButton("Allocate Available", requirementsGroup);
    m_exportMissingPartsButton = new QPushButton("Export Missing Parts CSV", requirementsGroup);
    m_procureMissingPartsButton = new QPushButton("Procure Missing Parts...", requirementsGroup);
    m_exportPullListButton = new QPushButton("Export Pull List CSV", requirementsGroup);
    m_importPullListButton = new QPushButton("Import Pull List CSV", requirementsGroup);
    m_interactivePullButton = new QPushButton("Pull Build...", requirementsGroup);

    requirementsHeaderLayout->addWidget(m_requirementsLabel, 1);
    requirementsHeaderLayout->addWidget(m_loadSetFromRebrickableButton);
    requirementsHeaderLayout->addWidget(m_importMocPartsButton);
    requirementsHeaderLayout->addWidget(m_allocateAvailableButton);
    requirementsHeaderLayout->addWidget(m_exportMissingPartsButton);
    requirementsHeaderLayout->addWidget(m_procureMissingPartsButton);
    requirementsHeaderLayout->addWidget(m_exportPullListButton);
    requirementsHeaderLayout->addWidget(m_importPullListButton);
    requirementsHeaderLayout->addWidget(m_interactivePullButton);

    requirementsLayout->addLayout(requirementsHeaderLayout);

    auto* requirementEntryLayout = new QHBoxLayout();

    m_partNumberEdit = new QLineEdit(requirementsGroup);
    m_partNumberEdit->setPlaceholderText("Example: 3001");

    m_colorCombo = new QComboBox(requirementsGroup);

    m_quantitySpin = new QSpinBox(requirementsGroup);
    m_quantitySpin->setRange(1, 99999);
    m_quantitySpin->setValue(1);

    m_spareCheck = new QCheckBox("Spare", requirementsGroup);
    m_addRequirementButton = new QPushButton("Add Requirement", requirementsGroup);

    requirementEntryLayout->addWidget(new QLabel("Part #:", requirementsGroup));
    requirementEntryLayout->addWidget(m_partNumberEdit, 2);
    requirementEntryLayout->addWidget(new QLabel("Color:", requirementsGroup));
    requirementEntryLayout->addWidget(m_colorCombo, 2);
    requirementEntryLayout->addWidget(new QLabel("Qty:", requirementsGroup));
    requirementEntryLayout->addWidget(m_quantitySpin);
    requirementEntryLayout->addWidget(m_spareCheck);
    requirementEntryLayout->addWidget(m_addRequirementButton);

    requirementsLayout->addLayout(requirementEntryLayout);

    m_requirementsTable = new QTableWidget(requirementsGroup);
    m_requirementsTable->setColumnCount(13);

    m_requirementsTable->setHorizontalHeaderLabels(QStringList() << "Part #"
                                                                 << "Name"
                                                                 << "Color"
                                                                 << "Required"
                                                                 << "Pulled"
                                                                 << "Remaining"
                                                                 << "Owned"
                                                                 << "This Req."
                                                                 << "Other Alloc."
                                                                 << "Available"
                                                                 << "Missing"
                                                                 << "Spare"
                                                                 << "Action");

    m_requirementsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_requirementsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_requirementsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_requirementsTable->verticalHeader()->setVisible(false);

    m_requirementsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_requirementsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_requirementsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    for (int column = 3; column < 13; ++column) {
        m_requirementsTable->horizontalHeader()->setSectionResizeMode(column,
                                                                      QHeaderView::ResizeToContents);
    }

    requirementsLayout->addWidget(m_requirementsTable, 1);

    m_buildsRequirementsSplitter = new QSplitter(Qt::Vertical, this);
    m_buildsRequirementsSplitter->addWidget(existingGroup);
    m_buildsRequirementsSplitter->addWidget(requirementsGroup);
    m_buildsRequirementsSplitter->setCollapsible(0, false);
    m_buildsRequirementsSplitter->setCollapsible(1, false);
    m_buildsRequirementsSplitter->setStretchFactor(0, 1);
    m_buildsRequirementsSplitter->setStretchFactor(1, 3);
    m_buildsRequirementsSplitter->setSizes(QList<int>() << 200 << 600);

    mainLayout->addWidget(m_buildsRequirementsSplitter, 1);

    QSettings settings;

    const QByteArray splitterState =
        settings.value("Builds/buildsRequirementsSplitterState").toByteArray();

    if (!splitterState.isEmpty())
        m_buildsRequirementsSplitter->restoreState(splitterState);

    connect(m_buildsRequirementsSplitter, &QSplitter::splitterMoved, this, [this]() {
        QSettings settings;
        settings.setValue("Builds/buildsRequirementsSplitterState",
                          m_buildsRequirementsSplitter->saveState());
    });

    m_statusLabel = new QLabel(this);
    mainLayout->addWidget(m_statusLabel);

    connect(m_addButton, &QPushButton::clicked, this, &BuildsWidget::addBuild);
    connect(m_allocateAvailableButton,
            &QPushButton::clicked,
            this,
            &BuildsWidget::allocateAvailable);
    connect(m_exportMissingPartsButton,
            &QPushButton::clicked,
            this,
            &BuildsWidget::exportMissingParts);
    connect(m_procureMissingPartsButton,
            &QPushButton::clicked,
            this,
            &BuildsWidget::procureMissingParts);
    connect(m_exportPullListButton, &QPushButton::clicked, this, &BuildsWidget::exportPullList);
    connect(m_importPullListButton, &QPushButton::clicked, this, &BuildsWidget::importPullList);
    connect(m_interactivePullButton, &QPushButton::clicked, this, &BuildsWidget::interactivePulling);

    connect(&m_workspaceContext,
            &WorkspaceContext::currentWorkspaceChanged,
            this,
            &BuildsWidget::workspaceChanged);

    connect(m_importMocPartsButton, &QPushButton::clicked, this, &BuildsWidget::importMocPartsCsv);

    connect(m_typeCombo, &QComboBox::currentIndexChanged, this, [this, numberLabel]() {
        const QString buildType = m_typeCombo->currentData().toString();

        const bool isSet = buildType == "Set";
        const bool isMoc = buildType == "MOC";

        m_setNumberEdit->setEnabled(m_workspaceContext.hasCurrentWorkspace() && (isSet || isMoc));

        if (isSet) {
            numberLabel->setText("Set Number:");
            m_setNumberEdit->setPlaceholderText("Example: 77239-1");
        } else if (isMoc) {
            numberLabel->setText("MOC Number:");
            m_setNumberEdit->setPlaceholderText("Example: MOC-227137");

            const int stockIndex = m_inventoryModeCombo->findData("Stock");

            if (stockIndex >= 0)
                m_inventoryModeCombo->setCurrentIndex(stockIndex);
        }
    });

    connect(m_inventoryModeCombo,
            &QComboBox::currentIndexChanged,
            this,
            [this]() {
                const bool completeSet =
                    m_inventoryModeCombo->currentData().toString() == "CompleteSet";

                ManufacturerRepository repository;

                if (!completeSet) {
                    const int legoIndex =
                        m_manufacturerCombo->findData(repository.legoManufacturerId());

                    if (legoIndex >= 0)
                        m_manufacturerCombo->setCurrentIndex(legoIndex);
                }

                m_manufacturerCombo->setEnabled(
                    m_workspaceContext.hasCurrentWorkspace() && completeSet);
            });

    connect(m_buildsTable,
            &QTableWidget::itemSelectionChanged,
            this,
            &BuildsWidget::buildSelectionChanged);

    connect(m_showArchivedBuildsCheck, &QCheckBox::toggled, this, [this](bool checked) {
        UserSettings::instance().setShowArchivedBuilds(checked);

        m_selectedBuildId = 0;
        loadBuilds();
        loadRequirements();
        updateRequirementUiState();
    });

    connect(m_addRequirementButton, &QPushButton::clicked, this, &BuildsWidget::addRequirement);
    connect(m_partNumberEdit, &QLineEdit::returnPressed, this, &BuildsWidget::addRequirement);

    connect(m_loadSetFromRebrickableButton, &QPushButton::clicked, this, [this]() {
        if (m_selectedBuildId <= 0) {
            QMessageBox::warning(this, "Set Import Preview", "Select a Set build first.");
            return;
        }

        BuildRepository repository;
        const std::optional<Build> build = repository.getById(m_selectedBuildId);

        if (!build) {
            QMessageBox::warning(this, "Set Import Preview", "Unable to load the selected Build.");
            return;
        }

        if (build->buildType() != "Set") {
            QMessageBox::information(this,
                                     "Set Import Preview",
                                     "Rebrickable Set import is only available "
                                     "for Set builds.");
            return;
        }

        const QString setNumber = build->setNumber().trimmed();

        if (setNumber.isEmpty()) {
            QMessageBox::warning(this,
                                 "Set Import Preview",
                                 "The selected Set build does not have "
                                 "a Set Number.");
            return;
        }

        SetImportPreviewDialog dialog(build->id(), setNumber, this);

        if (dialog.exec() == QDialog::Accepted)
            loadRequirements();
    });

    loadColors();
    loadManufacturers();

    workspaceChanged(m_workspaceContext.currentWorkspaceId());

    updateRequirementUiState();
}

void BuildsWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if (m_workspaceContext.hasCurrentWorkspace() && m_selectedBuildId > 0) {
        loadRequirements();
        updateRequirementUiState();
    }
}

void BuildsWidget::workspaceChanged(int workspaceId)
{
    Q_UNUSED(workspaceId);

    m_selectedBuildId = 0;

    loadBuilds();
    loadRequirements();
    updateUiState();
    updateRequirementUiState();
}

void BuildsWidget::refresh()
{
    loadBuilds();
    loadRequirements();
    updateUiState();
    updateRequirementUiState();
}


void BuildsWidget::loadBuilds()
{
    m_buildsTable->setRowCount(0);

    if (!m_workspaceContext.hasCurrentWorkspace()) {
        m_statusLabel->setText("Select a workspace to view builds.");
        return;
    }

    BuildRepository repository;
    ManufacturerRepository manufacturerRepository;

    const QList<Build> builds = repository.getByWorkspace(
        m_workspaceContext.currentWorkspaceId(),
        m_showArchivedBuildsCheck && m_showArchivedBuildsCheck->isChecked());

    int row = 0;

    for (const Build& build : builds) {
        m_buildsTable->insertRow(row);

        auto* typeItem = new QTableWidgetItem(build.buildType());
        auto* setNumberItem = new QTableWidgetItem(build.sourceReference());

        const QString inventoryModeText =
            build.inventoryMode() == "CompleteSet" ? "Complete Set" : "Build from Stock";

        auto* inventoryModeItem = new QTableWidgetItem(inventoryModeText);

        QString manufacturerText;

        if (build.inventoryMode() == "CompleteSet") {
            const std::optional<Manufacturer> manufacturer =
                manufacturerRepository.getById(build.manufacturerId());

            manufacturerText = manufacturer ? manufacturer->name() : QString("(Unknown)");
        } else {
            manufacturerText = QStringLiteral("From Stock");
        }

        auto* manufacturerItem = new QTableWidgetItem(manufacturerText);
        auto* nameItem = new QTableWidgetItem(build.name());
        auto* statusItem = new QTableWidgetItem(build.status());
        auto* notesItem = new QTableWidgetItem(build.notes());

        auto* actionCombo = new QComboBox(m_buildsTable);
        actionCombo->addItem("Actions...", QString());

        if (build.isActive()) {
            actionCombo->addItem("Edit Build...", "edit");

            if (build.status() == "Planned" || build.status() == "Pulling")
                actionCombo->addItem("Cancel Build...", "cancel");

            if (build.status() == "Disassembled" || build.status() == "Cancelled")
                actionCombo->addItem("Archive Build", "archive");
        } else {
            actionCombo->addItem("Reactivate Build", "reactivate");

            nameItem->setText(build.name() + " (Archived)");
            statusItem->setText(build.status() + " / Archived");
        }

        if (build.isActive()
            && build.status() != "Complete"
            && build.status() != "Disassembled"
            && build.status() != "Cancelled"
            && (build.inventoryMode() == "Stock"
                || build.inventoryMode() == "CompleteSet")) {
            actionCombo->addItem(build.inventoryMode() == "CompleteSet"
                                     ? "Complete Set..."
                                     : "Complete Build...",
                                 "complete");
        }

        if (build.isActive()
            && build.inventoryMode() == "Stock"
            && build.status() == "Complete") {
            actionCombo->addItem(build.buildType() == "MOC" ? "Disassemble MOC..."
                                                            : "Disassemble Build...",
                                 "disassemble");
        }

        if (build.isActive()
            && build.inventoryMode() == "CompleteSet"
            && build.status() == "Complete") {
            actionCombo->addItem("Disassemble Set...", "disassemble");
        }

        nameItem->setData(Qt::UserRole, build.id());

        m_buildsTable->setItem(row, 0, typeItem);
        m_buildsTable->setItem(row, 1, setNumberItem);
        m_buildsTable->setItem(row, 2, inventoryModeItem);
        m_buildsTable->setItem(row, 3, manufacturerItem);
        m_buildsTable->setItem(row, 4, nameItem);
        m_buildsTable->setItem(row, 5, statusItem);
        m_buildsTable->setItem(row, 6, notesItem);
        m_buildsTable->setCellWidget(row, 7, actionCombo);

        const int buildId = build.id();

        connect(actionCombo,
                &QComboBox::currentIndexChanged,
                this,
                [this, actionCombo, buildId](int index) {
                    if (index <= 0)
                        return;

                    const QString action = actionCombo->itemData(index).toString();

                    actionCombo->setCurrentIndex(0);

                    if (action == "edit") {
                        EditBuildDialog dialog(buildId, this);

                        if (dialog.exec() == QDialog::Accepted)
                            selectBuild(buildId);

                        return;
                    }

                    if (action == "cancel") {
                        BuildRepository buildRepository;

                        std::optional<Build> build = buildRepository.getById(buildId);

                        if (!build) {
                            QMessageBox::critical(this,
                                                  "Cancel Build",
                                                  "Unable to load the selected Build.");
                            return;
                        }

                        if (build->status() != "Planned"
                            && build->status() != "Pulling") {
                            QMessageBox::information(
                                this,
                                "Cancel Build",
                                "Only a Planned or Pulling Build can be cancelled.");
                            return;
                        }

                        BuildRequirementRepository requirementRepository;

                        const QList<BuildRequirement> requirements =
                            requirementRepository.getByBuild(buildId);

                        int totalPulled = 0;

                        for (const BuildRequirement& requirement : requirements)
                            totalPulled += qMax(requirement.quantityPulled(), 0);

                        QString message =
                            QString("Cancel this Build?\n\n%1\n\n"
                                    "The Build will remain in BrickSuite with a "
                                    "Cancelled status and can then be archived.")
                                .arg(build->name());

                        if (totalPulled > 0) {
                            message += QString(
                                "\n\n%1 piece(s) have already been physically pulled. "
                                "BrickSuite will first open the return/disassembly "
                                "workflow so those pieces can be returned safely to "
                                "loose inventory.")
                                           .arg(totalPulled);
                        } else {
                            message +=
                                "\n\nAny existing inventory allocations/reservations "
                                "will be released.";
                        }

                        const QMessageBox::StandardButton response =
                            QMessageBox::question(this,
                                                  "Cancel Build",
                                                  message,
                                                  QMessageBox::Yes | QMessageBox::No,
                                                  QMessageBox::No);

                        if (response != QMessageBox::Yes)
                            return;

                        if (totalPulled > 0) {
                            DisassembleSetDialog dialog(buildId, this);

                            if (dialog.exec() != QDialog::Accepted)
                                return;

                            build = buildRepository.getById(buildId);

                            if (!build) {
                                QMessageBox::critical(
                                    this,
                                    "Cancel Build",
                                    "The parts were returned, but BrickSuite could "
                                    "not reload the Build to mark it Cancelled.");
                                selectBuild(buildId);
                                return;
                            }
                        }

                        QSqlDatabase database = DatabaseManager::instance().database();

                        if (!database.transaction()) {
                            qCritical() << "Unable to start Build cancellation transaction."
                                        << "BuildId:" << buildId
                                        << "DatabaseError:" << database.lastError().text();

                            QMessageBox::critical(this,
                                                  "Cancel Build",
                                                  "Unable to start the cancellation.");
                            return;
                        }

                        BuildAllocationRepository allocationRepository;

                        if (!allocationRepository.removeAllForBuild(buildId)) {
                            database.rollback();

                            qCritical() << "Build cancellation failed while releasing allocations."
                                        << "BuildId:" << buildId;

                            QMessageBox::critical(
                                this,
                                "Cancel Build",
                                "Unable to release the Build's inventory allocations.");
                            return;
                        }

                        build->setStatus("Cancelled");

                        if (!buildRepository.update(*build)) {
                            database.rollback();

                            qCritical() << "Build cancellation failed while saving Cancelled status."
                                        << "BuildId:" << buildId;

                            QMessageBox::critical(this,
                                                  "Cancel Build",
                                                  "Unable to mark the Build Cancelled.");
                            return;
                        }

                        if (!database.commit()) {
                            qCritical() << "Unable to commit Build cancellation."
                                        << "BuildId:" << buildId
                                        << "DatabaseError:" << database.lastError().text();

                            database.rollback();

                            QMessageBox::critical(this,
                                                  "Cancel Build",
                                                  "Unable to commit the cancellation.");
                            return;
                        }

                        qInfo() << "Build cancelled."
                                << "BuildId:" << buildId
                                << "Name:" << build->name()
                                << "PreviouslyPulledPieces:" << totalPulled;

                        selectBuild(buildId);

                        QMessageBox::information(
                            this,
                            "Cancel Build",
                            QString("\"%1\" is now Cancelled.\n\n"
                                    "You may archive it from the Build Actions menu.")
                                .arg(build->name()));
                        return;
                    }

                    if (action == "archive") {
                        BuildRepository repository;

                        const std::optional<Build> build = repository.getById(buildId);

                        if (!build) {
                            QMessageBox::critical(this,
                                                  "Archive Build",
                                                  "Unable to load the selected Build.");
                            return;
                        }

                        if (build->status() != "Disassembled"
                            && build->status() != "Cancelled") {
                            QMessageBox::information(
                                this,
                                "Archive Build",
                                "Only a Disassembled or Cancelled Build can be archived.");
                            return;
                        }

                        const QMessageBox::StandardButton response =
                            QMessageBox::question(
                                this,
                                "Archive Build",
                                QString("Archive this Build?\n\n%1\n\n"
                                        "The Build and all of its history will remain "
                                        "in the database. You can show and reactivate "
                                        "archived Builds later.")
                                    .arg(build->name()),
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::No);

                        if (response != QMessageBox::Yes)
                            return;

                        if (!repository.setActive(buildId, false)) {
                            qCritical() << "Unable to archive Build."
                                        << "BuildId:" << buildId;

                            QMessageBox::critical(this,
                                                  "Archive Build",
                                                  "Unable to archive the selected Build.");
                            return;
                        }

                        qInfo() << "Build archived."
                                << "BuildId:" << buildId
                                << "Name:" << build->name()
                                << "Status:" << build->status();

                        m_selectedBuildId = 0;
                        loadBuilds();
                        loadRequirements();
                        updateRequirementUiState();

                        QMessageBox::information(this,
                                                 "Archive Build",
                                                 QString("\"%1\" has been archived.")
                                                     .arg(build->name()));
                        return;
                    }

                    if (action == "reactivate") {
                        BuildRepository repository;

                        std::optional<Build> build = repository.getById(buildId);

                        if (!build) {
                            QMessageBox::critical(this,
                                                  "Reactivate Build",
                                                  "Unable to load the selected Build.");
                            return;
                        }

                        if (build->status() == "Cancelled") {
                            build->setStatus("Planned");
                            build->setIsActive(true);

                            if (!repository.update(*build)) {
                                QMessageBox::critical(
                                    this,
                                    "Reactivate Build",
                                    "Unable to reactivate the selected Build.");
                                return;
                            }
                        } else {
                            if (!repository.setActive(buildId, true)) {
                                QMessageBox::critical(
                                    this,
                                    "Reactivate Build",
                                    "Unable to reactivate the selected Build.");
                                return;
                            }
                        }

                        qInfo() << "Build reactivated."
                                << "BuildId:" << buildId
                                << "Name:" << build->name()
                                << "Status:"
                                << (build->status() == "Cancelled" ? "Planned"
                                                                  : build->status());

                        selectBuild(buildId);

                        QMessageBox::information(this,
                                                 "Reactivate Build",
                                                 QString("\"%1\" is active again.")
                                                     .arg(build->name()));
                        return;
                    }

                    if (action == "complete") {
                        BuildRepository buildRepository;

                        std::optional<Build> build = buildRepository.getById(buildId);

                        if (!build) {
                            QMessageBox::critical(this,
                                                  "Complete Build",
                                                  "Unable to load the selected Build.");
                            return;
                        }

                        BuildRequirementRepository requirementRepository;

                        const QList<BuildRequirement> requirements =
                            requirementRepository.getByBuild(buildId);

                        if (requirements.isEmpty()) {
                            QMessageBox::information(this,
                                                     "Complete Build",
                                                     "This Build does not have any requirements.");
                            return;
                        }

                        if (build->inventoryMode() == "CompleteSet") {
                            int regularRows = 0;
                            int regularPieces = 0;
                            int spareRows = 0;
                            int sparePieces = 0;
                            int sparePiecesStored = 0;

                            for (const BuildRequirement& requirement : requirements) {
                                if (requirement.isSpare()) {
                                    ++spareRows;
                                    sparePieces += requirement.quantityRequired();
                                    sparePiecesStored += requirement.quantityReleased();
                                } else {
                                    ++regularRows;
                                    regularPieces += requirement.quantityRequired();
                                }
                            }

                            QString message =
                                QString("Mark this Complete Set Complete?\n\n%1")
                                    .arg(build->name());

                            if (!build->setNumber().trimmed().isEmpty())
                                message += QString("\nSet: %1").arg(build->setNumber());

                            message += QString("\n\nRegular Set Contents: %1 rows / %2 pieces"
                                               "\nBoxed Spare Contents: %3 rows / %4 pieces")
                                           .arg(regularRows)
                                           .arg(regularPieces)
                                           .arg(spareRows)
                                           .arg(sparePieces);

                            if (sparePiecesStored > 0) {
                                message += QString("\nSpare Pieces Already Stored: %1")
                                               .arg(sparePiecesStored);
                            }

                            if (sparePieces > sparePiecesStored) {
                                message +=
                                    "\n\nAfter completion, boxed spare parts can be "
                                    "transferred to My Loose Inventory using "
                                    "Store Spare...";
                            }

                            const QMessageBox::StandardButton response =
                                QMessageBox::question(this,
                                                      "Complete Set",
                                                      message,
                                                      QMessageBox::Yes | QMessageBox::No,
                                                      QMessageBox::No);

                            if (response != QMessageBox::Yes)
                                return;

                            build->setStatus("Complete");

                            if (!buildRepository.update(*build)) {
                                qCritical() << "Unable to mark Complete Set Complete."
                                            << "BuildId:" << buildId;

                                QMessageBox::critical(this,
                                                      "Complete Set",
                                                      "Unable to mark the Complete Set Complete.");
                                return;
                            }

                            qInfo() << "Complete Set completed."
                                    << "BuildId:" << buildId
                                    << "Name:" << build->name()
                                    << "RegularPieces:" << regularPieces
                                    << "SparePieces:" << sparePieces;

                            selectBuild(buildId);

                            QMessageBox::information(
                                this,
                                "Complete Set",
                                QString("\"%1\" is now Complete.")
                                    .arg(build->name()));
                            return;
                        }

                        int regularRows = 0;
                        int spareRows = 0;
                        int regularRequired = 0;
                        int regularPulled = 0;
                        int spareRequired = 0;
                        int sparePulled = 0;
                        int incompleteRows = 0;

                        for (const BuildRequirement& requirement : requirements) {
                            if (requirement.isSpare()) {
                                ++spareRows;
                                spareRequired += requirement.quantityRequired();
                                sparePulled += requirement.quantityPulled();
                                continue;
                            }

                            ++regularRows;
                            regularRequired += requirement.quantityRequired();
                            regularPulled += requirement.quantityPulled();

                            const int remaining =
                                qMax(requirement.quantityRequired()
                                         - requirement.quantityPulled(),
                                     0);

                            if (remaining > 0)
                                ++incompleteRows;
                        }

                        if (regularRows <= 0) {
                            QMessageBox::information(this,
                                                     "Complete Build",
                                                     "This Build does not have any regular "
                                                     "requirements to complete.");
                            return;
                        }

                        if (incompleteRows > 0) {
                            QMessageBox::information(
                                this,
                                "Complete Build",
                                QString("This Build is not ready to be marked Complete.\n\n"
                                        "Regular Required: %1\n"
                                        "Regular Pulled: %2\n"
                                        "Remaining: %3\n"
                                        "Incomplete Part/Color Rows: %4")
                                    .arg(regularRequired)
                                    .arg(regularPulled)
                                    .arg(qMax(regularRequired - regularPulled, 0))
                                    .arg(incompleteRows));
                            return;
                        }

                        const int unpulledSpares = qMax(spareRequired - sparePulled, 0);

                        QString message =
                            QString("Mark this Build Complete?\n\n%1")
                                .arg(build->name());

                        if (!build->setNumber().trimmed().isEmpty())
                            message += QString("\nSet: %1").arg(build->setNumber());

                        message += QString("\n\nRegular Requirements: %1 rows"
                                           "\nRegular Required: %2"
                                           "\nRegular Pulled: %3")
                                       .arg(regularRows)
                                       .arg(regularRequired)
                                       .arg(regularPulled);

                        if (spareRows > 0) {
                            message += QString("\n\nOptional Spare Rows: %1"
                                               "\nSpare Required: %2"
                                               "\nSpare Pulled: %3"
                                               "\nSpare Not Pulled: %4")
                                           .arg(spareRows)
                                           .arg(spareRequired)
                                           .arg(sparePulled)
                                           .arg(unpulledSpares);
                        }

                        message += "\n\nAll non-spare requirements are fulfilled.";

                        const QMessageBox::StandardButton response =
                            QMessageBox::question(this,
                                                  "Complete Build",
                                                  message,
                                                  QMessageBox::Yes | QMessageBox::No,
                                                  QMessageBox::No);

                        if (response != QMessageBox::Yes)
                            return;

                        build->setStatus("Complete");

                        if (!buildRepository.update(*build)) {
                            qCritical() << "Unable to mark Build Complete."
                                        << "BuildId:" << buildId;

                            QMessageBox::critical(this,
                                                  "Complete Build",
                                                  "Unable to mark the Build Complete.");
                            return;
                        }

                        qInfo() << "Build completed."
                                << "BuildId:" << buildId
                                << "Name:" << build->name()
                                << "RegularPieces:" << regularPulled
                                << "SparePiecesPulled:" << sparePulled;

                        selectBuild(buildId);

                        QMessageBox::information(this,
                                                 "Complete Build",
                                                 QString("\"%1\" is now Complete.")
                                                     .arg(build->name()));
                        return;
                    }

                    if (action == "disassemble") {
                        DisassembleSetDialog dialog(buildId, this);

                        if (dialog.exec() == QDialog::Accepted)
                            selectBuild(buildId);

                        return;
                    }
                });

        ++row;
    }

    if (builds.isEmpty()) {
        m_statusLabel->setText("No builds have been created.");
    } else {
        m_statusLabel->setText(QString("%1 build(s).").arg(builds.size()));
    }
}

void BuildsWidget::addBuild()
{
    if (!m_workspaceContext.hasCurrentWorkspace()) {
        QMessageBox::warning(this, "BrickSuite", "Select a workspace before creating a build.");
        return;
    }

    const QString buildType = m_typeCombo->currentData().toString();
    const QString setNumber = m_setNumberEdit->text().trimmed();
    const QString inventoryMode = m_inventoryModeCombo->currentData().toString();
    const QString name = m_nameEdit->text().trimmed();
    const QString status = m_statusCombo->currentData().toString();
    const QString notes = m_notesEdit->toPlainText().trimmed();

    if (name.isEmpty()) {
        QMessageBox::warning(this, "BrickSuite", "Enter a name for the build.");
        return;
    }

    if (buildType == "Set" && setNumber.isEmpty()) {
        QMessageBox::warning(this, "BrickSuite", "Enter a Set Number for the Set Build.");
        return;
    }

    if (buildType == "MOC" && setNumber.isEmpty()) {
        QMessageBox::warning(this, "BrickSuite", "Enter a MOC Number for the MOC Build.");
        return;
    }

    Build build;

    build.setWorkspaceId(m_workspaceContext.currentWorkspaceId());
    build.setBuildType(buildType);
    build.setSetNumber(setNumber);
    build.setInventoryMode(inventoryMode);
    build.setManufacturerId(m_manufacturerCombo->currentData().toInt());
    build.setName(name);
    build.setStatus(status);
    build.setNotes(notes);

    BuildRepository repository;

    if (!repository.create(build)) {
        QMessageBox::critical(this, "BrickSuite", "Unable to create the build.");
        return;
    }

    m_setNumberEdit->clear();
    m_inventoryModeCombo->setCurrentIndex(0);

    ManufacturerRepository manufacturerRepository;

    const int legoIndex =
        m_manufacturerCombo->findData(manufacturerRepository.legoManufacturerId());

    if (legoIndex >= 0)
        m_manufacturerCombo->setCurrentIndex(legoIndex);

    m_nameEdit->clear();
    m_notesEdit->clear();
    m_statusCombo->setCurrentIndex(0);

    selectBuild(build.id());

    if (build.buildType() == "Set"
        && build.inventoryMode() == "CompleteSet"
        && !build.setNumber().trimmed().isEmpty()) {
        SetImportPreviewDialog dialog(build.id(),
                                      build.setNumber().trimmed(),
                                      this);

        if (dialog.exec() == QDialog::Accepted)
            loadRequirements();

        updateRequirementUiState();
    }

    QMessageBox::information(this,
                             "BrickSuite",
                             QString("Build \"%1\" created successfully.").arg(build.name()));
}

void BuildsWidget::updateUiState()
{
    const bool enabled = m_workspaceContext.hasCurrentWorkspace();

    m_typeCombo->setEnabled(enabled);

    const QString buildType = m_typeCombo->currentData().toString();
    const bool hasNumberField = buildType == "Set" || buildType == "MOC";

    m_setNumberEdit->setEnabled(enabled && hasNumberField);
    m_inventoryModeCombo->setEnabled(enabled);

    const bool completeSet =
        m_inventoryModeCombo->currentData().toString() == "CompleteSet";

    m_manufacturerCombo->setEnabled(enabled && completeSet);
    m_nameEdit->setEnabled(enabled);
    m_statusCombo->setEnabled(enabled);
    m_notesEdit->setEnabled(enabled);
    m_addButton->setEnabled(enabled);
    m_newBuildGroup->setEnabled(enabled);
}

void BuildsWidget::loadManufacturers()
{
    m_manufacturerCombo->clear();

    ManufacturerRepository repository;

    const QList<Manufacturer> manufacturers = repository.getAll(true);

    for (const Manufacturer& manufacturer : manufacturers)
        m_manufacturerCombo->addItem(manufacturer.name(), manufacturer.id());

    const int legoIndex =
        m_manufacturerCombo->findData(repository.legoManufacturerId());

    if (legoIndex >= 0)
        m_manufacturerCombo->setCurrentIndex(legoIndex);

    m_manufacturerCombo->setEnabled(
        m_workspaceContext.hasCurrentWorkspace()
        && m_inventoryModeCombo->currentData().toString() == "CompleteSet");
}

void BuildsWidget::loadColors()
{
    m_colorCombo->clear();

    ColorRepository repository;

    const QList<Color> colors = repository.getAll();

    for (const Color& color : colors) {
        ColorComboHelper::addColorItem(m_colorCombo, color.name(), color.id(), color.rgb());
    }
}

void BuildsWidget::buildSelectionChanged()
{
    m_selectedBuildId = 0;

    const int row = m_buildsTable->currentRow();

    if (row < 0) {
        loadRequirements();
        updateRequirementUiState();
        return;
    }

    QTableWidgetItem* nameItem = m_buildsTable->item(row, 4);

    m_selectedBuildId = nameItem->data(Qt::UserRole).toInt();

    loadRequirements();
    updateRequirementUiState();

    if (m_selectedBuildId > 0)
        m_newBuildGroup->setChecked(false);
}


void BuildsWidget::loadRequirements()
{
    m_requirementsTable->setRowCount(0);

    if (m_selectedBuildId <= 0) {
        m_requirementsLabel->setText("Select a build to view its requirements.");
        return;
    }

    const int buildRow = m_buildsTable->currentRow();

    QString buildDescription;

    if (buildRow >= 0) {
        QTableWidgetItem* setNumberItem = m_buildsTable->item(buildRow, 1);
        QTableWidgetItem* nameItem = m_buildsTable->item(buildRow, 4);

        const QString setNumber = setNumberItem ? setNumberItem->text() : QString();
        const QString name = nameItem ? nameItem->text() : QString();

        buildDescription = !setNumber.isEmpty()
                               ? QString("%1 — %2").arg(setNumber, name)
                               : name;
    }

    BuildRepository buildRepository;

    const std::optional<Build> selectedBuild =
        buildRepository.getById(m_selectedBuildId);

    const bool completeSet =
        selectedBuild && selectedBuild->inventoryMode() == "CompleteSet";

    const bool completeSetIsComplete =
        completeSet && selectedBuild->status() == "Complete";

    m_requirementsLabel->setText(
        completeSet
            ? QString("Set Contents for: %1").arg(buildDescription)
            : QString("Requirements for: %1").arg(buildDescription));

    BuildRequirementRepository requirementRepository;
    const QList<BuildRequirement> requirements =
        requirementRepository.getByBuild(m_selectedBuildId);

    PartRepository partRepository;
    ColorRepository colorRepository;
    InventoryRecordRepository inventoryRepository;
    BuildAllocationRepository allocationRepository;

    int row = 0;

    for (const BuildRequirement& requirement : requirements) {
        //
        // The first three columns continue to describe the canonical
        // requirement. Stock fulfillment below uses effective identity.
        //
        const std::optional<Part> part =
            partRepository.getById(requirement.partId());

        const std::optional<Color> color =
            colorRepository.getById(requirement.colorId());

        m_requirementsTable->insertRow(row);

        const QString partNumber = part ? part->partNumber() : QString();
        const QString partName = part ? part->name() : QString("(Part unavailable)");
        const QString colorName = color ? color->name() : QString("(Color unavailable)");

        auto* partNumberItem = new QTableWidgetItem(partNumber);
        partNumberItem->setData(Qt::UserRole, requirement.id());

        auto* nameItem = new QTableWidgetItem(partName);
        auto* colorItem = new QTableWidgetItem(colorName);

        if (color) {
            QString normalizedRgb = color->rgb().trimmed();

            if (!normalizedRgb.isEmpty() && !normalizedRgb.startsWith('#'))
                normalizedRgb.prepend('#');

            const QColor sourceColor(normalizedRgb);

            if (sourceColor.isValid()) {
                const QColor backgroundColor =
                    m_requirementsTable->palette().color(QPalette::Base);

                colorItem->setForeground(
                    ColorComboHelper::readableColor(sourceColor, backgroundColor));
            }
        }

        //
        // Make a substitution visible without changing the requirement table
        // schema. The canonical identity remains the row title; the tooltip
        // identifies the effective stock identity used for fulfillment.
        //
        const int effectivePartId = requirement.effectivePartId();
        const int effectiveColorId = requirement.effectiveColorId();

        if (effectivePartId != requirement.partId()
            || effectiveColorId != requirement.colorId()) {
            const std::optional<Part> effectivePart =
                partRepository.getById(effectivePartId);

            const std::optional<Color> effectiveColor =
                colorRepository.getById(effectiveColorId);

            const QString effectivePartText =
                effectivePart
                    ? QString("%1 — %2").arg(effectivePart->partNumber(),
                                             effectivePart->name())
                    : QString::number(effectivePartId);

            const QString effectiveColorText =
                effectiveColor ? effectiveColor->name()
                               : QString::number(effectiveColorId);

            const QString tooltip =
                QString("Fulfilled from stock as:\n%1\nColor: %2")
                    .arg(effectivePartText, effectiveColorText);

            partNumberItem->setToolTip(tooltip);
            nameItem->setToolTip(tooltip);
            colorItem->setToolTip(tooltip);
        }

        const int workspaceId = m_workspaceContext.currentWorkspaceId();

        const int owned =
            inventoryRepository.totalQuantityForPartColor(workspaceId,
                                                           effectivePartId,
                                                           effectiveColorId);

        const int quantityPulled = requirement.quantityPulled();

        const int remainingRequired =
            qMax(requirement.quantityRequired() - quantityPulled, 0);

        //
        // Total allocation is measured against the effective physical
        // identity because all requirements and Builds compete for the
        // same loose stock.
        //
        const int totalAllocated =
            allocationRepository.totalAllocatedForPartColor(workspaceId,
                                                             effectivePartId,
                                                             effectiveColorId);

        //
        // Only reservations explicitly tied to this requirement satisfy it.
        //
        const int thisRequirementAllocated =
            allocationRepository.totalAllocatedForRequirement(requirement.id());

        const int otherAllocated =
            qMax(totalAllocated - thisRequirementAllocated, 0);

        const int available =
            qMax(owned - totalAllocated, 0);

        const int missing =
            requirement.isSpare()
                ? 0
                : qMax(remainingRequired
                           - thisRequirementAllocated
                           - available,
                       0);

        auto* requiredItem =
            new QTableWidgetItem(QString::number(requirement.quantityRequired()));

        auto* pulledItem =
            new QTableWidgetItem(QString::number(quantityPulled));

        auto* remainingItem =
            new QTableWidgetItem(QString::number(remainingRequired));

        auto* ownedItem =
            new QTableWidgetItem(QString::number(owned));

        auto* thisBuildItem =
            new QTableWidgetItem(QString::number(thisRequirementAllocated));

        auto* otherBuildsItem =
            new QTableWidgetItem(QString::number(otherAllocated));

        auto* availableItem =
            new QTableWidgetItem(QString::number(available));

        auto* missingItem =
            new QTableWidgetItem(QString::number(missing));

        auto* spareItem =
            new QTableWidgetItem(requirement.isSpare() ? "Yes" : "No");

        auto* actionCombo = new QComboBox(m_requirementsTable);

        actionCombo->addItem("Actions...");

        if (!completeSet) {
            actionCombo->addItem("Edit", "edit");
            actionCombo->addItem("Delete", "delete");
            actionCombo->addItem("Allocate...", "allocate");
        } else if (completeSetIsComplete && requirement.isSpare()) {
            const int spareRemaining =
                qMax(requirement.quantityRequired()
                         - requirement.quantityReleased(),
                     0);

            if (spareRemaining > 0) {
                actionCombo->addItem("Store Spare...", "store_spare");
            } else {
                actionCombo->clear();
                actionCombo->addItem("Stored");
                actionCombo->setEnabled(false);
            }
        } else {
            actionCombo->clear();
            actionCombo->addItem("—");
            actionCombo->setEnabled(false);
        }

        const int requirementId = requirement.id();
        const bool requirementIsSpare = requirement.isSpare();

        connect(actionCombo,
                &QComboBox::currentIndexChanged,
                this,
                [this, actionCombo, requirementId, requirementIsSpare](int index) {
                    if (index <= 0)
                        return;

                    const QString action = actionCombo->itemData(index).toString();

                    if (action == "store_spare") {
                        storeSpare(requirementId);
                        loadRequirements();
                        updateRequirementUiState();
                        return;
                    }

                    if (action == "edit") {
                        EditBuildRequirementDialog dialog(requirementId, this);

                        if (dialog.exec() == QDialog::Accepted) {
                            loadRequirements();
                            return;
                        }
                    } else if (action == "delete") {
                        const QMessageBox::StandardButton response =
                            QMessageBox::warning(this,
                                                 "Delete Build Requirement",
                                                 "Delete this requirement from the build?",
                                                 QMessageBox::Yes | QMessageBox::No,
                                                 QMessageBox::No);

                        if (response == QMessageBox::Yes) {
                            BuildRequirementRepository repository;

                            if (!repository.remove(requirementId)) {
                                QMessageBox::critical(
                                    this,
                                    "BrickSuite",
                                    "Unable to delete the build requirement.");
                            } else {
                                loadRequirements();
                                return;
                            }
                        }
                    } else if (action == "allocate") {
                        if (requirementIsSpare) {
                            QMessageBox::information(this,
                                                     "Allocate Requirement",
                                                     "Spare requirements are optional and "
                                                     "are not allocated from workshop "
                                                     "inventory by default.");

                            actionCombo->setCurrentIndex(0);
                            return;
                        }

                        BuildRepository buildRepository;

                        const std::optional<Build> build =
                            buildRepository.getById(m_selectedBuildId);

                        if (!build) {
                            QMessageBox::warning(this,
                                                 "Allocate Requirement",
                                                 "Unable to load the selected Build.");

                            actionCombo->setCurrentIndex(0);
                            return;
                        }

                        if (build->inventoryMode() != "Stock") {
                            QMessageBox::information(this,
                                                     "Allocate Requirement",
                                                     "Inventory allocation is available "
                                                     "only for Build from Stock.");

                            actionCombo->setCurrentIndex(0);
                            return;
                        }

                        AllocateBuildRequirementDialog dialog(
                            m_workspaceContext.currentWorkspaceId(),
                            m_selectedBuildId,
                            requirementId,
                            this);

                        if (dialog.exec() == QDialog::Accepted) {
                            loadRequirements();
                            return;
                        }
                    }

                    actionCombo->setCurrentIndex(0);
                });

        requiredItem->setTextAlignment(Qt::AlignCenter);
        pulledItem->setTextAlignment(Qt::AlignCenter);
        remainingItem->setTextAlignment(Qt::AlignCenter);
        ownedItem->setTextAlignment(Qt::AlignCenter);
        thisBuildItem->setTextAlignment(Qt::AlignCenter);
        otherBuildsItem->setTextAlignment(Qt::AlignCenter);
        availableItem->setTextAlignment(Qt::AlignCenter);
        missingItem->setTextAlignment(Qt::AlignCenter);
        spareItem->setTextAlignment(Qt::AlignCenter);

        m_requirementsTable->setItem(row, 0, partNumberItem);
        m_requirementsTable->setItem(row, 1, nameItem);
        m_requirementsTable->setItem(row, 2, colorItem);
        m_requirementsTable->setItem(row, 3, requiredItem);
        m_requirementsTable->setItem(row, 4, pulledItem);
        m_requirementsTable->setItem(row, 5, remainingItem);
        m_requirementsTable->setItem(row, 6, ownedItem);
        m_requirementsTable->setItem(row, 7, thisBuildItem);
        m_requirementsTable->setItem(row, 8, otherBuildsItem);
        m_requirementsTable->setItem(row, 9, availableItem);
        m_requirementsTable->setItem(row, 10, missingItem);
        m_requirementsTable->setItem(row, 11, spareItem);
        m_requirementsTable->setCellWidget(row, 12, actionCombo);

        ++row;
    }
}

void BuildsWidget::addRequirement()
{
    if (m_selectedBuildId <= 0) {
        QMessageBox::warning(this, "BrickSuite", "Select a build before adding a requirement.");
        return;
    }

    BuildRepository buildRepository;

    const std::optional<Build> build =
        buildRepository.getById(m_selectedBuildId);

    if (!build) {
        QMessageBox::warning(this,
                             "BrickSuite",
                             "Unable to load the selected Build.");
        return;
    }

    if (build->inventoryMode() == "CompleteSet") {
        QMessageBox::information(
            this,
            "Complete Set Requirements",
            "Complete Set requirements are sourced from the canonical "
            "Set contents. Use Load Set from Rebrickable to load or "
            "refresh them.");
        return;
    }

    const QString partNumber = m_partNumberEdit->text().trimmed();

    if (partNumber.isEmpty()) {
        QMessageBox::warning(this, "BrickSuite", "Enter a part number.");
        return;
    }

    PartRepository partRepository;
    const std::optional<Part> part = partRepository.getByPartNumber(partNumber);

    if (!part) {
        QMessageBox::warning(this,
                             "BrickSuite",
                             QString("Part %1 was not found in the "
                                     "BrickSuite Parts Catalog.")
                                 .arg(partNumber));
        return;
    }

    const int colorId = m_colorCombo->currentData().toInt();

    if (colorId <= 0) {
        QMessageBox::warning(this, "BrickSuite", "Select a valid color.");
        return;
    }

    BuildRequirement requirement;

    requirement.setBuildId(m_selectedBuildId);
    requirement.setPartId(part->id());
    requirement.setColorId(colorId);
    requirement.setQuantityRequired(m_quantitySpin->value());
    requirement.setIsSpare(m_spareCheck->isChecked());

    BuildRequirementRepository repository;

    if (!repository.create(requirement)) {
        QMessageBox::critical(this,
                              "BrickSuite",
                              QString("Unable to add the requirement.\n\n"
                                      "The same Part / Color / Spare "
                                      "combination may already exist "
                                      "for this build."));
        return;
    }

    m_partNumberEdit->clear();
    m_quantitySpin->setValue(1);
    m_spareCheck->setChecked(false);
    m_partNumberEdit->setFocus();

    loadRequirements();
}

void BuildsWidget::allocateAvailable()
{
    if (!m_workspaceContext.hasCurrentWorkspace() || m_selectedBuildId <= 0) {
        QMessageBox::warning(this, "Allocate Available", "Select a Build first.");
        return;
    }

    BuildRepository buildRepository;

    const std::optional<Build> build = buildRepository.getById(m_selectedBuildId);

    if (!build) {
        QMessageBox::critical(this, "Allocate Available", "Unable to load the selected Build.");
        return;
    }

    if (build->inventoryMode() != "Stock") {
        QMessageBox::information(this,
                                 "Allocate Available",
                                 "Automatic inventory allocation is available only for "
                                 "Build from Stock.");
        return;
    }

    BuildRequirementRepository requirementRepository;
    InventoryRecordRepository inventoryRepository;
    BuildAllocationRepository allocationRepository;

    const QList<BuildRequirement> requirements =
        requirementRepository.getByBuild(m_selectedBuildId);

    int regularRequirementCount = 0;

    for (const BuildRequirement& requirement : requirements) {
        if (!requirement.isSpare())
            ++regularRequirementCount;
    }

    if (regularRequirementCount == 0) {
        QMessageBox::information(
            this,
            "Allocate Available",
            "This Build does not have any non-spare requirements to allocate.");
        return;
    }

    int preferredStorageLocationId = 0;
    QString preferredStoragePath;

    {
        QDialog preferenceDialog(this);
        preferenceDialog.setWindowTitle("Allocate Available");

        auto* layout = new QFormLayout(&preferenceDialog);

        auto* explanation = new QLabel(
            "Choose a preferred storage location to allocate from first.\n"
            "If it does not contain enough available parts, BrickSuite will "
            "continue allocating from other storage locations.",
            &preferenceDialog);

        explanation->setWordWrap(true);
        layout->addRow(explanation);

        auto* storageCombo = new QComboBox(&preferenceDialog);
        storageCombo->addItem("No Preferred Storage", 0);

        StorageLocationRepository storageRepository;

        const QList<StorageLocation> locations =
            storageRepository.getByWorkspace(m_workspaceContext.currentWorkspaceId());

        for (const StorageLocation& location : locations) {
            if (storageRepository.hasChildren(location.id()))
                continue;

            QStringList pathParts;
            int currentId = location.id();
            int safetyCount = 0;

            while (currentId > 0 && safetyCount < 100) {
                const std::optional<StorageLocation> current =
                    storageRepository.getById(currentId);

                if (!current)
                    break;

                pathParts.prepend(current->name());
                currentId = current->parentLocationId();
                ++safetyCount;
            }

            const QString path =
                pathParts.isEmpty() ? location.name()
                                    : pathParts.join(" / ");

            storageCombo->addItem(path, location.id());
        }

        layout->addRow("Preferred Storage:", storageCombo);

        auto* buttonBox =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                 &preferenceDialog);

        layout->addRow(buttonBox);

        connect(buttonBox,
                &QDialogButtonBox::accepted,
                &preferenceDialog,
                &QDialog::accept);

        connect(buttonBox,
                &QDialogButtonBox::rejected,
                &preferenceDialog,
                &QDialog::reject);

        if (preferenceDialog.exec() != QDialog::Accepted)
            return;

        preferredStorageLocationId = storageCombo->currentData().toInt();
        preferredStoragePath = storageCombo->currentText();
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    if (!database.transaction()) {
        qCritical() << "Unable to start Allocate Available transaction."
                    << "BuildId:" << m_selectedBuildId
                    << "DatabaseError:" << database.lastError().text();

        QMessageBox::critical(this,
                              "Allocate Available",
                              "Unable to start the automatic allocation transaction.");
        return;
    }

    int allocationsCreated = 0;
    int allocationsUpdated = 0;
    int piecesAdded = 0;
    int preferredPiecesAdded = 0;

    for (const BuildRequirement& requirement : requirements) {
        if (requirement.isSpare())
            continue;

        const int remainingRequired =
            qMax(requirement.quantityRequired()
                     - requirement.quantityPulled(),
                 0);

        if (remainingRequired <= 0)
            continue;

        //
        // Requirement allocation total, not Build/Part/Color total.
        //
        int requirementAllocated =
            allocationRepository.totalAllocatedForRequirement(requirement.id());

        int stillNeeded =
            qMax(remainingRequired - requirementAllocated, 0);

        if (stillNeeded <= 0)
            continue;

        const int effectivePartId = requirement.effectivePartId();
        const int effectiveColorId = requirement.effectiveColorId();

        const QList<InventoryRecord> sourceRecords =
            inventoryRepository.getByPartColor(
                m_workspaceContext.currentWorkspaceId(),
                effectivePartId,
                effectiveColorId);

        QList<InventoryRecord> records;

        if (preferredStorageLocationId > 0) {
            for (const InventoryRecord& record : sourceRecords) {
                if (record.storageLocationId() == preferredStorageLocationId)
                    records.append(record);
            }

            for (const InventoryRecord& record : sourceRecords) {
                if (record.storageLocationId() != preferredStorageLocationId)
                    records.append(record);
            }
        } else {
            records = sourceRecords;
        }

        for (const InventoryRecord& record : records) {
            if (stillNeeded <= 0)
                break;

            const int totalAllocated =
                allocationRepository.totalAllocatedForInventoryRecord(record.id());

            //
            // Only this requirement's own allocation is reusable by this
            // requirement. Reservations for other requirements in this same
            // Build remain committed.
            //
            int currentRequirementAllocated = 0;
            std::optional<BuildAllocation> existingAllocation;

            const QList<BuildAllocation> recordAllocations =
                allocationRepository.getByInventoryRecord(record.id());

            for (const BuildAllocation& allocation : recordAllocations) {
                if (allocation.buildRequirementId() == requirement.id()) {
                    currentRequirementAllocated += allocation.quantityAllocated();

                    if (!existingAllocation)
                        existingAllocation = allocation;
                }
            }

            const int otherAllocated =
                qMax(totalAllocated - currentRequirementAllocated, 0);

            const int maximumForRequirement =
                qMax(record.quantity() - otherAllocated, 0);

            const int additionalCapacity =
                qMax(maximumForRequirement - currentRequirementAllocated, 0);

            if (additionalCapacity <= 0)
                continue;

            const int quantityToAdd =
                qMin(stillNeeded, additionalCapacity);

            if (quantityToAdd <= 0)
                continue;

            const int newAllocationQuantity =
                currentRequirementAllocated + quantityToAdd;

            if (existingAllocation) {
                existingAllocation->setQuantityAllocated(newAllocationQuantity);

                if (!allocationRepository.update(*existingAllocation)) {
                    qCritical() << "Allocate Available failed updating allocation."
                                << "BuildId:" << m_selectedBuildId
                                << "RequirementId:" << requirement.id()
                                << "InventoryRecordId:" << record.id();

                    database.rollback();

                    QMessageBox::critical(
                        this,
                        "Allocate Available",
                        "Unable to update an existing Build allocation. "
                        "No automatic allocations were saved.");

                    loadRequirements();
                    return;
                }

                ++allocationsUpdated;
            } else {
                BuildAllocation allocation;

                allocation.setBuildId(m_selectedBuildId);
                allocation.setBuildRequirementId(requirement.id());
                allocation.setInventoryRecordId(record.id());
                allocation.setPartId(effectivePartId);
                allocation.setColorId(effectiveColorId);
                allocation.setStorageLocationId(record.storageLocationId());
                allocation.setQuantityAllocated(quantityToAdd);

                if (!allocationRepository.create(allocation)) {
                    qCritical() << "Allocate Available failed creating allocation."
                                << "BuildId:" << m_selectedBuildId
                                << "RequirementId:" << requirement.id()
                                << "InventoryRecordId:" << record.id()
                                << "PartId:" << effectivePartId
                                << "ColorId:" << effectiveColorId
                                << "Quantity:" << quantityToAdd;

                    database.rollback();

                    QMessageBox::critical(
                        this,
                        "Allocate Available",
                        "Unable to create a Build allocation. "
                        "No automatic allocations were saved.");

                    loadRequirements();
                    return;
                }

                ++allocationsCreated;
            }

            piecesAdded += quantityToAdd;

            if (preferredStorageLocationId > 0
                && record.storageLocationId() == preferredStorageLocationId) {
                preferredPiecesAdded += quantityToAdd;
            }

            stillNeeded -= quantityToAdd;
            requirementAllocated += quantityToAdd;
        }
    }

    if (!database.commit()) {
        qCritical() << "Unable to commit Allocate Available transaction."
                    << "BuildId:" << m_selectedBuildId
                    << "DatabaseError:" << database.lastError().text();

        database.rollback();

        QMessageBox::critical(this,
                              "Allocate Available",
                              "Unable to save the automatic allocations. "
                              "No automatic allocations were saved.");

        loadRequirements();
        return;
    }

    int satisfiedRequirements = 0;
    int partiallySatisfiedRequirements = 0;
    int stillMissingRequirements = 0;
    int spareRequirementsSkipped = 0;

    for (const BuildRequirement& requirement : requirements) {
        if (requirement.isSpare()) {
            ++spareRequirementsSkipped;
            continue;
        }

        const int remainingRequired =
            qMax(requirement.quantityRequired()
                     - requirement.quantityPulled(),
                 0);

        if (remainingRequired <= 0) {
            ++satisfiedRequirements;
            continue;
        }

        const int allocated =
            allocationRepository.totalAllocatedForRequirement(requirement.id());

        if (allocated >= remainingRequired) {
            ++satisfiedRequirements;
        } else if (allocated > 0) {
            ++partiallySatisfiedRequirements;
        } else {
            ++stillMissingRequirements;
        }
    }

    loadRequirements();
    updateRequirementUiState();

    QString message;

    if (piecesAdded <= 0) {
        message =
            "BrickSuite did not find any additional loose inventory that could be allocated.";
    } else {
        message =
            QString("BrickSuite automatically allocated %1 piece(s).\n\n"
                    "Requirements satisfied: %2\n"
                    "Partially satisfied: %3\n"
                    "Still missing: %4")
                .arg(piecesAdded)
                .arg(satisfiedRequirements)
                .arg(partiallySatisfiedRequirements)
                .arg(stillMissingRequirements);

        if (preferredStorageLocationId > 0) {
            message +=
                QString("\n\nPreferred storage: %1\n"
                        "Pieces allocated there first: %2")
                    .arg(preferredStoragePath)
                    .arg(preferredPiecesAdded);
        }

        if (allocationsCreated > 0 || allocationsUpdated > 0) {
            message +=
                QString("\n\nAllocation records created: %1\n"
                        "Allocation records updated: %2")
                    .arg(allocationsCreated)
                    .arg(allocationsUpdated);
        }
    }

    if (spareRequirementsSkipped > 0) {
        message +=
            QString("\n\nSpare requirements skipped: %1")
                .arg(spareRequirementsSkipped);
    }

    qInfo() << "Allocate Available completed."
            << "BuildId:" << m_selectedBuildId
            << "PiecesAllocated:" << piecesAdded
            << "RequirementsSatisfied:" << satisfiedRequirements
            << "RequirementsPartial:" << partiallySatisfiedRequirements
            << "RequirementsMissing:" << stillMissingRequirements
            << "AllocationRowsCreated:" << allocationsCreated
            << "AllocationRowsUpdated:" << allocationsUpdated
            << "PreferredStorageLocationId:" << preferredStorageLocationId
            << "PreferredPiecesAllocated:" << preferredPiecesAdded;

    QMessageBox::information(this, "Allocate Available", message);
}


void BuildsWidget::updateRequirementUiState()
{
    const bool enabled =
        m_workspaceContext.hasCurrentWorkspace() && m_selectedBuildId > 0;

    bool buildIsActive = false;
    bool completeSet = false;
    bool completeSetIsComplete = false;

    if (enabled) {
        BuildRepository repository;

        const std::optional<Build> selectedBuild =
            repository.getById(m_selectedBuildId);

        buildIsActive = selectedBuild && selectedBuild->isActive();

        completeSet =
            selectedBuild && selectedBuild->inventoryMode() == "CompleteSet";

        completeSetIsComplete =
            completeSet && selectedBuild->status() == "Complete";
    }

    const bool canManuallyEditRequirements =
        enabled && buildIsActive && !completeSet;

    m_partNumberEdit->setEnabled(canManuallyEditRequirements);
    m_colorCombo->setEnabled(canManuallyEditRequirements);
    m_quantitySpin->setEnabled(canManuallyEditRequirements);
    m_spareCheck->setEnabled(canManuallyEditRequirements);
    m_addRequirementButton->setEnabled(canManuallyEditRequirements);

    m_requirementsTable->setEnabled(enabled);

    const QList<int> stockOnlyColumns = {
        4,  // Pulled
        5,  // Remaining
        6,  // Owned
        7,  // This Requirement
        8,  // Other Allocations
        9,  // Available
        10  // Missing
    };

    for (int column : stockOnlyColumns)
        m_requirementsTable->setColumnHidden(column, completeSet);

    m_requirementsTable->setColumnHidden(
        12,
        completeSet && !completeSetIsComplete);

    bool canLoadSet = false;
    bool canAllocateAvailable = false;
    bool canExportPullList = false;
    bool canExportMissingParts = false;
    bool canProcureMissingParts = false;
    bool canImportMoc = false;

    if (enabled) {
        BuildRepository repository;

        const std::optional<Build> build =
            repository.getById(m_selectedBuildId);

        if (build && build->isActive()) {
            canLoadSet =
                build->buildType() == "Set"
                && !build->setNumber().trimmed().isEmpty();

            canImportMoc =
                build->buildType() == "MOC"
                && build->inventoryMode() == "Stock";

            canAllocateAvailable = build->inventoryMode() == "Stock";
            canExportPullList = build->inventoryMode() == "Stock";
            canExportMissingParts = build->inventoryMode() == "Stock";
            canProcureMissingParts = build->inventoryMode() == "Stock";
        }
    }

    m_allocateAvailableButton->setEnabled(canAllocateAvailable);
    m_exportPullListButton->setEnabled(canExportPullList);
    m_importPullListButton->setEnabled(canExportPullList);
    m_interactivePullButton->setEnabled(canExportPullList);
    m_importMocPartsButton->setEnabled(canImportMoc);
    m_exportMissingPartsButton->setEnabled(canExportMissingParts);
    m_procureMissingPartsButton->setEnabled(canProcureMissingParts);
    m_loadSetFromRebrickableButton->setEnabled(canLoadSet);

    m_loadSetFromRebrickableButton->setText(
        completeSet
            ? QStringLiteral("Load / Refresh Set Contents")
            : QStringLiteral("Load Set from Rebrickable"));
}

void BuildsWidget::storeSpare(int requirementId)
{
    if (requirementId <= 0 || m_selectedBuildId <= 0)
        return;

    BuildRepository buildRepository;
    BuildRequirementRepository requirementRepository;

    const std::optional<Build> build =
        buildRepository.getById(m_selectedBuildId);

    const std::optional<BuildRequirement> requirement =
        requirementRepository.getById(requirementId);

    if (!build || !requirement) {
        QMessageBox::critical(this,
                              "Store Spare",
                              "Unable to load the Complete Set or spare requirement.");
        return;
    }

    if (build->inventoryMode() != "CompleteSet"
        || build->status() != "Complete") {
        QMessageBox::information(this,
                                 "Store Spare",
                                 "Spare parts can be stored after the Complete Set "
                                 "has been marked Complete.");
        return;
    }

    if (!requirement->isSpare()
        || requirement->buildId() != build->id()) {
        QMessageBox::information(this,
                                 "Store Spare",
                                 "The selected requirement is not a boxed spare "
                                 "for this Complete Set.");
        return;
    }

    const int quantityRemaining =
        qMax(requirement->quantityRequired()
                 - requirement->quantityReleased(),
             0);

    if (quantityRemaining <= 0) {
        QMessageBox::information(this,
                                 "Store Spare",
                                 "All pieces for this spare requirement have "
                                 "already been stored.");
        return;
    }

    PartRepository partRepository;
    ColorRepository colorRepository;

    const std::optional<Part> part =
        partRepository.getById(requirement->partId());

    const std::optional<Color> color =
        colorRepository.getById(requirement->colorId());

    QDialog dialog(this);
    dialog.setWindowTitle("Store Spare");

    auto* layout = new QFormLayout(&dialog);

    auto* partLabel =
        new QLabel(part ? part->partNumber()
                        : QString::number(requirement->partId()),
                   &dialog);

    auto* nameLabel =
        new QLabel(part ? part->name() : QString("(Part unavailable)"),
                   &dialog);

    auto* colorLabel =
        new QLabel(color ? color->name()
                         : QString::number(requirement->colorId()),
                   &dialog);

    auto* quantitySpin = new QSpinBox(&dialog);
    quantitySpin->setRange(1, quantityRemaining);
    quantitySpin->setValue(quantityRemaining);

    auto* storageCombo = new QComboBox(&dialog);

    StorageLocationRepository storageRepository;

    const QList<StorageLocation> locations =
        storageRepository.getByWorkspace(build->workspaceId());

    for (const StorageLocation& location : locations) {
        if (storageRepository.hasChildren(location.id()))
            continue;

        QStringList pathParts;
        int currentId = location.id();
        int safetyCount = 0;

        while (currentId > 0 && safetyCount < 100) {
            const std::optional<StorageLocation> current =
                storageRepository.getById(currentId);

            if (!current)
                break;

            pathParts.prepend(current->name());
            currentId = current->parentLocationId();
            ++safetyCount;
        }

        const QString path =
            pathParts.isEmpty() ? location.name()
                                : pathParts.join(" / ");

        storageCombo->addItem(path, location.id());
    }

    if (storageCombo->count() == 0) {
        QMessageBox::warning(this,
                             "Store Spare",
                             "No active leaf storage locations are available.");
        return;
    }

    layout->addRow("Part #:", partLabel);
    layout->addRow("Name:", nameLabel);
    layout->addRow("Color:", colorLabel);
    layout->addRow("Available to Store:",
                   new QLabel(QString::number(quantityRemaining), &dialog));
    layout->addRow("Quantity:", quantitySpin);
    layout->addRow("Storage:", storageCombo);

    auto* buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                             &dialog);

    layout->addRow(buttonBox);

    connect(buttonBox,
            &QDialogButtonBox::accepted,
            &dialog,
            &QDialog::accept);

    connect(buttonBox,
            &QDialogButtonBox::rejected,
            &dialog,
            &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const int quantity = quantitySpin->value();
    const int storageLocationId = storageCombo->currentData().toInt();

    if (quantity <= 0 || storageLocationId <= 0)
        return;

    QSqlDatabase database = DatabaseManager::instance().database();

    if (!database.transaction()) {
        QMessageBox::critical(this,
                              "Store Spare",
                              "Unable to start the spare-storage transaction.");
        return;
    }

    const std::optional<Build> currentBuild =
        buildRepository.getById(m_selectedBuildId);

    std::optional<BuildRequirement> currentRequirement =
        requirementRepository.getById(requirementId);

    const std::optional<StorageLocation> destination =
        storageRepository.getById(storageLocationId);

    if (!currentBuild
        || !currentRequirement
        || !destination
        || currentBuild->inventoryMode() != "CompleteSet"
        || currentBuild->status() != "Complete"
        || !currentRequirement->isSpare()
        || currentRequirement->buildId() != currentBuild->id()
        || !destination->isActive()
        || destination->workspaceId() != currentBuild->workspaceId()) {
        database.rollback();

        QMessageBox::critical(this,
                              "Store Spare",
                              "The Complete Set, spare requirement, or storage "
                              "destination changed. No changes were saved.");
        return;
    }

    const int currentRemaining =
        qMax(currentRequirement->quantityRequired()
                 - currentRequirement->quantityReleased(),
             0);

    if (quantity > currentRemaining) {
        database.rollback();

        QMessageBox::warning(this,
                             "Store Spare",
                             QString("Only %1 spare piece(s) remain available "
                                     "to store. No changes were saved.")
                                 .arg(currentRemaining));
        return;
    }

    InventoryRecord record;

    record.setWorkspaceId(currentBuild->workspaceId());
    record.setPartId(currentRequirement->partId());
    record.setColorId(currentRequirement->colorId());
    record.setStorageLocationId(storageLocationId);
    record.setManufacturerId(currentBuild->manufacturerId());
    record.setCondition("New");
    record.setOwnershipType("Owned");
    record.setQuantity(quantity);

    const QString notes =
        QString("Stored boxed spare from Complete Set %1%2.")
            .arg(currentBuild->name())
            .arg(currentBuild->setNumber().trimmed().isEmpty()
                     ? QString()
                     : QString(" (%1)").arg(currentBuild->setNumber()));

    InventoryRecordRepository inventoryRepository;

    if (!inventoryRepository.addOrIncreaseQuantity(record,
                                                   "SetSpareRelease",
                                                   "Build",
                                                   QString::number(currentBuild->id()),
                                                   notes,
                                                   false)) {
        database.rollback();

        QMessageBox::critical(this,
                              "Store Spare",
                              "Unable to add the spare part to My Loose Inventory. "
                              "No changes were saved.");
        return;
    }

    currentRequirement->setQuantityReleased(
        currentRequirement->quantityReleased() + quantity);

    if (!requirementRepository.update(*currentRequirement)) {
        database.rollback();

        QMessageBox::critical(this,
                              "Store Spare",
                              "Unable to record the stored spare quantity. "
                              "No changes were saved.");
        return;
    }

    if (!database.commit()) {
        database.rollback();

        QMessageBox::critical(this,
                              "Store Spare",
                              "Unable to commit the spare-storage transaction. "
                              "No changes were saved.");
        return;
    }

    qInfo() << "Complete Set spare stored."
            << "BuildId:" << currentBuild->id()
            << "RequirementId:" << requirementId
            << "PartId:" << currentRequirement->partId()
            << "ColorId:" << currentRequirement->colorId()
            << "ManufacturerId:" << currentBuild->manufacturerId()
            << "Quantity:" << quantity
            << "StorageLocationId:" << storageLocationId
            << "QuantityReleased:" << currentRequirement->quantityReleased();

    QMessageBox::information(
        this,
        "Store Spare",
        QString("%1 spare piece(s) stored in My Loose Inventory.")
            .arg(quantity));
}

void BuildsWidget::exportPullList()
{
    if (m_selectedBuildId <= 0) {
        QMessageBox::warning(this, "Export Pull List", "Select a Build first.");
        return;
    }

    BuildRepository buildRepository;

    const std::optional<Build> build =
        buildRepository.getById(m_selectedBuildId);

    if (!build) {
        QMessageBox::critical(this, "Export Pull List", "Unable to load the selected Build.");
        return;
    }

    if (build->inventoryMode() != "Stock") {
        QMessageBox::information(this,
                                 "Export Pull List",
                                 "Pull Lists are available only for "
                                 "Build from Stock.");
        return;
    }

    BuildAllocationRepository allocationRepository;

    const QList<BuildAllocation> allocations =
        allocationRepository.getByBuild(m_selectedBuildId);

    if (allocations.isEmpty()) {
        QMessageBox::information(this,
                                 "Export Pull List",
                                 "This Build does not have any allocated "
                                 "inventory to export.");
        return;
    }

    QString defaultName;

    if (!build->setNumber().trimmed().isEmpty()) {
        defaultName =
            QString("BrickSuite_Pull_%1.csv").arg(build->setNumber().trimmed());
    } else {
        QString safeName = build->name().trimmed();

        safeName.replace(QRegularExpression(R"([^A-Za-z0-9_-]+)"), "_");

        defaultName = QString("BrickSuite_Pull_%1.csv").arg(safeName);
    }

    const QString fileName =
        QFileDialog::getSaveFileName(this,
                                     "Export Pull List CSV",
                                     defaultName,
                                     "CSV Files (*.csv)");

    if (fileName.isEmpty())
        return;

    QFile file(fileName);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this,
                              "Export Pull List",
                              QString("Unable to create:\n\n%1").arg(fileName));
        return;
    }

    QTextStream stream(&file);

    stream << QChar(0xFEFF);

    auto csvValue = [](QString value) {
        value.replace("\"", "\"\"");
        return QString("\"%1\"").arg(value);
    };

    stream << "Allocation ID,"
           << "Build,"
           << "Set Number,"
           << "Part Number,"
           << "Part Name,"
           << "Color,"
           << "Storage Location,"
           << "Quantity Allocated,"
           << "Quantity Pulled"
           << "\n";

    PartRepository partRepository;
    ColorRepository colorRepository;
    StorageLocationRepository storageRepository;

    int rowsWritten = 0;

    for (const BuildAllocation& allocation : allocations) {
        //
        // Allocations carry the effective physical Part / Color identity,
        // so the pull list intentionally exports allocation identity rather
        // than the canonical requirement identity.
        //
        const std::optional<Part> part =
            partRepository.getById(allocation.partId());

        const std::optional<Color> color =
            colorRepository.getById(allocation.colorId());

        QStringList pathParts;

        int currentLocationId = allocation.storageLocationId();
        int safetyCount = 0;

        while (currentLocationId > 0 && safetyCount < 100) {
            const std::optional<StorageLocation> location =
                storageRepository.getById(currentLocationId);

            if (!location)
                break;

            pathParts.prepend(location->name());

            currentLocationId = location->parentLocationId();
            ++safetyCount;
        }

        QString storagePath = pathParts.join(" / ");

        if (storagePath.isEmpty()) {
            storagePath =
                QString("Location %1").arg(allocation.storageLocationId());
        }

        const QString partNumber =
            part ? part->partNumber()
                 : QString::number(allocation.partId());

        const QString partName =
            part ? part->name() : QString();

        const QString colorName =
            color ? color->name()
                  : QString::number(allocation.colorId());

        stream << allocation.id() << ","
               << csvValue(build->name()) << ","
               << csvValue(build->setNumber()) << ","
               << csvValue(partNumber) << ","
               << csvValue(partName) << ","
               << csvValue(colorName) << ","
               << csvValue(storagePath) << ","
               << allocation.quantityAllocated() << ","
               << ""
               << "\n";

        ++rowsWritten;
    }

    file.close();

    QMessageBox::information(this,
                             "Export Pull List",
                             QString("Pull List exported successfully.\n\n"
                                     "Rows: %1\n"
                                     "File:\n%2")
                                 .arg(rowsWritten)
                                 .arg(fileName));
}

void BuildsWidget::importPullList()
{
    if (m_selectedBuildId <= 0) {
        QMessageBox::warning(this, "Import Pull List", "Select a Build first.");
        return;
    }

    BuildRepository repository;

    const std::optional<Build> build =
        repository.getById(m_selectedBuildId);

    if (!build)
        return;

    if (build->inventoryMode() != "Stock") {
        QMessageBox::information(this,
                                 "Import Pull List",
                                 "Pull List reconciliation is available "
                                 "only for Build from Stock.");
        return;
    }

    const QString fileName =
        QFileDialog::getOpenFileName(this,
                                     "Import Pull List CSV",
                                     QString(),
                                     "CSV Files (*.csv)");

    if (fileName.isEmpty())
        return;

    ImportPullListDialog dialog(m_selectedBuildId, fileName, this);

    if (dialog.exec() == QDialog::Accepted)
        loadRequirements();
}

void BuildsWidget::interactivePulling()
{
    if (m_selectedBuildId <= 0) {
        QMessageBox::warning(this,
                             "Interactive Build Pulling",
                             "Select a Build first.");
        return;
    }

    BuildRepository repository;
    const std::optional<Build> build = repository.getById(m_selectedBuildId);

    if (!build)
        return;

    if (build->inventoryMode() != "Stock") {
        QMessageBox::information(this,
                                 "Interactive Build Pulling",
                                 "Interactive pulling is available only for Build from Stock.");
        return;
    }

    InteractiveBuildPullingDialog dialog(m_selectedBuildId, this);
    dialog.exec();

    loadRequirements();
}

void BuildsWidget::selectBuild(int buildId)
{
    if (buildId <= 0)
        return;

    loadBuilds();

    for (int row = 0; row < m_buildsTable->rowCount(); ++row) {
        QTableWidgetItem* nameItem = m_buildsTable->item(row, 4);

        if (!nameItem)
            continue;

        const int rowBuildId =
            nameItem->data(Qt::UserRole).toInt();

        if (rowBuildId != buildId)
            continue;

        m_buildsTable->setCurrentCell(row, 3);
        m_buildsTable->selectRow(row);
        m_buildsTable->scrollToItem(nameItem,
                                    QAbstractItemView::PositionAtCenter);
        return;
    }
}

void BuildsWidget::procureMissingParts()
{
    if (m_selectedBuildId <= 0) {
        QMessageBox::warning(this,
                             "Procure Missing Parts",
                             "Select a Build first.");
        return;
    }

    ProcurementDraftService service;

    const ProcurementDraftService::Result result =
        service.createBrickLinkDraft(
            m_workspaceContext.currentWorkspaceId(),
            m_selectedBuildId);

    if (!result.success) {
        QMessageBox::critical(this,
                              "Procure Missing Parts",
                              result.message);
        return;
    }

    if (result.draft.items.isEmpty()) {
        QMessageBox::information(this,
                                 "Procure Missing Parts",
                                 result.message);
        return;
    }

    ProcurementPreviewDialog dialog(result.draft, this);
    dialog.exec();
}

void BuildsWidget::exportMissingParts()
{
    if (m_selectedBuildId <= 0) {
        QMessageBox::warning(this, "Export Missing Parts", "Select a Build first.");
        return;
    }

    BuildRepository buildRepository;

    const std::optional<Build> build =
        buildRepository.getById(m_selectedBuildId);

    if (!build) {
        QMessageBox::critical(this,
                              "Export Missing Parts",
                              "Unable to load the selected Build.");
        return;
    }

    if (build->inventoryMode() != "Stock") {
        QMessageBox::information(this,
                                 "Export Missing Parts",
                                 "Missing Parts Lists are available only "
                                 "for Build from Stock.");
        return;
    }

    MissingPartsService service;

    const QList<MissingPartsService::MissingPart> missingParts =
        service.getMissingParts(
            m_workspaceContext.currentWorkspaceId(),
            m_selectedBuildId);

    if (missingParts.isEmpty()) {
        QMessageBox::information(this,
                                 "Export Missing Parts",
                                 "This Build currently has no missing "
                                 "non-spare parts.");
        return;
    }

    QString defaultName;

    if (!build->setNumber().trimmed().isEmpty()) {
        defaultName =
            QString("BrickSuite_Missing_%1.csv")
                .arg(build->setNumber().trimmed());
    } else {
        QString safeName = build->name().trimmed();

        safeName.replace(QRegularExpression(R"([^A-Za-z0-9_-]+)"), "_");

        defaultName =
            QString("BrickSuite_Missing_%1.csv").arg(safeName);
    }

    const QString fileName =
        QFileDialog::getSaveFileName(this,
                                     "Export Missing Parts CSV",
                                     defaultName,
                                     "CSV Files (*.csv)");

    if (fileName.isEmpty())
        return;

    QFile file(fileName);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this,
                              "Export Missing Parts",
                              QString("Unable to create:\n\n%1").arg(fileName));
        return;
    }

    QTextStream stream(&file);

    stream << QChar(0xFEFF);

    auto csvValue = [](QString value) {
        value.replace("\"", "\"\"");
        return QString("\"%1\"").arg(value);
    };

    stream << "Build,"
           << "Set Number,"
           << "Part Number,"
           << "Part Name,"
           << "Color,"
           << "Required,"
           << "Pulled,"
           << "Remaining,"
           << "Available,"
           << "Missing"
           << "\n";

    int totalPiecesMissing = 0;

    for (const MissingPartsService::MissingPart& item : missingParts) {
        stream << csvValue(build->name()) << ","
               << csvValue(build->setNumber()) << ","
               << csvValue(item.partNumber) << ","
               << csvValue(item.partName) << ","
               << csvValue(item.colorName) << ","
               << item.required << ","
               << item.pulled << ","
               << item.remaining << ","
               << item.available << ","
               << item.missing << "\n";

        totalPiecesMissing += item.missing;
    }

    file.close();

    QMessageBox::information(this,
                             "Export Missing Parts",
                             QString("Missing Parts List exported successfully.\n\n"
                                     "Part/Color Rows: %1\n"
                                     "Pieces Missing: %2\n"
                                     "File:\n%3")
                                 .arg(missingParts.size())
                                 .arg(totalPiecesMissing)
                                 .arg(fileName));
}


void BuildsWidget::importMocPartsCsv()
{
    if (m_selectedBuildId <= 0) {
        QMessageBox::warning(this, "Import MOC Parts", "Select a MOC Build first.");
        return;
    }

    BuildRepository buildRepository;

    const std::optional<Build> build =
        buildRepository.getById(m_selectedBuildId);

    if (!build) {
        QMessageBox::critical(this,
                              "Import MOC Parts",
                              "Unable to load the selected Build.");
        return;
    }

    if (build->buildType() != "MOC") {
        QMessageBox::information(this,
                                 "Import MOC Parts",
                                 "MOC parts import is available only "
                                 "for MOC Builds.");
        return;
    }

    if (build->inventoryMode() != "Stock") {
        QMessageBox::information(this,
                                 "Import MOC Parts",
                                 "MOC requirements use the "
                                 "Build from Stock inventory mode.");
        return;
    }

    const QString fileName =
        QFileDialog::getOpenFileName(this,
                                     "Import Rebrickable MOC Parts CSV",
                                     QString(),
                                     "CSV Files (*.csv)");

    if (fileName.isEmpty())
        return;

    BuildRequirementRepository requirementRepository;

    const QList<BuildRequirement> existing =
        requirementRepository.getByBuild(m_selectedBuildId);

    bool replaceExisting = false;

    if (!existing.isEmpty()) {
        const QMessageBox::StandardButton response =
            QMessageBox::warning(this,
                                 "Import MOC Parts",
                                 QString("This MOC already has %1 Build "
                                         "Requirement row(s).\n\n"
                                         "Replace the existing requirements "
                                         "with the selected Rebrickable MOC CSV?")
                                     .arg(existing.size()),
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No);

        if (response != QMessageBox::Yes)
            return;

        replaceExisting = true;
    }

    RebrickableMocCsvImporter importer;

    const RebrickableMocCsvImporter::Result result =
        importer.importFile(m_selectedBuildId,
                            fileName,
                            replaceExisting);

    if (!result.success) {
        qWarning() << "MOC requirements import failed."
                   << "BuildId:" << m_selectedBuildId
                   << "File:" << fileName
                   << "Reason:" << result.message;

        QMessageBox::critical(this,
                              "Import MOC Parts",
                              result.message);
        return;
    }

    bool buildMetadataUpdated = false;

    const MocFileMetadata metadata =
        parseRebrickableMocFileName(fileName);

    if (metadata.recognized) {
        BuildRepository repository;

        std::optional<Build> updatedBuild =
            repository.getById(m_selectedBuildId);

        if (updatedBuild) {
            bool changed = false;

            if (!metadata.mocNumber.isEmpty()
                && updatedBuild->setNumber() != metadata.mocNumber) {
                updatedBuild->setSetNumber(metadata.mocNumber);
                changed = true;
            }

            if (!metadata.sourceSetNumber.isEmpty()) {
                QString alternateNote =
                    QString("Alternate build from Set %1")
                        .arg(metadata.sourceSetNumber);

                SetCatalogRepository setCatalogRepository;

                const std::optional<SetCatalogItem> sourceSet =
                    setCatalogRepository.getBySetNumber(
                        metadata.sourceSetNumber);

                if (sourceSet
                    && !sourceSet->name().trimmed().isEmpty()) {
                    alternateNote +=
                        QString(" — %1")
                            .arg(sourceSet->name().trimmed());
                }

                if (updatedBuild->notes().trimmed().isEmpty()) {
                    updatedBuild->setNotes(alternateNote);
                    changed = true;
                } else if (!updatedBuild->notes().contains(
                               alternateNote,
                               Qt::CaseInsensitive)) {
                    updatedBuild->setNotes(
                        updatedBuild->notes().trimmed()
                        + "\n"
                        + alternateNote);

                    changed = true;
                }
            }

            if (changed) {
                if (!repository.update(*updatedBuild)) {
                    qWarning()
                        << "MOC requirements imported, but filename metadata update failed."
                        << "BuildId:" << m_selectedBuildId
                        << "File:" << fileName;

                    QMessageBox::warning(
                        this,
                        "Import MOC Parts",
                        "The MOC requirements were imported, "
                        "but BrickSuite was unable to save the "
                        "metadata parsed from the CSV filename.");
                } else {
                    buildMetadataUpdated = true;
                }
            }
        }
    }

    if (buildMetadataUpdated) {
        selectBuild(m_selectedBuildId);
    } else {
        loadRequirements();
    }

    qInfo() << "MOC requirements imported."
            << "BuildId:" << m_selectedBuildId
            << "File:" << fileName
            << "CsvRows:" << result.rowsRead
            << "RequirementRows:" << result.requirementsCreated
            << "RegularPieces:" << result.regularPieces
            << "SparePieces:" << result.sparePieces
            << "MetadataUpdated:" << buildMetadataUpdated;

    QMessageBox::information(this,
                             "Import MOC Parts",
                             QString("MOC requirements imported successfully.\n\n"
                                     "CSV Rows Read: %1\n"
                                     "Requirement Rows: %2\n"
                                     "Regular Pieces: %3\n"
                                     "Spare Pieces: %4\n"
                                     "Total Pieces: %5")
                                 .arg(result.rowsRead)
                                 .arg(result.requirementsCreated)
                                 .arg(result.regularPieces)
                                 .arg(result.sparePieces)
                                 .arg(result.regularPieces
                                      + result.sparePieces));
}
