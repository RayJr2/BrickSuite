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

#include "ImportPullListDialog.h"

#include "../../models/Build.h"
#include "../../models/BuildAllocation.h"
#include "../../models/BuildRequirement.h"
#include "../../models/Color.h"
#include "../../models/Part.h"
#include "../../models/StorageLocation.h"

#include "../../repositories/BuildAllocationRepository.h"
#include "../../repositories/BuildRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/StorageLocationRepository.h"

#include "../../services/builds/BuildPullingService.h"

#include <QAbstractItemView>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QVBoxLayout>

ImportPullListDialog::ImportPullListDialog(int buildId, const QString& fileName, QWidget* parent)
    : QDialog(parent)
    , m_buildId(buildId)
    , m_fileName(fileName)
{
    setWindowTitle("Import Pull List Preview");

    resize(1200, 700);

    auto* mainLayout = new QVBoxLayout(this);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    mainLayout->addWidget(m_summaryLabel);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);

    m_table = new QTableWidget(this);

    m_table->setColumnCount(9);

    m_table->setHorizontalHeaderLabels(QStringList() << "Part #"
                                                     << "Name"
                                                     << "Color"
                                                     << "Storage Location"
                                                     << "CSV Allocated"
                                                     << "Actual Allocated"
                                                     << "Pulled"
                                                     << "Status"
                                                     << "Message");

    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);

    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    for (int column = 4; column <= 7; ++column) {
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }

    m_table->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Stretch);

    mainLayout->addWidget(m_table, 1);

    auto* buttonBox = new QDialogButtonBox(this);

    m_reconcileButton = buttonBox->addButton("Reconcile Pull List", QDialogButtonBox::AcceptRole);
    m_closeButton = buttonBox->addButton("Close", QDialogButtonBox::RejectRole);

    m_reconcileButton->setEnabled(false);

    mainLayout->addWidget(buttonBox);

    connect(m_reconcileButton, &QPushButton::clicked, this, &ImportPullListDialog::reconcile);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);

    if (!loadCsv()) {
        m_reconcileButton->setEnabled(false);
        return;
    }

    buildPreview();
}

bool ImportPullListDialog::parseCsvLine(const QString& line, QStringList& fields) const
{
    fields.clear();

    QString field;
    bool inQuotes = false;

    for (int index = 0; index < line.size(); ++index) {
        const QChar ch = line.at(index);

        if (ch == '"') {
            if (inQuotes && index + 1 < line.size() && line.at(index + 1) == '"') {
                field += '"';
                ++index;
            } else {
                inQuotes = !inQuotes;
            }

            continue;
        }

        if (ch == ',' && !inQuotes) {
            fields.append(field);
            field.clear();
            continue;
        }

        field += ch;
    }

    if (inQuotes)
        return false;

    fields.append(field);

    return true;
}

bool ImportPullListDialog::loadCsv()
{
    QFile file(m_fileName);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Import Pull List", "Unable to open the selected CSV file.");
        return false;
    }

    QTextStream stream(&file);

    if (stream.atEnd()) {
        QMessageBox::warning(this, "Import Pull List", "The selected CSV file is empty.");
        return false;
    }

    QString headerLine = stream.readLine();

    if (!headerLine.isEmpty() && headerLine.front() == QChar(0xFEFF)) {
        headerLine.remove(0, 1);
    }

    QStringList headers;

    if (!parseCsvLine(headerLine, headers)) {
        QMessageBox::warning(this, "Import Pull List", "The CSV header is invalid.");
        return false;
    }

    const QStringList expectedHeaders = {"Allocation ID",
                                         "Build",
                                         "Set Number",
                                         "Part Number",
                                         "Part Name",
                                         "Color",
                                         "Storage Location",
                                         "Quantity Allocated",
                                         "Quantity Pulled"};

    if (headers != expectedHeaders) {
        QMessageBox::warning(
            this,
            "Import Pull List",
            "The selected CSV does not have the current BrickSuite "
            "Pull List columns.\n\n"
            "Pull Lists exported before Allocation ID support cannot be "
            "reconciled reliably when multiple allocations share the same "
            "Part / Color / Storage. Please export a new Pull List.");

        return false;
    }

    while (!stream.atEnd()) {
        const QString line = stream.readLine();

        if (line.trimmed().isEmpty())
            continue;

        QStringList fields;

        if (!parseCsvLine(line, fields) || fields.size() != 9) {
            PreviewRow row;
            row.status = RowStatus::InvalidRow;
            row.message = "Invalid CSV row.";
            m_rows.append(row);
            continue;
        }

        PreviewRow row;

        bool allocationIdOk = false;
        row.allocationIdCsv = fields.at(0).trimmed().toInt(&allocationIdOk);

        row.buildName = fields.at(1).trimmed();
        row.setNumber = fields.at(2).trimmed();
        row.partNumber = fields.at(3).trimmed();
        row.partName = fields.at(4).trimmed();
        row.colorName = fields.at(5).trimmed();
        row.storagePath = fields.at(6).trimmed();

        bool allocatedOk = false;
        row.quantityAllocatedCsv = fields.at(7).trimmed().toInt(&allocatedOk);

        const QString pulledText = fields.at(8).trimmed();

        //
        // A blank Quantity Pulled means zero pieces were pulled from this
        // allocation. This keeps the exported Pull List convenient to use:
        // the user only needs to enter quantities for rows actually pulled.
        //
        if (pulledText.isEmpty()) {
            row.quantityPulled = 0;
            row.pulledEntered = true;
        } else {
            bool pulledOk = false;

            row.quantityPulled = pulledText.toInt(&pulledOk);
            row.pulledEntered = pulledOk && row.quantityPulled >= 0;

            if (!row.pulledEntered) {
                row.status = RowStatus::InvalidRow;
                row.message = "Quantity Pulled is not a valid "
                              "non-negative integer.";
            }
        }

        if (!allocationIdOk || row.allocationIdCsv <= 0) {
            row.status = RowStatus::InvalidRow;
            row.message = "Allocation ID is invalid.";
        }

        if (!allocatedOk || row.quantityAllocatedCsv <= 0) {
            row.status = RowStatus::InvalidRow;
            row.message = "Quantity Allocated is invalid.";
        }

        m_rows.append(row);
    }

    file.close();

    if (m_rows.isEmpty()) {
        QMessageBox::warning(this,
                             "Import Pull List",
                             "The Pull List does not contain any data rows.");
        return false;
    }

    return true;
}

