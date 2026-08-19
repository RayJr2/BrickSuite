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

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

InventoryImportPreviewDialog::InventoryImportPreviewDialog(
    const RebrickableInventoryImportPreview& preview, QWidget* parent)
    : QDialog(parent)
    , m_preview(preview)
{
    setWindowTitle(
        QStringLiteral("%1 Inventory Preview")
            .arg(inventoryCsvOperationName(m_preview.operation)));
    resize(1100, 600);

    auto* mainLayout = new QVBoxLayout(this);

    m_summaryLabel = new QLabel(this);

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

    m_table->setHorizontalHeaderLabels(QStringList() << "Part #"
                                                     << "Name"
                                                     << "Color"
                                                     << "CSV Qty"
                                                     << "Current Qty"
                                                     << resultColumn
                                                     << "Difference"
                                                     << "Status"
                                                     << "Error");

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
        if (m_preview.operation == InventoryCsvOperation::CompareOnly) {
            importButton->setText(QStringLiteral("Done"));
        } else {
            importButton->setText(
                QStringLiteral("Apply %1")
                    .arg(inventoryCsvOperationName(m_preview.operation)));
        }

        importButton->setEnabled(
            m_preview.failedRows == 0
            && m_preview.validRows > 0);
    }

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(m_summaryLabel);

    mainLayout->addWidget(m_table);

    mainLayout->addWidget(m_buttonBox);

    populateSummary();
    populateTable();
}

void InventoryImportPreviewDialog::populateSummary()
{
    m_summaryLabel->setText(
        QString("Operation: %1\n"
                "File: %2\n"
                "Rows processed: %3    "
                "Valid: %4    "
                "Needs Review / Errors: %5    "
                "CSV pieces: %6")
            .arg(inventoryCsvOperationName(m_preview.operation))
            .arg(m_preview.sourceFileName)
            .arg(m_preview.rowsProcessed)
            .arg(m_preview.validRows)
            .arg(m_preview.failedRows)
            .arg(m_preview.totalCsvQuantity));
}

void InventoryImportPreviewDialog::populateTable()
{
    m_table->setRowCount(0);

    int row = 0;

    for (const RebrickableInventoryImportPreviewRow& previewRow : m_preview.rows) {
        m_table->insertRow(row);

        m_table->setItem(row, 0, new QTableWidgetItem(previewRow.partNumber));

        m_table->setItem(row, 1, new QTableWidgetItem(previewRow.partName));

        m_table->setItem(row, 2, new QTableWidgetItem(previewRow.colorName));

        m_table->setItem(row, 3, new QTableWidgetItem(QString::number(previewRow.csvQuantity)));

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