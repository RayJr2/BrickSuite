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

#include "InventoryImportPreviewDialog.h"

#include "../../services/inventory/RebrickableInventoryDiffCsvWriter.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStringConverter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QVBoxLayout>

InventoryImportPreviewDialog::InventoryImportPreviewDialog(
    const InventoryImportPreview& preview, QWidget* parent)
    : QDialog(parent)
    , m_preview(preview)
{
    setWindowTitle(
        QStringLiteral("%1 Inventory Preview")
            .arg(inventoryCsvOperationName(m_preview.operation)));
    resize(1100, 600);

    auto* mainLayout = new QVBoxLayout(this);

    m_scopeLabel = new QLabel(this);
    m_scopeLabel->setWordWrap(true);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);

    if (m_preview.operation == InventoryCsvOperation::CompareOnly) {
        m_scopeLabel->setText(QStringLiteral("<b>Comparing:</b> Rebrickable Part List \"%1\" "
                                             "↔ BrickSuite \"%2\"")
                                  .arg(m_preview.sourcePartListName, m_preview.storageDisplayName));
    }

    m_table = new QTableWidget(this);

    m_table->setColumnCount(9);

    QString resultColumn = QStringLiteral("Result Qty");

    switch (m_preview.operation) {
    case InventoryCsvOperation::Append:
        resultColumn = QStringLiteral("After Append");
        break;
    case InventoryCsvOperation::Replace:
        resultColumn = QStringLiteral("After Replace");
        break;
    case InventoryCsvOperation::Subtract:
        resultColumn = QStringLiteral("After Subtract");
        break;
    case InventoryCsvOperation::CompareOnly:
        resultColumn = QStringLiteral("BrickSuite Qty");
        break;
    }

    QString csvColumn = QStringLiteral("CSV Qty");
    QString currentColumn = QStringLiteral("Current Qty");
    QString statusColumn = QStringLiteral("Status");

    if (m_preview.operation == InventoryCsvOperation::CompareOnly) {
        csvColumn = QStringLiteral("Rebrickable Qty");
        currentColumn = QStringLiteral("BrickSuite Qty");
        statusColumn = QStringLiteral("Action");
    }

    m_table->setHorizontalHeaderLabels(QStringList() << "Part #"
                                                     << "Name"
                                                     << "Color"
                                                     << csvColumn
                                                     << currentColumn
                                                     << resultColumn
                                                     << "Difference"
                                                     << statusColumn
                                                     << "Error");

    if (m_preview.operation == InventoryCsvOperation::CompareOnly) {
        // Result Qty is identical to BrickSuite Qty in a non-mutating compare.
        m_table->setColumnHidden(5, true);
        m_table->setColumnHidden(8, true);
    }

    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_table->setSelectionMode(QAbstractItemView::SingleSelection);

    m_table->verticalHeader()->setVisible(false);

    m_table->horizontalHeader()->setStretchLastSection(false);

    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Stretch);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    if (QPushButton* importButton = m_buttonBox->button(QDialogButtonBox::Ok)) {
        const bool previewOnlyBrickOwl =
            m_preview.source == InventoryImportSource::BrickOwlOrderCsv;

        if (m_preview.operation == InventoryCsvOperation::CompareOnly
            || previewOnlyBrickOwl) {
            importButton->setText(QStringLiteral("Done"));
        } else {
            importButton->setText(
                QStringLiteral("Apply %1")
                    .arg(inventoryCsvOperationName(m_preview.operation)));
        }

        importButton->setEnabled(
            previewOnlyBrickOwl
            || (m_preview.failedRows == 0
                && m_preview.validRows > 0));
    }

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (m_preview.operation == InventoryCsvOperation::CompareOnly) {
        mainLayout->addWidget(m_scopeLabel);

        auto* filterLayout = new QHBoxLayout();
        filterLayout->addWidget(new QLabel(QStringLiteral("Show:"), this));

        m_filterCombo = new QComboBox(this);
        m_filterCombo->addItem(QStringLiteral("Differences Only"),
                               QStringLiteral("Differences"));
        m_filterCombo->addItem(QStringLiteral("All"),
                               QStringLiteral("All"));
        m_filterCombo->addItem(QStringLiteral("Append to Rebrickable"),
                               QStringLiteral("Append"));
        m_filterCombo->addItem(QStringLiteral("Subtract from Rebrickable"),
                               QStringLiteral("Subtract"));
        m_filterCombo->addItem(QStringLiteral("Matches"),
                               QStringLiteral("Match"));

        filterLayout->addWidget(m_filterCombo);
        filterLayout->addStretch(1);
        mainLayout->addLayout(filterLayout);

        connect(m_filterCombo,
                &QComboBox::currentIndexChanged,
                this,
                &InventoryImportPreviewDialog::applyCompareFilter);

        auto* exportLayout = new QHBoxLayout();

        m_exportAppendButton =
            new QPushButton(QStringLiteral("Export Append CSV..."), this);

        m_exportSubtractButton =
            new QPushButton(QStringLiteral("Export Subtract CSV..."), this);

        exportLayout->addWidget(m_exportAppendButton);
        exportLayout->addWidget(m_exportSubtractButton);
        exportLayout->addStretch(1);

        mainLayout->addLayout(exportLayout);

        connect(m_exportAppendButton,
                &QPushButton::clicked,
                this,
                &InventoryImportPreviewDialog::exportAppendCsv);

        connect(m_exportSubtractButton,
                &QPushButton::clicked,
                this,
                &InventoryImportPreviewDialog::exportSubtractCsv);
    }

    mainLayout->addWidget(m_summaryLabel);

    mainLayout->addWidget(m_table);

    mainLayout->addWidget(m_buttonBox);

    populateSummary();
    populateTable();

    if (m_preview.operation == InventoryCsvOperation::CompareOnly) {
        applyCompareFilter();

        bool hasAppendRows = false;
        bool hasSubtractRows = false;

        for (const InventoryImportPreviewRow& row : m_preview.rows) {
            if (row.status == QStringLiteral("Error"))
                continue;

            hasAppendRows = hasAppendRows || row.difference > 0;
            hasSubtractRows = hasSubtractRows || row.difference < 0;
        }

        const bool exportAllowed = m_preview.failedRows == 0;

        if (m_exportAppendButton)
            m_exportAppendButton->setEnabled(exportAllowed && hasAppendRows);

        if (m_exportSubtractButton)
            m_exportSubtractButton->setEnabled(exportAllowed && hasSubtractRows);
    }
}

