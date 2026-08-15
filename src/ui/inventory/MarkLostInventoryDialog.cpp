#include "MarkLostInventoryDialog.h"

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
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>

MarkLostInventoryDialog::MarkLostInventoryDialog(int inventoryRecordId, QWidget* parent)
    : QDialog(parent)
    , m_inventoryRecordId(inventoryRecordId)
{
    setWindowTitle("Mark Inventory Lost");

    resize(500, 300);

    auto* layout = new QFormLayout(this);

    m_partLabel = new QLabel(this);

    m_colorLabel = new QLabel(this);

    m_locationLabel = new QLabel(this);

    m_currentQuantityLabel = new QLabel(this);

    m_quantityLostSpin = new QSpinBox(this);

    m_notesEdit = new QTextEdit(this);

    m_notesEdit->setMaximumHeight(90);

    m_notesEdit->setPlaceholderText("Optional note about where or how the part was lost.");

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    if (QPushButton* okButton = m_buttonBox->button(QDialogButtonBox::Ok)) {
        okButton->setText("Mark Lost");
    }

    layout->addRow("Part:", m_partLabel);

    layout->addRow("Color:", m_colorLabel);

    layout->addRow("Storage:", m_locationLabel);

    layout->addRow("Current Quantity:", m_currentQuantityLabel);

    layout->addRow("Quantity Lost:", m_quantityLostSpin);

    layout->addRow("Notes:", m_notesEdit);

    layout->addRow(m_buttonBox);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &MarkLostInventoryDialog::markLost);

    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (!loadInventory()) {
        if (QPushButton* okButton = m_buttonBox->button(QDialogButtonBox::Ok)) {
            okButton->setEnabled(false);
        }
    }
}

bool MarkLostInventoryDialog::loadInventory()
{
    InventoryRecordRepository inventoryRepository;

    const std::optional<InventoryRecord> record = inventoryRepository.getById(m_inventoryRecordId);

    if (!record) {
        QMessageBox::critical(this,
                              "Mark Inventory Lost",
                              "Unable to load the selected inventory record.");

        return false;
    }

    if (record->quantity() <= 0) {
        QMessageBox::information(this,
                                 "Mark Inventory Lost",
                                 "This inventory record does not contain any "
                                 "loose pieces that can be marked lost.");

        return false;
    }

    m_currentQuantity = record->quantity();

    PartRepository partRepository;

    const std::optional<Part> part = partRepository.getById(record->partId());

    ColorRepository colorRepository;

    const std::optional<Color> color = colorRepository.getById(record->colorId());

    StorageLocationRepository storageRepository;

    const std::optional<StorageLocation> location = storageRepository.getById(
        record->storageLocationId());

    if (part) {
        m_partLabel->setText(QString("%1 — %2").arg(part->partNumber(), part->name()));
    } else {
        m_partLabel->setText(QString("Part ID %1").arg(record->partId()));
    }

    m_colorLabel->setText(color ? color->name() : QString("Color ID %1").arg(record->colorId()));

    m_locationLabel->setText(location ? location->name()
                                      : QString("Location ID %1").arg(record->storageLocationId()));

    m_currentQuantityLabel->setText(QString::number(m_currentQuantity));

    m_quantityLostSpin->setRange(1, m_currentQuantity);

    m_quantityLostSpin->setValue(1);

    return true;
}

void MarkLostInventoryDialog::markLost()
{
    const int quantityLost = m_quantityLostSpin->value();

    if (quantityLost <= 0 || quantityLost > m_currentQuantity) {
        QMessageBox::warning(this, "Mark Inventory Lost", "Enter a valid quantity to mark lost.");

        return;
    }

    const QMessageBox::StandardButton response
        = QMessageBox::warning(this,
                               "Mark Inventory Lost",
                               QString("Mark %1 piece(s) as lost?\n\n"
                                       "Current Quantity: %2\n"
                                       "Quantity Lost: %3\n"
                                       "Remaining Loose Inventory: %4\n\n"
                                       "This will reduce My Loose Inventory "
                                       "and create a Lost movement-history record.")
                                   .arg(quantityLost)
                                   .arg(m_currentQuantity)
                                   .arg(quantityLost)
                                   .arg(m_currentQuantity - quantityLost),
                               QMessageBox::Yes | QMessageBox::No,
                               QMessageBox::No);

    if (response != QMessageBox::Yes)
        return;

    InventoryRecordRepository repository;

    if (!repository.markLost(m_inventoryRecordId,
                             quantityLost,
                             m_notesEdit->toPlainText().trimmed())) {
        QMessageBox::critical(this,
                              "Mark Inventory Lost",
                              "Unable to mark the inventory as lost.\n\n"
                              "No changes were saved.");

        return;
    }

    QMessageBox::information(this,
                             "Mark Inventory Lost",
                             QString("%1 piece(s) marked lost successfully.").arg(quantityLost));

    accept();
}