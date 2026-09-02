#include "ReferenceDataDialog.h"

#include "ManufacturerEditDialog.h"
#include "../../repositories/ManufacturerRepository.h"
#include "../../services/reference/ManufacturerManagementService.h"
#include "../help/HelpManager.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QLocale>
#include <QPushButton>
#include <QShortcut>
#include <QTableWidget>
#include <QVBoxLayout>

ReferenceDataDialog::ReferenceDataDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Lists & Reference Data");
    resize(900, 430);
    auto* layout = new QVBoxLayout(this);
    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({"Name", "Code", "Active", "Origin",
                                        "Inventory Usage", "Build / Provenance", "Modified"});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(m_table);
    auto* actions = new QHBoxLayout;
    auto* add = new QPushButton("Add...", this);
    m_edit = new QPushButton("Edit...", this);
    m_activate = new QPushButton("Activate", this);
    m_deactivate = new QPushButton("Deactivate", this);
    actions->addWidget(add); actions->addWidget(m_edit); actions->addWidget(m_activate);
    actions->addWidget(m_deactivate); actions->addStretch();
    layout->addLayout(actions);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Help | QDialogButtonBox::Close, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Help), &QPushButton::clicked, this,
            [this]() { HelpManager::showTopic(HelpTopic::ReferenceData, this); });
    new QShortcut(QKeySequence::HelpContents, this, [this]() {
        HelpManager::showTopic(HelpTopic::ReferenceData, this);
    });
    connect(add, &QPushButton::clicked, this, &ReferenceDataDialog::addManufacturer);
    connect(m_edit, &QPushButton::clicked, this, &ReferenceDataDialog::editManufacturer);
    connect(m_activate, &QPushButton::clicked, this, [this]() { changeActiveState(true); });
    connect(m_deactivate, &QPushButton::clicked, this, [this]() { changeActiveState(false); });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &ReferenceDataDialog::updateActions);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int) { editManufacturer(); });
    reload();
}

int ReferenceDataDialog::selectedManufacturerId() const
{
    const auto items = m_table->selectedItems();
    return items.isEmpty() ? 0 : items.first()->data(Qt::UserRole).toInt();
}

void ReferenceDataDialog::reload()
{
    const int selectedId = selectedManufacturerId();
    m_table->setRowCount(0);
    ManufacturerRepository repository;
    for (const Manufacturer& value : repository.getAll(false)) {
        const ManufacturerUsage usage = repository.usage(value.id());
        const int row = m_table->rowCount(); m_table->insertRow(row);
        auto* name = new QTableWidgetItem(value.name()); name->setData(Qt::UserRole, value.id());
        m_table->setItem(row, 0, name);
        m_table->setItem(row, 1, new QTableWidgetItem(value.code()));
        m_table->setItem(row, 2, new QTableWidgetItem(value.isActive() ? "Yes" : "No"));
        m_table->setItem(row, 3, new QTableWidgetItem(value.origin()));
        m_table->setItem(row, 4, new QTableWidgetItem(usage.success
            ? QString("%1 records / %2 pieces").arg(usage.inventoryRecordCount).arg(usage.inventoryPieceQuantity)
            : "Unavailable"));
        m_table->setItem(row, 5, new QTableWidgetItem(usage.success
            ? QString("%1 Builds / %2 rows (%3 pieces)").arg(usage.buildCount).arg(usage.provenanceCount).arg(usage.provenancePieceQuantity)
            : "Unavailable"));
        m_table->setItem(row, 6, new QTableWidgetItem(
            QLocale().toString(value.modifiedUtc().toLocalTime(), QLocale::ShortFormat)));
        if (value.id() == selectedId) m_table->selectRow(row);
    }
    m_table->resizeColumnsToContents();
    updateActions();
}

void ReferenceDataDialog::addManufacturer()
{
    ManufacturerEditDialog dialog(nullptr, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const auto result = ManufacturerManagementService().create(dialog.manufacturer());
    if (!result.success) { QMessageBox::warning(this, "Add Manufacturer", result.message); return; }
    reload(); emit manufacturersChanged();
}

void ReferenceDataDialog::editManufacturer()
{
    const auto value = ManufacturerRepository().getById(selectedManufacturerId());
    if (!value) return;
    if (value->origin() != "User") {
        QMessageBox::information(this, "Edit Manufacturer",
                                 "BrickSuite and provider-owned Manufacturers are managed by their source.");
        return;
    }
    ManufacturerEditDialog dialog(&*value, this);
    if (dialog.exec() != QDialog::Accepted) return;
    const auto result = ManufacturerManagementService().edit(dialog.manufacturer());
    if (!result.success) { QMessageBox::warning(this, "Edit Manufacturer", result.message); return; }
    reload(); emit manufacturersChanged();
}

void ReferenceDataDialog::changeActiveState(bool active)
{
    const int id = selectedManufacturerId();
    const auto usage = ManufacturerManagementService().usage(id);
    if (!usage.success) { QMessageBox::warning(this, "Manufacturer", usage.message); return; }
    if (!active && usage.usage.inUse()) {
        const QString text = QString("This Manufacturer is used by %1 inventory record(s), %2 inventory piece(s), %3 Build(s), and %4 provenance row(s).\n\nExisting records will remain intact and readable. Deactivate it?")
            .arg(usage.usage.inventoryRecordCount).arg(usage.usage.inventoryPieceQuantity)
            .arg(usage.usage.buildCount).arg(usage.usage.provenanceCount);
        if (QMessageBox::question(this, "Deactivate Manufacturer", text) != QMessageBox::Yes) return;
    }
    const auto result = ManufacturerManagementService().setActive(id, active);
    if (!result.success) { QMessageBox::warning(this, "Manufacturer", result.message); return; }
    reload(); emit manufacturersChanged();
}

void ReferenceDataDialog::updateActions()
{
    const auto value = ManufacturerRepository().getById(selectedManufacturerId());
    const bool user = value && value->origin() == "User";
    m_edit->setEnabled(user);
    m_activate->setEnabled(user && !value->isActive());
    m_deactivate->setEnabled(user && value->isActive());
}
