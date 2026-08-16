#include "BuildsWidget.h"

#include "../../app/WorkspaceContext.h"
#include "../../database/DatabaseManager.h"
#include "AllocateBuildRequirementDialog.h"
#include "DisassembleSetDialog.h"
#include "EditBuildDialog.h"
#include "EditBuildRequirementDialog.h"
#include "ImportPullListDialog.h"
#include "SetImportPreviewDialog.h"

#include "../../models/Build.h"
#include "../../models/BuildRequirement.h"
#include "../../models/Color.h"
#include "../../models/Part.h"
#include "../../models/SetCatalogItem.h"

#include "../../repositories/BuildAllocationRepository.h"
#include "../../repositories/BuildRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/SetCatalogRepository.h"
#include "../../repositories/StorageLocationRepository.h"

#include "../../import/RebrickableMocCsvImporter.h"
#include "../../services/builds/MissingPartsService.h"

#include "../../ui/helpers/ColorComboHelper.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
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
#include <QShowEvent>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>

namespace {

/***** Helpers *****/
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

    //
    // Expected Rebrickable alternate-build filename:
    //
    // rebrickable_parts_moc-227137-77239-cabrio.csv
    //
    // Capture:
    //   1 = moc-227137
    //   2 = 77239
    //
    const QRegularExpression expression(R"(^rebrickable_parts_(moc-\d+)-(\d+)(?:-|$))",
                                        QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch match = expression.match(baseName);

    if (!match.hasMatch())
        return metadata;

    metadata.mocNumber = match.captured(1).toUpper();

    const QString sourceSetBase = match.captured(2);

    if (!sourceSetBase.isEmpty()) {
        //
        // Alternate-build filenames contain the base
        // Set number. Rebrickable's normal retail Set
        // reference uses inventory suffix "-1".
        //
        metadata.sourceSetNumber = sourceSetBase + "-1";
    }

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

    //
    // ------------------------------------------------------------
    // New Build
    // ------------------------------------------------------------
    //
    // The group is checkable so it can collapse when an existing
    // build is selected. The user can reopen it whenever another
    // build needs to be created.
    //
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

    formLayout->addRow("Name:", m_nameEdit);

    formLayout->addRow("Status:", m_statusCombo);

    formLayout->addRow("Notes:", m_notesEdit);

    formLayout->addRow(QString(), m_addButton);

    newBuildGroupLayout->addWidget(m_newBuildContent);

    //
    // Important:
    // Add the GROUP to the main layout, not m_newBuildContent.
    //
    mainLayout->addWidget(m_newBuildGroup);

    connect(m_newBuildGroup, &QGroupBox::toggled, m_newBuildContent, &QWidget::setVisible);

    //
    // ------------------------------------------------------------
    // Existing Builds
    // ------------------------------------------------------------
    //
    auto* existingGroup = new QGroupBox("Builds", this);

    auto* existingLayout = new QVBoxLayout(existingGroup);

    auto* existingHeaderLayout = new QHBoxLayout();

    existingHeaderLayout->addStretch(1);

    m_showArchivedBuildsCheck = new QCheckBox("Show Archived", existingGroup);

    existingHeaderLayout->addWidget(m_showArchivedBuildsCheck);

    existingLayout->addLayout(existingHeaderLayout);

    m_buildsTable = new QTableWidget(existingGroup);

    m_buildsTable->setColumnCount(7);

    m_buildsTable->setHorizontalHeaderLabels(QStringList() << "Type"
                                                           << "Set / MOC #"
                                                           << "Inventory Mode"
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

    m_buildsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    m_buildsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    m_buildsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);

    m_buildsTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    //
    // Add the Builds table to its group layout.
    //
    existingLayout->addWidget(m_buildsTable);

    //
    // ------------------------------------------------------------
    // Build Requirements
    // ------------------------------------------------------------
    //
    auto* requirementsGroup = new QGroupBox("Build Requirements", this);

    auto* requirementsLayout = new QVBoxLayout(requirementsGroup);

    auto* requirementsHeaderLayout = new QHBoxLayout();

    m_requirementsLabel = new QLabel("Select a build to view its requirements.", requirementsGroup);

    m_loadSetFromRebrickableButton = new QPushButton("Load Set from Rebrickable", requirementsGroup);

    m_importMocPartsButton = new QPushButton("Import MOC Parts CSV", requirementsGroup);

    m_allocateAvailableButton = new QPushButton("Allocate Available", requirementsGroup);

    m_exportMissingPartsButton = new QPushButton("Export Missing Parts CSV", requirementsGroup);

    m_exportPullListButton = new QPushButton("Export Pull List CSV", requirementsGroup);
    m_importPullListButton = new QPushButton("Import Pull List CSV", requirementsGroup);

    requirementsHeaderLayout->addWidget(m_requirementsLabel, 1);

    requirementsHeaderLayout->addWidget(m_loadSetFromRebrickableButton);

    requirementsHeaderLayout->addWidget(m_importMocPartsButton);

    requirementsHeaderLayout->addWidget(m_allocateAvailableButton);

    requirementsHeaderLayout->addWidget(m_exportMissingPartsButton);

    requirementsHeaderLayout->addWidget(m_exportPullListButton);

    requirementsHeaderLayout->addWidget(m_importPullListButton);

    requirementsLayout->addLayout(requirementsHeaderLayout);

    //
    // Compact horizontal requirement entry row.
    //
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

    //
    // Requirements table.
    //
    m_requirementsTable = new QTableWidget(requirementsGroup);

    m_requirementsTable->setColumnCount(13);

    m_requirementsTable->setHorizontalHeaderLabels(QStringList() << "Part #"
                                                                 << "Name"
                                                                 << "Color"
                                                                 << "Required"
                                                                 << "Pulled"
                                                                 << "Remaining"
                                                                 << "Owned"
                                                                 << "This Build"
                                                                 << "Other Builds"
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

    for (int column = 3; column <= 13; ++column) {
        m_requirementsTable->horizontalHeader()->setSectionResizeMode(column,
                                                                      QHeaderView::ResizeToContents);
    }

    requirementsLayout->addWidget(m_requirementsTable, 1);

    //
    // ------------------------------------------------------------
    // Builds / Requirements vertical splitter
    // ------------------------------------------------------------
    //
    // The user can resize the middle Builds area and lower
    // Build Requirements area depending on the current task.
    //
    m_buildsRequirementsSplitter = new QSplitter(Qt::Vertical, this);

    m_buildsRequirementsSplitter->addWidget(existingGroup);

    m_buildsRequirementsSplitter->addWidget(requirementsGroup);

    //
    // Both sections remain usable and cannot be collapsed
    // completely by dragging the splitter.
    //
    m_buildsRequirementsSplitter->setCollapsible(0, false);

    m_buildsRequirementsSplitter->setCollapsible(1, false);

    //
    // Initial preference: Requirements gets more room,
    // matching the previous 1:3 layout.
    //
    m_buildsRequirementsSplitter->setStretchFactor(0, 1);

    m_buildsRequirementsSplitter->setStretchFactor(1, 3);

    //
    // Give the splitter a sensible default before restoring
    // the user's previously saved position.
    //
    m_buildsRequirementsSplitter->setSizes(QList<int>() << 200 << 600);

    mainLayout->addWidget(m_buildsRequirementsSplitter, 1);

    //
    // Restore the position from the previous BrickSuite
    // session when available.
    //
    QSettings settings;

    const QByteArray splitterState = settings.value("Builds/buildsRequirementsSplitterState")
                                         .toByteArray();

    if (!splitterState.isEmpty()) {
        m_buildsRequirementsSplitter->restoreState(splitterState);
    }

    //
    // Remember the position whenever the user moves it.
    //
    connect(m_buildsRequirementsSplitter, &QSplitter::splitterMoved, this, [this]() {
        QSettings settings;

        settings.setValue("Builds/buildsRequirementsSplitterState",
                          m_buildsRequirementsSplitter->saveState());
    });

    //
    // Bottom status.
    //
    m_statusLabel = new QLabel(this);

    mainLayout->addWidget(m_statusLabel);

    //
    // ------------------------------------------------------------
    // Connections
    // ------------------------------------------------------------
    //
    connect(m_addButton, &QPushButton::clicked, this, &BuildsWidget::addBuild);

    connect(m_allocateAvailableButton,
            &QPushButton::clicked,
            this,
            &BuildsWidget::allocateAvailable);

    connect(m_exportMissingPartsButton,
            &QPushButton::clicked,
            this,
            &BuildsWidget::exportMissingParts);

    connect(m_exportPullListButton, &QPushButton::clicked, this, &BuildsWidget::exportPullList);
    connect(m_importPullListButton, &QPushButton::clicked, this, &BuildsWidget::importPullList);

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

            //
            // MOCs are always built from stock.
            //
            const int stockIndex = m_inventoryModeCombo->findData("Stock");

            if (stockIndex >= 0) {
                m_inventoryModeCombo->setCurrentIndex(stockIndex);
            }
        }
    });

    connect(m_buildsTable,
            &QTableWidget::itemSelectionChanged,
            this,
            &BuildsWidget::buildSelectionChanged);

    connect(m_showArchivedBuildsCheck, &QCheckBox::toggled, this, [this]() {
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
        }
    });

    loadColors();

    workspaceChanged(m_workspaceContext.currentWorkspaceId());

    updateRequirementUiState();
}

void BuildsWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    //
    // Inventory may have changed while the user was working
    // elsewhere in BrickSuite. Recalculate only the currently
    // selected Build Requirements when the Builds page becomes
    // visible. This updates Owned / Available / Missing without
    // unnecessarily rebuilding the Builds list.
    //
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

    const QList<Build> builds = repository.getByWorkspace(
        m_workspaceContext.currentWorkspaceId(),
        m_showArchivedBuildsCheck && m_showArchivedBuildsCheck->isChecked());

    int row = 0;

    for (const Build& build : builds) {
        m_buildsTable->insertRow(row);

        auto* typeItem = new QTableWidgetItem(build.buildType());

        auto* setNumberItem = new QTableWidgetItem(build.setNumber());

        QString inventoryModeText;

        if (build.inventoryMode() == "CompleteSet") {
            inventoryModeText = "Complete Set";
        } else {
            inventoryModeText = "Build from Stock";
        }

        auto* inventoryModeItem = new QTableWidgetItem(inventoryModeText);

        auto* nameItem = new QTableWidgetItem(build.name());

        auto* statusItem = new QTableWidgetItem(build.status());

        auto* notesItem = new QTableWidgetItem(build.notes());

        auto* actionCombo = new QComboBox(m_buildsTable);

        actionCombo->addItem("Actions...", QString());

        if (build.isActive()) {
            actionCombo->addItem("Edit Build...", "edit");

            //
            // A Build may be archived only after it has been
            // disassembled. This prevents an assembled, pulling,
            // or planned Build from disappearing from the active
            // workflow while it still represents live work.
            //
            if (build.status() == "Planned" || build.status() == "Pulling") {
                actionCombo->addItem("Cancel Build...", "cancel");
            }

            if (build.status() == "Disassembled" || build.status() == "Cancelled") {
                actionCombo->addItem("Archive Build", "archive");
            }
        } else {
            actionCombo->addItem("Reactivate Build", "reactivate");

            nameItem->setText(build.name() + " (Archived)");
            statusItem->setText(build.status() + " / Archived");
        }

        if (build.isActive() && build.inventoryMode() == "Stock" && build.status() != "Complete"
            && build.status() != "Disassembled" && build.status() != "Cancelled") {
            actionCombo->addItem("Complete Build...", "complete");
        }

        if (build.isActive() && build.inventoryMode() == "Stock" && build.status() == "Complete") {
            actionCombo->addItem(build.buildType() == "MOC" ? "Disassemble MOC..."
                                                            : "Disassemble Build...",
                                 "disassemble");
        }

        if (build.isActive() && build.inventoryMode() == "CompleteSet" && build.status() == "Complete") {
            actionCombo->addItem("Disassemble Set...", "disassemble");
        }

        //
        // Store Build ID on the Name item.
        //
        nameItem->setData(Qt::UserRole, build.id());

        m_buildsTable->setItem(row, 0, typeItem);

        m_buildsTable->setItem(row, 1, setNumberItem);

        m_buildsTable->setItem(row, 2, inventoryModeItem);

        m_buildsTable->setItem(row, 3, nameItem);

        m_buildsTable->setItem(row, 4, statusItem);

        m_buildsTable->setItem(row, 5, notesItem);

        m_buildsTable->setCellWidget(row, 6, actionCombo);

        const int buildId = build.id();

        connect(actionCombo,
                &QComboBox::currentIndexChanged,
                this,
                [this, actionCombo, buildId](int index) {
                    if (index <= 0)
                        return;

                    const QString action = actionCombo->itemData(index).toString();

                    //
                    // Reset immediately so the same action
                    // can be selected again later.
                    //
                    actionCombo->setCurrentIndex(0);

                    if (action == "edit") {
                        EditBuildDialog dialog(buildId, this);

                        if (dialog.exec() == QDialog::Accepted) {
                            //
                            // Reload and keep the edited Build
                            // selected.
                            //
                            selectBuild(buildId);
                        }
                    } else if (action == "cancel") {
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
                        const QList<BuildRequirement> requirements
                            = requirementRepository.getByBuild(buildId);

                        int totalPulled = 0;

                        for (const BuildRequirement& requirement : requirements) {
                            totalPulled += qMax(requirement.quantityPulled(), 0);
                        }

                        QString message
                            = QString("Cancel this Build?\n\n%1\n\n"
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

                        const QMessageBox::StandardButton response
                            = QMessageBox::question(this,
                                                    "Cancel Build",
                                                    message,
                                                    QMessageBox::Yes | QMessageBox::No,
                                                    QMessageBox::No);

                        if (response != QMessageBox::Yes)
                            return;

                        //
                        // If physical pieces have already been pulled, reuse
                        // BrickSuite's existing controlled return workflow.
                        // That workflow restores loose inventory, records the
                        // movements, and reduces Quantity Pulled safely.
                        //
                        if (totalPulled > 0) {
                            DisassembleSetDialog dialog(buildId, this);

                            if (dialog.exec() != QDialog::Accepted)
                                return;

                            //
                            // The return dialog finishes in Disassembled.
                            // Cancellation is the user's actual intent, so
                            // convert the terminal state to Cancelled.
                            //
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
                            QMessageBox::critical(this,
                                                  "Cancel Build",
                                                  "Unable to start the cancellation.");
                            return;
                        }

                        //
                        // Allocations are reservations, not historical movement
                        // records. Once a Build is cancelled they must be
                        // released so the loose pieces become available to
                        // other Builds.
                        //
                        BuildAllocationRepository allocationRepository;

                        if (!allocationRepository.removeAllForBuild(buildId)) {
                            database.rollback();

                            QMessageBox::critical(
                                this,
                                "Cancel Build",
                                "Unable to release the Build's inventory allocations.");
                            return;
                        }

                        build->setStatus("Cancelled");

                        if (!buildRepository.update(*build)) {
                            database.rollback();

                            QMessageBox::critical(this,
                                                  "Cancel Build",
                                                  "Unable to mark the Build Cancelled.");
                            return;
                        }

                        if (!database.commit()) {
                            database.rollback();

                            QMessageBox::critical(this,
                                                  "Cancel Build",
                                                  "Unable to commit the cancellation.");
                            return;
                        }

                        selectBuild(buildId);

                        QMessageBox::information(
                            this,
                            "Cancel Build",
                            QString("\"%1\" is now Cancelled.\n\n"
                                    "You may archive it from the Build Actions menu.")
                                .arg(build->name()));
                        return;
                    } else if (action == "archive") {
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

                        const QMessageBox::StandardButton response
                            = QMessageBox::question(
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
                            QMessageBox::critical(this,
                                                  "Archive Build",
                                                  "Unable to archive the selected Build.");
                            return;
                        }

                        m_selectedBuildId = 0;
                        loadBuilds();
                        loadRequirements();
                        updateRequirementUiState();

                        QMessageBox::information(this,
                                                 "Archive Build",
                                                 QString("\"%1\" has been archived.")
                                                     .arg(build->name()));
                        return;
                    } else if (action == "reactivate") {
                        BuildRepository repository;

                        std::optional<Build> build = repository.getById(buildId);

                        if (!build) {
                            QMessageBox::critical(this,
                                                  "Reactivate Build",
                                                  "Unable to load the selected Build.");
                            return;
                        }

                        //
                        // Cancelled is a terminal workflow state. If the
                        // user brings an archived Cancelled Build back,
                        // it returns to Planned so normal work can resume.
                        //
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

                        selectBuild(buildId);

                        QMessageBox::information(this,
                                                 "Reactivate Build",
                                                 QString("\"%1\" is active again.")
                                                     .arg(build->name()));
                        return;
                    } else if (action == "complete") {
                        BuildRepository buildRepository;

                        std::optional<Build> build = buildRepository.getById(buildId);

                        if (!build) {
                            QMessageBox::critical(this,
                                                  "Complete Build",
                                                  "Unable to load the selected Build.");

                            return;
                        }

                        BuildRequirementRepository requirementRepository;

                        const QList<BuildRequirement> requirements = requirementRepository
                                                                         .getByBuild(buildId);

                        if (requirements.isEmpty()) {
                            QMessageBox::information(this,
                                                     "Complete Build",
                                                     "This Build does not have any requirements.");

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

                            const int remaining = qMax(requirement.quantityRequired()
                                                           - requirement.quantityPulled(),
                                                       0);

                            if (remaining > 0) {
                                ++incompleteRows;
                            }
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

                        QString message = QString("Mark this Build Complete?\n\n"
                                                  "%1")
                                              .arg(build->name());

                        if (!build->setNumber().trimmed().isEmpty()) {
                            message += QString("\nSet: %1").arg(build->setNumber());
                        }

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

                        const QMessageBox::StandardButton response
                            = QMessageBox::question(this,
                                                    "Complete Build",
                                                    message,
                                                    QMessageBox::Yes | QMessageBox::No,
                                                    QMessageBox::No);

                        if (response != QMessageBox::Yes)
                            return;

                        build->setStatus("Complete");

                        if (!buildRepository.update(*build)) {
                            QMessageBox::critical(this,
                                                  "Complete Build",
                                                  "Unable to mark the Build Complete.");

                            return;
                        }

                        selectBuild(buildId);

                        QMessageBox::information(this,
                                                 "Complete Build",
                                                 QString("\"%1\" is now Complete.")
                                                     .arg(build->name()));

                        return;
                    } else if (action == "disassemble") {
                        DisassembleSetDialog dialog(buildId, this);

                        if (dialog.exec() == QDialog::Accepted) {
                            //
                            // Refresh the Build Status and the
                            // requirement calculations.
                            //
                            selectBuild(buildId);

                            return;
                        }
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
    m_nameEdit->clear();
    m_notesEdit->clear();

    m_statusCombo->setCurrentIndex(0);

    loadBuilds();

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

    m_nameEdit->setEnabled(enabled);

    m_statusCombo->setEnabled(enabled);

    m_notesEdit->setEnabled(enabled);

    m_addButton->setEnabled(enabled);

    m_newBuildGroup->setEnabled(enabled);
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

    QTableWidgetItem* nameItem = m_buildsTable->item(row, 3);

    m_selectedBuildId = nameItem->data(Qt::UserRole).toInt();

    loadRequirements();

    updateRequirementUiState();

    //
    // Once an existing Build is selected, collapse
    // the creation form and give the workspace to
    // the requirements area.
    //
    if (m_selectedBuildId > 0) {
        m_newBuildGroup->setChecked(false);
    }
}

void BuildsWidget::loadRequirements()
{
    m_requirementsTable->setRowCount(0);

    if (m_selectedBuildId <= 0) {
        m_requirementsLabel->setText("Select a build to view its requirements.");

        return;
    }

    //
    // Show selected Build identity.
    //
    const int buildRow = m_buildsTable->currentRow();

    QString buildDescription;

    if (buildRow >= 0) {
        QTableWidgetItem* setNumberItem = m_buildsTable->item(buildRow, 1);

        QTableWidgetItem* nameItem = m_buildsTable->item(buildRow, 3);

        const QString setNumber = setNumberItem ? setNumberItem->text() : QString();

        const QString name = nameItem ? nameItem->text() : QString();

        if (!setNumber.isEmpty()) {
            buildDescription = QString("%1 — %2").arg(setNumber, name);
        } else {
            buildDescription = name;
        }
    }

    m_requirementsLabel->setText(QString("Requirements for: %1").arg(buildDescription));

    BuildRequirementRepository requirementRepository;

    const QList<BuildRequirement> requirements = requirementRepository.getByBuild(m_selectedBuildId);

    PartRepository partRepository;

    ColorRepository colorRepository;

    InventoryRecordRepository inventoryRepository;

    BuildAllocationRepository allocationRepository;

    int row = 0;

    for (const BuildRequirement& requirement : requirements) {
        const std::optional<Part> part = partRepository.getById(requirement.partId());

        const std::optional<Color> color = colorRepository.getById(requirement.colorId());

        m_requirementsTable->insertRow(row);

        const QString partNumber = part ? part->partNumber() : QString();

        const QString partName = part ? part->name() : QString("(Part unavailable)");

        const QString colorName = color ? color->name() : QString("(Color unavailable)");

        auto* partNumberItem = new QTableWidgetItem(partNumber);

        partNumberItem->setData(Qt::UserRole, requirement.id());

        auto* nameItem = new QTableWidgetItem(partName);

        auto* colorItem = new QTableWidgetItem(colorName);

        //
        // Color text using the existing contrast helper.
        //
        if (color) {
            QString normalizedRgb = color->rgb().trimmed();

            if (!normalizedRgb.isEmpty() && !normalizedRgb.startsWith('#')) {
                normalizedRgb.prepend('#');
            }

            const QColor sourceColor(normalizedRgb);

            if (sourceColor.isValid()) {
                const QColor backgroundColor = m_requirementsTable->palette().color(QPalette::Base);

                colorItem->setForeground(
                    ColorComboHelper::readableColor(sourceColor, backgroundColor));
            }
        }

        const int workspaceId = m_workspaceContext.currentWorkspaceId();

        const int owned = inventoryRepository.totalQuantityForPartColor(workspaceId,
                                                                        requirement.partId(),
                                                                        requirement.colorId());

        const int quantityPulled = requirement.quantityPulled();

        const int remainingRequired = qMax(requirement.quantityRequired() - quantityPulled, 0);

        //
        // Total quantity committed to any Build
        // in this workspace.
        //
        const int totalAllocated = allocationRepository
                                       .totalAllocatedForPartColor(workspaceId,
                                                                   requirement.partId(),
                                                                   requirement.colorId());

        //
        // Quantity already committed specifically
        // to the currently selected Build.
        //
        const int thisBuildAllocated
            = allocationRepository.totalAllocatedForPartColorForBuild(m_selectedBuildId,
                                                                      requirement.partId(),
                                                                      requirement.colorId());

        //
        // Anything allocated elsewhere is still owned,
        // but is not available to this Build.
        //
        const int otherBuildsAllocated = qMax(totalAllocated - thisBuildAllocated, 0);

        //
        // Available means physically owned and not
        // committed to any Build.
        //
        const int available = qMax(owned - totalAllocated, 0);

        //
        // Spare requirements are informational / optional.
        //
        // They do not make the Build incomplete and therefore
        // never contribute to Missing by default.
        //
        const int missing = requirement.isSpare()
                                ? 0
                                : qMax(remainingRequired - thisBuildAllocated - available, 0);

        auto* requiredItem = new QTableWidgetItem(QString::number(requirement.quantityRequired()));

        auto* pulledItem = new QTableWidgetItem(QString::number(quantityPulled));

        auto* remainingItem = new QTableWidgetItem(QString::number(remainingRequired));

        auto* ownedItem = new QTableWidgetItem(QString::number(owned));

        auto* thisBuildItem = new QTableWidgetItem(QString::number(thisBuildAllocated));

        auto* otherBuildsItem = new QTableWidgetItem(QString::number(otherBuildsAllocated));

        auto* availableItem = new QTableWidgetItem(QString::number(available));

        auto* missingItem = new QTableWidgetItem(QString::number(missing));

        auto* spareItem = new QTableWidgetItem(requirement.isSpare() ? "Yes" : "No");

        auto* actionCombo = new QComboBox(m_requirementsTable);

        actionCombo->addItem("Actions...");
        actionCombo->addItem("Edit", "edit");
        actionCombo->addItem("Delete", "delete");
        actionCombo->addItem("Allocate...", "allocate");

        const int requirementId = requirement.id();
        const bool requirementIsSpare = requirement.isSpare();

        connect(actionCombo,
                &QComboBox::currentIndexChanged,
                this,
                [this, actionCombo, requirementId, requirementIsSpare](int index) {
                    if (index <= 0)
                        return;

                    const QString action = actionCombo->itemData(index).toString();

                    if (action == "edit") {
                        EditBuildRequirementDialog dialog(requirementId, this);

                        if (dialog.exec() == QDialog::Accepted) {
                            loadRequirements();

                            return;
                        }
                    } else if (action == "delete") {
                        const QMessageBox::StandardButton response
                            = QMessageBox::warning(this,
                                                   "Delete Build Requirement",
                                                   "Delete this requirement from the build?",
                                                   QMessageBox::Yes | QMessageBox::No,
                                                   QMessageBox::No);

                        if (response == QMessageBox::Yes) {
                            BuildRequirementRepository repository;

                            if (!repository.remove(requirementId)) {
                                QMessageBox::critical(this,
                                                      "BrickSuite",
                                                      "Unable to delete the build requirement.");
                            } else {
                                loadRequirements();

                                return;
                            }
                        }
                    } else if (action == "allocate") {
                        //
                        // Spare requirements are informational and
                        // are not allocated automatically.
                        //
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

                        const std::optional<Build> build = buildRepository.getById(
                            m_selectedBuildId);

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

                        AllocateBuildRequirementDialog dialog(m_workspaceContext.currentWorkspaceId(),
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
        ownedItem->setTextAlignment(Qt::AlignCenter);
        thisBuildItem->setTextAlignment(Qt::AlignCenter);
        otherBuildsItem->setTextAlignment(Qt::AlignCenter);
        availableItem->setTextAlignment(Qt::AlignCenter);
        missingItem->setTextAlignment(Qt::AlignCenter);
        spareItem->setTextAlignment(Qt::AlignCenter);
        requiredItem->setTextAlignment(Qt::AlignCenter);
        pulledItem->setTextAlignment(Qt::AlignCenter);
        remainingItem->setTextAlignment(Qt::AlignCenter);

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

    const QList<BuildRequirement> requirements = requirementRepository.getByBuild(m_selectedBuildId);

    int regularRequirementCount = 0;

    for (const BuildRequirement& requirement : requirements) {
        if (!requirement.isSpare())
            ++regularRequirementCount;
    }

    if (regularRequirementCount == 0) {
        QMessageBox::information(this,
                                 "Allocate Available",
                                 "This Build does not have any non-spare requirements to allocate.");

        return;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    if (!database.transaction()) {
        QMessageBox::critical(this,
                              "Allocate Available",
                              "Unable to start the automatic allocation transaction.");

        return;
    }

    int allocationsCreated = 0;
    int allocationsUpdated = 0;
    int piecesAdded = 0;

    for (const BuildRequirement& requirement : requirements) {
        //
        // Spare requirements are intentionally informational / optional.
        // Match the existing single-requirement Allocate behavior and skip them.
        //
        if (requirement.isSpare())
            continue;

        const int remainingRequired = qMax(requirement.quantityRequired()
                                               - requirement.quantityPulled(),
                                           0);

        if (remainingRequired <= 0)
            continue;

        int thisBuildAllocated = allocationRepository.totalAllocatedForPartColorForBuild(
            m_selectedBuildId,
            requirement.partId(),
            requirement.colorId());

        int stillNeeded = qMax(remainingRequired - thisBuildAllocated, 0);

        if (stillNeeded <= 0)
            continue;

        //
        // InventoryRecordRepository returns deterministic ordering by
        // storage_location_id, condition, ownership_type. We deliberately
        // use that existing ordering instead of inventing a storage preference.
        //
        const QList<InventoryRecord> records = inventoryRepository.getByPartColor(
            m_workspaceContext.currentWorkspaceId(),
            requirement.partId(),
            requirement.colorId());

        for (const InventoryRecord& record : records) {
            if (stillNeeded <= 0)
                break;

            const int totalAllocated = allocationRepository.totalAllocatedForInventoryRecord(
                record.id());

            const int currentBuildAllocated
                = allocationRepository.totalAllocatedForInventoryRecordForBuild(record.id(),
                                                                                 m_selectedBuildId);

            const int otherBuildsAllocated = qMax(totalAllocated - currentBuildAllocated, 0);

            const int maximumForThisBuild = qMax(record.quantity() - otherBuildsAllocated, 0);

            const int additionalCapacity = qMax(maximumForThisBuild - currentBuildAllocated, 0);

            if (additionalCapacity <= 0)
                continue;

            const int quantityToAdd = qMin(stillNeeded, additionalCapacity);

            if (quantityToAdd <= 0)
                continue;

            const int newAllocationQuantity = currentBuildAllocated + quantityToAdd;

            const QList<BuildAllocation> recordAllocations = allocationRepository.getByInventoryRecord(
                record.id());

            std::optional<BuildAllocation> existingAllocation;

            for (const BuildAllocation& allocation : recordAllocations) {
                if (allocation.buildId() == m_selectedBuildId) {
                    existingAllocation = allocation;
                    break;
                }
            }

            if (existingAllocation) {
                existingAllocation->setQuantityAllocated(newAllocationQuantity);

                if (!allocationRepository.update(*existingAllocation)) {
                    database.rollback();

                    QMessageBox::critical(this,
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
                allocation.setInventoryRecordId(record.id());
                allocation.setPartId(requirement.partId());
                allocation.setColorId(requirement.colorId());
                allocation.setStorageLocationId(record.storageLocationId());
                allocation.setQuantityAllocated(quantityToAdd);

                if (!allocationRepository.create(allocation)) {
                    database.rollback();

                    QMessageBox::critical(this,
                                          "Allocate Available",
                                          "Unable to create a Build allocation. "
                                          "No automatic allocations were saved.");

                    loadRequirements();

                    return;
                }

                ++allocationsCreated;
            }

            piecesAdded += quantityToAdd;
            stillNeeded -= quantityToAdd;
            thisBuildAllocated += quantityToAdd;
        }
    }

    if (!database.commit()) {
        database.rollback();

        QMessageBox::critical(this,
                              "Allocate Available",
                              "Unable to save the automatic allocations. "
                              "No automatic allocations were saved.");

        loadRequirements();

        return;
    }

    //
    // Recalculate the final Build state for the summary. This is intentionally
    // done once after the transaction rather than refreshing for every row.
    //
    int satisfiedRequirements = 0;
    int partiallySatisfiedRequirements = 0;
    int stillMissingRequirements = 0;
    int spareRequirementsSkipped = 0;

    for (const BuildRequirement& requirement : requirements) {
        if (requirement.isSpare()) {
            ++spareRequirementsSkipped;
            continue;
        }

        const int remainingRequired = qMax(requirement.quantityRequired()
                                               - requirement.quantityPulled(),
                                           0);

        if (remainingRequired <= 0) {
            ++satisfiedRequirements;
            continue;
        }

        const int allocated = allocationRepository.totalAllocatedForPartColorForBuild(
            m_selectedBuildId,
            requirement.partId(),
            requirement.colorId());

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
        message = "BrickSuite did not find any additional loose inventory that could be allocated.";
    } else {
        message = QString("BrickSuite automatically allocated %1 piece(s).\n\n"
                          "Requirements satisfied: %2\n"
                          "Partially satisfied: %3\n"
                          "Still missing: %4")
                      .arg(piecesAdded)
                      .arg(satisfiedRequirements)
                      .arg(partiallySatisfiedRequirements)
                      .arg(stillMissingRequirements);

        if (allocationsCreated > 0 || allocationsUpdated > 0) {
            message += QString("\n\nAllocation records created: %1\n"
                               "Allocation records updated: %2")
                           .arg(allocationsCreated)
                           .arg(allocationsUpdated);
        }
    }

    if (spareRequirementsSkipped > 0) {
        message += QString("\n\nSpare requirements skipped: %1").arg(spareRequirementsSkipped);
    }

    QMessageBox::information(this, "Allocate Available", message);
}

void BuildsWidget::updateRequirementUiState()
{
    const bool enabled = m_workspaceContext.hasCurrentWorkspace() && m_selectedBuildId > 0;

    bool buildIsActive = false;

    if (enabled) {
        BuildRepository repository;
        const std::optional<Build> selectedBuild = repository.getById(m_selectedBuildId);
        buildIsActive = selectedBuild && selectedBuild->isActive();
    }

    m_partNumberEdit->setEnabled(enabled && buildIsActive);

    m_colorCombo->setEnabled(enabled && buildIsActive);

    m_quantitySpin->setEnabled(enabled && buildIsActive);

    m_spareCheck->setEnabled(enabled && buildIsActive);

    m_addRequirementButton->setEnabled(enabled && buildIsActive);

    //
    // Archived requirements remain visible for history/reference,
    // but the Build is read-only until reactivated.
    //
    m_requirementsTable->setEnabled(enabled);

    bool canLoadSet = false;
    bool canAllocateAvailable = false;
    bool canExportPullList = false;
    bool canExportMissingParts = false;
    bool canImportMoc = false;

    if (enabled) {
        BuildRepository repository;

        const std::optional<Build> build = repository.getById(m_selectedBuildId);

        if (build && build->isActive()) {
            canLoadSet = build->buildType() == "Set" && !build->setNumber().trimmed().isEmpty();

            canImportMoc = build->buildType() == "MOC" && build->inventoryMode() == "Stock";

            canAllocateAvailable = build->inventoryMode() == "Stock";

            canExportPullList = build->inventoryMode() == "Stock";

            canExportMissingParts = build->inventoryMode() == "Stock";
        }
    }

    m_allocateAvailableButton->setEnabled(canAllocateAvailable);

    m_exportPullListButton->setEnabled(canExportPullList);

    m_importPullListButton->setEnabled(canExportPullList);

    m_importMocPartsButton->setEnabled(canImportMoc);

    m_exportMissingPartsButton->setEnabled(canExportMissingParts);

    m_loadSetFromRebrickableButton->setEnabled(canLoadSet);
}

void BuildsWidget::exportPullList()
{
    if (m_selectedBuildId <= 0) {
        QMessageBox::warning(this, "Export Pull List", "Select a Build first.");

        return;
    }

    BuildRepository buildRepository;

    const std::optional<Build> build = buildRepository.getById(m_selectedBuildId);

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

    const QList<BuildAllocation> allocations = allocationRepository.getByBuild(m_selectedBuildId);

    if (allocations.isEmpty()) {
        QMessageBox::information(this,
                                 "Export Pull List",
                                 "This Build does not have any allocated "
                                 "inventory to export.");

        return;
    }

    QString defaultName;

    if (!build->setNumber().trimmed().isEmpty()) {
        defaultName = QString("BrickSuite_Pull_%1.csv").arg(build->setNumber().trimmed());
    } else {
        QString safeName = build->name().trimmed();

        safeName.replace(QRegularExpression(R"([^A-Za-z0-9_-]+)"), "_");

        defaultName = QString("BrickSuite_Pull_%1.csv").arg(safeName);
    }

    const QString fileName = QFileDialog::getSaveFileName(this,
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

    //
    // UTF-8 BOM helps Excel recognize UTF-8 CSV
    // correctly on Windows.
    //
    stream << QChar(0xFEFF);

    auto csvValue = [](QString value) {
        value.replace("\"", "\"\"");

        return QString("\"%1\"").arg(value);
    };

    //
    // One row per exact Build Allocation / storage
    // record. Quantity Pulled is deliberately blank
    // for the user to complete during the physical pull.
    //
    stream << "Build,"
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
        const std::optional<Part> part = partRepository.getById(allocation.partId());

        const std::optional<Color> color = colorRepository.getById(allocation.colorId());

        //
        // Build the complete storage path by walking
        // from the allocated location back to its root.
        //
        QStringList pathParts;

        int currentLocationId = allocation.storageLocationId();

        int safetyCount = 0;

        while (currentLocationId > 0 && safetyCount < 100) {
            const std::optional<StorageLocation> location = storageRepository.getById(
                currentLocationId);

            if (!location)
                break;

            pathParts.prepend(location->name());

            currentLocationId = location->parentLocationId();

            ++safetyCount;
        }

        QString storagePath = pathParts.join(" / ");

        if (storagePath.isEmpty()) {
            storagePath = QString("Location %1").arg(allocation.storageLocationId());
        }

        const QString partNumber = part ? part->partNumber() : QString::number(allocation.partId());

        const QString partName = part ? part->name() : QString();

        const QString colorName = color ? color->name() : QString::number(allocation.colorId());

        stream << csvValue(build->name()) << "," << csvValue(build->setNumber()) << ","
               << csvValue(partNumber) << "," << csvValue(partName) << "," << csvValue(colorName)
               << "," << csvValue(storagePath) << "," << allocation.quantityAllocated() << ","
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

    const std::optional<Build> build = repository.getById(m_selectedBuildId);

    if (!build)
        return;

    if (build->inventoryMode() != "Stock") {
        QMessageBox::information(this,
                                 "Import Pull List",
                                 "Pull List reconciliation is available "
                                 "only for Build from Stock.");

        return;
    }

    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          "Import Pull List CSV",
                                                          QString(),
                                                          "CSV Files (*.csv)");

    if (fileName.isEmpty())
        return;

    ImportPullListDialog dialog(m_selectedBuildId, fileName, this);

    if (dialog.exec() == QDialog::Accepted) {
        loadRequirements();
    }
}

void BuildsWidget::selectBuild(int buildId)
{
    if (buildId <= 0)
        return;

    loadBuilds();

    for (int row = 0; row < m_buildsTable->rowCount(); ++row) {
        QTableWidgetItem* nameItem = m_buildsTable->item(row, 3);

        if (!nameItem)
            continue;

        const int rowBuildId = nameItem->data(Qt::UserRole).toInt();

        if (rowBuildId != buildId)
            continue;

        m_buildsTable->setCurrentCell(row, 3);

        m_buildsTable->selectRow(row);

        m_buildsTable->scrollToItem(nameItem, QAbstractItemView::PositionAtCenter);

        return;
    }
}

void BuildsWidget::exportMissingParts()
{
    if (m_selectedBuildId <= 0) {
        QMessageBox::warning(this, "Export Missing Parts", "Select a Build first.");

        return;
    }

    BuildRepository buildRepository;

    const std::optional<Build> build = buildRepository.getById(m_selectedBuildId);

    if (!build) {
        QMessageBox::critical(this, "Export Missing Parts", "Unable to load the selected Build.");

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

    const QList<MissingPartsService::MissingPart> missingParts
        = service.getMissingParts(m_workspaceContext.currentWorkspaceId(), m_selectedBuildId);

    if (missingParts.isEmpty()) {
        QMessageBox::information(this,
                                 "Export Missing Parts",
                                 "This Build currently has no missing "
                                 "non-spare parts.");

        return;
    }

    QString defaultName;

    if (!build->setNumber().trimmed().isEmpty()) {
        defaultName = QString("BrickSuite_Missing_%1.csv").arg(build->setNumber().trimmed());
    } else {
        QString safeName = build->name().trimmed();

        safeName.replace(QRegularExpression(R"([^A-Za-z0-9_-]+)"), "_");

        defaultName = QString("BrickSuite_Missing_%1.csv").arg(safeName);
    }

    const QString fileName = QFileDialog::getSaveFileName(this,
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

    //
    // UTF-8 BOM helps Excel recognize UTF-8
    // correctly on Windows.
    //
    stream << QChar(0xFEFF);

    auto csvValue = [](QString value) {
        value.replace("\"", "\"\"");

        return QString("\"%1\"").arg(value);
    };

    //
    // Provider-neutral v1 acquisition list.
    //
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
        stream << csvValue(build->name()) << "," << csvValue(build->setNumber()) << ","
               << csvValue(item.partNumber) << "," << csvValue(item.partName) << ","
               << csvValue(item.colorName) << "," << item.required << "," << item.pulled << ","
               << item.remaining << "," << item.available << "," << item.missing << "\n";

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

    const std::optional<Build> build = buildRepository.getById(m_selectedBuildId);

    if (!build) {
        QMessageBox::critical(this, "Import MOC Parts", "Unable to load the selected Build.");

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

    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          "Import Rebrickable MOC Parts CSV",
                                                          QString(),
                                                          "CSV Files (*.csv)");

    if (fileName.isEmpty())
        return;

    BuildRequirementRepository requirementRepository;

    const QList<BuildRequirement> existing = requirementRepository.getByBuild(m_selectedBuildId);

    bool replaceExisting = false;

    if (!existing.isEmpty()) {
        const QMessageBox::StandardButton response
            = QMessageBox::warning(this,
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

    const RebrickableMocCsvImporter::Result result = importer.importFile(m_selectedBuildId,
                                                                         fileName,
                                                                         replaceExisting);

    if (!result.success) {
        QMessageBox::critical(this, "Import MOC Parts", result.message);

        return;
    }

    bool buildMetadataUpdated = false;

    const MocFileMetadata metadata = parseRebrickableMocFileName(fileName);

    if (metadata.recognized) {
        BuildRepository repository;

        std::optional<Build> updatedBuild = repository.getById(m_selectedBuildId);

        if (updatedBuild) {
            bool changed = false;

            //
            // Populate or correct the MOC reference from
            // the Rebrickable export filename.
            //
            if (!metadata.mocNumber.isEmpty() && updatedBuild->setNumber() != metadata.mocNumber) {
                updatedBuild->setSetNumber(metadata.mocNumber);

                changed = true;
            }

            // Populate or append to the Notes field with the source Set reference from
            // the Rebrickable export filename.
            if (!metadata.sourceSetNumber.isEmpty()) {
                QString alternateNote = QString("Alternate build from Set %1")
                                            .arg(metadata.sourceSetNumber);

                //
                // Resolve the source Set through BrickSuite's
                // local Sets Catalog when possible.
                //
                SetCatalogRepository setCatalogRepository;

                const std::optional<SetCatalogItem> sourceSet = setCatalogRepository.getBySetNumber(
                    metadata.sourceSetNumber);

                if (sourceSet && !sourceSet->name().trimmed().isEmpty()) {
                    alternateNote += QString(" — %1").arg(sourceSet->name().trimmed());
                }

                if (updatedBuild->notes().trimmed().isEmpty()) {
                    updatedBuild->setNotes(alternateNote);

                    changed = true;
                } else if (!updatedBuild->notes().contains(alternateNote, Qt::CaseInsensitive)) {
                    updatedBuild->setNotes(updatedBuild->notes().trimmed() + "\n" + alternateNote);

                    changed = true;
                }
            }

            if (changed) {
                if (!repository.update(*updatedBuild)) {
                    QMessageBox::warning(this,
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
        //
        // Reload the Builds table so the parsed
        // MOC Number and Notes appear immediately,
        // while keeping this MOC selected.
        //
        selectBuild(m_selectedBuildId);
    } else {
        loadRequirements();
    }

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
                                 .arg(result.regularPieces + result.sparePieces));
}