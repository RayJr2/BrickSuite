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
#include "../../import/InventoryImportTypes.h"
#include "../../import/RebrickableInventoryImporter.h"
#include "../../import/BrickOwlInventoryImporter.h"
#include "../../models/StorageLocation.h"
#include "../../repositories/StorageLocationRepository.h"

#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFile>
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
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QTextStream>
#include <QVBoxLayout>

ImportInventoryDialog::ImportInventoryDialog(WorkspaceContext& workspaceContext, QWidget* parent)
    : QDialog(parent)
    , m_workspaceContext(workspaceContext)
{
    setWindowTitle("Import Inventory");
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

    // Provider / format
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem(
        QStringLiteral("Auto Detect"),
        static_cast<int>(InventoryImportSource::Unknown));
    m_formatCombo->addItem(
        QStringLiteral("Rebrickable Inventory"),
        static_cast<int>(InventoryImportSource::RebrickableCsv));
    m_formatCombo->addItem(
        QStringLiteral("BrickOwl Order"),
        static_cast<int>(InventoryImportSource::BrickOwlOrderCsv));

    m_formatStatusLabel = new QLabel(this);
    m_formatStatusLabel->setWordWrap(true);
    m_formatStatusLabel->setText(QStringLiteral("Select an inventory CSV file."));

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
        QStringLiteral("Append adds CSV quantities. Replace makes the selected storage "
                       "match the CSV exactly, including reducing parts absent from the CSV "
                       "to zero. Subtract removes CSV quantities. Compare Only makes no "
                       "inventory changes."));

    // Storage
    m_storageCombo = new QComboBox(this);

    // Condition
    m_conditionCombo = new QComboBox(this);

    m_conditionCombo->addItem("Used");
    m_conditionCombo->addItem("New");

    // Ownership
    m_ownershipCombo = new QComboBox(this);

    m_ownershipCombo->addItem("Owned");

    formLayout->addRow("File:", fileWidget);
    formLayout->addRow("Format:", m_formatCombo);
    formLayout->addRow(QString(), m_formatStatusLabel);

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

    connect(m_formatCombo,
            &QComboBox::currentIndexChanged,
            this,
            [this]() {
                refreshImportFormatState();
            });

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
    refreshImportFormatState();
}

void ImportInventoryDialog::browseForFile()
{
    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          "Select Inventory CSV",
                                                          QString(),
                                                          "CSV Files (*.csv);;All Files (*.*)");

    if (filePath.isEmpty())
        return;

    m_fileEdit->setText(filePath);

    suggestStorageFromFileName(filePath);
    refreshImportFormatState();
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

InventoryImportSource ImportInventoryDialog::detectImportSource(
    const QString& filePath) const
{
    if (filePath.trimmed().isEmpty())
        return InventoryImportSource::Unknown;

    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return InventoryImportSource::Unknown;

    QTextStream stream(&file);
    QString headerLine = stream.readLine();

    if (!headerLine.isEmpty() && headerLine.front() == QChar(0xFEFF))
        headerLine.remove(0, 1);

    const QStringList rawHeaders = headerLine.split(',', Qt::KeepEmptyParts);
    QSet<QString> headers;

    for (QString header : rawHeaders) {
        header = header.trimmed();

        if (header.size() >= 2
            && header.startsWith(QLatin1Char('"'))
            && header.endsWith(QLatin1Char('"'))) {
            header = header.mid(1, header.size() - 2);
        }

        headers.insert(header.toLower());
    }

    // Actual BrickOwl received-order CSV headers include these fields.
    if (headers.contains(QStringLiteral("order id"))
        && headers.contains(QStringLiteral("name"))
        && headers.contains(QStringLiteral("color name"))
        && headers.contains(QStringLiteral("boid"))
        && headers.contains(QStringLiteral("lot id"))
        && headers.contains(QStringLiteral("ordered quantity"))) {
        return InventoryImportSource::BrickOwlOrderCsv;
    }

    // Existing Rebrickable owned-parts/category CSV contract.
    if (headers.contains(QStringLiteral("part"))
        && headers.contains(QStringLiteral("color"))
        && headers.contains(QStringLiteral("quantity"))) {
        return InventoryImportSource::RebrickableCsv;
    }

    return InventoryImportSource::Unknown;
}

