#include "MyCollectionWidget.h"

#include "CollectionItemDialog.h"
#include "../help/HelpManager.h"
#include "../help/HelpTopic.h"
#include "../../app/WorkspaceContext.h"
#include "../../models/CollectionSearchCriteria.h"
#include "../../models/CollectionSearchResult.h"
#include "../../models/StorageLocation.h"
#include "../../repositories/CollectionRepository.h"
#include "../../repositories/StorageLocationRepository.h"
#include "../../services/collection/CollectionItemService.h"
#include "../../services/images/MinifigImageService.h"
#include "../../services/images/SetImageService.h"
#include "../../settings/UserSettings.h"

#include <QComboBox>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
constexpr int ItemIdRole = Qt::UserRole;
constexpr int ImageKeyRole = Qt::UserRole + 1;
QString locationPath(const StorageLocation& location, const QHash<int, StorageLocation>& byId)
{
    QStringList parts{location.name()};
    QSet<int> seen;
    int parent = location.parentLocationId();
    while (parent > 0 && !seen.contains(parent) && byId.contains(parent)) {
        seen.insert(parent);
        const auto value = byId.value(parent);
        parts.prepend(value.name());
        parent = value.parentLocationId();
    }
    return parts.join(QStringLiteral(" → "));
}
}

MyCollectionWidget::MyCollectionWidget(WorkspaceContext& workspaceContext, QWidget* parent)
    : QWidget(parent), m_workspaceContext(workspaceContext)
{
    HelpManager::setContextTopic(this, HelpTopic::MyCollection);
    m_setImages = new SetImageService(this);
    m_minifigImages = new MinifigImageService(this);
    auto* root = new QVBoxLayout(this);
    auto* filters = new QHBoxLayout;
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Reference, name, or nickname");
    m_searchEdit->setMinimumWidth(220);
    m_searchEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("All Types", static_cast<int>(CollectionItemType::Invalid));
    for (auto type : {CollectionItemType::Set, CollectionItemType::Minifig, CollectionItemType::Moc})
        m_typeCombo->addItem(collectionItemTypeToString(type), static_cast<int>(type));
    m_stateCombo = new QComboBox(this);
    m_stateCombo->addItem("All States", static_cast<int>(CollectionItemState::Invalid));
    for (auto state : {CollectionItemState::Assembled, CollectionItemState::Unassembled,
                       CollectionItemState::PartiallyAssembled, CollectionItemState::Sealed})
        m_stateCombo->addItem(collectionItemStateToString(state), static_cast<int>(state));
    m_conditionCombo = new QComboBox(this);
    m_conditionCombo->addItem("All Conditions", static_cast<int>(CollectionItemCondition::Invalid));
    for (auto condition : {CollectionItemCondition::New, CollectionItemCondition::Used})
        m_conditionCombo->addItem(collectionItemConditionToString(condition), static_cast<int>(condition));
    m_completenessCombo = new QComboBox(this);
    m_completenessCombo->addItem("All Completeness", static_cast<int>(CollectionItemCompleteness::Invalid));
    for (auto completeness : {CollectionItemCompleteness::Unknown,
                              CollectionItemCompleteness::Complete,
                              CollectionItemCompleteness::Incomplete})
        m_completenessCombo->addItem(collectionItemCompletenessToString(completeness),
                                     static_cast<int>(completeness));
    m_locationCombo = new QComboBox(this);
    m_locationCombo->setMinimumWidth(240);
    m_locationCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_activeCombo = new QComboBox(this);
    m_activeCombo->addItem("Active", 1);
    m_activeCombo->addItem("Archived", 0);
    m_activeCombo->addItem("Active and Archived", -1);
    auto* searchButton = new QPushButton("Search", this);
    m_typeCombo->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_stateCombo->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_conditionCombo->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_completenessCombo->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_activeCombo->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    searchButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    filters->addWidget(new QLabel("Search:", this)); filters->addWidget(m_searchEdit, 2);
    filters->addWidget(new QLabel("Type:", this)); filters->addWidget(m_typeCombo);
    filters->addWidget(new QLabel("State:", this)); filters->addWidget(m_stateCombo);
    filters->addWidget(new QLabel("Condition:", this)); filters->addWidget(m_conditionCombo);
    filters->addWidget(new QLabel("Completeness:", this)); filters->addWidget(m_completenessCombo);
    filters->addWidget(new QLabel("Location:", this)); filters->addWidget(m_locationCombo, 1);
    filters->addWidget(m_activeCombo); filters->addWidget(searchButton);

    m_messageLabel = new QLabel(this);
    m_table = new QTableWidget(this);
    m_table->setColumnCount(11);
    m_table->setHorizontalHeaderLabels({"Image", "Type", "Reference", "Name", "Nickname",
                                        "State", "Condition", "Completeness", "Location",
                                        "Source", "Actions"});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->setDefaultSectionSize(56);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    auto* paging = new QHBoxLayout;
    m_summaryLabel = new QLabel(this); m_pageLabel = new QLabel(this);
    m_previousButton = new QPushButton("Previous", this);
    m_nextButton = new QPushButton("Next", this);
    paging->addWidget(m_summaryLabel); paging->addStretch(); paging->addWidget(m_previousButton);
    paging->addWidget(m_pageLabel); paging->addWidget(m_nextButton);
    root->addWidget(new QLabel("My Collection", this)); root->addLayout(filters);
    root->addWidget(m_messageLabel); root->addWidget(m_table, 1); root->addLayout(paging);

    auto criteriaChange = [this]() { loadPage(true); };
    connect(searchButton, &QPushButton::clicked, this, criteriaChange);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, criteriaChange);
    for (QComboBox* combo : {m_typeCombo, m_stateCombo, m_conditionCombo,
                             m_completenessCombo, m_locationCombo, m_activeCombo})
        connect(combo, &QComboBox::currentIndexChanged, this, criteriaChange);
    connect(m_previousButton, &QPushButton::clicked, this, [this]() {
        if (effectiveCriteriaKey() != m_loadedCriteriaKey) m_page = 0;
        else if (m_page > 0) --m_page;
        loadPage();
    });
    connect(m_nextButton, &QPushButton::clicked, this, [this]() {
        if (effectiveCriteriaKey() != m_loadedCriteriaKey) m_page = 0;
        else ++m_page;
        loadPage();
    });
    connect(&m_workspaceContext, &WorkspaceContext::currentWorkspaceChanged,
            this, [this](int) { refresh(); });

    auto applyImage = [this](const QString& key, const QString& path, const QString& type) {
        for (int row = 0; row < m_table->rowCount(); ++row) {
            auto* identity = m_table->item(row, 0);
            if (!identity || identity->data(ImageKeyRole).toString() != type + "|" + key) continue;
            auto* label = qobject_cast<QLabel*>(m_table->cellWidget(row, 0));
            QPixmap pixmap(path);
            if (label && !pixmap.isNull()) label->setPixmap(pixmap.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    };
    connect(m_setImages, &SetImageService::imageReady, this,
            [applyImage](const QString& key, const QString& path) { applyImage(key, path, "Set"); });
    connect(m_minifigImages, &MinifigImageService::imageReady, this,
            [applyImage](const QString& key, const QString& path) { applyImage(key, path, "Minifig"); });
    refresh();
}

QString MyCollectionWidget::effectiveCriteriaKey() const
{
    return QString("%1|%2|%3|%4|%5|%6|%7|%8").arg(m_workspaceContext.currentWorkspaceId())
        .arg(m_searchEdit->text().trimmed()).arg(m_typeCombo->currentData().toInt())
        .arg(m_stateCombo->currentData().toInt()).arg(m_conditionCombo->currentData().toInt())
        .arg(m_completenessCombo->currentData().toInt()).arg(m_locationCombo->currentData().toInt())
        .arg(m_activeCombo->currentData().toInt());
}

void MyCollectionWidget::refresh()
{
    loadLocations();
    m_page = 0;
    loadPage(true);
}

void MyCollectionWidget::selectCollectionItem(int collectionItemId)
{
    const auto selected = CollectionRepository().displayById(collectionItemId);
    if (!selected || selected->item.workspaceId != m_workspaceContext.currentWorkspaceId()) {
        refresh();
        return;
    }
    const QSignalBlocker typeBlocker(m_typeCombo);
    const QSignalBlocker stateBlocker(m_stateCombo);
    const QSignalBlocker conditionBlocker(m_conditionCombo);
    const QSignalBlocker completenessBlocker(m_completenessCombo);
    const QSignalBlocker locationBlocker(m_locationCombo);
    const QSignalBlocker activeBlocker(m_activeCombo);
    m_searchEdit->setText(selected->displayReference);
    m_typeCombo->setCurrentIndex(
        m_typeCombo->findData(static_cast<int>(selected->item.type)));
    m_stateCombo->setCurrentIndex(0);
    m_conditionCombo->setCurrentIndex(0);
    m_completenessCombo->setCurrentIndex(0);
    m_locationCombo->setCurrentIndex(0);
    m_activeCombo->setCurrentIndex(m_activeCombo->findData(selected->item.isActive ? 1 : 0));
    CollectionSearchCriteria criteria;
    criteria.workspaceId = m_workspaceContext.currentWorkspaceId();
    criteria.searchText = selected->displayReference;
    criteria.type = selected->item.type;
    criteria.activeState = selected->item.isActive ? 1 : 0;
    criteria.limit = UserSettings::instance().resultsPerPage();
    const int total = CollectionRepository().count(criteria);
    const int pages = qMax(1, (total + criteria.limit - 1) / criteria.limit);
    m_page = 0;
    for (int page = 0; page < pages; ++page) {
        criteria.offset = page * criteria.limit;
        const auto pageResults = CollectionRepository().search(criteria);
        bool found = false;
        for (const auto& result : pageResults) {
            if (result.item.id == collectionItemId) { found = true; break; }
        }
        if (found) { m_page = page; break; }
    }
    m_loadedCriteriaKey = effectiveCriteriaKey();
    loadPage();
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem* item = m_table->item(row, 0);
        if (item && item->data(ItemIdRole).toInt() == collectionItemId) {
            m_table->selectRow(row);
            m_table->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            break;
        }
    }
}

