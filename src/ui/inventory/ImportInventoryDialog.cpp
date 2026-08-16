#include "ImportInventoryDialog.h"
#include "InventoryImportPreviewDialog.h"

#include "../../app/WorkspaceContext.h"
#include "../../database/DatabaseManager.h"
#include "../../import/RebrickableInventoryImporter.h"
#include "../../models/StorageLocation.h"
#include "../../repositories/StorageLocationRepository.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QSet>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlDatabase>
#include <QStringList>
#include <QVBoxLayout>

ImportInventoryDialog::ImportInventoryDialog(WorkspaceContext& workspaceContext, QWidget* parent)
    : QDialog(parent)
    , m_workspaceContext(workspaceContext)
{
    setWindowTitle("Import Rebrickable Inventory CSV");
    resize(600, 300);

    auto* mainLayout = new QVBoxLayout(this);

    auto* formLayout = new QFormLayout();

    // File selector
    auto* fileLayout = new QHBoxLayout();

    m_fileEdit = new QLineEdit(this);

    m_fileEdit->setReadOnly(true);

    m_browseButton = new QPushButton("Browse...", this);

    fileLayout->addWidget(m_fileEdit, 1);

    fileLayout->addWidget(m_browseButton);

    auto* fileWidget = new QWidget(this);

    fileWidget->setLayout(fileLayout);

    // Storage
    m_storageCombo = new QComboBox(this);

    // Condition
    m_conditionCombo = new QComboBox(this);

    m_conditionCombo->addItem("Used");
    m_conditionCombo->addItem("New");

    // Ownership
    m_ownershipCombo = new QComboBox(this);

    m_ownershipCombo->addItem("Owned");

    formLayout->addRow("CSV File:", fileWidget);

    formLayout->addRow("Storage:", m_storageCombo);

    formLayout->addRow("Condition:", m_conditionCombo);

    formLayout->addRow("Ownership:", m_ownershipCombo);

    mainLayout->addLayout(formLayout);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    if (QPushButton* okButton = m_buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setText("Import");
    }

    mainLayout->addWidget(m_buttonBox);

    connect(m_browseButton, &QPushButton::clicked, this, &ImportInventoryDialog::browseForFile);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &ImportInventoryDialog::importFile);

    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadStorageLocations();
}

void ImportInventoryDialog::browseForFile()
{
    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          "Select Rebrickable Inventory CSV",
                                                          QString(),
                                                          "CSV Files (*.csv);;All Files (*.*)");

    if (filePath.isEmpty())
        return;

    m_fileEdit->setText(filePath);
}

void ImportInventoryDialog::loadStorageLocations()
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
}

void ImportInventoryDialog::importFile()
{
    if (!m_workspaceContext.hasCurrentWorkspace()) {
        QMessageBox::warning(this, "BrickSuite", "Select a workspace before importing inventory.");

        return;
    }

    const QString filePath = m_fileEdit->text().trimmed();

    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "BrickSuite", "Select a CSV file to import.");

        return;
    }

    const int storageLocationId = m_storageCombo->currentData().toInt();

    if (storageLocationId <= 0) {
        QMessageBox::warning(this, "BrickSuite", "Select a storage location.");

        return;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    RebrickableInventoryImporter importer(database);

    RebrickableInventoryImporter::ImportOptions options;

    options.workspaceId = m_workspaceContext.currentWorkspaceId();

    options.storageLocationId = storageLocationId;

    options.condition = m_conditionCombo->currentText().trimmed();

    options.ownershipType = m_ownershipCombo->currentText().trimmed();

    RebrickableInventoryImporter::ImportResult result;

    // Preview
    RebrickableInventoryImportPreview preview;

    if (!importer.previewOwnedParts(filePath, options, preview)) {
        QMessageBox::critical(this,
                              "BrickSuite",
                              "Unable to preview the selected Rebrickable inventory file.");

        return;
    }

    InventoryImportPreviewDialog previewDialog(preview, this);

    if (previewDialog.exec() != QDialog::Accepted) {
        return;
    }

    if (!importer.importOwnedParts(filePath, options, result)) {
        QMessageBox::critical(this,
                              "BrickSuite",
                              QString("Import failed.\n\n"
                                      "Processed: %1\n"
                                      "Imported: %2\n"
                                      "Failed: %3")
                                  .arg(result.rowsProcessed)
                                  .arg(result.rowsImported)
                                  .arg(result.rowsFailed));

        return;
    }

    QMessageBox::information(this,
                             "BrickSuite",
                             QString("Import completed successfully.\n\n"
                                     "Rows imported: %1\n"
                                     "Total pieces added: %2")
                                 .arg(result.rowsImported)
                                 .arg(result.totalQuantityImported));

    accept();
}