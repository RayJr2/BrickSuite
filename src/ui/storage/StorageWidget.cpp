#include "StorageWidget.h"

#include "../../app/WorkspaceContext.h"
#include "../../models/StorageLocation.h"
#include "../../models/StorageLocationType.h"
#include "../../repositories/StorageLocationRepository.h"
#include "../../repositories/StorageLocationTypeRepository.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <qheaderview.h>

StorageWidget::StorageWidget(
    WorkspaceContext& workspaceContext,
    QWidget* parent)
    : QWidget(parent),
      m_workspaceContext(workspaceContext)
{
    auto* layout = new QVBoxLayout(this);

    auto* titleLabel = new QLabel("Storage", this);

    m_tree = new QTreeWidget(this);

    m_tree->setColumnCount(2);
    // Set auto fit content
    m_tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tree->setHeaderLabels(
        QStringList() << "Location" << "Type");

    m_addButton = new QPushButton("Add Location", this);

    m_editButton = new QPushButton("Edit Location", this);

    m_deactivateButton = new QPushButton("Deactivate Location", this);

    m_reactivateButton = new QPushButton("Reactivate Location", this);

    auto* buttonLayout = new QHBoxLayout();

    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_editButton);
    buttonLayout->addWidget(m_deactivateButton);
    buttonLayout->addWidget(m_reactivateButton);

    layout->addWidget(titleLabel);
    layout->addWidget(m_tree);
    layout->addLayout(buttonLayout);

    connect(m_addButton, &QPushButton::clicked, this, &StorageWidget::addLocation);

    connect(
        &m_workspaceContext,
        &WorkspaceContext::currentWorkspaceChanged,
        this,
        &StorageWidget::workspaceChanged);

    connect(m_editButton, &QPushButton::clicked, this, &StorageWidget::editLocation);

    connect(m_deactivateButton, &QPushButton::clicked, this, &StorageWidget::deactivateLocation);

    connect(m_reactivateButton, &QPushButton::clicked, this, &StorageWidget::reactivateLocation);

    workspaceChanged(
        m_workspaceContext.currentWorkspaceId());

    m_editButton->setEnabled(false);
    m_deactivateButton->setEnabled(false);
    m_reactivateButton->setEnabled(false);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        QTreeWidgetItem* item = m_tree->currentItem();
        const bool selected = item != nullptr;
        const bool isActive = selected && item->data(0, Qt::UserRole + 2).toBool();

        m_editButton->setEnabled(selected);
        m_deactivateButton->setEnabled(selected && isActive);
        m_reactivateButton->setEnabled(selected && !isActive);
    });
}

void StorageWidget::workspaceChanged(int workspaceId)
{
    Q_UNUSED(workspaceId);

    loadStorageTree();
}

void StorageWidget::loadStorageTree()
{
    m_tree->clear();

    m_editButton->setEnabled(false);
    m_deactivateButton->setEnabled(false);
    m_reactivateButton->setEnabled(false);

    if (!m_workspaceContext.hasCurrentWorkspace())
    {
        m_addButton->setEnabled(false);
        return;
    }

    m_addButton->setEnabled(true);

    StorageLocationRepository locationRepository;

    StorageLocationTypeRepository typeRepository;

    const QList<StorageLocation> locations =
        locationRepository.getByWorkspaceIncludingInactive(
            m_workspaceContext.currentWorkspaceId());

    const QList<StorageLocationType> types =
        typeRepository.getAll();

    QHash<int, QString> typeNames;

    for (const StorageLocationType& type : types)
    {
        typeNames.insert(
            type.id(),
            type.name());
    }

    QHash<int, QTreeWidgetItem*> items;

    // First pass:
    // Create every tree item.
    for (const StorageLocation& location : locations)
    {
        auto* item =
            new QTreeWidgetItem();

        item->setText(
            0,
            location.isActive()
                ? location.name()
                : QString("%1 (Inactive)").arg(location.name()));

        item->setText(
            1,
            typeNames.value(
                location.locationTypeId()));

        item->setData(
            0,
            Qt::UserRole,
            location.id());

        item->setData(
            0,
            Qt::UserRole + 1,
            location.parentLocationId());

        item->setData(
            0,
            Qt::UserRole + 2,
            location.isActive());

        items.insert(
            location.id(),
            item);
    }

    // Second pass:
    // Attach children to parents.
    for (const StorageLocation& location : locations)
    {
        QTreeWidgetItem* item =
            items.value(location.id());

        if (!item)
            continue;

        if (location.parentLocationId() > 0)
        {
            QTreeWidgetItem* parentItem =
                items.value(
                    location.parentLocationId());

            if (parentItem)
            {
                parentItem->addChild(item);
                continue;
            }
        }

        m_tree->addTopLevelItem(item);
    }

    m_tree->expandAll();
}