InventoryImportSource ImportInventoryDialog::selectedImportSource() const
{
    if (!m_formatCombo)
        return InventoryImportSource::Unknown;

    const auto selected =
        static_cast<InventoryImportSource>(m_formatCombo->currentData().toInt());

    if (selected != InventoryImportSource::Unknown)
        return selected;

    return detectImportSource(m_fileEdit ? m_fileEdit->text().trimmed() : QString());
}

void ImportInventoryDialog::refreshImportFormatState()
{
    if (!m_formatCombo || !m_formatStatusLabel || !m_operationCombo || !m_buttonBox)
        return;

    const auto explicitSource =
        static_cast<InventoryImportSource>(m_formatCombo->currentData().toInt());
    const InventoryImportSource effectiveSource = selectedImportSource();

    if (m_fileEdit->text().trimmed().isEmpty()) {
        m_formatStatusLabel->setText(
            explicitSource == InventoryImportSource::Unknown
                ? QStringLiteral("Select an inventory CSV file.")
                : QStringLiteral("Selected format: %1")
                      .arg(inventoryImportSourceName(explicitSource)));
    } else if (effectiveSource == InventoryImportSource::Unknown) {
        m_formatStatusLabel->setText(
            QStringLiteral("Format not recognized. Select the format explicitly."));
    } else if (explicitSource == InventoryImportSource::Unknown) {
        m_formatStatusLabel->setText(
            QStringLiteral("Detected: %1")
                .arg(inventoryImportSourceName(effectiveSource)));
    } else {
        m_formatStatusLabel->setText(
            QStringLiteral("Selected format: %1")
                .arg(inventoryImportSourceName(effectiveSource)));
    }

    const bool brickOwl = effectiveSource == InventoryImportSource::BrickOwlOrderCsv;

    // Receiving a BrickOwl order is intentionally Append-only. The parser
    // and resolver arrive in M23.5.4; expose/detect the provider now without
    // allowing an unsupported import to reach the Rebrickable parser.
    for (int index = 0; index < m_operationCombo->count(); ++index) {
        const auto operation =
            static_cast<InventoryCsvOperation>(
                m_operationCombo->itemData(index).toInt());

        const bool enabled =
            !brickOwl || operation == InventoryCsvOperation::Append;

        if (auto* model = qobject_cast<QStandardItemModel*>(m_operationCombo->model())) {
            if (QStandardItem* item = model->item(index))
                item->setEnabled(enabled);
        }
    }

    if (brickOwl) {
        const int appendIndex =
            m_operationCombo->findData(static_cast<int>(InventoryCsvOperation::Append));
        if (appendIndex >= 0)
            m_operationCombo->setCurrentIndex(appendIndex);

        m_formatStatusLabel->setText(
            m_formatStatusLabel->text()
            + QStringLiteral(" — Append-only receiving preview."));
    }

    if (QPushButton* okButton = m_buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setEnabled(
            effectiveSource == InventoryImportSource::RebrickableCsv
            || effectiveSource == InventoryImportSource::BrickOwlOrderCsv);
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

    const InventoryImportSource source = selectedImportSource();

    if (source == InventoryImportSource::Unknown) {
        QMessageBox::warning(
            this,
            QStringLiteral("BrickSuite"),
            QStringLiteral("Unable to identify the inventory file format."));
        return;
    }

    const int storageLocationId = m_storageCombo->currentData().toInt();

    if (storageLocationId <= 0) {
        QMessageBox::warning(this, "BrickSuite", "Select a storage location.");

        return;
    }

    QSqlDatabase database = DatabaseManager::instance().database();

    InventoryImportOptions options;

    options.workspaceId = m_workspaceContext.currentWorkspaceId();

    options.storageLocationId = storageLocationId;

    options.condition = m_conditionCombo->currentText().trimmed();

    options.ownershipType = m_ownershipCombo->currentText().trimmed();

    options.operation =
        static_cast<InventoryCsvOperation>(
            m_operationCombo->currentData().toInt());

    InventoryImportResult result;

    // Preview
    InventoryImportPreview preview;
    bool previewSucceeded = false;

    if (source == InventoryImportSource::BrickOwlOrderCsv) {
        BrickOwlInventoryImporter importer(database);
        previewSucceeded = importer.previewOrder(filePath, options, preview);
    } else {
        RebrickableInventoryImporter importer(database);
        previewSucceeded = importer.previewOwnedParts(filePath, options, preview);
    }

    if (!previewSucceeded) {
        qCritical() << "Inventory import preview failed."
                    << "File:" << filePath
                    << "Source:" << inventoryImportSourceName(source)
                    << "WorkspaceId:" << options.workspaceId
                    << "StorageLocationId:" << options.storageLocationId;

        QMessageBox::critical(
            this,
            QStringLiteral("BrickSuite"),
            QStringLiteral("Unable to preview the selected %1 file.")
                .arg(inventoryImportSourceName(source)));

        return;
    }

    preview.storageDisplayName = m_storageCombo->currentText().trimmed();

    if (options.operation == InventoryCsvOperation::CompareOnly) {
        QFileInfo sourceInfo(filePath);
        QString listName = sourceInfo.completeBaseName().trimmed();

        const QString prefix = QStringLiteral("rebrickable_parts_");

        if (listName.startsWith(prefix, Qt::CaseInsensitive))
            listName = listName.mid(prefix.length());

        const QRegularExpression duplicateDownloadSuffix(
            R"(\s*\(\d+\)\s*$)");
        listName.remove(duplicateDownloadSuffix);

        listName.replace(QRegularExpression(QStringLiteral("[-_]+")),
                         QStringLiteral(" "));
        listName = listName.simplified();

        preview.sourcePartListName =
            listName.isEmpty()
                ? sourceInfo.completeBaseName()
                : listName;
    }

    InventoryImportPreviewDialog previewDialog(preview, this);

    if (previewDialog.exec() != QDialog::Accepted)
        return;

    if (source == InventoryImportSource::BrickOwlOrderCsv) {
        QMessageBox::information(
            this,
            QStringLiteral("BrickSuite"),
            QStringLiteral(
                "BrickOwl parsing and part/color resolution preview completed. "
                "No BrickSuite inventory was changed."));
        return;
    }

    if (options.operation == InventoryCsvOperation::CompareOnly) {
        QMessageBox::information(
            this,
            QStringLiteral("BrickSuite"),
            QStringLiteral("Comparison completed. No BrickSuite inventory was changed."));
        accept();
        return;
    }

    if (options.operation == InventoryCsvOperation::Replace) {
        int rowsRemoved = 0;
        int piecesRemoved = 0;

        for (const InventoryImportPreviewRow& row : preview.rows) {
            if (!row.presentInSource
                && row.presentInBrickSuite
                && row.currentQuantity > 0) {
                ++rowsRemoved;
                piecesRemoved += row.currentQuantity;
            }
        }

        QMessageBox confirm(this);
        confirm.setIcon(QMessageBox::Warning);
        confirm.setWindowTitle(QStringLiteral("Confirm Replace Inventory"));
        confirm.setText(
            QStringLiteral("Replace will make the selected BrickSuite storage location "
                           "match this CSV exactly."));
        confirm.setInformativeText(
            QStringLiteral("Parts/colors currently in \"%1\" but absent from the CSV "
                           "will be reduced to zero.\n\n"
                           "Storage-only rows to zero: %2\n"
                           "Pieces removed from those rows: %3\n\n"
                           "Continue with Replace?")
                .arg(preview.storageDisplayName)
                .arg(rowsRemoved)
                .arg(piecesRemoved));

        QPushButton* replaceButton =
            confirm.addButton(QStringLiteral("Replace Inventory"),
                              QMessageBox::AcceptRole);
        confirm.addButton(QMessageBox::Cancel);
        confirm.setDefaultButton(QMessageBox::Cancel);

        confirm.exec();

        if (confirm.clickedButton() != replaceButton)
            return;
    }

    RebrickableInventoryImporter importer(database);

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