void MyCollectionWidget::loadLocations()
{
    const int selected = m_locationCombo->currentData().toInt();
    m_locationCombo->blockSignals(true);
    m_locationCombo->clear();
    m_locationCombo->addItem("All Locations", 0);
    m_locationCombo->addItem("Unassigned", -1);
    const auto locations = StorageLocationRepository().getCollectionHierarchy(
        m_workspaceContext.currentWorkspaceId());
    QHash<int, StorageLocation> byId;
    for (const auto& location : locations) byId.insert(location.id(), location);
    for (const auto& location : locations)
        m_locationCombo->addItem(locationPath(location, byId), location.id());
    const int index = m_locationCombo->findData(selected);
    if (index >= 0) m_locationCombo->setCurrentIndex(index);
    m_locationCombo->blockSignals(false);
}

void MyCollectionWidget::loadPage(bool criteriaChanged)
{
    const QString key = effectiveCriteriaKey();
    if (criteriaChanged || key != m_loadedCriteriaKey) m_page = 0;
    m_loadedCriteriaKey = key;
    m_minifigImages->clearQueuedRequests();
    CollectionSearchCriteria criteria;
    criteria.workspaceId = m_workspaceContext.currentWorkspaceId();
    criteria.searchText = m_searchEdit->text().trimmed();
    criteria.type = static_cast<CollectionItemType>(m_typeCombo->currentData().toInt());
    criteria.state = static_cast<CollectionItemState>(m_stateCombo->currentData().toInt());
    criteria.condition = static_cast<CollectionItemCondition>(m_conditionCombo->currentData().toInt());
    criteria.completeness = static_cast<CollectionItemCompleteness>(m_completenessCombo->currentData().toInt());
    criteria.storageLocationId = m_locationCombo->currentData().toInt();
    criteria.activeState = m_activeCombo->currentData().toInt();
    criteria.limit = UserSettings::instance().resultsPerPage();
    criteria.offset = m_page * criteria.limit;
    CollectionRepository repository;
    m_total = repository.count(criteria);
    const int pages = qMax(1, (m_total + criteria.limit - 1) / criteria.limit);
    if (m_page >= pages) { m_page = pages - 1; criteria.offset = m_page * criteria.limit; }
    const auto results = repository.search(criteria);
    m_table->setRowCount(0);
    for (const auto& result : results) {
        const int row = m_table->rowCount(); m_table->insertRow(row);
        auto* imageItem = new QTableWidgetItem;
        imageItem->setData(ItemIdRole, result.item.id);
        imageItem->setData(ImageKeyRole,
            collectionItemTypeToString(result.item.type) + "|" + result.displayReference);
        m_table->setItem(row, 0, imageItem);
        auto* image = new QLabel("No image", m_table); image->setAlignment(Qt::AlignCenter);
        m_table->setCellWidget(row, 0, image);
        m_table->setItem(row, 1, new QTableWidgetItem(collectionItemTypeToString(result.item.type)));
        m_table->setItem(row, 2, new QTableWidgetItem(result.displayReference));
        m_table->setItem(row, 3, new QTableWidgetItem(result.displayName));
        m_table->setItem(row, 4, new QTableWidgetItem(result.item.nickname));
        m_table->setItem(row, 5, new QTableWidgetItem(collectionItemStateToString(result.item.state)));
        m_table->setItem(row, 6, new QTableWidgetItem(collectionItemConditionToString(result.item.condition)));
        m_table->setItem(row, 7, new QTableWidgetItem(collectionItemCompletenessToString(result.item.completeness)));
        const int locationIndex = m_locationCombo->findData(result.item.storageLocationId);
        const QString location = result.item.storageLocationId <= 0 ? QStringLiteral("Unassigned")
            : (locationIndex >= 0 ? m_locationCombo->itemText(locationIndex) : result.locationName);
        m_table->setItem(row, 8, new QTableWidgetItem(location));
        const QString buildIdentity = result.sourceBuildReference.isEmpty()
            ? result.sourceBuildName
            : (result.sourceBuildName.isEmpty() ? result.sourceBuildReference
                : QString("%1 (%2)").arg(result.sourceBuildName, result.sourceBuildReference));
        const QString source = result.item.sourceBuildId > 0
            ? QString("Build: %1").arg(buildIdentity) : QStringLiteral("Catalog / Existing Collection");
        m_table->setItem(row, 9, new QTableWidgetItem(source));
        auto* actions = new QComboBox(m_table);
        actions->addItem("Actions..."); actions->addItem("Details / Edit", "details");
        actions->addItem(result.item.isActive ? "Archive" : "Reactivate",
                         result.item.isActive ? "archive" : "reactivate");
        m_table->setCellWidget(row, 10, actions);
        connect(actions, &QComboBox::currentIndexChanged, this,
                [this, actions, id=result.item.id, active=result.item.isActive](int index) {
            if (index <= 0) return;
            const QString action = actions->itemData(index).toString(); actions->setCurrentIndex(0);
            handleAction(id, active, action);
        });
        if (result.item.type == CollectionItemType::Set && !result.imageUrl.isEmpty())
            m_setImages->requestSetImage(result.displayReference, result.imageUrl);
        else if (result.item.type == CollectionItemType::Minifig && !result.imageUrl.isEmpty())
            m_minifigImages->requestMinifigImage(result.displayReference, result.imageUrl);
    }
    m_messageLabel->setText(results.isEmpty() ? "No Collection items match the current filters."
        : QString("Showing %1 - %2.").arg(criteria.offset + 1).arg(criteria.offset + results.size()));
    updatePaging();
}

