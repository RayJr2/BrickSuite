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

#include "ImportInventoryDialog.h"
#include "InventoryImportPreviewDialog.h"

#include "../../app/WorkspaceContext.h"
#include "../../database/DatabaseManager.h"
#include "../../import/InventoryCsvOperation.h"
#include "../../import/RebrickableInventoryImporter.h"
#include "../../models/StorageLocation.h"
#include "../../repositories/StorageLocationRepository.h"

#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QSet>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QStringList>
#include <QVBoxLayout>

ImportInventoryDialog::ImportInventoryDialog(WorkspaceContext& workspaceContext, QWidget* parent)
    : QDialog(parent)
    , m_workspaceContext(workspaceContext)
{
    setWindowTitle("Import Rebrickable Inventory CSV");
    resize(620, 340);

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

    // Operation
    m_operationCombo = new QComboBox(this);
    m_operationCombo->addItem(
        QStringLiteral("Append"),
        static_cast<int>(InventoryCsvOperation::Append));
    m_operationCombo->addItem(
        QStringLiteral("Replace"),
        static_cast<int>(InventoryCsvOperation::Replace));
    m_operationCombo->addItem(
        QStringLiteral("Subtract"),
        static_cast<int>(InventoryCsvOperation::Subtract));
    m_operationCombo->addItem(
        QStringLiteral("Compare Only"),
        static_cast<int>(InventoryCsvOperation::CompareOnly));

    m_operationCombo->setToolTip(
        QStringLiteral("Append adds CSV quantities. Replace makes CSV rows match their "
                       "quantities. Subtract removes CSV quantities. Compare Only makes "
                       "no inventory changes."));

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

    formLayout->addRow("Operation:", m_operationCombo);

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

    connect(m_operationCombo,
            &QComboBox::currentIndexChanged,
            this,
            [this]() {
                if (QPushButton* okButton =
                        m_buttonBox->button(QDialogButtonBox::Ok)) {
                    const auto operation =
                        static_cast<InventoryCsvOperation>(
                            m_operationCombo->currentData().toInt());

                    okButton->setText(
                        operation == InventoryCsvOperation::CompareOnly
                            ? QStringLiteral("Compare")
                            : QStringLiteral("Preview"));
                }
            });

    if (QPushButton* okButton = m_buttonBox->button(QDialogButtonBox::Ok))
        okButton->setText(QStringLiteral("Preview"));

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

    suggestStorageFromFileName(filePath);
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

QString ImportInventoryDialog::normalizedStorageKey(const QString& text)
{
    QString normalized = text.toLower().trimmed();

    //
    // Rebrickable category exports use filename-friendly slugs such as:
    //
    //     technic-special
    //     transportation-sea-air
    //     bricks-round-curved
    //
    // Storage names may use spaces, commas, ampersands, etc. Treat all
    // non-alphanumeric characters as separators so both forms normalize
    // to the same comparison key.
    //
    normalized.replace(QRegularExpression("[^a-z0-9]+"), " ");
    normalized = normalized.simplified();

    return normalized;
}

void ImportInventoryDialog::suggestStorageFromFileName(const QString& filePath)
{
    if (filePath.trimmed().isEmpty() || m_storageCombo->count() <= 0)
        return;

    const QFileInfo fileInfo(filePath);

    QString baseName = fileInfo.completeBaseName().trimmed();

    if (baseName.isEmpty())
        return;

    //
    // Rebrickable owned-parts category exports currently use names such as:
    //
    //     rebrickable_parts_technic-special.csv
    //
    // Strip the known prefix before comparing the remaining category slug
    // with BrickSuite's active operational storage leaf names.
    //
    const QString prefix = "rebrickable_parts_";

    if (baseName.startsWith(prefix, Qt::CaseInsensitive)) {
        baseName = baseName.mid(prefix.length());
    }

    //
    // Browsers commonly append a duplicate-download suffix, for example:
    //
    //     rebrickable_parts_panels (1).csv
    //     rebrickable_parts_panels (2).csv
    //
    // Treat only a trailing " (number)" suffix as disposable. This keeps
    // storage matching conservative while still handling normal browser
    // duplicate filenames.
    //
    const QRegularExpression duplicateDownloadSuffix(
        R"(\s*\(\d+\)\s*$)");

    baseName.remove(duplicateDownloadSuffix);
    baseName = baseName.trimmed();

    const QString fileKey = normalizedStorageKey(baseName);

    if (fileKey.isEmpty())
        return;

    int matchingIndex = -1;
    int matchCount = 0;

    for (int index = 0; index < m_storageCombo->count(); ++index) {
        const QString displayPath = m_storageCombo->itemText(index);

        //
        // The combo displays the complete hierarchy, e.g.
        //
        //     Shelf Bottom / Technic Special
        //
        // Rebrickable's filename contains only the category name, so compare
        // against the leaf component. The full path remains visible to the
        // user and the selection can always be overridden.
        //
        const QString leafName = displayPath.section('/', -1).trimmed();

        if (normalizedStorageKey(leafName) == fileKey) {
            matchingIndex = index;
            ++matchCount;
        }
    }

    //
    // Auto-select only an unambiguous exact normalized match. If two active
    // leaf locations have the same name, BrickSuite does not guess which one
    // the user intended.
    //
    if (matchCount == 1 && matchingIndex >= 0) {
        m_storageCombo->setCurrentIndex(matchingIndex);

        m_storageCombo->setToolTip(
            QString("Suggested from CSV filename: %1")
                .arg(fileInfo.fileName()));
    } else {
        m_storageCombo->setToolTip(QString());
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

    options.operation =
        static_cast<InventoryCsvOperation>(
            m_operationCombo->currentData().toInt());

    RebrickableInventoryImporter::ImportResult result;

    // Preview
    RebrickableInventoryImportPreview preview;

    if (!importer.previewOwnedParts(filePath, options, preview)) {
        qCritical() << "Inventory import preview failed."
                    << "File:" << filePath
                    << "WorkspaceId:" << options.workspaceId
                    << "StorageLocationId:" << options.storageLocationId;

        QMessageBox::critical(this,
                              "BrickSuite",
                              "Unable to preview the selected Rebrickable inventory file.");

        return;
    }

    InventoryImportPreviewDialog previewDialog(preview, this);

    if (previewDialog.exec() != QDialog::Accepted)
        return;

    if (options.operation == InventoryCsvOperation::CompareOnly) {
        QMessageBox::information(
            this,
            QStringLiteral("BrickSuite"),
            QStringLiteral("Comparison completed. No BrickSuite inventory was changed."));
        accept();
        return;
    }

    if (!importer.importPreview(preview, options, result)) {
        qCritical() << "Inventory CSV operation failed."
                    << "File:" << filePath
                    << "Operation:" << inventoryCsvOperationName(options.operation)
                    << "RowsProcessed:" << result.rowsProcessed
                    << "RowsChanged:" << result.rowsImported
                    << "RowsFailed:" << result.rowsFailed;

        QMessageBox::critical(
            this,
            QStringLiteral("BrickSuite"),
            QStringLiteral("%1 failed.\n\n"
                           "Processed: %2\n"
                           "Changed: %3\n"
                           "Failed: %4")
                .arg(inventoryCsvOperationName(options.operation))
                .arg(result.rowsProcessed)
                .arg(result.rowsImported)
                .arg(result.rowsFailed));
        return;
    }

    QMessageBox::information(
        this,
        QStringLiteral("BrickSuite"),
        QStringLiteral("%1 completed successfully.\n\n"
                       "Rows changed: %2\n"
                       "Pieces changed: %3")
            .arg(inventoryCsvOperationName(options.operation))
            .arg(result.rowsImported)
            .arg(result.totalQuantityImported));

    accept();
}