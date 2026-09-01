/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */

#include "MoveInventoryDialog.h"

#include "../../app/WorkspaceContext.h"

#include "../../models/InventoryRecord.h"
#include "../../models/Part.h"
#include "../../models/StorageLocation.h"

#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/StorageLocationRepository.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHash>
#include <QSet>
#include <QLabel>
#include <QDebug>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>

namespace
{
// Session-only convenience. This deliberately is not persisted in UserSettings;
// it resets automatically when BrickSuite exits.
int s_lastMoveDestinationStorageLocationId = 0;
}

MoveInventoryDialog::MoveInventoryDialog(int inventoryRecordId,
                                         WorkspaceContext& workspaceContext,
                                         QWidget* parent)
    : QDialog(parent)
    , m_inventoryRecordId(inventoryRecordId)
    , m_workspaceContext(workspaceContext)
{
    setWindowTitle("Move Inventory");
    resize(550, 300);

    auto* layout = new QFormLayout(this);

    m_partLabel = new QLabel(this);
    m_currentStorageLabel = new QLabel(this);
    m_availableLabel = new QLabel(this);
    m_destinationCombo = new QComboBox(this);
    m_quantitySpin = new QSpinBox(this);
    m_quantitySpin->setMinimum(1);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    if (QPushButton* okButton = m_buttonBox->button(QDialogButtonBox::Ok))
        okButton->setText("Move");

    layout->addRow("Part:", m_partLabel);
    layout->addRow("Current Storage:", m_currentStorageLabel);
    layout->addRow("Available Quantity:", m_availableLabel);
    layout->addRow("Destination:", m_destinationCombo);
    layout->addRow("Quantity to Move:", m_quantitySpin);
    layout->addRow(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &MoveInventoryDialog::moveInventory);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (!loadInventoryRecord()) {
        if (QPushButton* moveButton = m_buttonBox->button(QDialogButtonBox::Ok))
            moveButton->setEnabled(false);
        return;
    }

    loadStorageLocations();
}

bool MoveInventoryDialog::loadInventoryRecord()
{
    InventoryRecordRepository inventoryRepository;
    const std::optional<InventoryRecord> record = inventoryRepository.getById(m_inventoryRecordId);

    if (!record || record->quantity() <= 0) {
        QMessageBox::warning(this, "BrickSuite", "Unable to load movable inventory.");
        return false;
    }

    if (record->workspaceId() != m_workspaceContext.currentWorkspaceId())
        return false;

    m_sourceStorageLocationId = record->storageLocationId();
    m_availableQuantity = record->quantity();
    m_availableLabel->setText(QString::number(m_availableQuantity));
    m_quantitySpin->setMaximum(m_availableQuantity);
    m_quantitySpin->setValue(m_availableQuantity);

    PartRepository partRepository;
    const std::optional<Part> part = partRepository.getById(record->partId());

    if (part)
        m_partLabel->setText(QString("%1 — %2").arg(part->partNumber()).arg(part->name()));

    return true;
}

void MoveInventoryDialog::loadStorageLocations()
{
    StorageLocationRepository repository;
    const QList<StorageLocation> locations = repository.getByWorkspace(
        m_workspaceContext.currentWorkspaceId());

    QHash<int, StorageLocation> locationById;
    QSet<int> activeParentIds;

    for (const StorageLocation& location : locations) {
        locationById.insert(location.id(), location);
        if (location.parentLocationId() > 0)
            activeParentIds.insert(location.parentLocationId());
    }

    QString sourcePath;

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

        const QString path = pathParts.join(" / ");

        if (location.id() == m_sourceStorageLocationId) {
            sourcePath = path;
            continue;
        }

        if (activeParentIds.contains(location.id()))
            continue;

        m_destinationCombo->addItem(path, location.id());
    }

    m_currentStorageLabel->setText(sourcePath);

    // Restore the last successful destination from this BrickSuite session,
    // provided it is valid for this move (the source itself is excluded).
    if (s_lastMoveDestinationStorageLocationId > 0) {
        const int rememberedIndex =
            m_destinationCombo->findData(s_lastMoveDestinationStorageLocationId);
        if (rememberedIndex >= 0)
            m_destinationCombo->setCurrentIndex(rememberedIndex);
    }
}

void MoveInventoryDialog::moveInventory()
{
    const int destinationId = m_destinationCombo->currentData().toInt();

    if (destinationId <= 0) {
        QMessageBox::warning(this, "BrickSuite", "Select a destination storage location.");
        return;
    }

    const int quantity = m_quantitySpin->value();

    if (quantity <= 0 || quantity > m_availableQuantity) {
        QMessageBox::warning(this, "BrickSuite", "Enter a valid quantity to move.");
        return;
    }

    InventoryRecordRepository repository;

    if (!repository.moveInventory(m_inventoryRecordId, destinationId, quantity)) {
        qCritical() << "Inventory move failed."
                    << "InventoryRecordId:" << m_inventoryRecordId
                    << "DestinationStorageLocationId:" << destinationId
                    << "Quantity:" << quantity;
        QMessageBox::critical(this, "BrickSuite", "Unable to move inventory.");
        return;
    }

    // Remember only a successfully completed move.
    s_lastMoveDestinationStorageLocationId = destinationId;

    qInfo() << "Inventory moved."
            << "InventoryRecordId:" << m_inventoryRecordId
            << "FromStorageLocationId:" << m_sourceStorageLocationId
            << "ToStorageLocationId:" << destinationId
            << "Quantity:" << quantity;

    accept();
}
