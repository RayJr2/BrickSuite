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
 */

#include "InventoryImportPreviewDialog.h"

#include "../../models/ExternalPartIdentifier.h"
#include "../../models/ExternalPartMapping.h"
#include "../../models/Part.h"
#include "../../repositories/ExternalPartIdentifierRepository.h"
#include "../../repositories/ExternalPartMappingRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../services/inventory/RebrickableInventoryDiffCsvWriter.h"
#include "../../services/parts/PartResolver.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QStringConverter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QVBoxLayout>

namespace
{
QStringList sourceCandidates(const InventoryImportPreviewRow& row)
{
    QStringList candidates;

    for (QString value : row.sourcePartNumber.split(QLatin1Char('/'),
                                                    Qt::SkipEmptyParts)) {
        value = value.trimmed();

        if (!value.isEmpty() && !candidates.contains(value, Qt::CaseInsensitive))
            candidates.append(value);
    }

    return candidates;
}

QList<Part> resolvePartMatches(const QString& value)
{
    QList<Part> result;
    QSet<int> partIds;

    const QString candidate = value.trimmed();

    if (candidate.isEmpty())
        return result;

    PartResolver resolver;
    const PartResolutionResult direct = resolver.resolve(candidate);

    if (direct.hasResolvedPart && direct.part.id() > 0)
        partIds.insert(direct.part.id());

    ExternalPartMappingRepository mappingRepository;
    const QList<ExternalPartMapping> mappings =
        mappingRepository.findByProviderAndExternalId(
            QStringLiteral("BrickLink"),
            candidate);

    for (const ExternalPartMapping& mapping : mappings) {
        if (mapping.partId > 0)
            partIds.insert(mapping.partId);
    }

    ExternalPartIdentifierRepository identifierRepository;
    const QList<ExternalPartIdentifier> identifiers =
        identifierRepository.findByProviderAndExternalId(
            QStringLiteral("BrickLink"),
            candidate,
            true);

    for (const ExternalPartIdentifier& identifier : identifiers) {
        if (identifier.partId > 0)
            partIds.insert(identifier.partId);
    }

    PartRepository partRepository;

    for (int partId : partIds) {
        const std::optional<Part> part = partRepository.getById(partId);

        if (part)
            result.append(*part);
    }

    return result;
}
}