void ImportPullListDialog::buildPreview()
{
    m_table->setRowCount(0);

    m_exactCount = 0;
    m_partialCount = 0;
    m_zeroCount = 0;
    m_problemCount = 0;

    BuildRepository buildRepository;

    const std::optional<Build> build = buildRepository.getById(m_buildId);

    if (!build) {
        m_statusLabel->setText("Unable to load the selected Build.");
        ++m_problemCount;
        return;
    }

    BuildAllocationRepository allocationRepository;
    BuildRequirementRepository requirementRepository;
    PartRepository partRepository;
    ColorRepository colorRepository;

    for (PreviewRow& row : m_rows) {
        if (row.status == RowStatus::InvalidRow) {
            ++m_problemCount;
            continue;
        }

        if (row.buildName != build->name() || row.setNumber != build->setNumber()) {
            row.status = RowStatus::InvalidRow;
            row.message = "CSV Build/Set does not match "
                          "the selected Build.";
            ++m_problemCount;
            continue;
        }

        const std::optional<BuildAllocation> allocation =
            allocationRepository.getById(row.allocationIdCsv);

        if (!allocation || allocation->buildId() != m_buildId) {
            row.status = RowStatus::AllocationNotFound;
            row.message =
                "The exported Build allocation no longer exists for this Build.";
            ++m_problemCount;
            continue;
        }

        //
        // Phase 3 allocations must identify the exact Build Requirement
        // they satisfy. This prevents two requirements that share the same
        // effective Part / Color from being reconciled against one another.
        //
        const std::optional<BuildRequirement> requirement =
            requirementRepository.getById(allocation->buildRequirementId());

        if (!requirement
            || requirement->buildId() != m_buildId
            || requirement->isSpare()
            || requirement->effectivePartId() != allocation->partId()
            || requirement->effectiveColorId() != allocation->colorId()) {
            row.status = RowStatus::InvalidRow;
            row.message =
                "Allocation is no longer linked to the matching Build Requirement.";
            ++m_problemCount;
            continue;
        }

        const std::optional<Part> part =
            partRepository.getById(allocation->partId());

        const std::optional<Color> color =
            colorRepository.getById(allocation->colorId());

        if (!part || !color) {
            row.status = RowStatus::InvalidRow;
            row.message = "Unable to resolve the current allocation Part or Color.";
            ++m_problemCount;
            continue;
        }

        const QString allocationPath =
            storagePath(allocation->storageLocationId());

        if (part->partNumber() != row.partNumber
            || part->name() != row.partName
            || color->name() != row.colorName
            || allocationPath != row.storagePath) {
            row.status = RowStatus::InvalidRow;
            row.message =
                "Allocation details changed since export.";
            ++m_problemCount;
            continue;
        }

        row.allocationId = allocation->id();
        row.inventoryRecordId = allocation->inventoryRecordId();
        row.partId = allocation->partId();
        row.colorId = allocation->colorId();
        row.storageLocationId = allocation->storageLocationId();
        row.actualAllocated = allocation->quantityAllocated();

        if (row.quantityAllocatedCsv != row.actualAllocated) {
            row.status = RowStatus::InvalidRow;
            row.message = QString("Allocation changed since export "
                                  "(CSV %1, current %2).")
                              .arg(row.quantityAllocatedCsv)
                              .arg(row.actualAllocated);
            ++m_problemCount;
            continue;
        }

        if (!row.pulledEntered) {
            row.status = RowStatus::NotEntered;
            row.message = "Quantity Pulled has not been entered.";
            ++m_problemCount;
            continue;
        }

        if (row.quantityPulled > row.actualAllocated) {
            row.status = RowStatus::OverPull;
            row.message = "Quantity Pulled exceeds the "
                          "current allocation.";
            ++m_problemCount;
            continue;
        }

        if (row.quantityPulled == 0) {
            row.status = RowStatus::ZeroPull;
            ++m_zeroCount;
        } else if (row.quantityPulled == row.actualAllocated) {
            row.status = RowStatus::ExactPull;
            ++m_exactCount;
        } else {
            row.status = RowStatus::PartialPull;
            ++m_partialCount;
        }
    }

    int tableRow = 0;

    for (const PreviewRow& row : m_rows) {
        m_table->insertRow(tableRow);

        const QString pulledText = row.pulledEntered ? QString::number(row.quantityPulled)
                                                     : QString();

        QStringList values = {row.partNumber,
                              row.partName,
                              row.colorName,
                              row.storagePath,
                              QString::number(row.quantityAllocatedCsv),
                              row.actualAllocated > 0 ? QString::number(row.actualAllocated)
                                                      : QString(),
                              pulledText,
                              statusText(row.status),
                              row.message};

        for (int column = 0; column < values.size(); ++column) {
            auto* item = new QTableWidgetItem(values.at(column));

            if (column >= 4 && column <= 7) {
                item->setTextAlignment(Qt::AlignCenter);
            }

            m_table->setItem(tableRow, column, item);
        }

        ++tableRow;
    }

    m_summaryLabel->setText(QString("<b>Pull List Reconciliation Preview</b><br>"
                                    "Build: %1<br>"
                                    "Set: %2<br><br>"
                                    "Exact Pull: %3<br>"
                                    "Partial Pull: %4<br>"
                                    "Zero Pull: %5<br>"
                                    "Problems: %6")
                                .arg(build->name())
                                .arg(build->setNumber())
                                .arg(m_exactCount)
                                .arg(m_partialCount)
                                .arg(m_zeroCount)
                                .arg(m_problemCount));

    if (m_problemCount == 0) {
        m_statusLabel->setText("All Pull List rows are valid. "
                               "No database changes have been made yet.");
        m_reconcileButton->setEnabled(true);
    } else {
        m_statusLabel->setText("Reconciliation is disabled until "
                               "all Pull List rows are valid.");
        m_reconcileButton->setEnabled(false);
    }
}

