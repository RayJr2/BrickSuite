/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */

#include "RemoveInventoryDialog.h"

#include "../../models/Color.h"
#include "../../models/InventoryRecord.h"
#include "../../models/Part.h"
#include "../../models/StorageLocation.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/StorageLocationRepository.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>

RemoveInventoryDialog::RemoveInventoryDialog(int inventoryRecordId, QWidget* parent)
    : QDialog(parent)
    , m_inventoryRecordId(inventoryRecordId)
{
    setWindowTitle("Remove Inventory Entry");
    resize(560, 220);

    auto* layout = new QFormLayout(this);
    m_partLabel = new QLabel(this);
    m_contextLabel = new QLabel(this);
    m_quantitySpin = new QSpinBox(this);
    m_quantitySpin->setMinimum(1);
    m_notesEdit = new QLineEdit(this);
    m_notesEdit->setPlaceholderText("Optional reason for correction/removal");
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (auto* ok = m_buttonBox->button(QDialogButtonBox::Ok))
        ok->setText("Remove");

    layout->addRow("Part:", m_partLabel);
    layout->addRow("Current Inventory:", m_contextLabel);
    layout->addRow("Quantity to Remove:", m_quantitySpin);
    layout->addRow("Notes:", m_notesEdit);
    layout->addRow(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &RemoveInventoryDialog::removeEntry);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (!loadInventoryRecord()) {
        if (auto* ok = m_buttonBox->button(QDialogButtonBox::Ok))
            ok->setEnabled(false);
    }
}

bool RemoveInventoryDialog::loadInventoryRecord()
{
    InventoryRecordRepository inventoryRepository;
    const std::optional<InventoryRecord> record = inventoryRepository.getById(m_inventoryRecordId);
    if (!record || record->quantity() <= 0)
        return false;

    PartRepository partRepository;
    ColorRepository colorRepository;
    StorageLocationRepository storageRepository;

    const std::optional<Part> part = partRepository.getById(record->partId());
    const std::optional<Color> color = colorRepository.getById(record->colorId());
    const std::optional<StorageLocation> storage = storageRepository.getById(record->storageLocationId());

    if (part)
        m_partLabel->setText(QString("%1 — %2").arg(part->partNumber(), part->name()));

    QStringList context;
    if (color)
        context << QString("Color: %1").arg(color->name());
    if (storage)
        context << QString("Storage: %1").arg(storage->name());
    context << QString("Available Qty: %1").arg(record->quantity());
    m_contextLabel->setText(context.join("   |   "));

    m_quantitySpin->setMaximum(record->quantity());
    m_quantitySpin->setValue(record->quantity());
    return true;
}

void RemoveInventoryDialog::removeEntry()
{
    const auto answer = QMessageBox::question(
        this,
        "BrickSuite",
        QString("Remove %1 piece(s) from this inventory entry?\n\n"
                "This records a correction in inventory history; no database record is physically deleted.")
            .arg(m_quantitySpin->value()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (answer != QMessageBox::Yes)
        return;

    InventoryRecordRepository repository;
    if (!repository.removeEntry(m_inventoryRecordId, m_quantitySpin->value(), m_notesEdit->text())) {
        QMessageBox::critical(this, "BrickSuite", "Unable to remove the inventory entry.");
        return;
    }

    accept();
}
