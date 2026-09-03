#include "CollectionItemDialog.h"

#include "../help/HelpManager.h"
#include "../help/HelpTopic.h"
#include "../../models/CollectionSearchResult.h"
#include "../../models/StorageLocation.h"
#include "../../repositories/CollectionRepository.h"
#include "../../repositories/StorageLocationRepository.h"
#include "../../services/collection/CollectionItemService.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QLocale>
#include <QPushButton>
#include <QSet>
#include <QStringList>
#include <QTextEdit>

namespace {
QString pathFor(const StorageLocation& location, const QHash<int, StorageLocation>& byId)
{
    QStringList names{location.name()};
    int parentId = location.parentLocationId();
    QSet<int> visited;
    while (parentId > 0 && !visited.contains(parentId) && byId.contains(parentId)) {
        visited.insert(parentId);
        const StorageLocation parent = byId.value(parentId);
        names.prepend(parent.name());
        parentId = parent.parentLocationId();
    }
    return names.join(QStringLiteral(" → "));
}
}

CollectionItemDialog::CollectionItemDialog(int collectionItemId, QWidget* parent)
    : QDialog(parent), m_itemId(collectionItemId)
{
    setWindowTitle("Collection Details / Edit");
    resize(560, 430);
    HelpManager::setContextTopic(this, HelpTopic::MyCollection);

    const auto result = CollectionRepository().displayById(collectionItemId);
    auto* form = new QFormLayout(this);
    if (!result) {
        form->addRow(new QLabel("The Collection item is unavailable.", this));
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        form->addRow(buttons);
        return;
    }

    const CollectionItem& item = result->item;
    m_allowPartsSource = item.allowPartsSource;
    form->addRow("Type:", new QLabel(collectionItemTypeToString(item.type), this));
    form->addRow("Reference:", new QLabel(result->displayReference, this));
    form->addRow("Name:", new QLabel(result->displayName, this));
    QString source = item.sourceBuildId > 0
        ? QString("Build: %1%2").arg(result->sourceBuildName,
              result->sourceBuildReference.isEmpty() ? QString()
                  : QString(" (%1)").arg(result->sourceBuildReference))
        : QStringLiteral("Catalog / Existing Collection");
    form->addRow("Source:", new QLabel(source, this));
    form->addRow("Created:", new QLabel(QLocale().toString(item.createdUtc.toLocalTime(), QLocale::ShortFormat), this));
    form->addRow("Modified:", new QLabel(QLocale().toString(item.modifiedUtc.toLocalTime(), QLocale::ShortFormat), this));

    m_stateCombo = new QComboBox(this);
    for (CollectionItemState state : {CollectionItemState::Assembled,
                                      CollectionItemState::Unassembled,
                                      CollectionItemState::PartiallyAssembled,
                                      CollectionItemState::Sealed}) {
        m_stateCombo->addItem(collectionItemStateToString(state), static_cast<int>(state));
    }
    m_stateCombo->setCurrentIndex(m_stateCombo->findData(static_cast<int>(item.state)));
    m_conditionCombo = new QComboBox(this);
    for (const auto condition : {CollectionItemCondition::New, CollectionItemCondition::Used})
        m_conditionCombo->addItem(collectionItemConditionToString(condition),
                                  static_cast<int>(condition));
    m_conditionCombo->setCurrentIndex(m_conditionCombo->findData(static_cast<int>(item.condition)));
    m_completenessCombo = new QComboBox(this);
    for (const auto completeness : {CollectionItemCompleteness::Unknown,
                                    CollectionItemCompleteness::Complete,
                                    CollectionItemCompleteness::Incomplete})
        m_completenessCombo->addItem(collectionItemCompletenessToString(completeness),
                                     static_cast<int>(completeness));
    m_completenessCombo->setCurrentIndex(
        m_completenessCombo->findData(static_cast<int>(item.completeness)));

    m_locationCombo = new QComboBox(this);
    m_locationCombo->addItem("Unassigned", 0);
    StorageLocationRepository locations;
    const QList<StorageLocation> all = locations.getByWorkspaceIncludingInactive(item.workspaceId);
    QHash<int, StorageLocation> byId;
    for (const StorageLocation& location : all) byId.insert(location.id(), location);
    for (const StorageLocation& location : all) {
        const bool current = location.id() == item.storageLocationId;
        const bool valid = locations.isValidCollectionDestination(item.workspaceId, location.id());
        if (!valid && !current) continue;
        QString label = pathFor(location, byId);
        if (!location.isActive()) label += " (Inactive)";
        m_locationCombo->addItem(label, location.id());
    }
    const int locationIndex = m_locationCombo->findData(item.storageLocationId);
    if (locationIndex >= 0) m_locationCombo->setCurrentIndex(locationIndex);

    m_nicknameEdit = new QLineEdit(item.nickname, this);
    m_notesEdit = new QTextEdit(item.notes, this);
    form->addRow("State:", m_stateCombo);
    form->addRow("Condition:", m_conditionCombo);
    form->addRow("Completeness:", m_completenessCombo);
    form->addRow("Collection Location:", m_locationCombo);
    form->addRow("Nickname:", m_nicknameEdit);
    form->addRow("Notes:", m_notesEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close
                                          | QDialogButtonBox::Help, this);
    connect(buttons->button(QDialogButtonBox::Save), &QPushButton::clicked,
            this, &CollectionItemDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::helpRequested, this, [this]() {
        HelpManager::showTopic(HelpTopic::MyCollection, this);
    });
    form->addRow(buttons);
}

void CollectionItemDialog::save()
{
    const auto result = CollectionItemService().updateDetails(
        m_itemId,
        static_cast<CollectionItemState>(m_stateCombo->currentData().toInt()),
        m_locationCombo->currentData().toInt(), m_nicknameEdit->text(),
        m_notesEdit->toPlainText(), m_allowPartsSource != 0,
        static_cast<CollectionItemCondition>(m_conditionCombo->currentData().toInt()),
        static_cast<CollectionItemCompleteness>(m_completenessCombo->currentData().toInt()));
    if (!result.success) {
        QMessageBox::critical(this, "Update Collection Item", result.message);
        return;
    }
    emit itemChanged();
    accept();
}