void InventoryImportPreviewDialog::populateSummary()
{
    if (m_preview.operation == InventoryCsvOperation::CompareOnly) {
        int matches = 0;
        int appendRows = 0;
        int appendPieces = 0;
        int subtractRows = 0;
        int subtractPieces = 0;
        int comparedRows = 0;
        int errorRows = 0;

        for (const InventoryImportPreviewRow& row : m_preview.rows) {
            if (row.status == QStringLiteral("Error")) {
                ++errorRows;
                continue;
            }

            ++comparedRows;

            if (row.difference > 0) {
                ++appendRows;
                appendPieces += row.difference;
            } else if (row.difference < 0) {
                ++subtractRows;
                subtractPieces += -row.difference;
            } else {
                ++matches;
            }
        }

        m_summaryLabel->setText(
            QStringLiteral(
                "Compared rows: %1    Matches: %2    "
                "Append to Rebrickable: %3 rows / %4 pieces    "
                "Subtract from Rebrickable: %5 rows / %6 pieces    "
                "Errors: %7")
                .arg(comparedRows)
                .arg(matches)
                .arg(appendRows)
                .arg(appendPieces)
                .arg(subtractRows)
                .arg(subtractPieces)
                .arg(errorRows));
        return;
    }

    m_summaryLabel->setText(
        QString("Source: %1    Operation: %2\n"
                "File: %3\n"
                "Rows processed: %4    "
                "Resolved: %5    "
                "Needs Review / Errors: %6    "
                "Source pieces: %7")
            .arg(inventoryImportSourceName(m_preview.source))
            .arg(inventoryCsvOperationName(m_preview.operation))
            .arg(m_preview.sourceFileName)
            .arg(m_preview.rowsProcessed)
            .arg(m_preview.validRows)
            .arg(m_preview.failedRows)
            .arg(m_preview.totalSourceQuantity));
}

