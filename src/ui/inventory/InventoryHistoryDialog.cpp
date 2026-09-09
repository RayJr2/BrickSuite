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

#include "InventoryHistoryDialog.h"

#include "../../app/WorkspaceContext.h"

#include "../../models/Color.h"
#include "../../models/InventoryHistoryResult.h"
#include "../../models/Part.h"
#include "../../models/StorageLocation.h"

#include "../../repositories/ColorRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/StorageLocationRepository.h"
#include "../../services/application/ApplicationServices.h"
#include "../../services/application/RemoteReadApplicationServices.h"

#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

InventoryHistoryDialog::InventoryHistoryDialog(int partId,
                                               int colorId,
                                               WorkspaceContext& workspaceContext,
                                               InventoryApplicationService& inventoryService,
                                               QWidget* parent,
                                               RemoteReadApplicationServices* remoteReads,
                                               const QString& partNumber,
                                               int rebrickableColorId)
    : QDialog(parent)
    , m_partId(partId)
    , m_colorId(colorId)
    , m_workspaceContext(workspaceContext)
    , m_inventoryService(inventoryService)
    , m_remoteReads(remoteReads)
    , m_partNumber(partNumber)
    , m_rebrickableColorId(rebrickableColorId)
{
    setWindowTitle("Inventory History");
    resize(1100, 500);

    auto* layout = new QVBoxLayout(this);

    m_titleLabel = new QLabel(this);

    m_table = new QTableWidget(this);

    m_table->setColumnCount(9);

    m_table->setHorizontalHeaderLabels(QStringList() << "Date / Time"
                                                     << "Event"
                                                     << "Qty"
                                                     << "From"
                                                     << "To"
                                                     << "Condition"
                                                     << "Ownership"
                                                     << "Reference"
                                                     << "Details / Notes");

    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_table->verticalHeader()->setVisible(false);

    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    m_table->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Stretch);

    layout->addWidget(m_titleLabel);

    layout->addWidget(m_table);

    buildStoragePathCache();
    loadHeader();
    loadHistory();
}

void InventoryHistoryDialog::loadHeader()
{
    PartRepository partRepository;
    ColorRepository colorRepository;

    const std::optional<Part> part = partRepository.getById(m_partId);

    const std::optional<Color> color = colorRepository.getById(m_colorId);

    QString title = "Inventory History";

    if (part) {
        title = QString("%1 — %2").arg(part->partNumber()).arg(part->name());
    }

    if (color) {
        title += QString(" / %1").arg(color->name());
    }

    m_titleLabel->setText(title);
}

void InventoryHistoryDialog::buildStoragePathCache()
{
    m_storagePathById.clear();

    if (m_remoteReads) return;

    if (!m_workspaceContext.hasCurrentWorkspace())
        return;

    StorageLocationRepository repository;

    const QList<StorageLocation> locations = repository.getByWorkspace(
        m_workspaceContext.currentWorkspaceId());

    QHash<int, StorageLocation> locationById;

    for (const StorageLocation& location : locations) {
        locationById.insert(location.id(), location);
    }

    for (const StorageLocation& location : locations) {
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

        m_storagePathById.insert(location.id(), pathParts.join(" / "));
    }
}

QString InventoryHistoryDialog::storagePathForId(int storageLocationId) const
{
    return m_storagePathById.value(storageLocationId);
}

void InventoryHistoryDialog::loadHistory()
{
    m_table->setRowCount(0);

    if (!m_workspaceContext.hasCurrentWorkspace())
        return;

    if (m_remoteReads) {
        m_titleLabel->setText(QStringLiteral("Loading Inventory History from BrickSuite Host..."));
        m_remoteReads->inventoryHistory(m_workspaceContext.currentWorkspaceId(), m_partNumber,
            m_rebrickableColorId, this,
            [this](AsyncReadResult<QList<RemoteReadDto::InventoryHistoryRow>> result) {
                if (!result.succeeded()) {
                    m_titleLabel->setText(result.error == AsyncReadError::Timeout
                        ? QStringLiteral("The BrickSuite Host did not respond in time.")
                        : QStringLiteral("Unable to load Inventory History from BrickSuite Host."));
                    return;
                }
                int row = 0;
                m_table->setRowCount(result.value->size());
                for (const auto& entry : *result.value) {
                    const QString reference = QString("%1 %2").arg(entry.referenceType,
                        entry.referenceId).trimmed();
                    m_table->setItem(row, 0, new QTableWidgetItem(entry.createdUtc.toLocalTime().toString("yyyy-MM-dd hh:mm:ss")));
                    m_table->setItem(row, 1, new QTableWidgetItem(entry.movementType));
                    m_table->setItem(row, 2, new QTableWidgetItem(QString::number(entry.quantityChange)));
                    m_table->setItem(row, 3, new QTableWidgetItem(entry.fromStoragePath));
                    m_table->setItem(row, 4, new QTableWidgetItem(entry.toStoragePath));
                    m_table->setItem(row, 5, new QTableWidgetItem(entry.condition));
                    m_table->setItem(row, 6, new QTableWidgetItem(entry.ownershipType));
                    m_table->setItem(row, 7, new QTableWidgetItem(reference));
                    m_table->setItem(row, 8, new QTableWidgetItem(entry.notes));
                    ++row;
                }
                if (result.value->isEmpty()) m_titleLabel->setText(QStringLiteral("No Inventory History."));
                else loadHeader();
            });
        return;
    }

    const QList<InventoryHistoryResult> history
        = m_inventoryService.history(m_workspaceContext.currentWorkspaceId(), m_partId, m_colorId);

    int row = 0;

    for (const InventoryHistoryResult& result : history) {
        m_table->insertRow(row);

        const QString dateText = result.createdUtc.toLocalTime().toString("yyyy-MM-dd hh:mm:ss");

        QString fromPath = storagePathForId(result.fromStorageLocationId);

        QString toPath = storagePathForId(result.toStorageLocationId);

        QString reference;

        if (!result.referenceType.isEmpty() || !result.referenceId.isEmpty()) {
            reference = QString("%1 %2").arg(result.referenceType).arg(result.referenceId).trimmed();
        }

        m_table->setItem(row, 0, new QTableWidgetItem(dateText));

        m_table->setItem(row, 1, new QTableWidgetItem(result.movementType));

        m_table->setItem(row, 2, new QTableWidgetItem(QString::number(result.quantityChange)));

        m_table->setItem(row, 3, new QTableWidgetItem(fromPath));

        m_table->setItem(row, 4, new QTableWidgetItem(toPath));

        m_table->setItem(row, 5, new QTableWidgetItem(result.condition));

        m_table->setItem(row, 6, new QTableWidgetItem(result.ownershipType));

        m_table->setItem(row, 7, new QTableWidgetItem(reference));

        m_table->setItem(row, 8, new QTableWidgetItem(result.notes));

        ++row;
    }
}