QString ImportPullListDialog::statusText(RowStatus status) const
{
    switch (status) {
    case RowStatus::Pending:
        return "Pending";

    case RowStatus::ExactPull:
        return "Exact Pull";

    case RowStatus::PartialPull:
        return "Partial Pull";

    case RowStatus::ZeroPull:
        return "Zero Pull";

    case RowStatus::NotEntered:
        return "Not Entered";

    case RowStatus::OverPull:
        return "Over Pull";

    case RowStatus::AllocationNotFound:
        return "Allocation Not Found";

    case RowStatus::InvalidRow:
    default:
        return "Invalid";
    }
}

QString ImportPullListDialog::storagePath(int storageLocationId) const
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

void ImportPullListDialog::reconcile()
{
    if (m_problemCount > 0)
        return;

    const QMessageBox::StandardButton response
        = QMessageBox::question(this,
                                "Reconcile Pull List",
                                QString("Reconcile this Pull List?\n\n"
                                        "Exact Pull rows: %1\n"
                                        "Partial Pull rows: %2\n"
                                        "Zero Pull rows: %3\n\n"
                                        "Pulled quantities will be removed from "
                                        "physical inventory and their Build "
                                        "allocations will be reduced or removed.")
                                    .arg(m_exactCount)
                                    .arg(m_partialCount)
                                    .arg(m_zeroCount),
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::No);

    if (response != QMessageBox::Yes)
        return;

    QList<BuildPullingService::PullRequest> requests;
    requests.reserve(m_rows.size());

    for (const PreviewRow& row : m_rows) {
        BuildPullingService::PullRequest request;
        request.allocationId = row.allocationId;
        request.quantity = row.quantityPulled;
        requests.append(request);
    }

    //
    // CSV reconciliation and the interactive pulling UI now share the same
    // application-service transaction. Requirement ownership follows the
    // allocation's exact build_requirement_id; the dialog no longer performs
    // its own Part/Color-based fulfillment updates.
    //
    BuildPullingService service;
    const BuildPullingService::PullResult result = service.recordPulls(requests);

    if (!result.success) {
        qCritical() << "Pull List reconciliation failed through BuildPullingService."
                    << "BuildId:" << m_buildId
                    << "Message:" << result.message;

        QMessageBox::critical(this,
                              "Reconcile Pull List",
                              QString("Unable to reconcile the Pull List.\n\n"
                                      "%1\n\n"
                                      "No changes were saved.")
                                  .arg(result.message));
        return;
    }

    qInfo() << "Pull List reconciled through BuildPullingService."
            << "BuildId:" << m_buildId
            << "Rows:" << result.rowsPulled
            << "PiecesPulled:" << result.piecesPulled
            << "ExactRows:" << m_exactCount
            << "PartialRows:" << m_partialCount
            << "ZeroRows:" << m_zeroCount;

    QMessageBox::information(this,
                             "Reconcile Pull List",
                             "Pull List reconciled successfully.");

    accept();
}
