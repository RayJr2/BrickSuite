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
#include "BuildActionEligibility.h"

#include "../../settings/UserSettings.h"

#include "../../app/WorkspaceContext.h"
#include "../../database/DatabaseManager.h"
#include "AllocateBuildRequirementDialog.h"
#include "DisassembleSetDialog.h"
#include "../collection/CatalogCollectionDialog.h"
#include "../collection/RemoteCollectionMutationDialog.h"
#include "../../repositories/CollectionRepository.h"
#include "../../services/collection/CollectionItemService.h"
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
#include "../../services/builds/BuildMutationService.h"
#include "../../services/builds/BuildRequirementMutationService.h"
#include "../../services/builds/BuildLifecycleService.h"
#include "../../services/builds/BuildAllocationMutationService.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/ManufacturerRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/SetCatalogRepository.h"
#include "../../repositories/StorageLocationRepository.h"

#include "../../import/RebrickableMocCsvImporter.h"
#include "../../services/builds/MissingPartsService.h"
#include "../../services/builds/BuildRequirementAvailabilityService.h"
#include "../../services/images/PartImageService.h"
#include "../../services/parts/PartExternalIdEnrichmentService.h"
#include "../../services/procurement/ProcurementDraftService.h"
#include "../../services/storage/SessionStorageSelectionService.h"
#include "../../services/application/ApplicationServices.h"
#include "../../services/application/RemoteReadApplicationServices.h"
#include "../../services/application/RemotePullingApplicationService.h"
#include "../../services/application/RemoteCollectionMutationApplicationService.h"
#include "../../services/application/RemoteBuildMutationApplicationService.h"
#include "../../services/application/dto/RemoteBuildMutationDtos.h"
#include "../../ui/procurement/ProcurementPreviewDialog.h"
#include "../helpers/PartSearchCompleterHelper.h"

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
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPalette>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>
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
#include <utility>
#include <memory>

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

RemoteBuildMutationDto::ExpectedState expectedState(
    const RemoteReadDto::BuildSummary& build)
{
    RemoteBuildMutationDto::ExpectedState value;
    value.modifiedUtc = build.modifiedUtc.toUTC().toString(Qt::ISODateWithMs);
    value.buildType = build.buildType;
    value.reference = build.buildType == QStringLiteral("Minifig")
        ? build.minifigNumber : build.setNumber;
    value.inventoryMode = build.inventoryMode;
    value.manufacturer = build.manufacturerDisplay;
    value.name = build.name;
    value.status = build.status;
    value.notes = build.notes;
    value.active = build.active;
    return value;
}

RemoteBuildMutationDto::RequirementExpectedState expectedRequirementState(
    const RemoteReadDto::BuildRequirement& r)
{
    RemoteBuildMutationDto::RequirementExpectedState value;
    value.requirementId=r.requirementId;value.buildId=r.buildId;
    value.modifiedUtc=r.modifiedUtc.toUTC().toString(Qt::ISODateWithMs);
    value.partNumber=r.partNumber;value.rebrickableColorId=r.rebrickableColorId;
    value.substitutePartNumber=r.substitutePartNumber;
    value.substituteRebrickableColorId=r.substituteRebrickableColorId;
    value.quantityRequired=r.quantityRequired;value.quantityPulled=r.quantityPulled;
    value.quantityReleased=r.quantityReleased;value.spare=r.spare;
    return value;
}

} // namespace

BuildsWidget::BuildsWidget(
    WorkspaceContext& workspaceContext,
    SessionStorageSelectionService& sessionStorageSelectionService,
    BuildApplicationService& buildService,
    QWidget* parent,
    RemoteReadApplicationServices* remoteReads,
    PartExternalIdEnrichmentService* enrichmentService,
    RemotePullingApplicationService* remotePulling,
    RemoteCollectionMutationApplicationService* remoteCollection,
    RemoteBuildMutationApplicationService* remoteBuildMutations)
    : QWidget(parent)
    , m_workspaceContext(workspaceContext)
    , m_sessionStorageSelectionService(sessionStorageSelectionService)
    , m_buildService(buildService)
    , m_remoteReads(remoteReads)
    , m_enrichmentService(enrichmentService)
    , m_remotePulling(remotePulling)
    , m_remoteCollection(remoteCollection)
    , m_remoteBuildMutations(remoteBuildMutations)
    , m_remoteMode(remoteReads != nullptr)
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
    PartSearchCompleterHelper::install(
        m_partNumberEdit, [this]() { updateRequirementUiState(); });

    m_colorCombo = new QComboBox(requirementsGroup);
    connect(m_colorCombo, &QComboBox::currentIndexChanged,
            this, &BuildsWidget::updateRequirementUiState);

    m_quantitySpin = new QSpinBox(requirementsGroup);
    m_quantitySpin->setRange(1, 99999);
    m_quantitySpin->setValue(1);
    connect(m_quantitySpin, &QSpinBox::valueChanged,
            this, &BuildsWidget::updateRequirementUiState);

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

                if (!completeSet) {
                    const int legoIndex = m_remoteMode
                        ? m_manufacturerCombo->findText(QStringLiteral("LEGO"), Qt::MatchFixedString)
                        : m_manufacturerCombo->findData(ManufacturerRepository().legoManufacturerId());

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

        if (dialog.exec() == QDialog::Accepted) {
            loadRequirements();
            emit hostBuildRequirementsMutationCommitted(
                m_workspaceContext.currentWorkspaceId(), build->id(), false);
        }
    });

    loadColors();
    loadManufacturers();

    workspaceChanged(m_workspaceContext.currentWorkspaceId());

    updateRequirementUiState();

}

void BuildsWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if (m_remoteMode) {
        loadBuilds();
        return;
    }

    if (m_workspaceContext.hasCurrentWorkspace() && m_selectedBuildId > 0) {
        loadRequirements();
        updateRequirementUiState();
    }
}