void InventoryImportPreviewDialog::populateTable()
{
    m_table->setRowCount(0);

    int row = 0;

    for (const InventoryImportPreviewRow& previewRow : m_preview.rows) {
        m_table->insertRow(row);

        m_table->setItem(row, 0, new QTableWidgetItem(previewRow.partNumber));

        m_table->setItem(row, 1, new QTableWidgetItem(previewRow.partName));

        m_table->setItem(row, 2, new QTableWidgetItem(previewRow.colorName));

        m_table->setItem(row, 3, new QTableWidgetItem(QString::number(previewRow.sourceQuantity)));

        m_table->setItem(row, 4, new QTableWidgetItem(QString::number(previewRow.currentQuantity)));

        m_table->setItem(row,
                         5,
                         new QTableWidgetItem(QString::number(previewRow.resultingQuantity)));

        m_table->setItem(row, 6, new QTableWidgetItem(QString::number(previewRow.difference)));

        m_table->setItem(row, 7, new QTableWidgetItem(previewRow.status));

        m_table->setItem(row, 8, new QTableWidgetItem(previewRow.errorMessage));

        ++row;
    }
}


void InventoryImportPreviewDialog::applyCompareFilter()
{
    if (m_preview.operation != InventoryCsvOperation::CompareOnly
        || !m_filterCombo) {
        return;
    }

    const QString filter =
        m_filterCombo->currentData().toString();

    for (int rowIndex = 0;
         rowIndex < m_preview.rows.size();
         ++rowIndex) {
        const InventoryImportPreviewRow& row =
            m_preview.rows.at(rowIndex);

        bool show = true;

        if (row.status == QStringLiteral("Error")) {
            show = true;
        } else if (filter == QStringLiteral("Differences")) {
            show = row.difference != 0;
        } else if (filter == QStringLiteral("Append")) {
            show = row.difference > 0;
        } else if (filter == QStringLiteral("Subtract")) {
            show = row.difference < 0;
        } else if (filter == QStringLiteral("Match")) {
            show = row.difference == 0;
        }

        m_table->setRowHidden(rowIndex, !show);
    }
}


void InventoryImportPreviewDialog::exportAppendCsv()
{
    exportDiffCsv(true);
}

void InventoryImportPreviewDialog::exportSubtractCsv()
{
    exportDiffCsv(false);
}

void InventoryImportPreviewDialog::exportDiffCsv(bool append)
{
    if (m_preview.operation != InventoryCsvOperation::CompareOnly)
        return;

    RebrickableInventoryDiffCsvWriter writer;

    const auto deltaType =
        append
            ? RebrickableInventoryDiffCsvWriter::DeltaType::Append
            : RebrickableInventoryDiffCsvWriter::DeltaType::Subtract;

    const RebrickableInventoryDiffCsvWriter::Result result =
        writer.write(m_preview, deltaType);

    if (!result.success) {
        QMessageBox::warning(
            this,
            QStringLiteral("Rebrickable Diff CSV"),
            result.message);
        return;
    }

    const QFileInfo sourceInfo(m_preview.sourceFilePath);

    const QString suggestedPath =
        sourceInfo.dir().filePath(
            RebrickableInventoryDiffCsvWriter::suggestedFileName(
                m_preview,
                deltaType));

    QString fileName =
        QFileDialog::getSaveFileName(
            this,
            append
                ? QStringLiteral("Save Rebrickable Append CSV")
                : QStringLiteral("Save Rebrickable Subtract CSV"),
            suggestedPath,
            QStringLiteral("CSV Files (*.csv)"));

    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(QStringLiteral(".csv"),
                           Qt::CaseInsensitive)) {
        fileName += QStringLiteral(".csv");
    }

    QFile file(fileName);

    if (!file.open(QIODevice::WriteOnly
                   | QIODevice::Truncate
                   | QIODevice::Text)) {
        QMessageBox::critical(
            this,
            QStringLiteral("Rebrickable Diff CSV"),
            QStringLiteral("Unable to save the CSV file.\n\n%1")
                .arg(file.errorString()));
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << result.csv;
    file.close();

    QMessageBox::information(
        this,
        QStringLiteral("Rebrickable Diff CSV"),
        QStringLiteral("%1 CSV saved successfully.\n\n"
                       "Rows: %2\n"
                       "Pieces: %3\n\n"
                       "On Rebrickable, import this file and choose %4 Parts.")
            .arg(append
                     ? QStringLiteral("Append")
                     : QStringLiteral("Subtract"))
            .arg(result.rows)
            .arg(result.pieces)
            .arg(append
                     ? QStringLiteral("Append")
                     : QStringLiteral("Subtract")));
}

