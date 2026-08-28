/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 */

#include "CorrectInventoryDialog.h"

#include "../../app/WorkspaceContext.h"
#include "../../models/Color.h"
#include "../../models/InventoryRecord.h"
#include "../../models/Part.h"
#include "../../models/StorageLocation.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/StorageLocationRepository.h"

#include <QCompleter>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTimer>
#include <QtGlobal>

CorrectInventoryDialog::CorrectInventoryDialog(int inventoryRecordId,
                                               WorkspaceContext& workspaceContext,
                                               QWidget* parent)
    : QDialog(parent)
    , m_inventoryRecordId(inventoryRecordId)
    , m_workspaceContext(workspaceContext)
{
    setWindowTitle("Correct Inventory Entry");
    resize(640, 280);

    auto* layout = new QFormLayout(this);

    m_currentPartLabel = new QLabel(this);
    m_contextLabel = new QLabel(this);
    m_contextLabel->setWordWrap(true);

    m_partSearchEdit = new QLineEdit(this);
    m_partSearchEdit->setPlaceholderText("Search by Part Number or Name");

    m_searchModel = new QStandardItemModel(this);
    m_completer = new QCompleter(m_searchModel, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    m_completer->setMaxVisibleItems(12);
    m_partSearchEdit->setCompleter(m_completer);

    m_resolvedLabel = new QLabel(this);
    m_resolvedLabel->setVisible(false);

    m_quantitySpin = new QSpinBox(this);
    m_quantitySpin->setMinimum(1);

    m_notesEdit = new QLineEdit(this);
    m_notesEdit->setPlaceholderText("Optional correction note");

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);

    layout->addRow("Current Part:", m_currentPartLabel);
    layout->addRow("Current Inventory:", m_contextLabel);
    layout->addRow("Correct Part:", m_partSearchEdit);
    layout->addRow("Resolved:", m_resolvedLabel);
    layout->addRow("Quantity to Correct:", m_quantitySpin);
    layout->addRow("Notes:", m_notesEdit);
    layout->addRow(m_buttonBox);

    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(200);

    connect(m_partSearchEdit, &QLineEdit::textEdited, this, [this]() {
        m_replacementPartId = 0;
        m_resolvedLabel->clear();
        m_resolvedLabel->setVisible(false);
        updateSaveButtonState();
        m_searchTimer->start();
    });
    connect(m_searchTimer, &QTimer::timeout, this, &CorrectInventoryDialog::updatePartSearch);
    connect(m_completer,
            QOverload<const QModelIndex&>::of(&QCompleter::activated),
            this,
            &CorrectInventoryDialog::applySelectedPart);
    connect(m_partSearchEdit, &QLineEdit::returnPressed, this, &CorrectInventoryDialog::resolveEnteredPart);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &CorrectInventoryDialog::saveCorrection);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (!loadInventoryRecord()) {
        if (auto* save = m_buttonBox->button(QDialogButtonBox::Save))
            save->setEnabled(false);
    }

    updateSaveButtonState();
}

bool CorrectInventoryDialog::loadInventoryRecord()
{
    InventoryRecordRepository inventoryRepository;
    const std::optional<InventoryRecord> record = inventoryRepository.getById(m_inventoryRecordId);
    if (!record)
        return false;

    m_originalPartId = record->partId();
    m_currentQuantity = record->quantity();
    m_quantitySpin->setMaximum(qMax(1, m_currentQuantity));
    m_quantitySpin->setValue(m_currentQuantity);

    PartRepository partRepository;
    ColorRepository colorRepository;
    StorageLocationRepository storageRepository;

    const std::optional<Part> part = partRepository.getById(record->partId());
    const std::optional<Color> color = colorRepository.getById(record->colorId());
    const std::optional<StorageLocation> storage = storageRepository.getById(record->storageLocationId());

    if (part)
        m_currentPartLabel->setText(QString("%1 — %2").arg(part->partNumber(), part->name()));

    QStringList context;
    if (color)
        context << QString("Color: %1").arg(color->name());
    if (storage)
        context << QString("Storage: %1").arg(storage->name());
    context << QString("Available Qty: %1").arg(m_currentQuantity);
    m_contextLabel->setText(context.join("   |   "));

    return true;
}

void CorrectInventoryDialog::updatePartSearch()
{
    m_searchModel->clear();

    PartRepository repository;
    const QList<Part> parts = repository.searchForInventoryEntry(m_partSearchEdit->text(), 20);

    for (const Part& part : parts) {
        auto* item = new QStandardItem(QString("%1 — %2").arg(part.partNumber(), part.name()));
        item->setData(part.id(), Qt::UserRole);
        item->setData(part.partNumber(), Qt::UserRole + 1);
        m_searchModel->appendRow(item);
    }

    if (!parts.isEmpty())
        m_completer->complete();
}

void CorrectInventoryDialog::applySelectedPart(const QModelIndex& index)
{
    if (!index.isValid())
        return;

    const int partId = index.data(Qt::UserRole).toInt();
    if (partId <= 0)
        return;

    setResolvedPart(partId, index.data(Qt::DisplayRole).toString());
}

void CorrectInventoryDialog::resolveEnteredPart()
{
    PartRepository repository;
    const QString entered = m_partSearchEdit->text().trimmed();
    if (entered.isEmpty())
        return;

    if (const std::optional<Part> exact = repository.getByPartNumber(entered)) {
        setResolvedPart(exact->id(), QString("%1 — %2").arg(exact->partNumber(), exact->name()));
        return;
    }

    const QList<Part> matches = repository.searchForInventoryEntry(entered, 20);
    for (const Part& part : matches) {
        if (part.partNumber().compare(entered, Qt::CaseInsensitive) == 0) {
            setResolvedPart(part.id(), QString("%1 — %2").arg(part.partNumber(), part.name()));
            return;
        }
    }
}

void CorrectInventoryDialog::setResolvedPart(int partId, const QString& displayText)
{
    m_replacementPartId = partId;
    m_partSearchEdit->setText(displayText.section(" — ", 0, 0));
    m_resolvedLabel->setText(displayText);
    m_resolvedLabel->setVisible(true);
    updateSaveButtonState();
}

void CorrectInventoryDialog::updateSaveButtonState()
{
    if (auto* save = m_buttonBox->button(QDialogButtonBox::Save))
        save->setEnabled(m_replacementPartId > 0 && m_replacementPartId != m_originalPartId && m_currentQuantity > 0);
}

void CorrectInventoryDialog::saveCorrection()
{
    InventoryRecordRepository repository;

    if (!repository.correctEntry(m_inventoryRecordId,
                                 m_replacementPartId,
                                 m_quantitySpin->value(),
                                 m_notesEdit->text())) {
        QMessageBox::critical(this, "BrickSuite", "Unable to correct the inventory entry.");
        return;
    }

    accept();
}
