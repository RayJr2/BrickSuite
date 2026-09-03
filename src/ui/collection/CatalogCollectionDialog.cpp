#include "CatalogCollectionDialog.h"

#include "../help/HelpManager.h"
#include "../help/HelpTopic.h"
#include "../../models/StorageLocation.h"
#include "../../repositories/StorageLocationRepository.h"
#include "../../services/collection/CollectionItemService.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QStringList>
#include <QTextEdit>

namespace {
QString qualifiedPath(const StorageLocation& location,
                      const QHash<int, StorageLocation>& locations)
{
    QStringList names{location.name()};
    QSet<int> visited;
    int parentId = location.parentLocationId();
    while (parentId > 0 && !visited.contains(parentId) && locations.contains(parentId)) {
        visited.insert(parentId);
        const StorageLocation parent = locations.value(parentId);
        names.prepend(parent.name());
        parentId = parent.parentLocationId();
    }
    return names.join(QStringLiteral(" → "));
}
}

CatalogCollectionDialog::CatalogCollectionDialog(
    int workspaceId, CollectionItemType type, int catalogId,
    const QString& reference, const QString& name, QWidget* parent)
    : QDialog(parent), m_workspaceId(workspaceId), m_type(type), m_catalogId(catalogId)
{
    setWindowTitle("Add to Collection");
    resize(520, 360);
    HelpManager::setContextTopic(this, HelpTopic::MyCollection);
    auto* form = new QFormLayout(this);
    form->addRow("Type:", new QLabel(collectionItemTypeToString(type), this));
    form->addRow("Reference:", new QLabel(reference, this));
    auto* nameLabel = new QLabel(name, this);
    nameLabel->setWordWrap(true);
    form->addRow("Name:", nameLabel);

    m_stateCombo = new QComboBox(this);
    for (const auto state : {CollectionItemState::Assembled, CollectionItemState::Unassembled,
                             CollectionItemState::PartiallyAssembled, CollectionItemState::Sealed}) {
        m_stateCombo->addItem(collectionItemStateToString(state), static_cast<int>(state));
    }

    m_locationCombo = new QComboBox(this);
    m_locationCombo->addItem("Unassigned", 0);
    StorageLocationRepository repository;
    const QList<StorageLocation> hierarchy = repository.getCollectionHierarchy(workspaceId);
    QHash<int, StorageLocation> byId;
    for (const auto& location : hierarchy) byId.insert(location.id(), location);
    for (const auto& location : hierarchy) {
        if (!repository.isValidCollectionDestination(workspaceId, location.id())) continue;
        m_locationCombo->addItem(qualifiedPath(location, byId), location.id());
    }
    m_locationCombo->setMinimumWidth(260);
    m_nicknameEdit = new QLineEdit(this);
    m_notesEdit = new QTextEdit(this);
    form->addRow("State:", m_stateCombo);
    form->addRow("Collection Location:", m_locationCombo);
    form->addRow("Nickname:", m_nicknameEdit);
    form->addRow("Notes:", m_notesEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel
                                          | QDialogButtonBox::Help, this);
    connect(buttons->button(QDialogButtonBox::Save), &QPushButton::clicked,
            this, &CatalogCollectionDialog::createItem);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::helpRequested, this, [this]() {
        HelpManager::showTopic(HelpTopic::MyCollection, this);
    });
    form->addRow(buttons);
}

int CatalogCollectionDialog::createdCollectionItemId() const
{
    return m_createdItemId;
}

void CatalogCollectionDialog::createItem()
{
    const auto state = static_cast<CollectionItemState>(m_stateCombo->currentData().toInt());
    const int locationId = m_locationCombo->currentData().toInt();
    CollectionItemService service;
    CollectionItemService::Result result;
    if (m_type == CollectionItemType::Set) {
        result = service.createSet(m_workspaceId, m_catalogId, state, locationId, 0,
                                   m_nicknameEdit->text(), m_notesEdit->toPlainText());
    } else if (m_type == CollectionItemType::Minifig) {
        result = service.createMinifig(m_workspaceId, m_catalogId, state, locationId, 0,
                                       m_nicknameEdit->text(), m_notesEdit->toPlainText());
    } else {
        QMessageBox::critical(this, "Add to Collection", "This catalog type cannot be added.");
        return;
    }
    if (!result.success) {
        QMessageBox::critical(this, "Add to Collection", result.message);
        return;
    }
    m_createdItemId = result.collectionItemId;
    QMessageBox::information(this, "Add to Collection",
                             "A new physical item was added to My Collection.");
    accept();
}