void MyCollectionWidget::handleAction(int itemId, bool active, const QString& action)
{
    if (action == "details") {
        CollectionItemDialog dialog(itemId, this);
        connect(&dialog, &CollectionItemDialog::itemChanged, this, &MyCollectionWidget::refresh);
        dialog.exec();
        return;
    }
    const bool reactivate = action == "reactivate";
    if (action != "archive" && !reactivate) return;
    const auto answer = QMessageBox::question(this, reactivate ? "Reactivate Collection Item" : "Archive Collection Item",
        reactivate ? "Reactivate this Collection item?" : "Archive this Collection item?",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;
    const auto result = CollectionItemService().setActive(itemId, reactivate);
    if (!result.success) QMessageBox::critical(this, "Update Collection Item", result.message);
    else refresh();
    Q_UNUSED(active);
}

void MyCollectionWidget::updatePaging()
{
    const int perPage = UserSettings::instance().resultsPerPage();
    const int pages = qMax(1, (m_total + perPage - 1) / perPage);
    m_previousButton->setEnabled(m_page > 0);
    m_nextButton->setEnabled(m_page + 1 < pages);
    m_pageLabel->setText(QString("Page %1 of %2").arg(m_page + 1).arg(pages));
    m_summaryLabel->setText(QString("%1 %2").arg(QLocale().toString(m_total), m_total == 1 ? "Item" : "Items"));
}