void BuildsWidget::workspaceChanged(int workspaceId)
{
    Q_UNUSED(workspaceId);

    if (m_remoteMode) {
        m_pendingRemoteAddRequest.reset();
        m_pendingRemoteRequirementAdd.reset();
        m_pendingRemoteAllocateAvailable.reset();
        m_addButton->setText(QStringLiteral("Add Build"));
        for (QDialog* dialog : findChildren<QDialog*>()) {
            if (dialog->objectName().startsWith(QStringLiteral("remoteBuild")))
                dialog->close();
        }
    }

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

void BuildsWidget::refreshRemoteBuildsPreservingSelection()
{
    if (!m_remoteMode) {
        refresh();
        return;
    }
    m_restoreSelectedBuildId = m_selectedBuildId;
    m_restoreNewBuildExpanded = m_newBuildGroup->isChecked();
    loadRemoteBuilds();
}

void BuildsWidget::refreshRemoteRequirements()
{
    if (!m_remoteMode) {
        loadRequirements();
        return;
    }
    if (m_selectedBuildId <= 0) {
        emit remoteRequirementsRefreshFinished(true);
        return;
    }
    m_remoteRequirements.clear();
    loadRemoteRequirements();
}

void BuildsWidget::refreshOpenRemotePulling(const std::optional<qint64>& buildId)
{
    if (buildId && *buildId > 0) {
        if (m_remotePullingDialogs.find(int(*buildId)))
            emit remotePullingRefreshRequested(int(*buildId));
        return;
    }
    const QString prefix = QStringLiteral("remoteBuildPullingDialog_");
    for (QDialog* dialog : findChildren<QDialog*>()) {
        if (!dialog->objectName().startsWith(prefix)) continue;
        bool valid = false;
        const int id = dialog->objectName().mid(prefix.size()).toInt(&valid);
        if (valid) emit remotePullingRefreshRequested(id);
    }
}

bool BuildsWidget::hasOpenRemotePulling(const std::optional<qint64>& buildId) const
{
    if (buildId && *buildId > 0)
        return m_remotePullingDialogs.find(int(*buildId)) != nullptr;
    for (QDialog* dialog : findChildren<QDialog*>())
        if (dialog->objectName().startsWith(QStringLiteral("remoteBuildPullingDialog_")))
            return true;
    return false;
}

QList<int> BuildsWidget::openRemotePullingBuildIds() const
{
    QList<int> result;
    const QString prefix = QStringLiteral("remoteBuildPullingDialog_");
    for (QDialog* dialog : findChildren<QDialog*>()) {
        if (!dialog->objectName().startsWith(prefix)) continue;
        bool valid = false;
        const int id = dialog->objectName().mid(prefix.size()).toInt(&valid);
        if (valid) result.append(id);
    }
    return result;
}

int BuildsWidget::selectedBuildId() const
{
    return m_selectedBuildId;
}

void BuildsWidget::setRemoteSessionConnected(bool connected)
{
    if (!m_remoteMode) return;
    m_remoteSessionConnected = connected;
    for (QDialog* dialog : findChildren<QDialog*>()) {
        if (!connected && dialog->objectName().startsWith(QStringLiteral("remoteBuildWorkflow_"))) {
            dialog->close();
            continue;
        }
        if (!dialog->objectName().startsWith(QStringLiteral("remoteBuildPullingDialog_"))) continue;
        if (QLabel* status = dialog->findChild<QLabel*>(QStringLiteral("remotePullingStatus"))) {
            if (!connected)
                status->setText(QStringLiteral("Host disconnected; displayed Pulling state may be stale."));
        }
    }
    if (!connected) {
        ++m_buildListGeneration;
        ++m_requirementGeneration;
        m_statusLabel->setText(m_buildsTable->rowCount() > 0
            ? QStringLiteral("Host disconnected; displayed Builds may be stale.")
            : QStringLiteral("BrickSuite Host Builds are unavailable."));
        m_requirementsLabel->setText(m_requirementsTable->rowCount() > 0
            ? QStringLiteral("Host disconnected; displayed requirements may be stale.")
            : QStringLiteral("Build requirements are unavailable from this Host."));
    }
    m_showArchivedBuildsCheck->setEnabled(connected);
    if (connected)
        loadRemoteManufacturerChoices();
    else
        ++m_manufacturerGeneration;
    updateUiState();
    updateRequirementUiState();
    if (!m_remoteRequirements.isEmpty())
        renderRemoteRequirements();
}

void BuildsWidget::reloadManufacturers()
{
    if (m_remoteMode) {
        loadRemoteManufacturerChoices();
        return;
    }
    const int selectedId = m_manufacturerCombo->currentData().toInt();
    loadManufacturers();
    const int index = m_manufacturerCombo->findData(selectedId);
    if (index >= 0)
        m_manufacturerCombo->setCurrentIndex(index);
}


void BuildsWidget::loadBuilds()
{
    if (m_remoteMode) {
        loadRemoteBuilds();
        return;
    }

    const ApplicationServiceStatus serviceStatus = m_buildService.status();
    if (!serviceStatus.isAvailable()) {
        m_selectedBuildId = 0;
        m_buildsTable->setRowCount(0);
        m_requirementsTable->setRowCount(0);
        m_statusLabel->setText(serviceStatus.message);
        updateUiState();
        updateRequirementUiState();
        return;
    }
    m_buildsTable->setRowCount(0);

    if (!m_workspaceContext.hasCurrentWorkspace()) {
        m_statusLabel->setText("Select a workspace to view builds.");
        return;
    }

    ManufacturerRepository manufacturerRepository;

    const QList<Build> builds = m_buildService.list(
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

        const auto linkedCollection = CollectionRepository().getBySourceBuild(build.id());
        if (linkedCollection) {
            actionCombo->addItem("View Collection Item...", "view_collection");
        } else if (build.isActive() && build.status() == "Complete"
                   && ((build.buildType() == "Set" && build.setCatalogId() > 0)
                       || (build.buildType() == "Minifig" && build.minifigCatalogId() > 0)
                       || (build.buildType() == "MOC" && build.setCatalogId() == 0
                           && build.minifigCatalogId() == 0))) {
            actionCombo->addItem("Add to Collection...", "add_collection");
        }
        if (build.buildType() == "Set" && build.setCatalogId() <= 0
            && !build.setNumber().trimmed().isEmpty()) {
            actionCombo->addItem("Link to Sets Catalog...", "link_set_catalog");
        }

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

                    if (action == "view_collection") {
                        const auto linked = CollectionRepository().getBySourceBuild(buildId);
                        if (linked) emit collectionItemRequested(linked->id);
                        return;
                    }

                    if (action == "add_collection") {
                        const auto build = BuildRepository().getById(buildId);
                        if (!build) return;
                        CollectionItemType type = collectionItemTypeFromString(build->buildType());
                        CatalogCollectionDialog dialog(build->workspaceId(), type,
                            type == CollectionItemType::Set ? build->setCatalogId() : build->minifigCatalogId(),
                            build->sourceReference(), build->name(), this, buildId);
                        if (dialog.exec() == QDialog::Accepted && dialog.createdCollectionItemId() > 0) {
                            emit collectionItemRequested(dialog.createdCollectionItemId());
                            loadBuilds();
                        }
                        return;
                    }

                    if (action == "link_set_catalog") {
                        CollectionItemService service;
                        const auto preview = service.previewLegacySetBuildLink(buildId);
                        if (!preview.result.success) {
                            QMessageBox::information(this, "Link to Sets Catalog",
                                                     preview.result.message);
                            return;
                        }
                        const QString message = QString(
                            "Link this historical Build to the exact Sets Catalog match?\n\n"
                            "Build: %1 (%2)\n"
                            "Sets Catalog: %3 (%4)\n\n"
                            "Only the Build's authoritative catalog relationship will be saved. "
                            "Its name, reference, requirements, inventory mode, status, and "
                            "inventory history will not change.")
                            .arg(preview.buildName, preview.buildReference,
                                 preview.catalogName, preview.catalogReference);
                        if (QMessageBox::question(this, "Link to Sets Catalog", message,
                                                  QMessageBox::Yes | QMessageBox::No,
                                                  QMessageBox::No) != QMessageBox::Yes)
                            return;
                        const auto result = service.linkLegacySetBuild(buildId,
                                                                       preview.setCatalogId);
                        if (!result.success)
                            QMessageBox::critical(this, "Link to Sets Catalog", result.message);
                        else {
                            QMessageBox::information(this, "Link to Sets Catalog",
                                                     result.message);
                            selectBuild(buildId);
                            emit hostBuildMetadataMutationCommitted(
                                m_workspaceContext.currentWorkspaceId(), buildId);
                        }
                        return;
                    }

                    if (action == "edit") {
                        EditBuildDialog dialog(buildId, this);

                        if (dialog.exec() == QDialog::Accepted) {
                            selectBuild(buildId);
                            emit hostBuildMetadataMutationCommitted(
                                m_workspaceContext.currentWorkspaceId(), buildId);
                        }

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

                        QList<DisassembleSetDialog::ReturnSelection> cancellationSelections;
                        int cancellationCollectionState = 0;
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
                            DisassembleSetDialog dialog(buildId,
                                                        m_sessionStorageSelectionService,
                                                        this,
                                                        true);

                            if (dialog.exec() != QDialog::Accepted)
                                return;
                            cancellationSelections = dialog.returnSelections();
                            cancellationCollectionState = dialog.linkedCollectionState();
                        }

                        QList<BuildLifecycleService::DisassemblyReturn> returns;
                        CollectionItemState collectionState = CollectionItemState::Unassembled;
                        if (totalPulled > 0) {
                            for (const auto& selection : cancellationSelections) {
                                returns.append({selection.requirementId,
                                                selection.partId,
                                                selection.colorId,
                                                selection.manufacturerId,
                                                selection.storageLocationId,
                                                selection.quantity,
                                                selection.spare});
                            }
                            if (cancellationCollectionState > 0) {
                                collectionState = static_cast<CollectionItemState>(
                                    cancellationCollectionState);
                            }
                        }
                        const auto cancelled = BuildLifecycleService().cancel(
                            buildId, returns, collectionState);
                        if (!cancelled.success) {
                            QMessageBox::critical(this, "Cancel Build", cancelled.message);
                            return;
                        }
                        build = cancelled.build;

                        qInfo() << "Build cancelled."
                                << "BuildId:" << buildId
                                << "Name:" << build->name()
                                << "PreviouslyPulledPieces:" << totalPulled;

                        selectBuild(buildId);
                        emit hostBuildRequirementsMutationCommitted(
                            m_workspaceContext.currentWorkspaceId(), buildId, totalPulled > 0);

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

                        const auto archived = BuildMutationService().setActive(buildId, false);
                        if (!archived.success) {
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
                        emit hostBuildMetadataMutationCommitted(
                            m_workspaceContext.currentWorkspaceId(), buildId);

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

                        const auto reactivated = BuildMutationService().setActive(buildId, true);
                        if (!reactivated.success) {
                            QMessageBox::critical(
                                this,
                                "Reactivate Build",
                                reactivated.message);
                            return;
                        }
                        build = reactivated.build;

                        qInfo() << "Build reactivated."
                                << "BuildId:" << buildId
                                << "Name:" << build->name()
                                << "Status:"
                                << (build->status() == "Cancelled" ? "Planned"
                                                                  : build->status());

                        selectBuild(buildId);
                        emit hostBuildMetadataMutationCommitted(
                            m_workspaceContext.currentWorkspaceId(), buildId);

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

                            const auto completed = BuildMutationService().complete(buildId);

                            if (!completed.success) {
                                qCritical() << "Unable to mark Complete Set Complete."
                                            << "BuildId:" << buildId;

                                QMessageBox::critical(this,
                                                      "Complete Set",
                                                      completed.message);
                                return;
                            }
                            build = completed.build;

                            qInfo() << "Complete Set completed."
                                    << "BuildId:" << buildId
                                    << "Name:" << build->name()
                                    << "RegularPieces:" << regularPieces
                                    << "SparePieces:" << sparePieces;

                            selectBuild(buildId);
                            emit hostBuildMetadataMutationCommitted(
                                m_workspaceContext.currentWorkspaceId(), buildId);

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

                        const auto completed = BuildMutationService().complete(buildId);

                        if (!completed.success) {
                            qCritical() << "Unable to mark Build Complete."
                                        << "BuildId:" << buildId;

                            QMessageBox::critical(this,
                                                  "Complete Build",
                                                  completed.message);
                            return;
                        }
                        build = completed.build;

                        qInfo() << "Build completed."
                                << "BuildId:" << buildId
                                << "Name:" << build->name()
                                << "RegularPieces:" << regularPulled
                                << "SparePiecesPulled:" << sparePulled;

                        selectBuild(buildId);
                        emit hostBuildMetadataMutationCommitted(
                            m_workspaceContext.currentWorkspaceId(), buildId);

                        QMessageBox::information(this,
                                                 "Complete Build",
                                                 QString("\"%1\" is now Complete.")
                                                     .arg(build->name()));
                        return;
                    }

                    if (action == "disassemble") {
                        DisassembleSetDialog dialog(buildId,
                                                    m_sessionStorageSelectionService, this);

                        if (dialog.exec() == QDialog::Accepted) {
                            selectBuild(buildId);
                            emit hostBuildRequirementsMutationCommitted(
                                m_workspaceContext.currentWorkspaceId(), buildId, true);
                        }

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

void BuildsWidget::loadRemoteBuilds()
{
    const quint64 generation = ++m_buildListGeneration;
    ++m_requirementGeneration;
    m_selectedBuildId = 0;
    m_remoteRequirements.clear();
    m_buildsTable->clearContents();
    m_buildsTable->setRowCount(0);
    m_requirementsTable->clearContents();
    m_requirementsTable->setRowCount(0);
    updateRequirementUiState();
    m_showArchivedBuildsCheck->setEnabled(false);

    if (!m_workspaceContext.hasCurrentWorkspace()) {
        m_showArchivedBuildsCheck->setEnabled(true);
        m_statusLabel->setText("Select a Host workspace to view Builds.");
        emit remoteBuildsRefreshFinished(false);
        return;
    }
    if (!m_remoteReads || !m_remoteReads->isAvailableFor(QStringLiteral("builds.list"))) {
        m_showArchivedBuildsCheck->setEnabled(true);
        m_statusLabel->setText("Builds are unavailable from this BrickSuite Host.");
        emit remoteBuildsRefreshFinished(false);
        return;
    }

    m_statusLabel->setText("Loading Builds from BrickSuite Host...");
    const int workspaceId = m_workspaceContext.currentWorkspaceId();
    const bool archived = m_showArchivedBuildsCheck->isChecked();
    m_remoteReads->listBuilds(workspaceId, archived, this,
        [this, generation, workspaceId](AsyncReadResult<QList<RemoteReadDto::BuildSummary>> result) {
            if (generation != m_buildListGeneration
                || workspaceId != m_workspaceContext.currentWorkspaceId())
                return;
            m_showArchivedBuildsCheck->setEnabled(true);
            if (!result.succeeded()) {
                m_statusLabel->setText(result.message.isEmpty()
                    ? QStringLiteral("Unable to load Builds from BrickSuite Host.")
                    : result.message);
                emit remoteBuildsRefreshFinished(false);
                return;
            }
            renderRemoteBuilds(*result.value);
            emit remoteBuildsRefreshFinished(true);
        });
}

void BuildsWidget::renderRemoteBuilds(const QList<RemoteReadDto::BuildSummary>& builds)
{
    m_buildsTable->setRowCount(builds.size());
    for (int row = 0; row < builds.size(); ++row) {
        const auto& build = builds.at(row);
        const QString reference = build.buildType == QStringLiteral("Minifig")
            ? build.minifigNumber : build.setNumber;
        const QString mode = build.inventoryMode == QStringLiteral("CompleteSet")
            ? QStringLiteral("Complete Set") : QStringLiteral("Build from Stock");
        auto* nameItem = new QTableWidgetItem(build.name);
        nameItem->setData(Qt::UserRole, build.buildId);
        nameItem->setData(Qt::UserRole + 1, build.inventoryMode);
        nameItem->setData(Qt::UserRole + 2, build.status);
        nameItem->setData(Qt::UserRole + 3, build.active);
        m_buildsTable->setItem(row, 0, new QTableWidgetItem(build.buildType));
        m_buildsTable->setItem(row, 1, new QTableWidgetItem(reference));
        m_buildsTable->setItem(row, 2, new QTableWidgetItem(mode));
        m_buildsTable->setItem(row, 3, new QTableWidgetItem(
            build.manufacturerDisplay.isEmpty() ? QStringLiteral("From Stock")
                                                : build.manufacturerDisplay));
        m_buildsTable->setItem(row, 4, nameItem);
        m_buildsTable->setItem(row, 5, new QTableWidgetItem(build.status));
        m_buildsTable->setItem(row, 6, new QTableWidgetItem(build.active
            ? build.notes : QString("%1%2Archived").arg(build.notes,
                build.notes.isEmpty() ? QString() : QStringLiteral(" / "))));

        auto* actions = new QComboBox(m_buildsTable);
        actions->addItem("Actions...", QString());
        if (m_remoteReads->isAvailableFor(QStringLiteral("builds.get")))
            actions->addItem("View Details...", "details");
        if (BuildActionEligibility::supportsStockFulfillment(
                build.active, build.inventoryMode, build.status)
            && m_remoteReads->isAvailableFor(QStringLiteral("builds.pulling")))
            actions->addItem("View Pulling...", "pulling");
        if (build.active && build.status == QStringLiteral("Complete") && m_remoteCollection
            && m_remoteCollection->isAvailableFor(QStringLiteral("collection.add")))
            actions->addItem(QStringLiteral("Add to Collection..."), QStringLiteral("add_collection"));
        if (m_remoteBuildMutations) {
            if (build.active && m_remoteBuildMutations->isAvailableFor(QStringLiteral("builds.edit")))
                actions->addItem(QStringLiteral("Edit Build..."), QStringLiteral("edit"));
            if (build.active && (build.status == QStringLiteral("Planned")
                                 || build.status == QStringLiteral("Pulling"))) {
                if (m_remoteBuildMutations->isAvailableFor(QStringLiteral("builds.complete")))
                    actions->addItem(build.inventoryMode == QStringLiteral("CompleteSet")
                                         ? QStringLiteral("Complete Set...")
                                         : QStringLiteral("Complete Build..."),
                                     QStringLiteral("complete"));
                if (m_remoteBuildMutations->isAvailableFor(QStringLiteral("builds.cancel")))
                    actions->addItem(QStringLiteral("Cancel Build..."), QStringLiteral("cancel"));
            }
            if (build.active && (build.status == QStringLiteral("Cancelled")
                                 || build.status == QStringLiteral("Disassembled"))
                && m_remoteBuildMutations->isAvailableFor(QStringLiteral("builds.setActive")))
                actions->addItem(QStringLiteral("Archive Build"), QStringLiteral("archive"));
            if (!build.active
                && m_remoteBuildMutations->isAvailableFor(QStringLiteral("builds.setActive")))
                actions->addItem(QStringLiteral("Reactivate Build"), QStringLiteral("reactivate"));
            if(build.active&&build.status==QStringLiteral("Complete")
                &&m_remoteBuildMutations->isAvailableFor(QStringLiteral("builds.disassemble")))
                actions->addItem(build.inventoryMode==QStringLiteral("CompleteSet")?QStringLiteral("Disassemble Set..."):QStringLiteral("Disassemble Build..."),QStringLiteral("disassemble"));
        }
        connect(actions, &QComboBox::currentIndexChanged, this,
            [this, actions, build](int index) {
                if (index <= 0) return;
                const QString action = actions->itemData(index).toString();
                actions->setCurrentIndex(0);
                if (action == "details") showRemoteDetails(int(build.buildId));
                else if (action == "pulling") showRemotePulling(int(build.buildId));
                else if (action == "add_collection") addRemoteBuildToCollection(build);
                else if (action == "edit") editRemoteBuild(build);
                else if (action == "complete") submitRemoteBuildMutation(action, build);
                else if (action == "cancel") submitRemoteBuildMutation(action, build);
                else if (action == "disassemble") submitRemoteBuildMutation(action, build);
                else if (action == "archive") submitRemoteBuildMutation(action, build, false);
                else if (action == "reactivate") submitRemoteBuildMutation(action, build, true);
            });
        m_buildsTable->setCellWidget(row, 7, actions);
    }
    m_statusLabel->setText(builds.isEmpty()
        ? QStringLiteral("No Builds were found in this Host workspace.")
        : QStringLiteral("%1 Host Build(s).").arg(builds.size()));
    if (m_restoreSelectedBuildId > 0) {
        const int wanted = m_restoreSelectedBuildId;
        const bool restoreNewBuildExpanded = m_restoreNewBuildExpanded;
        m_restoreSelectedBuildId = 0;
        for (int row = 0; row < m_buildsTable->rowCount(); ++row) {
            QTableWidgetItem* item = m_buildsTable->item(row, 4);
            if (item && item->data(Qt::UserRole).toInt() == wanted) {
                m_buildsTable->setCurrentCell(row, 4);
                m_buildsTable->selectRow(row);
                break;
            }
        }
        m_newBuildGroup->setChecked(restoreNewBuildExpanded);
    }
}

void BuildsWidget::addRemoteBuildToCollection(const RemoteReadDto::BuildSummary& build)
{
    if (!m_remoteReads || !m_remoteCollection
        || !m_remoteCollection->isAvailableFor(QStringLiteral("collection.add"))) return;
    const int workspaceId=m_workspaceContext.currentWorkspaceId();
    if(build.workspaceId!=workspaceId)return;
    m_remoteReads->listStorage(workspaceId,this,[this,workspaceId,build](AsyncReadResult<QList<RemoteReadDto::StorageSummary>> result){
        if(workspaceId!=m_workspaceContext.currentWorkspaceId()||!result.succeeded()){if(!result.succeeded())QMessageBox::warning(this,"Add to Collection",result.message);return;}
        RemoteCollectionMutationDto::Request seed;seed.workspaceId=workspaceId;seed.sourceType="Build";seed.buildId=build.buildId;
        seed.state="Assembled";seed.condition="Used";seed.completeness="Complete";
        const QString reference=build.buildType==QStringLiteral("Minifig")?build.minifigNumber:build.setNumber;
        auto*dialog=new RemoteCollectionMutationDialog("collection.add",workspaceId,*m_remoteCollection,*result.value,
            seed,reference,build.name,this);
        connect(&m_workspaceContext,&WorkspaceContext::currentWorkspaceChanged,dialog,&QDialog::reject);
        connect(dialog,&RemoteCollectionMutationDialog::mutationCompleted,this,[this](int item){emit collectionItemRequested(item);refreshRemoteBuildsPreservingSelection();});
        dialog->open();
    });
}

void BuildsWidget::editRemoteBuild(const RemoteReadDto::BuildSummary& build)
{
    if (!m_remoteBuildMutations || !m_remoteReads) return;
    const int workspaceId = m_workspaceContext.currentWorkspaceId();
    m_remoteReads->getBuild(workspaceId, build.buildId, this,
        [this, workspaceId](AsyncReadResult<RemoteReadDto::BuildDetail> result) {
            if (workspaceId != m_workspaceContext.currentWorkspaceId()) return;
            if (!result.succeeded()) {
                QMessageBox::warning(this, QStringLiteral("Edit Build"), result.message); return;
            }
            const auto authoritative = *result.value;
            auto* dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setWindowTitle(QStringLiteral("Edit Build"));
            auto* form = new QFormLayout(dialog);
            form->addRow(QStringLiteral("Type:"), new QLabel(authoritative.buildType, dialog));
            const QString reference = authoritative.buildType == QStringLiteral("Minifig")
                ? authoritative.minifigNumber : authoritative.setNumber;
            form->addRow(QStringLiteral("Reference:"), new QLabel(reference, dialog));
            auto* name = new QLineEdit(authoritative.name, dialog);
            auto* manufacturer = new QComboBox(dialog);
            manufacturer->setObjectName(QStringLiteral("remoteBuildManufacturerCombo"));
            for (const QString& name : m_remoteManufacturerNames) {
                manufacturer->addItem(name, name);
            }
            int manufacturerIndex = manufacturer->findText(
                authoritative.manufacturerDisplay, Qt::MatchFixedString);
            if (manufacturerIndex < 0 && !authoritative.manufacturerDisplay.isEmpty()) {
                manufacturer->addItem(authoritative.manufacturerDisplay,
                                      authoritative.manufacturerDisplay);
                manufacturerIndex = manufacturer->count() - 1;
            }
            manufacturer->setCurrentIndex(manufacturerIndex);
            manufacturer->setEnabled(authoritative.inventoryMode == QStringLiteral("CompleteSet"));
            auto* notes = new QTextEdit(dialog); notes->setPlainText(authoritative.notes);
            auto* status = new QLabel(dialog); status->setWordWrap(true);
            auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel,
                                                 dialog);
            form->addRow(QStringLiteral("Manufacturer:"), manufacturer);
            form->addRow(QStringLiteral("Name:"), name);
            form->addRow(QStringLiteral("Notes:"), notes);
            form->addRow(status); form->addRow(buttons);
            connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
            auto mutationId = std::make_shared<QString>();
            connect(buttons, &QDialogButtonBox::accepted, dialog,
                [this, dialog, buttons, name, manufacturer, notes, status,
                 authoritative, mutationId] {
                    if (name->text().trimmed().isEmpty()) {
                        status->setText(QStringLiteral("Enter a name for the Build.")); return;
                    }
                    RemoteBuildMutationDto::Request request;
                    request.workspaceId = authoritative.workspaceId;
                    request.buildId = authoritative.buildId;
                    if (mutationId->isEmpty()) *mutationId = RemoteMutationDto::newMutationId();
                    request.mutationId = *mutationId; request.expected = expectedState(authoritative);
                    request.name = name->text().trimmed();
                    request.manufacturer = manufacturer->currentText().trimmed();
                    request.notes = notes->toPlainText().trimmed(); buttons->setEnabled(false);
                    status->setText(QStringLiteral("Saving to BrickSuite Host..."));
                    m_remoteBuildMutations->submit(QStringLiteral("builds.edit"), request, dialog,
                        [this, dialog](const RemoteBuildMutationDto::Result&) {
                            dialog->accept(); refreshRemoteBuildsPreservingSelection();
                        }, [this, buttons, status, mutationId](const RemoteMutationDto::Error& error) {
                            buttons->setEnabled(true); status->setText(error.outcome == RemoteMutationDto::Outcome::Unknown
                                ? QStringLiteral("The outcome is unknown. Retry Safely to check the same mutation.")
                                : error.message);
                            if (error.outcome != RemoteMutationDto::Outcome::Unknown) mutationId->clear();
                            if (error.code == QStringLiteral("STALE_VERSION"))
                                refreshRemoteBuildsPreservingSelection();
                        });
                });
            connect(&m_workspaceContext, &WorkspaceContext::currentWorkspaceChanged,
                    dialog, &QDialog::reject);
            dialog->open();
        });
}

void BuildsWidget::submitRemoteBuildMutation(const QString& action,
                                              const RemoteReadDto::BuildSummary& build,
                                              bool desiredActive)
{
    if (!m_remoteBuildMutations || !m_remoteReads) return;
    const int workspaceId = m_workspaceContext.currentWorkspaceId();
    m_remoteReads->getBuild(workspaceId, build.buildId, this,
        [this, workspaceId, action, desiredActive]
        (AsyncReadResult<RemoteReadDto::BuildDetail> result) {
            if (workspaceId != m_workspaceContext.currentWorkspaceId()) return;
            if (!result.succeeded()) {
                QMessageBox::warning(this, QStringLiteral("Build"), result.message); return;
            }
            const auto authoritative=*result.value;
            if(action!=QStringLiteral("cancel")&&action!=QStringLiteral("disassemble")){
                openRemoteBuildMutationDialog(action,authoritative,desiredActive);return;
            }
            const auto returnsRead=[this,action](int workspace,int buildId,QObject*context,auto completion){
                if(action==QStringLiteral("disassemble"))m_remoteReads->buildDisassemblyReturns(workspace,buildId,context,std::move(completion));
                else m_remoteReads->buildCancellationReturns(workspace,buildId,context,std::move(completion));
            };
            returnsRead(workspaceId,authoritative.buildId,this,
                [this,workspaceId,authoritative,desiredActive,action](AsyncReadResult<QList<RemoteReadDto::BuildCancellationReturnRow>> pulled){
                    if(workspaceId!=m_workspaceContext.currentWorkspaceId())return;
                    if(!pulled.succeeded()){QMessageBox::warning(this,action==QStringLiteral("disassemble")?QStringLiteral("Disassemble Build"):QStringLiteral("Cancel Build"),pulled.message);return;}
                    if(pulled.value->isEmpty()){openRemoteBuildMutationDialog("cancel",authoritative,desiredActive);return;}
                    m_remoteReads->listStorage(workspaceId,this,[this,workspaceId,authoritative,rows=*pulled.value,desiredActive,action](AsyncReadResult<QList<RemoteReadDto::StorageSummary>> storage){
                        if(workspaceId!=m_workspaceContext.currentWorkspaceId())return;
                        if(!storage.succeeded()){QMessageBox::warning(this,action==QStringLiteral("disassemble")?QStringLiteral("Disassemble Build"):QStringLiteral("Cancel Build"),storage.message);return;}
                        const QString reference=authoritative.buildType==QStringLiteral("Minifig")?authoritative.minifigNumber:authoritative.setNumber;
                        auto*dialog=new DisassembleSetDialog(workspaceId,authoritative.name,reference,authoritative.inventoryMode,rows,*storage.value,m_sessionStorageSelectionService,this);
                        dialog->setObjectName(QStringLiteral("remoteBuildWorkflow_disassemblePlan"));
                        dialog->setAttribute(Qt::WA_DeleteOnClose);
                        connect(&m_workspaceContext,&WorkspaceContext::currentWorkspaceChanged,dialog,&QDialog::reject);
                        connect(dialog,&QDialog::accepted,this,[this,dialog,authoritative,desiredActive,action]{
                            QList<RemoteBuildMutationDto::ReturnRow> returns;
                            for(const auto&selection:dialog->returnSelections())returns.append({selection.requirementId,selection.manufacturerName,selection.storageLocationId,selection.quantity,selection.spare});
                            openRemoteBuildMutationDialog(action,authoritative,desiredActive,returns);
                        });
                        dialog->open();
                    });
                });
        });
}

void BuildsWidget::openRemoteBuildMutationDialog(const QString& action,
                                                  const RemoteReadDto::BuildSummary& build,
                                                  bool desiredActive,
                                                  const QList<RemoteBuildMutationDto::ReturnRow>& returns)
{
    QString operation;
    QString prompt;
    if (action == QStringLiteral("complete")) {
        operation = QStringLiteral("builds.complete");
        prompt = QStringLiteral("Mark \"%1\" Complete?").arg(build.name);
    } else if (action == QStringLiteral("cancel")) {
        operation = QStringLiteral("builds.cancel");
        prompt = QStringLiteral("Cancel \"%1\"? Allocations will be released. "
                                "If pieces have already been pulled, the Host will require "
                                "an explicit return plan.").arg(build.name);
    } else if(action==QStringLiteral("disassemble")) {
        operation=QStringLiteral("builds.disassemble");
        prompt=QStringLiteral("Disassemble \"%1\"? The selected pieces will be returned to Host Inventory and the Build will become Disassembled.").arg(build.name);
    } else {
        operation = QStringLiteral("builds.setActive");
        prompt = desiredActive ? QStringLiteral("Reactivate \"%1\"?").arg(build.name)
                               : QStringLiteral("Archive \"%1\"?").arg(build.name);
    }
    auto* dialog = new QDialog(this); dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setObjectName(QStringLiteral("remoteBuildWorkflow_mutation"));
    dialog->setWindowTitle(QStringLiteral("Build"));
    auto* layout = new QVBoxLayout(dialog);
    auto* question = new QLabel(prompt, dialog); question->setWordWrap(true);
    auto* status = new QLabel(dialog); status->setWordWrap(true);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Continue"));
    layout->addWidget(question); layout->addWidget(status); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    auto mutationId = std::make_shared<QString>();
    connect(buttons, &QDialogButtonBox::accepted, dialog,
        [this, dialog, buttons, status, mutationId, operation, build, desiredActive, returns] {
            RemoteBuildMutationDto::Request request;
            request.workspaceId = build.workspaceId; request.buildId = build.buildId;
            if (mutationId->isEmpty()) *mutationId = RemoteMutationDto::newMutationId();
            request.mutationId = *mutationId; request.expected = expectedState(build);
            request.desiredActive = desiredActive; buttons->setEnabled(false);
            request.returns = returns;
            status->setText(QStringLiteral("Saving to BrickSuite Host..."));
            m_remoteBuildMutations->submit(operation, request, dialog,
                [this, dialog](const RemoteBuildMutationDto::Result&) {
                    dialog->accept(); refreshRemoteBuildsPreservingSelection();
                }, [this, dialog, buttons, status, mutationId](const RemoteMutationDto::Error& error) {
                    buttons->setEnabled(true);
                    if (error.outcome == RemoteMutationDto::Outcome::Unknown) {
                        status->setText(QStringLiteral("The outcome is unknown. Retry Safely to check the same mutation."));
                        buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Retry Safely"));
                    } else {
                        mutationId->clear(); status->setText(error.message);
                    }
                    if (error.code == QStringLiteral("STALE_VERSION")
                        || error.code == QStringLiteral("CONFLICT")) {
                        refreshRemoteBuildsPreservingSelection();
                        dialog->reject();
                    }
                });
        });
    connect(&m_workspaceContext, &WorkspaceContext::currentWorkspaceChanged,
            dialog, &QDialog::reject);
    dialog->open();
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

    if (m_remoteMode) {
        if (!m_remoteBuildMutations
            || !m_remoteBuildMutations->isAvailableFor(QStringLiteral("builds.add"))) return;
        if (!m_pendingRemoteAddRequest) {
            RemoteBuildMutationDto::Request request;
            request.workspaceId = m_workspaceContext.currentWorkspaceId();
            request.mutationId = RemoteMutationDto::newMutationId();
            request.buildType = buildType; request.reference = setNumber;
            request.inventoryMode = inventoryMode;
            request.manufacturer = inventoryMode == QStringLiteral("CompleteSet")
                ? m_manufacturerCombo->currentText() : QString();
            request.initialStatus = QStringLiteral("Planned");
            request.name = name; request.notes = notes;
            m_pendingRemoteAddRequest = request;
        }
        const auto request = *m_pendingRemoteAddRequest;
        m_addButton->setEnabled(false);
        m_statusLabel->setText(QStringLiteral("Creating Build on BrickSuite Host..."));
        m_remoteBuildMutations->submit(QStringLiteral("builds.add"), request, this,
            [this](const RemoteBuildMutationDto::Result&) {
                m_pendingRemoteAddRequest.reset(); m_addButton->setText(QStringLiteral("Add Build"));
                m_setNumberEdit->clear(); m_nameEdit->clear(); m_notesEdit->clear();
                m_statusCombo->setCurrentIndex(0); refreshRemoteBuildsPreservingSelection();
                updateUiState();
            }, [this](const RemoteMutationDto::Error& error) {
                if (error.outcome == RemoteMutationDto::Outcome::Unknown) {
                    m_statusLabel->setText(QStringLiteral("The outcome is unknown. Retry Safely to check the same mutation."));
                    m_addButton->setText(QStringLiteral("Retry Safely"));
                } else {
                    m_pendingRemoteAddRequest.reset(); m_addButton->setText(QStringLiteral("Add Build"));
                    m_statusLabel->setText(error.message);
                }
                updateUiState();
            });
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

    const auto createResult = BuildMutationService().create(build);

    if (!createResult.success) {
        QMessageBox::critical(this, "BrickSuite", createResult.message);
        return;
    }
    build = createResult.build;

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
    bool requirementsImported = false;

    if (build.buildType() == "Set"
        && build.inventoryMode() == "CompleteSet"
        && !build.setNumber().trimmed().isEmpty()) {
        SetImportPreviewDialog dialog(build.id(),
                                      build.setNumber().trimmed(),
                                      this);

        if (dialog.exec() == QDialog::Accepted) {
            loadRequirements();
            requirementsImported = true;
        }

        updateRequirementUiState();
    }

    if (requirementsImported) {
        emit hostBuildRequirementsMutationCommitted(
            m_workspaceContext.currentWorkspaceId(), build.id(), false);
    } else {
        emit hostBuildMetadataMutationCommitted(
            m_workspaceContext.currentWorkspaceId(), build.id());
    }

    QMessageBox::information(this,
                             "BrickSuite",
                             QString("Build \"%1\" created successfully.").arg(build.name()));
}

void BuildsWidget::updateUiState()
{
    // Local Build creation is handled by BuildMutationService and must not be
    // gated by the read projection's availability. Remote clients keep the
    // expander usable while its mutation content remains disabled.
    const bool enabled = m_workspaceContext.hasCurrentWorkspace()
        && (!m_remoteMode || (m_remoteSessionConnected && m_remoteBuildMutations
            && m_remoteBuildMutations->isAvailableFor(QStringLiteral("builds.add"))));

    m_typeCombo->setEnabled(enabled);

    const QString buildType = m_typeCombo->currentData().toString();
    const bool hasNumberField = buildType == "Set" || buildType == "MOC";

    m_setNumberEdit->setEnabled(enabled && hasNumberField);
    m_inventoryModeCombo->setEnabled(enabled);

    const bool completeSet =
        m_inventoryModeCombo->currentData().toString() == "CompleteSet";

    m_manufacturerCombo->setEnabled(enabled && completeSet);
    m_nameEdit->setEnabled(enabled);
    m_statusCombo->setEnabled(enabled && !m_remoteMode);
    m_notesEdit->setEnabled(enabled);
    m_addButton->setEnabled(enabled);
    m_newBuildGroup->setEnabled(true);
    m_newBuildContent->setEnabled(enabled);
    if (m_remoteMode && m_pendingRemoteAddRequest) {
        m_typeCombo->setEnabled(false); m_setNumberEdit->setEnabled(false);
        m_inventoryModeCombo->setEnabled(false); m_manufacturerCombo->setEnabled(false);
        m_nameEdit->setEnabled(false); m_statusCombo->setEnabled(false); m_notesEdit->setEnabled(false);
        m_addButton->setEnabled(enabled);
    }
}

void BuildsWidget::loadManufacturers()
{
    m_manufacturerCombo->clear();

    if (m_remoteMode) {
        loadRemoteManufacturerChoices();
        return;
    }

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

void BuildsWidget::loadRemoteManufacturerChoices()
{
    if (!m_remoteMode || !m_remoteReads || !m_remoteSessionConnected)
        return;
    const quint64 generation = ++m_manufacturerGeneration;
    m_remoteReads->listManufacturerNames(this,
        [this, generation](AsyncReadResult<QStringList> result) {
            if (generation != m_manufacturerGeneration || !result.succeeded()) return;
            const QString current = m_manufacturerCombo->currentText();
            m_remoteManufacturerNames = *result.value;
            m_manufacturerCombo->clear();
            for (const QString& name : m_remoteManufacturerNames)
                m_manufacturerCombo->addItem(name, name);
            int index = m_manufacturerCombo->findText(current, Qt::MatchFixedString);
            if (index < 0)
                index = m_manufacturerCombo->findText(QStringLiteral("LEGO"), Qt::MatchFixedString);
            if (index >= 0) m_manufacturerCombo->setCurrentIndex(index);
            updateUiState();
        });
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

    if (m_remoteMode) {
        m_remoteRequirements.clear();
        loadRemoteRequirements();
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

    const std::optional<Build> selectedBuild =
        m_buildService.get(m_selectedBuildId);

    const bool completeSet =
        selectedBuild && selectedBuild->inventoryMode() == "CompleteSet";

    const bool completeSetIsComplete =
        completeSet && selectedBuild->status() == "Complete";

    m_requirementsLabel->setText(
        completeSet
            ? QString("Set Contents for: %1").arg(buildDescription)
            : QString("Requirements for: %1").arg(buildDescription));

    const QList<BuildRequirement> requirements =
        m_buildService.requirements(m_selectedBuildId);

    PartRepository partRepository;
    ColorRepository colorRepository;
    BuildRequirementAvailabilityService availabilityService;

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

        const int quantityPulled = requirement.quantityPulled();

        const int remainingRequired =
            qMax(requirement.quantityRequired() - quantityPulled, 0);

        //
        // Total allocation is measured against the effective physical
        // identity because all requirements and Builds compete for the
        // same loose stock.
        //
        const auto availability = availabilityService.project(workspaceId, requirement);

        auto* requiredItem =
            new QTableWidgetItem(QString::number(requirement.quantityRequired()));

        auto* pulledItem =
            new QTableWidgetItem(QString::number(quantityPulled));

        auto* remainingItem =
            new QTableWidgetItem(QString::number(remainingRequired));

        auto* ownedItem =
            new QTableWidgetItem(QString::number(availability.owned));

        auto* thisBuildItem =
            new QTableWidgetItem(QString::number(availability.thisRequirementAllocated));

        auto* otherBuildsItem =
            new QTableWidgetItem(QString::number(availability.otherAllocated));

        auto* availableItem =
            new QTableWidgetItem(QString::number(availability.available));

        auto* missingItem =
            new QTableWidgetItem(QString::number(availability.missing));

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
                            emit hostBuildRequirementsMutationCommitted(
                                m_workspaceContext.currentWorkspaceId(), m_selectedBuildId, false);
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
                            const auto result = BuildRequirementMutationService().remove(requirementId);

                            if (!result.success) {
                                QMessageBox::critical(
                                    this,
                                    "BrickSuite",
                                    result.message);
                            } else {
                                loadRequirements();
                                emit hostBuildRequirementsMutationCommitted(
                                    m_workspaceContext.currentWorkspaceId(), m_selectedBuildId, false);
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
                            emit hostBuildRequirementsMutationCommitted(
                                m_workspaceContext.currentWorkspaceId(), m_selectedBuildId, false);
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

void BuildsWidget::loadRemoteRequirements(int page)
{
    if (!m_remoteReads
        || !m_remoteReads->isAvailableFor(QStringLiteral("builds.requirements"))) {
        m_requirementsLabel->setText("Build requirements are unavailable from this Host.");
        emit remoteRequirementsRefreshFinished(false);
        return;
    }

    const int workspaceId = m_workspaceContext.currentWorkspaceId();
    const int buildId = m_selectedBuildId;
    const quint64 generation = page == 1 ? ++m_requirementGeneration : m_requirementGeneration;
    if (page == 1)
        m_requirementsLabel->setText("Loading Build requirements...");

    m_remoteReads->buildRequirements(workspaceId, buildId,
        RemoteReadDto::PageRequest{page, RemoteReadDto::MaximumPageSize}, this,
        [this, workspaceId, buildId, generation](
            AsyncReadResult<RemoteReadDto::Page<RemoteReadDto::BuildRequirement>> result) {
            if (generation != m_requirementGeneration || buildId != m_selectedBuildId
                || workspaceId != m_workspaceContext.currentWorkspaceId())
                return;
            if (!result.succeeded()) {
                m_requirementsLabel->setText(result.message.isEmpty()
                    ? QStringLiteral("Unable to load Build requirements from the Host.")
                    : result.message);
                emit remoteRequirementsRefreshFinished(false);
                return;
            }
            const auto& value = *result.value;
            m_remoteRequirements.append(value.rows);
            if (m_remoteRequirements.size() < value.totalRows) {
                loadRemoteRequirements(value.page + 1);
                return;
            }
            renderRemoteRequirements();
            emit remoteRequirementsRefreshFinished(true);
        });
}

void BuildsWidget::renderRemoteRequirements()
{
    QHash<QString, QString> partNames;
    QSet<QString> partNumbers;
    for (const auto& row : std::as_const(m_remoteRequirements)) {
        partNumbers.insert(row.partNumber);
        if (!row.substitutePartNumber.isEmpty()) partNumbers.insert(row.substitutePartNumber);
    }
    for (const Part& part : PartRepository().getByPartNumbers(partNumbers))
        partNames.insert(part.partNumber(), part.name());
    QHash<int, QString> colorNames;
    for (const Color& color : ColorRepository().getAll())
        colorNames.insert(color.rebrickableId(), color.name());

    m_requirementsTable->setRowCount(m_remoteRequirements.size());
    const bool buildWritable = selectedRemoteBuildSupportsStockFulfillment();
    const auto* selectedBuildItem=m_buildsTable->item(m_buildsTable->currentRow(),4);
    const bool completeSetComplete=selectedBuildItem
        &&selectedBuildItem->data(Qt::UserRole+1).toString()==QStringLiteral("CompleteSet")
        &&selectedBuildItem->data(Qt::UserRole+2).toString()==QStringLiteral("Complete")
        &&selectedBuildItem->data(Qt::UserRole+3).toBool();
    int staleRows = 0;
    for (int row = 0; row < m_remoteRequirements.size(); ++row) {
        const auto& requirement = m_remoteRequirements.at(row);
        const QString partName = partNames.value(requirement.partNumber,
                                                  requirement.partNameFallback);
        const QString colorName = colorNames.value(requirement.rebrickableColorId,
                                                    requirement.colorNameFallback);
        if (!partNames.contains(requirement.partNumber)
            || !colorNames.contains(requirement.rebrickableColorId))
            ++staleRows;
        auto* partItem = new QTableWidgetItem(requirement.partNumber);
        partItem->setData(Qt::UserRole, requirement.requirementId);
        if (!requirement.substitutePartNumber.isEmpty()) {
            partItem->setToolTip(QString("Fulfilled from stock as %1 (Rebrickable color %2)")
                .arg(requirement.substitutePartNumber)
                .arg(requirement.substituteRebrickableColorId));
        }
        const int remaining = qMax(requirement.quantityRequired - requirement.quantityPulled, 0);
        const QString dash = QStringLiteral("—");
        m_requirementsTable->setItem(row, 0, partItem);
        m_requirementsTable->setItem(row, 1, new QTableWidgetItem(
            partName.isEmpty() ? QStringLiteral("(Host catalog name unavailable)") : partName));
        m_requirementsTable->setItem(row, 2, new QTableWidgetItem(
            colorName.isEmpty() ? QString("Rebrickable color %1").arg(requirement.rebrickableColorId)
                                : colorName));
        m_requirementsTable->setItem(row, 3, new QTableWidgetItem(QString::number(requirement.quantityRequired)));
        m_requirementsTable->setItem(row, 4, new QTableWidgetItem(QString::number(requirement.quantityPulled)));
        m_requirementsTable->setItem(row, 5, new QTableWidgetItem(QString::number(remaining)));
        const int availability[] = {requirement.owned, requirement.thisRequirementAllocated,
                                    requirement.otherAllocated, requirement.available,
                                    requirement.missing};
        for (int column = 6; column <= 10; ++column) {
            const int value = availability[column - 6];
            auto* item = new QTableWidgetItem(value >= 0 ? QString::number(value) : dash);
            item->setTextAlignment(Qt::AlignCenter);
            m_requirementsTable->setItem(row, column, item);
        }
        m_requirementsTable->setItem(row, 11,
            new QTableWidgetItem(requirement.spare ? QStringLiteral("Yes") : QStringLiteral("No")));
        auto* actions=new QComboBox(m_requirementsTable);actions->addItem(QStringLiteral("Actions..."));
        const auto capability=[this](const char* name){return m_remoteBuildMutations&&m_remoteBuildMutations->isAvailableFor(QLatin1String(name));};
        const bool hasPulledOrReleased=requirement.quantityPulled>0||requirement.quantityReleased>0;
        const auto eligibility=BuildActionEligibility::remoteRequirementActions(m_remoteSessionConnected,buildWritable,false,false,capability("builds.requirements.edit"),capability("builds.requirements.remove"),capability("builds.allocations.set"),false,requirement.spare,hasPulledOrReleased,requirement.thisRequirementAllocated>0);
        if(eligibility.canEdit)actions->addItem(QStringLiteral("Edit"),QStringLiteral("edit"));
        if(eligibility.canRemove)actions->addItem(QStringLiteral("Delete"),QStringLiteral("delete"));
        if(eligibility.canSetAllocations)actions->addItem(QStringLiteral("Allocate..."),QStringLiteral("allocate"));
        const bool canStoreSpare=completeSetComplete&&requirement.spare
            &&requirement.quantityReleased<requirement.quantityRequired
            &&capability("builds.spare.store");
        if(canStoreSpare)actions->addItem(QStringLiteral("Store Spare..."),QStringLiteral("store_spare"));
        if(!eligibility.canEdit&&!eligibility.canRemove&&!eligibility.canSetAllocations&&!canStoreSpare){actions->clear();actions->addItem(QStringLiteral("Read-only"));actions->setEnabled(false);}
        connect(actions,&QComboBox::currentIndexChanged,this,[this,actions,requirement](int index){
            if(index<=0)return;const QString action=actions->itemData(index).toString();actions->setCurrentIndex(0);
            if(action==QStringLiteral("store_spare")){storeRemoteSpare(requirement);return;}
            if(action==QStringLiteral("allocate")){openRemoteAllocationDialog(requirement);return;}
            if(action==QStringLiteral("delete")){
                auto* prompt=new QMessageBox(QMessageBox::Warning,QStringLiteral("Delete Build Requirement"),QStringLiteral("Delete this requirement from the Build?"),QMessageBox::Yes|QMessageBox::No,this);prompt->setAttribute(Qt::WA_DeleteOnClose);prompt->setDefaultButton(QMessageBox::No);
                connect(prompt,&QMessageBox::finished,this,[this,prompt,requirement](int result){if(result!=QMessageBox::Yes)return;RemoteBuildMutationDto::Request request;request.workspaceId=m_workspaceContext.currentWorkspaceId();request.requirementId=requirement.requirementId;request.mutationId=RemoteMutationDto::newMutationId();request.expectedRequirement=expectedRequirementState(requirement);m_remoteBuildMutations->submit(QStringLiteral("builds.requirements.remove"),request,this,[this](const RemoteBuildMutationDto::Result&){loadRequirements();},[this](const RemoteMutationDto::Error&e){QMessageBox::warning(this,"Delete Build Requirement",e.message);loadRequirements();});});prompt->open();return;
            }
            auto* dialog = new QDialog(this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setWindowTitle(QStringLiteral("Edit Build Requirement"));
            auto* form = new QFormLayout(dialog);
            auto* original = new QLabel(
                QStringLiteral("%1 / Rebrickable color %2")
                    .arg(requirement.partNumber)
                    .arg(requirement.rebrickableColorId), dialog);
            auto* part = new QLineEdit(
                requirement.substitutePartNumber.isEmpty()
                    ? requirement.partNumber : requirement.substitutePartNumber,
                dialog);
            PartSearchCompleterHelper::install(part);
            auto* color = new QComboBox(dialog);
            for (const auto& catalogColor : ColorRepository().getAll())
                color->addItem(catalogColor.name(), catalogColor.rebrickableId());
            const int colorIndex = color->findData(
                requirement.substitutePartNumber.isEmpty()
                    ? requirement.rebrickableColorId
                    : requirement.substituteRebrickableColorId);
            if (colorIndex >= 0)
                color->setCurrentIndex(colorIndex);
            auto* quantity = new QSpinBox(dialog);
            quantity->setRange(1, 99999);
            quantity->setValue(requirement.quantityRequired);
            auto* spare = new QCheckBox(QStringLiteral("Spare part"), dialog);
            spare->setChecked(requirement.spare);
            auto* buttons = new QDialogButtonBox(
                QDialogButtonBox::Save | QDialogButtonBox::Cancel, dialog);
            form->addRow(QStringLiteral("Original:"), original);
            form->addRow(QStringLiteral("Use Part:"), part);
            form->addRow(QStringLiteral("Use Color:"), color);
            form->addRow(QStringLiteral("Qty Required:"), quantity);
            form->addRow(QString(), spare);
            form->addRow(buttons);
            connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
            connect(buttons, &QDialogButtonBox::accepted, dialog,
                    [this, dialog, part, color, quantity, spare, buttons, requirement]() {
                const auto selected = PartRepository().getByPartNumber(
                    PartSearchCompleterHelper::canonicalPartNumber(part));
                if (!selected) {
                    QMessageBox::warning(
                        dialog, "Edit Build Requirement",
                        "The selected Part is unavailable in the Client catalog.");
                    return;
                }
                RemoteBuildMutationDto::Request request;
                request.workspaceId = m_workspaceContext.currentWorkspaceId();
                request.requirementId = requirement.requirementId;
                request.mutationId = RemoteMutationDto::newMutationId();
                request.expectedRequirement = expectedRequirementState(requirement);
                const bool originalIdentity =
                    selected->partNumber() == requirement.partNumber
                    && color->currentData().toInt() == requirement.rebrickableColorId;
                if (!originalIdentity) {
                    request.substitutePartNumber = selected->partNumber();
                    request.substituteRebrickableColorId = color->currentData().toInt();
                }
                request.quantityRequired = quantity->value();
                request.spare = spare->isChecked();
                buttons->setEnabled(false);
                m_remoteBuildMutations->submit(
                    QStringLiteral("builds.requirements.edit"), request, dialog,
                    [this, dialog](const RemoteBuildMutationDto::Result&) {
                        dialog->close();
                        loadRequirements();
                    },
                    [this, dialog, buttons](const RemoteMutationDto::Error& error) {
                        if (error.code == QStringLiteral("STALE_VERSION")) {
                            dialog->close();
                            loadRequirements();
                            QMessageBox::warning(
                                this, "Edit Build Requirement",
                                "The requirement changed on the Host. The latest values "
                                "have been reloaded. Please edit it again.");
                            return;
                        }
                        buttons->setEnabled(true);
                        QMessageBox::warning(dialog, "Edit Build Requirement", error.message);
                    });
            });
            dialog->open();
        });
        m_requirementsTable->setCellWidget(row,12,actions);
    }
    m_requirementsLabel->setText(QString("%1 Host requirement(s).%2")
        .arg(m_remoteRequirements.size())
        .arg(staleRows > 0 ? QStringLiteral(" Some rows use Host fallback catalog text.") : QString()));
}

void BuildsWidget::storeRemoteSpare(const RemoteReadDto::BuildRequirement& requirement)
{
    if(!m_remoteReads||!m_remoteBuildMutations||!m_remoteBuildMutations->isAvailableFor(QStringLiteral("builds.spare.store")))return;
    const int workspace=m_workspaceContext.currentWorkspaceId();
    const int remaining=qMax(requirement.quantityRequired-requirement.quantityReleased,0);
    if(remaining<=0)return;
    m_remoteReads->listStorage(workspace,this,[this,workspace,requirement,remaining](AsyncReadResult<QList<RemoteReadDto::StorageSummary>> result){
        if(workspace!=m_workspaceContext.currentWorkspaceId()||!result.succeeded()){if(!result.succeeded())QMessageBox::warning(this,"Store Spare",result.message);return;}
        QSet<qint64> parents;for(const auto&row:*result.value)if(row.active&&row.parentStorageId>0)parents.insert(row.parentStorageId);
        auto*dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle(QStringLiteral("Store Spare"));
        dialog->setObjectName(QStringLiteral("remoteBuildWorkflow_storeSpare"));
        auto*form=new QFormLayout(dialog);form->addRow(QStringLiteral("Part:"),new QLabel(QString("%1 — %2").arg(requirement.partNumber,requirement.partNameFallback),dialog));
        form->addRow(QStringLiteral("Color:"),new QLabel(requirement.colorNameFallback,dialog));
        auto*quantity=new QSpinBox(dialog);quantity->setRange(1,remaining);quantity->setValue(remaining);
        auto*storage=new QComboBox(dialog);for(const auto&row:*result.value)if(row.active&&row.allowsInventory&&!parents.contains(row.storageId))storage->addItem(row.displayPath,row.storageId);
        const int remembered=storage->findData(m_sessionStorageSelectionService.rememberedDestination(workspace));if(remembered>=0)storage->setCurrentIndex(remembered);
        auto*status=new QLabel(dialog);status->setWordWrap(true);auto*buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,dialog);
        form->addRow(QStringLiteral("Available to Store:"),new QLabel(QString::number(remaining),dialog));form->addRow(QStringLiteral("Quantity:"),quantity);form->addRow(QStringLiteral("Storage:"),storage);form->addRow(status);form->addRow(buttons);
        if(storage->count()==0){status->setText(QStringLiteral("No active Host Inventory Storage destination is available."));buttons->button(QDialogButtonBox::Save)->setEnabled(false);}
        connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::close);auto mutationId=std::make_shared<QString>();
        connect(buttons,&QDialogButtonBox::accepted,dialog,[this,dialog,buttons,status,storage,quantity,workspace,requirement,mutationId]{
            RemoteBuildMutationDto::Request request;request.workspaceId=workspace;request.buildId=requirement.buildId;request.requirementId=requirement.requirementId;
            if(mutationId->isEmpty())*mutationId=RemoteMutationDto::newMutationId();request.mutationId=*mutationId;request.preferredStorageId=storage->currentData().toLongLong();request.quantity=quantity->value();request.expectedRequirement=expectedRequirementState(requirement);
            buttons->setEnabled(false);status->setText(QStringLiteral("Storing spare on BrickSuite Host..."));
            m_remoteBuildMutations->submit(QStringLiteral("builds.spare.store"),request,dialog,[this,dialog,workspace,storage](const RemoteBuildMutationDto::Result&){m_sessionStorageSelectionService.rememberDestination(workspace,storage->currentData().toInt());dialog->close();loadRequirements();},[this,dialog,buttons,status,mutationId](const RemoteMutationDto::Error&e){buttons->setEnabled(true);if(e.outcome==RemoteMutationDto::Outcome::Unknown){status->setText(QStringLiteral("The outcome is unknown. Retry Safely to check the same mutation."));buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("Retry Safely"));}else{mutationId->clear();status->setText(e.message);}if(e.code==QStringLiteral("STALE_VERSION")||e.code==QStringLiteral("CONFLICT")){dialog->close();loadRequirements();}});
        });
        connect(&m_workspaceContext,&WorkspaceContext::currentWorkspaceChanged,dialog,&QDialog::reject);dialog->open();
    });
}

void BuildsWidget::openRemoteAllocationDialog(const RemoteReadDto::BuildRequirement& requirement)
{
    if(!m_remoteReads||!m_remoteBuildMutations)return;
    const int workspace=m_workspaceContext.currentWorkspaceId();
    const QString effectivePart=requirement.substitutePartNumber.isEmpty()?requirement.partNumber:requirement.substitutePartNumber;
    const int effectiveColor=requirement.substituteRebrickableColorId>=0?requirement.substituteRebrickableColorId:requirement.rebrickableColorId;
    RemoteReadDto::InventorySearchRequest search;search.workspaceId=workspace;search.text=effectivePart;search.rebrickableColorId=effectiveColor;search.paging={1,RemoteReadDto::MaximumPageSize};
    m_remoteReads->searchInventory(search,this,[this,workspace,requirement,effectivePart,effectiveColor](AsyncReadResult<RemoteReadDto::Page<RemoteReadDto::InventoryRow>> inventoryResult){
        if(workspace!=m_workspaceContext.currentWorkspaceId()||!inventoryResult.succeeded()){if(!inventoryResult.succeeded())QMessageBox::warning(this,"Allocate Requirement",inventoryResult.message);return;}
        QList<RemoteReadDto::InventoryRow> candidates;for(const auto&r:inventoryResult.value->rows)if(r.partNumber==effectivePart&&r.rebrickableColorId==effectiveColor)candidates.append(r);
        m_remoteReads->pulling(workspace,requirement.buildId,{1,RemoteReadDto::MaximumPageSize},this,[this,workspace,requirement,candidates](AsyncReadResult<RemoteReadDto::Page<RemoteReadDto::PullingRow>> pullingResult){
            if(workspace!=m_workspaceContext.currentWorkspaceId()||!pullingResult.succeeded()){if(!pullingResult.succeeded())QMessageBox::warning(this,"Allocate Requirement",pullingResult.message);return;}
            QHash<qint64,RemoteReadDto::PullingRow> existing;for(const auto&r:pullingResult.value->rows)if(r.requirementId==requirement.requirementId)existing.insert(r.inventoryRecordId,r);
            auto* dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowTitle(QStringLiteral("Allocate Build Requirement"));dialog->resize(700,420);auto* layout=new QVBoxLayout(dialog);auto* table=new QTableWidget(candidates.size(),4,dialog);table->setHorizontalHeaderLabels({"Storage","Owned","Allocated","Desired"});table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);table->setEditTriggers(QAbstractItemView::NoEditTriggers);auto editors=std::make_shared<QList<QSpinBox*>>();
            for(int i=0;i<candidates.size();++i){const auto&row=candidates[i];const auto old=existing.value(row.inventoryRecordId);table->setItem(i,0,new QTableWidgetItem(row.storagePath));table->setItem(i,1,new QTableWidgetItem(QString::number(row.quantity)));table->setItem(i,2,new QTableWidgetItem(QString::number(old.quantityAllocated)));auto* spin=new QSpinBox(table);spin->setRange(0,qMin(row.quantity,qMax(requirement.quantityRequired-requirement.quantityPulled,0)));spin->setValue(old.quantityAllocated);table->setCellWidget(i,3,spin);editors->append(spin);}
            auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,dialog);layout->addWidget(table);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::close);connect(buttons,&QDialogButtonBox::accepted,dialog,[this,dialog,buttons,workspace,requirement,candidates,existing,editors](){RemoteBuildMutationDto::Request request;request.workspaceId=workspace;request.requirementId=requirement.requirementId;request.mutationId=RemoteMutationDto::newMutationId();request.expectedRequirement=expectedRequirementState(requirement);for(int i=0;i<candidates.size();++i){const auto&candidate=candidates[i];const auto old=existing.value(candidate.inventoryRecordId);RemoteBuildMutationDto::AllocationRow row;row.allocationId=old.allocationId;row.inventoryRecordId=candidate.inventoryRecordId;row.quantity=editors->at(i)->value();row.expectedQuantity=old.quantityAllocated;row.inventoryQuantity=candidate.quantity;request.allocations.append(row);}buttons->setEnabled(false);m_remoteBuildMutations->submit(QStringLiteral("builds.allocations.set"),request,dialog,[this,dialog](const RemoteBuildMutationDto::Result&){dialog->close();loadRequirements();},[this,dialog,buttons](const RemoteMutationDto::Error&e){buttons->setEnabled(true);QMessageBox::warning(dialog,"Allocate Requirement",e.message);loadRequirements();});});dialog->open();
        });
    });
}

void BuildsWidget::showRemoteDetails(int buildId)
{
    if (!m_remoteReads) return;
    const int workspaceId = m_workspaceContext.currentWorkspaceId();
    m_remoteReads->getBuild(workspaceId, buildId, this,
        [this, workspaceId, buildId](AsyncReadResult<RemoteReadDto::BuildDetail> result) {
            if (workspaceId != m_workspaceContext.currentWorkspaceId() || !result.succeeded()) {
                if (!result.succeeded()) QMessageBox::warning(this, "Host Build Details", result.message);
                return;
            }
            const auto& build = *result.value;
            const QString reference = build.buildType == "Minifig"
                ? build.minifigNumber : build.setNumber;
            QMessageBox::information(this, "Host Build Details",
                QString("%1\n\nType: %2\nReference: %3\nInventory Mode: %4\nStatus: %5\nActive: %6\n\nRemote Build details are read-only.")
                    .arg(build.name, build.buildType,
                         reference.isEmpty() ? QStringLiteral("(None)") : reference,
                         build.inventoryMode == "CompleteSet" ? QStringLiteral("Complete Set")
                                                                : QStringLiteral("Build from Stock"),
                         build.status, build.active ? QStringLiteral("Yes") : QStringLiteral("No")));
        });
}

void BuildsWidget::showRemotePulling(int buildId)
{
    if (!m_remoteReads) return;
    if (QDialog* existing = m_remotePullingDialogs.find(buildId)) {
        existing->show();
        existing->raise();
        existing->activateWindow();
        return;
    }

    auto* dialog = new QDialog(this);
    dialog->setObjectName(QStringLiteral("remoteBuildPullingDialog_%1").arg(buildId));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    m_remotePullingDialogs.track(buildId, dialog);
    connect(dialog, &QObject::destroyed, this, [this, buildId]() {
        m_remotePullingDialogs.forget(buildId);
    });
    const bool writable = m_remotePulling && m_remotePulling->isAvailable();
    dialog->setWindowTitle(writable ? QStringLiteral("Host Build Pulling")
                                    : QStringLiteral("Host Build Pulling (Read-only)"));
    dialog->resize(1150, 650);
    auto* layout = new QVBoxLayout(dialog);
    auto* status = new QLabel("Loading Pulling state from BrickSuite Host...", dialog);
    status->setObjectName(QStringLiteral("remotePullingStatus"));
    auto* table = new QTableWidget(dialog);
    table->setColumnCount(9);
    table->setHorizontalHeaderLabels({"Image", "Part", "Description", "Color", "Required",
                                      "Already Pulled", "Allocated Here", "Pulled",
                                      "Storage Location"});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->setIconSize(QSize(72, 72));
    for (int column : {0, 1, 3, 4, 5, 6, 7})
        table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Stretch);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    auto* recordButton = buttons->addButton(QStringLiteral("Record Pulls"),
                                             QDialogButtonBox::ActionRole);
    recordButton->setVisible(writable);
    recordButton->setEnabled(false);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(status);
    layout->addWidget(table);
    auto* capabilityNotice = new QLabel(writable
        ? QStringLiteral("Enter positive quantities as pieces physically pulled, then choose Record Pulls.")
        : QStringLiteral("This Host provides Pulling as read-only. Protocol 1.2 and Pulling write permission are required."), dialog);
    capabilityNotice->setWordWrap(true);
    layout->addWidget(capabilityNotice);
    layout->addWidget(buttons);
    dialog->show();
    emit remotePullingDialogOpened();

    const int workspaceId = m_workspaceContext.currentWorkspaceId();
    auto* images = new PartImageService(dialog);
    if (m_enrichmentService) {
        connect(m_enrichmentService,
                &PartExternalIdEnrichmentService::generalImageMetadataReady,
                images,
                &PartImageService::requestPartImage);
    }

    auto rowsByPart = std::make_shared<QHash<QString, QList<int>>>();
    connect(images, &PartImageService::imageReady, dialog,
            [table, rowsByPart](const QString& partNumber, const QString& path) {
                QPixmap pixmap(path);
                if (pixmap.isNull()) return;
                const QIcon icon(pixmap.scaled(72, 72, Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation));
                const QString key = partNumber.trimmed().toLower();
                for (int row : rowsByPart->value(key)) {
                    auto* item = table->item(row, 0);
                    if (item && item->data(Qt::UserRole).toString().trimmed().toLower() == key)
                        item->setIcon(icon);
                }
            });

    auto allRows = std::make_shared<QList<RemoteReadDto::PullingRow>>();
    auto editors = std::make_shared<QHash<qint64, QSpinBox*>>();
    auto pending = std::make_shared<std::optional<RemotePullingMutationDto::Request>>();
    auto mutationPending = std::make_shared<bool>(false);
    auto refreshGeneration = std::make_shared<quint64>(1);
    auto requestPage = std::make_shared<std::function<void(int, quint64)>>();
    *requestPage = [this, dialog, table, status, images, rowsByPart, allRows, editors,
                    recordButton, writable, requestPage, refreshGeneration, workspaceId,
                    buildId](int page, quint64 generation) {
        m_remoteReads->pulling(workspaceId, buildId,
            {page, RemoteReadDto::MaximumPageSize}, dialog,
            [this, dialog, table, status, images, rowsByPart, allRows, editors,
             recordButton, writable, requestPage, refreshGeneration, workspaceId,
             buildId, generation](AsyncReadResult<RemoteReadDto::Page<RemoteReadDto::PullingRow>> result) {
                if (generation != *refreshGeneration) return;
                if (!result.succeeded()) {
                    status->setText(result.message);
                    emit remotePullingRefreshFinished(buildId, false);
                    return;
                }
                allRows->append(result.value->rows);
                if (allRows->size() < result.value->totalRows) {
                    (*requestPage)(result.value->page + 1, generation);
                    return;
                }

                QSet<QString> numbers;
                for (const auto& row : std::as_const(*allRows)) numbers.insert(row.partNumber);
                QHash<QString, Part> localParts;
                for (const Part& part : PartRepository().getByPartNumbers(numbers)) {
                    localParts.insert(part.partNumber().trimmed().toLower(), part);
                }
                QHash<int, Color> localColors;
                for (const Color& color : ColorRepository().getAll())
                    localColors.insert(color.rebrickableId(), color);

                table->setUpdatesEnabled(false);
                table->setRowCount(allRows->size());
                rowsByPart->clear();
                editors->clear();
                QList<int> missingImagePartIds;
                QString previousStorage;
                for (int i = 0; i < allRows->size(); ++i) {
                    const auto& source = allRows->at(i);
                    const auto localPart = localParts.constFind(source.partNumber.trimmed().toLower());
                    const QString partNumber = localPart == localParts.cend()
                        ? source.partNumber : localPart->partNumber();
                    const QString description = localPart == localParts.cend()
                        ? source.partNameFallback : localPart->name();
                    const auto localColor = localColors.constFind(source.rebrickableColorId);
                    const QString colorName = localColor == localColors.cend()
                        ? source.colorNameFallback : localColor->name();

                    table->setRowHeight(i, 82);
                    auto* imageItem = new QTableWidgetItem;
                    imageItem->setTextAlignment(Qt::AlignCenter);
                    imageItem->setData(Qt::UserRole, partNumber);
                    imageItem->setToolTip(partNumber);
                    table->setItem(i, 0, imageItem);
                    table->setItem(i, 1, new QTableWidgetItem(partNumber));
                    auto* descriptionItem = new QTableWidgetItem(description);
                    descriptionItem->setToolTip(description);
                    table->setItem(i, 2, descriptionItem);
                    table->setItem(i, 3, new QTableWidgetItem(colorName));
                    for (int column = 4; column <= 6; ++column) {
                        const int value = column == 4 ? source.quantityRequired
                            : column == 5 ? source.quantityPulled
                            : source.quantityAllocated;
                        auto* quantity = new QTableWidgetItem(QString::number(value));
                        quantity->setTextAlignment(Qt::AlignCenter);
                        table->setItem(i, column, quantity);
                    }
                    if (writable) {
                        auto* pulled = new QSpinBox(table);
                        pulled->setRange(0, qMin(source.quantityAllocated,
                            qMax(source.quantityRequired - source.quantityPulled, 0)));
                        pulled->setAlignment(Qt::AlignCenter);
                        table->setCellWidget(i, 7, pulled);
                        editors->insert(source.allocationId, pulled);
                        connect(pulled, qOverload<int>(&QSpinBox::valueChanged), dialog,
                            [editors, recordButton](int) {
                                bool any = false;
                                for (auto* editor : std::as_const(*editors))
                                    any = any || (editor && editor->value() > 0);
                                recordButton->setEnabled(any);
                            });
                    } else {
                        auto* quantity = new QTableWidgetItem(QString::number(source.quantityPulled));
                        quantity->setTextAlignment(Qt::AlignCenter);
                        table->setItem(i, 7, quantity);
                    }
                    auto* storage = new QTableWidgetItem(source.storagePath);
                    if (i == 0 || source.storagePath.compare(previousStorage,
                                                              Qt::CaseInsensitive) != 0) {
                        QFont font = storage->font();
                        font.setBold(true);
                        storage->setFont(font);
                    }
                    previousStorage = source.storagePath;
                    table->setItem(i, 8, storage);

                    if (source.substitution) {
                        const QString tip = QStringLiteral("Substitution for the original Build requirement.");
                        table->item(i, 1)->setToolTip(tip);
                        table->item(i, 2)->setToolTip(description + QStringLiteral("\n") + tip);
                        table->item(i, 3)->setToolTip(tip);
                    }

                    const QString key = partNumber.trimmed().toLower();
                    (*rowsByPart)[key].append(i);
                    const QString cached = images->cachedImagePath(partNumber);
                    if (!cached.isEmpty()) images->requestPartImage(partNumber, {});
                    else {
                        imageItem->setText(QStringLiteral("Image"));
                        if (localPart != localParts.cend())
                            missingImagePartIds.append(localPart->id());
                    }
                }
                table->setUpdatesEnabled(true);
                if (m_enrichmentService)
                    m_enrichmentService->ensureGeneralImageMetadata(missingImagePartIds);
                status->setText(allRows->isEmpty()
                    ? QStringLiteral("No allocated pulling rows.")
                    : writable ? QStringLiteral("Host Pulling state is ready.")
                               : QStringLiteral("Host Pulling state (read-only)."));
                emit remotePullingRefreshFinished(buildId, true);
            });
    };
    connect(recordButton, &QPushButton::clicked, dialog,
        [this, dialog, status, recordButton, editors, allRows, pending, mutationPending,
         workspaceId, buildId]() {
            if (!m_remotePulling || *mutationPending) return;
            if (!m_remotePulling->isAvailable()) {
                status->setText(QStringLiteral("Reconnect to the current BrickSuite Host before recording or confirming this pull."));
                return;
            }
            RemotePullingMutationDto::Request request;
            if (*pending) {
                request = **pending;
            } else {
                request.workspaceId = workspaceId;
                request.buildId = buildId;
                request.mutationId = RemoteMutationDto::newMutationId();
                for (const auto& source : std::as_const(*allRows)) {
                    QSpinBox* editor = editors->value(source.allocationId);
                    if (!editor || editor->value() <= 0) continue;
                    request.rows.append({source.allocationId, editor->value(),
                        source.quantityAllocated, source.inventoryQuantity, source.quantityPulled});
                }
                if (request.rows.isEmpty()) return;
                *pending = request;
            }
            *mutationPending = true;
            recordButton->setEnabled(false);
            for (auto* editor : std::as_const(*editors)) if (editor) editor->setEnabled(false);
            status->setText(QStringLiteral("Recording Pulling changes on BrickSuite Host..."));
            m_remotePulling->record(request, dialog,
                [this, status, recordButton, pending, mutationPending, editors, buildId](
                    const RemotePullingMutationDto::Result& result) {
                    *mutationPending = false;
                    pending->reset();
                    recordButton->setText(QStringLiteral("Record Pulls"));
                    status->setText(QStringLiteral("Recorded %1 piece(s) from %2 row(s)%3.")
                        .arg(result.piecesRecorded).arg(result.submittedRows)
                        .arg(result.replayed ? QStringLiteral(" (confirmed replay)") : QString()));
                    emit remotePullingRefreshRequested(buildId);
                },
                [this, status, recordButton, pending, mutationPending, editors, buildId](
                    const RemoteMutationDto::Error& error) {
                    *mutationPending = false;
                    if (error.outcome == RemoteMutationDto::Outcome::Unknown) {
                        recordButton->setText(QStringLiteral("Retry Same Submission"));
                        recordButton->setEnabled(true);
                        status->setText(QStringLiteral("The Host response was lost. The pull may have been recorded. Reconnect and retry the same submission to confirm the result."));
                        return;
                    }
                    if (error.code == QStringLiteral("BUSY")) {
                        recordButton->setText(QStringLiteral("Retry Same Submission"));
                        recordButton->setEnabled(true);
                        status->setText(error.message);
                        return;
                    }
                    pending->reset();
                    recordButton->setText(QStringLiteral("Record Pulls"));
                    status->setText(error.code == QStringLiteral("CONFLICT")
                        ? QStringLiteral("Pulling data changed on the Host. The pull was not applied; refreshing authoritative state.")
                        : error.message);
                    emit remotePullingRefreshRequested(buildId);
                });
        });
    connect(this, &BuildsWidget::remotePullingRefreshRequested, dialog,
            [allRows, requestPage, refreshGeneration, status, buildId](int requestedBuildId) {
        if (requestedBuildId != buildId) return;
        ++*refreshGeneration;
        allRows->clear();
        status->setText(QStringLiteral("Refreshing Pulling state from BrickSuite Host..."));
        (*requestPage)(1, *refreshGeneration);
    });
    (*requestPage)(1, *refreshGeneration);
}

void BuildsWidget::refreshOpenLocalPulling(const std::optional<int>& buildId)
{
    for (InteractiveBuildPullingDialog* dialog
         : findChildren<InteractiveBuildPullingDialog*>()) {
        if (dialog && (!buildId || dialog->buildId() == *buildId))
            dialog->refreshAfterExternalCommit();
    }
}

void BuildsWidget::addRequirement()
{
    if (m_selectedBuildId <= 0) {
        QMessageBox::warning(this, "BrickSuite", "Select a build before adding a requirement.");
        return;
    }

    if (m_remoteMode) {
        if (!m_remoteBuildMutations
            || !m_remoteBuildMutations->isAvailableFor(QStringLiteral("builds.requirements.add")))
            return;
        const QString number=PartSearchCompleterHelper::canonicalPartNumber(m_partNumberEdit);
        const auto part=PartRepository().getByPartNumber(number);
        const auto color=ColorRepository().getById(m_colorCombo->currentData().toInt());
        if(!part||!color){QMessageBox::warning(this,"BrickSuite","Select a valid catalog Part and Color.");return;}
        RemoteBuildMutationDto::Request request;if(m_pendingRemoteRequirementAdd)request=*m_pendingRemoteRequirementAdd;else{request.workspaceId=m_workspaceContext.currentWorkspaceId();request.buildId=m_selectedBuildId;request.mutationId=RemoteMutationDto::newMutationId();request.partNumber=part->partNumber();request.rebrickableColorId=color->rebrickableId();request.quantityRequired=m_quantitySpin->value();request.spare=m_spareCheck->isChecked();m_pendingRemoteRequirementAdd=request;}
        m_addRequirementButton->setEnabled(false);
        m_remoteBuildMutations->submit(QStringLiteral("builds.requirements.add"),request,this,
            [this](const RemoteBuildMutationDto::Result&){m_pendingRemoteRequirementAdd.reset();m_addRequirementButton->setText("Add Requirement");m_partNumberEdit->clear();m_quantitySpin->setValue(1);m_spareCheck->setChecked(false);loadRequirements();},
            [this](const RemoteMutationDto::Error&e){if(e.outcome==RemoteMutationDto::Outcome::Unknown)m_addRequirementButton->setText("Retry Safely");else{m_pendingRemoteRequirementAdd.reset();m_addRequirementButton->setText("Add Requirement");}QMessageBox::warning(this,"Add Build Requirement",e.message);updateRequirementUiState();});
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

    const QString partNumber =
        PartSearchCompleterHelper::canonicalPartNumber(m_partNumberEdit);

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

    const auto result = BuildRequirementMutationService().add(requirement);

    if (!result.success) {
        QMessageBox::critical(this,
                              "BrickSuite",
                              result.message);
        return;
    }

    m_partNumberEdit->clear();
    m_quantitySpin->setValue(1);
    m_spareCheck->setChecked(false);
    m_partNumberEdit->setFocus();

    loadRequirements();
    emit hostBuildRequirementsMutationCommitted(
        m_workspaceContext.currentWorkspaceId(), m_selectedBuildId, false);
}

void BuildsWidget::allocateAvailable()
{
    if (!m_workspaceContext.hasCurrentWorkspace() || m_selectedBuildId <= 0) {
        QMessageBox::warning(this, "Allocate Available", "Select a Build first.");
        return;
    }

    if(m_remoteMode){
        if(!m_remoteReads||!m_remoteBuildMutations||!m_remoteBuildMutations->isAvailableFor(QStringLiteral("builds.allocateAvailable")))return;
        const int workspace=m_workspaceContext.currentWorkspaceId(),buildId=m_selectedBuildId;
        m_allocateAvailableButton->setEnabled(false);
        m_remoteReads->getBuild(workspace,buildId,this,[this,workspace,buildId](AsyncReadResult<RemoteReadDto::BuildDetail> result){
            if(workspace!=m_workspaceContext.currentWorkspaceId()||buildId!=m_selectedBuildId)return;
            if(!result.succeeded()){QMessageBox::warning(this,"Allocate Available",result.message);updateRequirementUiState();return;}
            RemoteBuildMutationDto::Request request;if(m_pendingRemoteAllocateAvailable)request=*m_pendingRemoteAllocateAvailable;else{request.workspaceId=workspace;request.buildId=buildId;request.mutationId=RemoteMutationDto::newMutationId();request.expected=expectedState(*result.value);m_pendingRemoteAllocateAvailable=request;}
            m_remoteBuildMutations->submit(QStringLiteral("builds.allocateAvailable"),request,this,
                [this](const RemoteBuildMutationDto::Result&r){m_pendingRemoteAllocateAvailable.reset();m_allocateAvailableButton->setText("Allocate Available");QMessageBox::information(this,"Allocate Available",QString("BrickSuite Host allocated %1 additional piece(s).").arg(r.effects.value("piecesAdded").toInt()));loadRequirements();},
                [this](const RemoteMutationDto::Error&e){if(e.outcome==RemoteMutationDto::Outcome::Unknown)m_allocateAvailableButton->setText("Retry Safely");else{m_pendingRemoteAllocateAvailable.reset();m_allocateAvailableButton->setText("Allocate Available");}QMessageBox::warning(this,"Allocate Available",e.message);loadRequirements();});
        });return;
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

    const auto allocationResult = BuildAllocationMutationService().allocateAvailable(
        m_selectedBuildId, preferredStorageLocationId);
    if (!allocationResult.success) {
        QMessageBox::critical(this, "Allocate Available", allocationResult.message);
        loadRequirements();
        return;
    }
    const int allocationsCreated = allocationResult.allocationsCreated;
    const int allocationsUpdated = allocationResult.allocationsUpdated;
    const int piecesAdded = allocationResult.piecesAdded;
    const int preferredPiecesAdded = allocationResult.preferredPiecesAdded;
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
    emit hostBuildRequirementsMutationCommitted(
        m_workspaceContext.currentWorkspaceId(), m_selectedBuildId, false);

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


bool BuildsWidget::selectedRemoteBuildSupportsStockFulfillment() const
{
    if (m_selectedBuildId <= 0)
        return false;

    for (int row = 0; row < m_buildsTable->rowCount(); ++row) {
        const QTableWidgetItem* item = m_buildsTable->item(row, 4);
        if (!item || item->data(Qt::UserRole).toInt() != m_selectedBuildId)
            continue;

        return BuildActionEligibility::supportsStockFulfillment(
            item->data(Qt::UserRole + 3).toBool(),
            item->data(Qt::UserRole + 1).toString(),
            item->data(Qt::UserRole + 2).toString());
    }

    return false;
}

void BuildsWidget::updateRequirementUiState()
{
    const bool enabled =
        m_workspaceContext.hasCurrentWorkspace() && m_selectedBuildId > 0;

    if (m_remoteMode) {
        const bool supportsStockFulfillment = enabled
            && selectedRemoteBuildSupportsStockFulfillment();
        const auto capability=[this](const char* name){return m_remoteBuildMutations&&m_remoteBuildMutations->isAvailableFor(QLatin1String(name));};
        const auto eligibility=BuildActionEligibility::remoteRequirementActions(m_remoteSessionConnected,supportsStockFulfillment,m_pendingRemoteRequirementAdd.has_value(),capability("builds.requirements.add"),false,false,false,capability("builds.allocateAvailable"));
        m_partNumberEdit->setEnabled(eligibility.canAdd);
        m_colorCombo->setEnabled(eligibility.canAdd);
        m_quantitySpin->setEnabled(eligibility.canAdd);
        m_spareCheck->setEnabled(eligibility.canAdd);
        m_addRequirementButton->setEnabled(
            BuildActionEligibility::canSubmitRequirement(
                eligibility.canAdd,
                PartSearchCompleterHelper::hasResolvablePart(m_partNumberEdit),
                m_colorCombo->currentData().toInt() > 0,
                m_quantitySpin->value() > 0));
        m_requirementsTable->setEnabled(enabled);
        m_loadSetFromRebrickableButton->setEnabled(false);
        m_importMocPartsButton->setEnabled(false);
        m_allocateAvailableButton->setEnabled(eligibility.canAllocateAvailable&&!m_pendingRemoteAllocateAvailable.has_value());
        m_exportPullListButton->setEnabled(false);
        m_importPullListButton->setEnabled(false);
        m_interactivePullButton->setEnabled(supportsStockFulfillment && m_remoteReads
            && m_remoteReads->isAvailableFor(QStringLiteral("builds.pulling")));
        m_interactivePullButton->setText("View Pulling...");
        m_exportMissingPartsButton->setEnabled(supportsStockFulfillment && m_remoteReads
            && m_remoteReads->isAvailableFor(QStringLiteral("builds.missingParts")));
        m_exportMissingPartsButton->setText("Export Missing Parts CSV");
        m_procureMissingPartsButton->setEnabled(false);
        for (int column = 4; column < 13; ++column)
            m_requirementsTable->setColumnHidden(column, false);
        return;
    }

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
    m_addRequirementButton->setEnabled(
        BuildActionEligibility::canSubmitRequirement(
            canManuallyEditRequirements,
            PartSearchCompleterHelper::hasResolvablePart(m_partNumberEdit),
            m_colorCombo->currentData().toInt() > 0,
            m_quantitySpin->value() > 0));

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
    bool canViewPulling = false;
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
                && !build->setNumber().trimmed().isEmpty()
                && !(build->inventoryMode() == "Stock" && build->setCatalogId() > 0);

            canImportMoc =
                build->buildType() == "MOC"
                && build->inventoryMode() == "Stock";

            canAllocateAvailable = build->inventoryMode() == "Stock";
            const bool supportsStockFulfillment =
                BuildActionEligibility::supportsStockFulfillment(
                    build->isActive(), build->inventoryMode(), build->status());
            canExportPullList = build->inventoryMode() == "Stock";
            canViewPulling = supportsStockFulfillment;
            canExportMissingParts = supportsStockFulfillment;
            canProcureMissingParts = build->inventoryMode() == "Stock";
        }
    }

    m_allocateAvailableButton->setEnabled(canAllocateAvailable);
    m_exportPullListButton->setEnabled(canExportPullList);
    m_importPullListButton->setEnabled(canExportPullList);
    m_interactivePullButton->setEnabled(canViewPulling);
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

    const int remembered = m_sessionStorageSelectionService.rememberedDestination(
        build->workspaceId());
    const int rememberedIndex = storageCombo->findData(remembered);
    if (rememberedIndex >= 0)
        storageCombo->setCurrentIndex(rememberedIndex);

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

    const auto stored = BuildLifecycleService().storeCompleteSetSpare(
        m_selectedBuildId, requirementId, storageLocationId, quantity);
    if (!stored.success) {
        QMessageBox::critical(this, "Store Spare", stored.message);
        return;
    }

    m_sessionStorageSelectionService.rememberDestination(
        stored.build.workspaceId(), storageLocationId);

    emit hostBuildRequirementsMutationCommitted(
        stored.build.workspaceId(), stored.build.id(), true);

    qInfo() << "Complete Set spare stored."
            << "BuildId:" << stored.build.id()
            << "RequirementId:" << requirementId
            << "PartId:" << requirement->partId()
            << "ColorId:" << requirement->colorId()
            << "ManufacturerId:" << stored.build.manufacturerId()
            << "Quantity:" << quantity
            << "StorageLocationId:" << storageLocationId
            << "QuantityReleased:" << requirement->quantityReleased() + quantity;

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

    if (dialog.exec() == QDialog::Accepted) {
        loadRequirements();
        emit hostBuildRequirementsMutationCommitted(
            m_workspaceContext.currentWorkspaceId(), m_selectedBuildId, true);
    }
}

void BuildsWidget::interactivePulling()
{
    if (m_selectedBuildId <= 0) {
        QMessageBox::warning(this,
                             "Interactive Build Pulling",
                             "Select a Build first.");
        return;
    }

    if (m_remoteMode) {
        showRemotePulling(m_selectedBuildId);
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
    connect(&dialog, &InteractiveBuildPullingDialog::pullsRecorded, this, [this] {
        emit hostBuildRequirementsMutationCommitted(
            m_workspaceContext.currentWorkspaceId(), m_selectedBuildId, true);
    });
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

    if (m_remoteMode) {
        const int buildId = m_selectedBuildId;
        const int workspaceId = m_workspaceContext.currentWorkspaceId();
        m_exportMissingPartsButton->setEnabled(false);
        m_remoteReads->getBuild(workspaceId, buildId, this,
            [this, workspaceId, buildId](AsyncReadResult<RemoteReadDto::BuildDetail> buildResult) {
                if (!buildResult.succeeded()) {
                    updateRequirementUiState();
                    QMessageBox::critical(this, "Export Missing Parts", buildResult.message);
                    return;
                }
                const RemoteReadDto::BuildDetail build = *buildResult.value;
                auto rows = std::make_shared<QList<RemoteReadDto::MissingPart>>();
                auto requestPage = std::make_shared<std::function<void(int)>>();
                *requestPage = [this, workspaceId, buildId, build, rows, requestPage](int page) {
                    m_remoteReads->missingParts(workspaceId, buildId,
                        {page, RemoteReadDto::MaximumPageSize}, this,
                        [this, workspaceId, buildId, build, rows, requestPage](
                            AsyncReadResult<RemoteReadDto::Page<RemoteReadDto::MissingPart>> result) {
                            if (!result.succeeded()) {
                                updateRequirementUiState();
                                QMessageBox::critical(this, "Export Missing Parts", result.message);
                                return;
                            }
                            rows->append(result.value->rows);
                            if (rows->size() < result.value->totalRows) {
                                (*requestPage)(result.value->page + 1);
                                return;
                            }
                            updateRequirementUiState();
                            if (rows->isEmpty()) {
                                QMessageBox::information(this, "Export Missing Parts",
                                                         "This Build currently has no missing non-spare parts.");
                                return;
                            }

                            QString safeReference = build.setNumber.trimmed();
                            if (safeReference.isEmpty()) safeReference = build.name.trimmed();
                            safeReference.replace(QRegularExpression(R"([^A-Za-z0-9_-]+)"), "_");
                            const QString fileName = QFileDialog::getSaveFileName(
                                this, "Export Missing Parts CSV",
                                QStringLiteral("BrickSuite_Missing_%1.csv").arg(safeReference),
                                "CSV Files (*.csv)");
                            if (fileName.isEmpty()) return;
                            QFile file(fileName);
                            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                                QMessageBox::critical(this, "Export Missing Parts",
                                                       QString("Unable to create:\n\n%1").arg(fileName));
                                return;
                            }
                            QTextStream stream(&file);
                            stream << QChar(0xFEFF);
                            const auto csv = [](QString value) {
                                value.replace('"', "\"\"");
                                return QString("\"%1\"").arg(value);
                            };
                            stream << "Build,Set Number,Part Number,Part Name,Color,Required,"
                                      "Pulled,Remaining,Available,Missing\n";
                            int totalMissing = 0;
                            for (const auto& row : std::as_const(*rows)) {
                                stream << csv(build.name) << ',' << csv(build.setNumber) << ','
                                       << csv(row.partNumber) << ',' << csv(row.partNameFallback) << ','
                                       << csv(row.colorNameFallback) << ',' << row.required << ','
                                       << row.pulled << ',' << row.remaining << ',' << row.available << ','
                                       << row.missing << '\n';
                                totalMissing += row.missing;
                            }
                            file.close();
                            QMessageBox::information(this, "Export Missing Parts",
                                QString("Missing Parts List exported successfully.\n\n"
                                        "Part/Color Rows: %1\nPieces Missing: %2\nFile:\n%3")
                                    .arg(rows->size()).arg(totalMissing).arg(fileName));
                        });
                };
                (*requestPage)(1);
            });
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

    const QList<MissingPartsService::MissingPart> missingParts =
        m_buildService.missingParts(
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

    emit hostBuildRequirementsMutationCommitted(
        m_workspaceContext.currentWorkspaceId(), m_selectedBuildId, false);

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