InventoryImportPreviewDialog::InventoryImportPreviewDialog(
    const InventoryImportPreview& preview, QWidget* parent)
    : QDialog(parent)
    , m_preview(preview)
{
    const bool brickOwl =
        m_preview.source == InventoryImportSource::BrickOwlOrderCsv;

    setWindowTitle(
        brickOwl
            ? QStringLiteral("BrickOwl Receiving Preview")
            : QStringLiteral("%1 Inventory Preview")
                  .arg(inventoryCsvOperationName(m_preview.operation)));

    resize(brickOwl ? 1320 : 1100, 650);

    auto* mainLayout = new QVBoxLayout(this);

    m_scopeLabel = new QLabel(this);
    m_scopeLabel->setWordWrap(true);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);

    if (m_preview.operation == InventoryCsvOperation::CompareOnly) {
        m_scopeLabel->setText(
            QStringLiteral("<b>Comparing:</b> Rebrickable Part List \"%1\" "
                           "↔ BrickSuite \"%2\"")
                .arg(m_preview.sourcePartListName,
                     m_preview.storageDisplayName));
    } else if (brickOwl) {
        m_scopeLabel->setText(
            QStringLiteral("<b>Receiving to:</b> %1<br>"
                           "Resolve or explicitly skip any <b>Needs Review</b> "
                           "rows before importing the order.")
                .arg(m_preview.storageDisplayName));
    }

    m_table = new QTableWidget(this);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);

    if (brickOwl) {
        auto* filterLayout = new QHBoxLayout();
        filterLayout->addWidget(new QLabel(QStringLiteral("Show:"), this));

        m_filterCombo = new QComboBox(this);
        m_filterCombo->addItem(QStringLiteral("All"), QStringLiteral("All"));
        m_filterCombo->addItem(QStringLiteral("Needs Review"),
                               QStringLiteral("Needs Review"));
        m_filterCombo->addItem(QStringLiteral("Resolved"),
                               QStringLiteral("Resolved"));
        m_filterCombo->addItem(QStringLiteral("Skipped"),
                               QStringLiteral("Skipped"));

        filterLayout->addWidget(m_filterCombo);

        m_resolvePartButton =
            new QPushButton(QStringLiteral("Resolve Selected Part..."), this);
        m_resolvePartButton->setEnabled(false);
        filterLayout->addWidget(m_resolvePartButton);

        m_skipRowButton =
            new QPushButton(QStringLiteral("Skip Selected Row"), this);
        m_skipRowButton->setEnabled(false);
        filterLayout->addWidget(m_skipRowButton);

        m_unskipRowButton =
            new QPushButton(QStringLiteral("Unskip Selected Row"), this);
        m_unskipRowButton->setEnabled(false);
        filterLayout->addWidget(m_unskipRowButton);

        filterLayout->addStretch(1);

        mainLayout->addWidget(m_scopeLabel);
        mainLayout->addLayout(filterLayout);

        connect(m_filterCombo,
                &QComboBox::currentIndexChanged,
                this,
                &InventoryImportPreviewDialog::applyBrickOwlFilter);

        connect(m_resolvePartButton,
                &QPushButton::clicked,
                this,
                &InventoryImportPreviewDialog::resolveSelectedBrickOwlPart);

        connect(m_skipRowButton,
                &QPushButton::clicked,
                this,
                &InventoryImportPreviewDialog::skipSelectedBrickOwlRow);

        connect(m_unskipRowButton,
                &QPushButton::clicked,
                this,
                &InventoryImportPreviewDialog::unskipSelectedBrickOwlRow);

        connect(m_table,
                &QTableWidget::itemSelectionChanged,
                this,
                &InventoryImportPreviewDialog::updateBrickOwlControls);

        connect(m_table,
                &QTableWidget::cellDoubleClicked,
                this,
                [this](int, int) {
                    resolveSelectedBrickOwlPart();
                });
    } else if (m_preview.operation == InventoryCsvOperation::CompareOnly) {
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

    m_buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                             this);

    if (QPushButton* okButton =
            m_buttonBox->button(QDialogButtonBox::Ok)) {
        if (brickOwl
            || m_preview.operation == InventoryCsvOperation::CompareOnly) {
            okButton->setText(QStringLiteral("Done"));
        } else {
            okButton->setText(
                QStringLiteral("Apply %1")
                    .arg(inventoryCsvOperationName(m_preview.operation)));
        }

        okButton->setEnabled(
            brickOwl
            || (m_preview.failedRows == 0
                && m_preview.validRows > 0));
    }

    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &QDialog::reject);

    mainLayout->addWidget(m_buttonBox);

    populateSummary();
    populateTable();

    if (brickOwl) {
        applyBrickOwlFilter();
        updateBrickOwlControls();
    } else if (m_preview.operation == InventoryCsvOperation::CompareOnly) {
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

const InventoryImportPreview& InventoryImportPreviewDialog::preview() const
{
    return m_preview;
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

    if (m_preview.source == InventoryImportSource::BrickOwlOrderCsv) {
        int skippedRows = 0;
        int skippedPieces = 0;
        int importPieces = 0;

        for (const InventoryImportPreviewRow& row : m_preview.rows) {
            if (row.status == QStringLiteral("Skipped")) {
                ++skippedRows;
                skippedPieces += row.sourceQuantity;
            } else if (row.partId > 0
                       && row.colorId > 0
                       && row.sourceQuantity > 0
                       && row.status != QStringLiteral("Needs Review")
                       && row.status != QStringLiteral("Error")) {
                importPieces += row.sourceQuantity;
            }
        }

        m_summaryLabel->setText(
            QStringLiteral(
                "Source: BrickOwl Order CSV    Operation: Append\n"
                "File: %1\n"
                "Rows processed: %2    Resolved: %3    "
                "Needs Review / Errors: %4    Skipped: %5    "
                "Import pieces: %6    Skipped pieces: %7")
                .arg(m_preview.sourceFileName)
                .arg(m_preview.rowsProcessed)
                .arg(m_preview.validRows)
                .arg(m_preview.failedRows)
                .arg(skippedRows)
                .arg(importPieces)
                .arg(skippedPieces));
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
    if (m_preview.source == InventoryImportSource::BrickOwlOrderCsv)
        populateBrickOwlTable();
    else
        populateStandardTable();
}

void InventoryImportPreviewDialog::populateStandardTable()
{
    m_table->clear();
    m_table->setRowCount(0);
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

    m_table->setHorizontalHeaderLabels(
        QStringList() << "Part #" << "Name" << "Color" << csvColumn
                      << currentColumn << resultColumn << "Difference"
                      << statusColumn << "Error");

    for (int column = 0; column < 9; ++column) {
        m_table->horizontalHeader()->setSectionResizeMode(
            column,
            column == 1 || column == 8
                ? QHeaderView::Stretch
                : QHeaderView::ResizeToContents);
    }

    if (m_preview.operation == InventoryCsvOperation::CompareOnly) {
        m_table->setColumnHidden(5, true);
        m_table->setColumnHidden(8, true);
    }

    int tableRow = 0;

    for (const InventoryImportPreviewRow& previewRow : m_preview.rows) {
        m_table->insertRow(tableRow);
        m_table->setItem(tableRow, 0,
                         new QTableWidgetItem(previewRow.partNumber));
        m_table->setItem(tableRow, 1,
                         new QTableWidgetItem(previewRow.partName));
        m_table->setItem(tableRow, 2,
                         new QTableWidgetItem(previewRow.colorName));
        m_table->setItem(tableRow, 3,
                         new QTableWidgetItem(
                             QString::number(previewRow.sourceQuantity)));
        m_table->setItem(tableRow, 4,
                         new QTableWidgetItem(
                             QString::number(previewRow.currentQuantity)));
        m_table->setItem(tableRow, 5,
                         new QTableWidgetItem(
                             QString::number(previewRow.resultingQuantity)));
        m_table->setItem(tableRow, 6,
                         new QTableWidgetItem(
                             QString::number(previewRow.difference)));
        m_table->setItem(tableRow, 7,
                         new QTableWidgetItem(previewRow.status));
        m_table->setItem(tableRow, 8,
                         new QTableWidgetItem(previewRow.errorMessage));
        ++tableRow;
    }
}

void InventoryImportPreviewDialog::populateBrickOwlTable()
{
    m_table->clear();
    m_table->setRowCount(0);
    m_table->setColumnCount(12);

    m_table->setHorizontalHeaderLabels(
        QStringList()
        << "Source Part"
        << "BrickSuite Part"
        << "Name"
        << "Color"
        << "Condition"
        << "Ordered"
        << "Refunded"
        << "Import Qty"
        << "Current Qty"
        << "After Append"
        << "Status"
        << "Error");

    for (int column = 0; column < 12; ++column) {
        const bool stretch = column == 2 || column == 11;
        m_table->horizontalHeader()->setSectionResizeMode(
            column,
            stretch ? QHeaderView::Stretch
                    : QHeaderView::ResizeToContents);
    }

    for (int index = 0; index < m_preview.rows.size(); ++index) {
        m_table->insertRow(index);
        refreshBrickOwlRow(index);
    }
}

void InventoryImportPreviewDialog::refreshBrickOwlRow(int previewRowIndex)
{
    if (previewRowIndex < 0
        || previewRowIndex >= m_preview.rows.size()
        || previewRowIndex >= m_table->rowCount()) {
        return;
    }

    const InventoryImportPreviewRow& row =
        m_preview.rows.at(previewRowIndex);

    const QStringList values = {
        row.sourcePartNumber,
        row.partNumber,
        row.partName,
        row.colorName.isEmpty() ? row.sourceColorName : row.colorName,
        row.sourceCondition,
        QString::number(row.sourceOrderedQuantity),
        QString::number(row.sourceRefundedQuantity),
        QString::number(row.sourceQuantity),
        QString::number(row.currentQuantity),
        QString::number(row.resultingQuantity),
        row.status,
        row.errorMessage
    };

    for (int column = 0; column < values.size(); ++column) {
        QTableWidgetItem* item = m_table->item(previewRowIndex, column);

        if (!item) {
            item = new QTableWidgetItem();
            m_table->setItem(previewRowIndex, column, item);
        }

        item->setText(values.at(column));
        item->setData(Qt::UserRole, previewRowIndex);
    }
}

void InventoryImportPreviewDialog::applyCompareFilter()
{
    if (m_preview.operation != InventoryCsvOperation::CompareOnly
        || !m_filterCombo) {
        return;
    }

    const QString filter = m_filterCombo->currentData().toString();

    for (int rowIndex = 0; rowIndex < m_preview.rows.size(); ++rowIndex) {
        const InventoryImportPreviewRow& row = m_preview.rows.at(rowIndex);

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

void InventoryImportPreviewDialog::applyBrickOwlFilter()
{
    if (m_preview.source != InventoryImportSource::BrickOwlOrderCsv
        || !m_filterCombo) {
        return;
    }

    const QString filter = m_filterCombo->currentData().toString();

    for (int rowIndex = 0; rowIndex < m_preview.rows.size(); ++rowIndex) {
        const InventoryImportPreviewRow& row = m_preview.rows.at(rowIndex);

        bool show = true;

        if (filter == QStringLiteral("Needs Review")) {
            show = row.status == QStringLiteral("Needs Review")
                   || row.status == QStringLiteral("Error");
        } else if (filter == QStringLiteral("Resolved")) {
            show = row.status != QStringLiteral("Needs Review")
                   && row.status != QStringLiteral("Error")
                   && row.status != QStringLiteral("Skipped");
        } else if (filter == QStringLiteral("Skipped")) {
            show = row.status == QStringLiteral("Skipped");
        }

        m_table->setRowHidden(rowIndex, !show);
    }
}

void InventoryImportPreviewDialog::updateBrickOwlControls()
{
    if (!m_resolvePartButton || !m_skipRowButton || !m_unskipRowButton)
        return;

    const int tableRow = m_table->currentRow();

    if (tableRow < 0 || tableRow >= m_preview.rows.size()) {
        m_resolvePartButton->setEnabled(false);
        m_skipRowButton->setEnabled(false);
        m_unskipRowButton->setEnabled(false);
        return;
    }

    const InventoryImportPreviewRow& row = m_preview.rows.at(tableRow);
    const bool skipped = row.status == QStringLiteral("Skipped");
    const bool needsReview =
        row.status == QStringLiteral("Needs Review")
        || row.status == QStringLiteral("Error");

    m_resolvePartButton->setEnabled(
        !skipped
        && row.partId <= 0
        && row.sourceQuantity > 0);

    m_skipRowButton->setEnabled(
        !skipped
        && needsReview
        && row.sourceQuantity > 0);

    m_unskipRowButton->setEnabled(skipped);
}

void InventoryImportPreviewDialog::resolveSelectedBrickOwlPart()
{
    if (m_preview.source != InventoryImportSource::BrickOwlOrderCsv)
        return;

    const int rowIndex = m_table->currentRow();

    if (rowIndex < 0 || rowIndex >= m_preview.rows.size())
        return;

    InventoryImportPreviewRow& row = m_preview.rows[rowIndex];

    if (row.partId > 0)
        return;

    QStringList candidates = sourceCandidates(row);
    QString entered;

    if (!candidates.isEmpty()) {
        bool ok = false;
        entered = QInputDialog::getItem(
            this,
            QStringLiteral("Resolve BrickOwl Part"),
            QStringLiteral(
                "Choose a BrickOwl candidate or type a BrickSuite / "
                "Rebrickable / BrickLink part number:"),
            candidates,
            0,
            true,
            &ok);

        if (!ok)
            return;
    } else {
        bool ok = false;
        entered = QInputDialog::getText(
            this,
            QStringLiteral("Resolve BrickOwl Part"),
            QStringLiteral(
                "Enter a BrickSuite / Rebrickable / BrickLink part number:"),
            QLineEdit::Normal,
            QString(),
            &ok);

        if (!ok)
            return;
    }

    if (!resolveBrickOwlPartNumber(rowIndex, entered))
        return;

    recountBrickOwlResolution();
    populateSummary();
    refreshBrickOwlRow(rowIndex);
    applyBrickOwlFilter();
    updateBrickOwlControls();
}

void InventoryImportPreviewDialog::skipSelectedBrickOwlRow()
{
    if (m_preview.source != InventoryImportSource::BrickOwlOrderCsv)
        return;

    const int rowIndex = m_table->currentRow();

    if (rowIndex < 0 || rowIndex >= m_preview.rows.size())
        return;

    InventoryImportPreviewRow& row = m_preview.rows[rowIndex];

    if (row.status == QStringLiteral("Skipped"))
        return;

    row.status = QStringLiteral("Skipped");
    row.errorMessage =
        QStringLiteral("Excluded from this import by user.");
    row.resultingQuantity = row.currentQuantity;
    row.difference = 0;

    recountBrickOwlResolution();
    populateSummary();
    refreshBrickOwlRow(rowIndex);
    applyBrickOwlFilter();
    updateBrickOwlControls();
}

void InventoryImportPreviewDialog::unskipSelectedBrickOwlRow()
{
    if (m_preview.source != InventoryImportSource::BrickOwlOrderCsv)
        return;

    const int rowIndex = m_table->currentRow();

    if (rowIndex < 0 || rowIndex >= m_preview.rows.size())
        return;

    InventoryImportPreviewRow& row = m_preview.rows[rowIndex];

    if (row.status != QStringLiteral("Skipped"))
        return;

    if (row.partId > 0 && row.colorId > 0 && row.sourceQuantity > 0) {
        row.resultingQuantity =
            row.currentQuantity + row.sourceQuantity;
        row.difference = row.sourceQuantity;
        row.status =
            row.currentQuantity > 0
                ? QStringLiteral("Append")
                : QStringLiteral("New Inventory");
        row.errorMessage.clear();
    } else {
        row.resultingQuantity = row.currentQuantity;
        row.difference = 0;
        row.status = QStringLiteral("Needs Review");
        row.errorMessage =
            QStringLiteral("Row restored; resolve the part/color or skip it.");
    }

    recountBrickOwlResolution();
    populateSummary();
    refreshBrickOwlRow(rowIndex);
    applyBrickOwlFilter();
    updateBrickOwlControls();
}

bool InventoryImportPreviewDialog::resolveBrickOwlPartNumber(
    int previewRowIndex,
    const QString& enteredPartNumber)
{
    if (previewRowIndex < 0
        || previewRowIndex >= m_preview.rows.size()) {
        return false;
    }

    const QList<Part> matches =
        resolvePartMatches(enteredPartNumber);

    if (matches.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Resolve BrickOwl Part"),
            QStringLiteral(
                "No BrickSuite part could be resolved from \"%1\".")
                .arg(enteredPartNumber.trimmed()));
        return false;
    }

    Part chosen = matches.first();

    if (matches.size() > 1) {
        QStringList choices;

        for (const Part& part : matches) {
            choices.append(
                QStringLiteral("%1 — %2")
                    .arg(part.partNumber(), part.name()));
        }

        bool ok = false;
        const QString selected = QInputDialog::getItem(
            this,
            QStringLiteral("Choose BrickSuite Part"),
            QStringLiteral(
                "More than one BrickSuite part matches this identifier:"),
            choices,
            0,
            false,
            &ok);

        if (!ok)
            return false;

        const int selectedIndex = choices.indexOf(selected);

        if (selectedIndex < 0)
            return false;

        chosen = matches.at(selectedIndex);
    }

    InventoryImportPreviewRow& row =
        m_preview.rows[previewRowIndex];

    row.partId = chosen.id();
    row.partNumber = chosen.partNumber();
    row.partName = chosen.name();

    // Color resolution was already completed by the BrickOwl parser for the
    // current M23.5.4 failures. If a future row also has an unresolved color,
    // keep it blocked instead of falsely marking the entire row resolved.
    if (row.colorId <= 0) {
        row.status = QStringLiteral("Needs Review");
        row.errorMessage =
            QStringLiteral("Part resolved manually; color still needs resolution.");
        return true;
    }

    row.resultingQuantity =
        row.currentQuantity + row.sourceQuantity;
    row.difference = row.sourceQuantity;
    row.status =
        row.currentQuantity > 0
            ? QStringLiteral("Append")
            : QStringLiteral("New Inventory");
    row.errorMessage.clear();

    return true;
}

void InventoryImportPreviewDialog::recountBrickOwlResolution()
{
    int validRows = 0;
    int failedRows = 0;

    for (const InventoryImportPreviewRow& row : m_preview.rows) {
        if (row.status == QStringLiteral("Skipped"))
            continue;

        if (row.partId > 0
            && row.colorId > 0
            && row.sourceQuantity > 0
            && row.status != QStringLiteral("Error")
            && row.status != QStringLiteral("Needs Review")) {
            ++validRows;
        } else {
            ++failedRows;
        }
    }

    m_preview.validRows = validRows;
    m_preview.failedRows = failedRows;
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