void StorageWidget::addLocation()
{
    if (!m_workspaceContext.hasCurrentWorkspace())
        return;

    StorageLocationTypeRepository typeRepository;

    const QList<StorageLocationType> types =
        typeRepository.getActive();

    if (types.isEmpty())
    {
        QMessageBox::warning(
            this,
            "BrickSuite",
            "No active storage location types are available.");

        return;
    }

    QDialog dialog(this);

    dialog.setWindowTitle(
        "Add Storage Location");

    auto* formLayout =
        new QFormLayout(&dialog);

    auto* nameEdit =
        new QLineEdit(&dialog);

    auto* typeCombo =
        new QComboBox(&dialog);

    for (const StorageLocationType& type : types)
    {
        typeCombo->addItem(
            type.name(),
            type.id());
    }

    auto* buttonBox =
        new QDialogButtonBox(
            QDialogButtonBox::Ok |
            QDialogButtonBox::Cancel,
            &dialog);

    formLayout->addRow(
        "Name:",
        nameEdit);

    formLayout->addRow(
        "Type:",
        typeCombo);

    formLayout->addRow(
        buttonBox);

    connect(
        buttonBox,
        &QDialogButtonBox::accepted,
        &dialog,
        &QDialog::accept);

    connect(
        buttonBox,
        &QDialogButtonBox::rejected,
        &dialog,
        &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString name =
        nameEdit->text().trimmed();

    if (name.isEmpty())
    {
        QMessageBox::warning(
            this,
            "BrickSuite",
            "Please enter a storage location name.");

        return;
    }

    const int locationTypeId =
        typeCombo->currentData().toInt();

    int parentLocationId = 0;

    if (QTreeWidgetItem* currentItem =
            m_tree->currentItem())
    {
        parentLocationId =
            currentItem
                ->data(
                    0,
                    Qt::UserRole)
                .toInt();
    }

    StorageLocation location;

    location.setWorkspaceId(
        m_workspaceContext.currentWorkspaceId());

    location.setParentLocationId(
        parentLocationId);

    location.setLocationTypeId(
        locationTypeId);

    location.setName(name);

    location.setIsActive(true);

    StorageLocationRepository repository;

    if (!repository.create(location))
    {
        QMessageBox::critical(
            this,
            "BrickSuite",
            "Unable to create the storage location.");

        return;
    }

    loadStorageTree();
}

void StorageWidget::editLocation()
{
    QTreeWidgetItem* selectedItem = m_tree->currentItem();

    if (!selectedItem)
        return;

    const int locationId = selectedItem->data(0, Qt::UserRole).toInt();

    StorageLocationRepository locationRepository;

    const std::optional<StorageLocation> existing = locationRepository.getById(locationId);

    if (!existing) {
        QMessageBox::warning(this, "BrickSuite", "Unable to load the selected storage location.");

        return;
    }

    StorageLocationTypeRepository typeRepository;

    const QList<StorageLocationType> types = typeRepository.getActive();

    const QList<StorageLocation> allLocations = locationRepository.getByWorkspace(
        m_workspaceContext.currentWorkspaceId());

    QDialog dialog(this);

    dialog.setWindowTitle("Edit Storage Location");

    auto* formLayout = new QFormLayout(&dialog);

    auto* nameEdit = new QLineEdit(existing->name(), &dialog);

    auto* typeCombo = new QComboBox(&dialog);

    for (const StorageLocationType& type : types) {
        typeCombo->addItem(type.name(), type.id());

        if (type.id() == existing->locationTypeId()) {
            typeCombo->setCurrentIndex(typeCombo->count() - 1);
        }
    }

    auto* parentCombo = new QComboBox(&dialog);

    parentCombo->addItem("(Top Level)", 0);

    for (const StorageLocation& location : allLocations) {
        // A location cannot be its own parent.
        if (location.id() == locationId)
            continue;

        // A location also cannot be moved beneath
        // one of its descendants.
        if (locationRepository.isDescendant(locationId, location.id())) {
            continue;
        }

        parentCombo->addItem(location.name(), location.id());

        if (location.id() == existing->parentLocationId()) {
            parentCombo->setCurrentIndex(parentCombo->count() - 1);
        }
    }

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);

    formLayout->addRow("Name:", nameEdit);

    formLayout->addRow("Type:", typeCombo);

    formLayout->addRow("Parent:", parentCombo);

    formLayout->addRow(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString newName = nameEdit->text().trimmed();

    if (newName.isEmpty()) {
        QMessageBox::warning(this, "BrickSuite", "Please enter a storage location name.");

        return;
    }

    StorageLocation updated = *existing;

    updated.setName(newName);

    updated.setLocationTypeId(typeCombo->currentData().toInt());

    updated.setParentLocationId(parentCombo->currentData().toInt());

    if (!locationRepository.update(updated)) {
        QMessageBox::critical(this, "BrickSuite", "Unable to update the storage location.");

        return;
    }

    loadStorageTree();
}

void StorageWidget::deactivateLocation()
{
    QTreeWidgetItem* selectedItem = m_tree->currentItem();

    if (!selectedItem)
        return;

    const int locationId = selectedItem->data(0, Qt::UserRole).toInt();

    const QString locationName = selectedItem->text(0);

    StorageLocationRepository repository;

    if (repository.hasChildren(locationId)) {
        QMessageBox::warning(this,
                             "BrickSuite",
                             QString("\"%1\" contains one or more storage locations.\n\n"
                                     "Move or deactivate its child locations first.")
                                 .arg(locationName));

        return;
    }

    if (repository.hasInventory(locationId)) {
        QMessageBox::warning(this,
                             "BrickSuite",
                             QString("\"%1\" contains loose inventory.\n\n"
                                     "Move the inventory to another storage location before "
                                     "deactivating it.")
                                 .arg(locationName));

        return;
    }
    const QMessageBox::StandardButton answer
        = QMessageBox::question(this,
                                "Deactivate Storage Location",
                                QString("Deactivate \"%1\"?\n\n"
                                        "The location will no longer appear in the active "
                                        "storage hierarchy.")
                                    .arg(locationName),
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::No);

    if (answer != QMessageBox::Yes)
        return;

    if (!repository.deactivate(locationId)) {
        QMessageBox::critical(this, "BrickSuite", "Unable to deactivate the storage location.");

        return;
    }

    loadStorageTree();
}

void StorageWidget::reactivateLocation()
{
    QTreeWidgetItem* selectedItem = m_tree->currentItem();

    if (!selectedItem)
        return;

    const int locationId = selectedItem->data(0, Qt::UserRole).toInt();
    const int parentLocationId = selectedItem->data(0, Qt::UserRole + 1).toInt();

    QString locationName = selectedItem->text(0);
    locationName.remove(" (Inactive)");

    StorageLocationRepository repository;

    if (parentLocationId > 0) {
        const std::optional<StorageLocation> parent = repository.getById(parentLocationId);

        if (!parent.has_value()) {
            QMessageBox::critical(this,
                                  "BrickSuite",
                                  "Unable to determine the parent storage location.");
            return;
        }

        if (!parent->isActive()) {
            QMessageBox::warning(this,
                                 "BrickSuite",
                                 QString("\"%1\" cannot be reactivated because its parent "
                                         "location is inactive.\n\n"
                                         "Reactivate the parent location first.")
                                     .arg(locationName));
            return;
        }
    }

    const QMessageBox::StandardButton answer
        = QMessageBox::question(this,
                                "Reactivate Storage Location",
                                QString("Reactivate \"%1\"?\n\n"
                                        "The location will again be available for active "
                                        "storage operations.")
                                    .arg(locationName),
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::No);

    if (answer != QMessageBox::Yes)
        return;

    if (!repository.reactivate(locationId)) {
        QMessageBox::critical(this, "BrickSuite", "Unable to reactivate the storage location.");
        return;
    }

    loadStorageTree();
}

