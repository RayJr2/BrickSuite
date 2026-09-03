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

#include "LostInventoryDialog.h"

#include "FoundInventoryDialog.h"

#include "../../app/WorkspaceContext.h"

#include "../../models/LostInventoryItem.h"
#include "../../models/StorageLocation.h"

#include "../../repositories/LostInventoryRepository.h"
#include "../../repositories/StorageLocationRepository.h"
#include "../../services/storage/SessionStorageSelectionService.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

LostInventoryDialog::LostInventoryDialog(
    WorkspaceContext& workspaceContext,
    SessionStorageSelectionService& sessionStorageSelectionService,
    QWidget* parent)
    : QDialog(parent)
    , m_workspaceContext(workspaceContext)
    , m_sessionStorageSelectionService(sessionStorageSelectionService)
{
    setWindowTitle("Lost Inventory");

    resize(950, 500);

    auto* layout = new QVBoxLayout(this);

    m_table = new QTableWidget(this);

    m_table->setColumnCount(7);

    m_table->setHorizontalHeaderLabels(QStringList() << "Part #"
                                                     << "Name"
                                                     << "Color"
                                                     << "Lost Qty"
                                                     << "Last Known Location"
                                                     << "Last Lost"
                                                     << "Action");

    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);

    m_table->verticalHeader()->setVisible(false);

    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);

    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    m_statusLabel = new QLabel(this);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(m_table, 1);

    layout->addWidget(m_statusLabel);

    layout->addWidget(buttonBox);

    loadLostInventory();
}

void LostInventoryDialog::loadLostInventory()
{
    m_table->setRowCount(0);

    if (!m_workspaceContext.hasCurrentWorkspace()) {
        m_statusLabel->setText("No workspace selected.");

        return;
    }

    LostInventoryRepository repository;

    const QList<LostInventoryItem> items = repository.getOutstanding(
        m_workspaceContext.currentWorkspaceId());

    //
    // Build full storage paths once.
    //
    StorageLocationRepository storageRepository;

    const QList<StorageLocation> locations = storageRepository.getByWorkspace(
        m_workspaceContext.currentWorkspaceId());

    QHash<int, StorageLocation> locationById;

    for (const StorageLocation& location : locations) {
        locationById.insert(location.id(), location);
    }

    auto storagePath = [&locationById](int locationId) {
        QStringList parts;

        int currentId = locationId;

        int safetyCount = 0;

        while (currentId > 0 && safetyCount < 100) {
            if (!locationById.contains(currentId)) {
                break;
            }

            const StorageLocation location = locationById.value(currentId);

            parts.prepend(location.name());

            currentId = location.parentLocationId();

            ++safetyCount;
        }

        return parts.join(" / ");
    };

    int row = 0;

    for (const LostInventoryItem& item : items) {
        m_table->insertRow(row);

        m_table->setItem(row, 0, new QTableWidgetItem(item.partNumber));

        m_table->setItem(row, 1, new QTableWidgetItem(item.partName));

        m_table->setItem(row, 2, new QTableWidgetItem(item.colorName));

        auto* quantityItem = new QTableWidgetItem(QString::number(item.outstandingQuantity));

        quantityItem->setTextAlignment(Qt::AlignCenter);

        m_table->setItem(row, 3, quantityItem);

        QString lastLocation = storagePath(item.lastStorageLocationId);

        if (lastLocation.isEmpty() && item.lastStorageLocationId > 0) {
            lastLocation = QString("Location %1").arg(item.lastStorageLocationId);
        }

        m_table->setItem(row, 4, new QTableWidgetItem(lastLocation));

        const QString lostDate = item.lastLostUtc.isValid()
                                     ? item.lastLostUtc.toLocalTime().toString("yyyy-MM-dd HH:mm")
                                     : QString();

        m_table->setItem(row, 5, new QTableWidgetItem(lostDate));

        auto* actionCombo = new QComboBox(m_table);

        actionCombo->addItem("Actions...", QString());

        actionCombo->addItem("Found / Return...", "found");

        m_table->setCellWidget(row, 6, actionCombo);

        const int partId = item.partId;

        const int colorId = item.colorId;

        connect(actionCombo,
                &QComboBox::currentIndexChanged,
                this,
                [this, actionCombo, partId, colorId](int index) {
                    if (index <= 0)
                        return;

                    const QString action = actionCombo->itemData(index).toString();

                    actionCombo->setCurrentIndex(0);

                    if (action != "found")
                        return;

                    FoundInventoryDialog dialog(m_workspaceContext.currentWorkspaceId(),
                                                partId,
                                                colorId,
                                                m_sessionStorageSelectionService,
                                                this);

                    if (dialog.exec() == QDialog::Accepted) {
                        loadLostInventory();
                    }
                });

        ++row;
    }

    if (items.isEmpty()) {
        m_statusLabel->setText("No outstanding lost inventory.");
    } else {
        m_statusLabel->setText(QString("%1 lost Part/Color item(s).").arg(items.size()));
    }
}
