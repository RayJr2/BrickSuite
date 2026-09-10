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

#include "StorageWidget.h"
#include "StorageLocationDialog.h"
#include "RemoteStorageActionEligibility.h"

#include "../../app/WorkspaceContext.h"
#include "../../database/DatabaseManager.h"
#include "../../models/StorageLocation.h"
#include "../../models/StorageLocationType.h"
#include "../../repositories/StorageLocationRepository.h"
#include "../../repositories/StorageLocationTypeRepository.h"
#include "../../services/application/RemoteReadApplicationServices.h"
#include "../../services/application/RemoteStorageMutationApplicationService.h"
#include "../../services/application/HostStorageMutationService.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDebug>
#include <QElapsedTimer>
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
#include <QSet>
#include <algorithm>
#include <qheaderview.h>

namespace {
bool commitStorageMutation(QSqlDatabase database,
                           const HostStorageMutationService::Result& result)
{
    if (!result.success) {
        database.rollback();
        return false;
    }
    if (database.commit()) return true;
    database.rollback();
    return false;
}
}

StorageWidget::StorageWidget(
    WorkspaceContext& workspaceContext,
    RemoteReadApplicationServices* remoteReads,
    RemoteStorageMutationApplicationService* remoteMutations,
    QWidget* parent)
    : QWidget(parent),
      m_workspaceContext(workspaceContext),
      m_remoteReads(remoteReads),m_remoteMutations(remoteMutations)
{
    auto* layout = new QVBoxLayout(this);

    auto* titleLabel = new QLabel("Storage", this);
    m_statusLabel = new QLabel(this);

    m_tree = new QTreeWidget(this);

    m_tree->setColumnCount(3);
    // Set auto fit content
    m_tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tree->setHeaderLabels(
        QStringList() << "Location" << "Type" << "Used For");

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
    layout->addWidget(m_statusLabel);
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

        if(m_remoteReads)updateRemoteActionState();
        else {m_editButton->setEnabled(selected);m_deactivateButton->setEnabled(selected&&isActive);m_reactivateButton->setEnabled(selected&&!isActive);}
    });
}

void StorageWidget::workspaceChanged(int workspaceId)
{
    ++m_actionGeneration;
    const bool preserveRemoteRetry=m_remoteReads&&m_retainedRequest
        &&(workspaceId<=0||workspaceId==m_retainedRequest->workspaceId);
    if(!preserveRemoteRetry){m_retainedRequest.reset();m_retainedOperation.clear();if(m_remoteDialog)m_remoteDialog->close();if(m_remoteConfirmation)m_remoteConfirmation->close();}
    loadStorageTree();
}

void StorageWidget::refresh()
{
    loadStorageTree();
}

void StorageWidget::setRemoteSessionConnected(bool connected)
{
    if (!m_remoteReads) return;
    m_remoteConnected=connected;if(!connected){m_remoteStale=true;++m_actionGeneration;if(m_remoteDialog&&!m_remoteMutationPending)m_remoteDialog->close();if(m_remoteConfirmation)m_remoteConfirmation->close();}
    updateRemoteActionState();
    if (!connected) {
        ++m_storageRequestToken;
        m_statusLabel->setText(m_tree->topLevelItemCount() > 0
            ? QStringLiteral("Host disconnected; displayed Storage may be stale.")
            : QStringLiteral("BrickSuite Host Storage is unavailable."));
    }
}

void StorageWidget::loadStorageTree()
{
    if (m_remoteReads) { loadRemoteStorageTree(); return; }
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
        QStringList capabilities;
        if (location.allowsInventory()) capabilities << "Inventory";
        if (location.allowsCollection()) capabilities << "Collection";
        item->setText(2, capabilities.isEmpty() ? "Hierarchy only" : capabilities.join(" + "));

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

void StorageWidget::loadRemoteStorageTree()
{
    m_tree->clear();m_remoteStorage.clear();m_remoteStale=true;setMutationControlsEnabled(false);
    const int workspaceId = m_workspaceContext.currentWorkspaceId();
    if (workspaceId <= 0) { m_statusLabel->setText(QStringLiteral("Select a Host Workspace.")); emit remoteRefreshFinished(false); return; }
    if (!m_remoteReads->isAvailableFor(QStringLiteral("storage.list"))) {
        m_statusLabel->setText(QStringLiteral("BrickSuite Host Storage is unavailable.")); emit remoteRefreshFinished(false); return;
    }
    m_statusLabel->setText(QStringLiteral("Loading Storage from BrickSuite Host..."));
    m_storageRequestTimer.restart();
    m_storageRequestToken = m_remoteReads->listStorage(workspaceId, true, this,
        [this,workspaceId](AsyncReadResult<QList<RemoteReadDto::StorageSummary>> result) {
            if(result.token!=m_storageRequestToken||workspaceId!=m_workspaceContext.currentWorkspaceId())return;
            if(!result.succeeded()){m_remoteStale=true;m_statusLabel->setText(result.message.isEmpty()?QStringLiteral("Unable to load Storage from BrickSuite Host."):result.message);updateRemoteActionState();emit remoteRefreshFinished(false);return;}
            m_remoteStorage=*result.value;m_remoteStale=false;
            const qint64 roundTripMs=m_storageRequestTimer.isValid()?m_storageRequestTimer.elapsed():0;
            QElapsedTimer constructionTimer;constructionTimer.start();QHash<qint64,QTreeWidgetItem*> items;
            for(const auto& location:*result.value){auto* item=new QTreeWidgetItem;
                item->setText(0,location.active?location.name:QStringLiteral("%1 (Inactive)").arg(location.name));
                item->setText(1,location.typeName);QStringList uses;if(location.allowsInventory)uses<<QStringLiteral("Inventory");if(location.allowsCollection)uses<<QStringLiteral("Collection");item->setText(2,uses.isEmpty()?QStringLiteral("Hierarchy only"):uses.join(QStringLiteral(" + ")));
                item->setData(0,Qt::UserRole,QVariant::fromValue<qint64>(location.storageId));item->setData(0,Qt::UserRole+1,QVariant::fromValue<qint64>(location.parentStorageId));item->setData(0,Qt::UserRole+2,location.active);items.insert(location.storageId,item);}
            for(const auto& location:*result.value){auto* item=items.value(location.storageId);auto* parent=items.value(location.parentStorageId);if(parent)parent->addChild(item);else m_tree->addTopLevelItem(item);}
            m_tree->expandAll();m_statusLabel->setText(result.value->isEmpty()?QStringLiteral("This Host Workspace has no Storage locations."):QStringLiteral("%1 Storage locations from BrickSuite Host.").arg(result.value->size()));
            qDebug().noquote()<<"Remote Storage rows="<<result.value->size()<<"roundtripMs="<<roundTripMs<<"hierarchyMs="<<constructionTimer.elapsed();
            updateRemoteActionState();
            emit remoteRefreshFinished(true);
        });
}

void StorageWidget::setMutationControlsEnabled(bool enabled)
{
    m_addButton->setEnabled(enabled);m_editButton->setEnabled(false);
    m_deactivateButton->setEnabled(false);m_reactivateButton->setEnabled(false);
}

void StorageWidget::fetchRemoteTypes(std::function<void(bool)> completion)
{
    if(!m_remoteReads||!m_remoteReads->isAvailableFor("storage.types.list")){completion(false);return;}
    const quint64 generation=m_actionGeneration;const int workspace=m_workspaceContext.currentWorkspaceId();
    m_remoteReads->listStorageTypes(this,[this,generation,workspace,completion=std::move(completion)](AsyncReadResult<QList<RemoteReadDto::StorageType>> result)mutable{
        if(generation!=m_actionGeneration||workspace!=m_workspaceContext.currentWorkspaceId())return;
        if(!result.succeeded()||result.value->isEmpty()){m_statusLabel->setText(result.message.isEmpty()?QStringLiteral("No active Host Storage types are available."):result.message);completion(false);return;}
        m_remoteTypes=*result.value;completion(true);
    });
}

QList<StorageLocationDialog::Choice> StorageWidget::remoteParentChoices(qint64 excludedId)const
{
    QList<StorageLocationDialog::Choice> result;
    for(const auto&candidate:m_remoteStorage){if(!candidate.active||candidate.storageId==excludedId)continue;bool descendant=false;qint64 current=candidate.parentStorageId;QSet<qint64>seen;while(current>0&&!seen.contains(current)){if(current==excludedId){descendant=true;break;}seen.insert(current);auto it=std::find_if(m_remoteStorage.cbegin(),m_remoteStorage.cend(),[current](const auto&row){return row.storageId==current;});if(it==m_remoteStorage.cend())break;current=it->parentStorageId;}if(!descendant)result.append({candidate.storageId,candidate.displayPath.isEmpty()?candidate.name:candidate.displayPath});}
    return result;
}

void StorageWidget::remoteAddLocation()
{
    if(m_remoteDialog){m_remoteDialog->raise();m_remoteDialog->activateWindow();return;}
    if(!m_addButton->isEnabled())return;
    fetchRemoteTypes([this](bool ok){if(!ok)return;openRemoteDialog(StorageLocationDialog::Mode::Add,std::nullopt);});
}

void StorageWidget::remoteEditLocation()
{
    if(m_remoteDialog){m_remoteDialog->raise();m_remoteDialog->activateWindow();return;}
    auto*item=m_tree->currentItem();if(!item||!m_editButton->isEnabled())return;
    const qint64 id=item->data(0,Qt::UserRole).toLongLong();const int workspace=m_workspaceContext.currentWorkspaceId();const quint64 generation=m_actionGeneration;
    m_statusLabel->setText(QStringLiteral("Loading Storage details from BrickSuite Host..."));
    m_remoteReads->getStorage(workspace,id,this,[this,generation,workspace](AsyncReadResult<RemoteReadDto::StorageDetail> result){if(generation!=m_actionGeneration||workspace!=m_workspaceContext.currentWorkspaceId())return;if(!result.succeeded()){m_statusLabel->setText(result.message.isEmpty()?QStringLiteral("Unable to load the selected Storage location."):result.message);return;}const auto detail=*result.value;fetchRemoteTypes([this,detail](bool ok){if(ok)openRemoteDialog(StorageLocationDialog::Mode::Edit,detail);});});
}

void StorageWidget::openRemoteDialog(StorageLocationDialog::Mode mode,const std::optional<RemoteReadDto::StorageDetail>&detail)
{
    if(m_remoteDialog)return;StorageLocationDialog::Values values;if(detail){values={detail->name,detail->description,detail->parentStorageId,detail->storageTypeId,detail->allowsInventory,detail->allowsCollection};}
    else if(auto*item=m_tree->currentItem())values.parentStorageId=item->data(0,Qt::UserRole).toLongLong();
    QList<StorageLocationDialog::Choice>types;for(const auto&type:m_remoteTypes)types.append({type.storageTypeId,type.name});
    auto*dialog=new StorageLocationDialog(mode,values,types,remoteParentChoices(detail?detail->storageId:0),this);m_remoteDialog=dialog;m_remoteDialogDetail=detail;m_retainedRequest.reset();m_retainedOperation.clear();
    connect(dialog,&StorageLocationDialog::submitRequested,this,&StorageWidget::submitRemoteDialog);
    connect(dialog,&QObject::destroyed,this,[this]{m_remoteDialog=nullptr;m_remoteDialogDetail.reset();m_retainedRequest.reset();m_retainedOperation.clear();m_remoteMutationPending=false;updateRemoteActionState();});
    dialog->open();
}

void StorageWidget::submitRemoteDialog()
{
    auto*dialog=m_remoteDialog.data();if(!dialog||m_remoteMutationPending||!m_remoteMutations)return;
    QString operation;RemoteStorageMutationDto::Request request;
    if(m_retainedRequest){operation=m_retainedOperation;request=*m_retainedRequest;}
    else {operation=m_remoteDialogDetail?QStringLiteral("storage.edit"):QStringLiteral("storage.add");const auto values=dialog->values();request.workspaceId=m_workspaceContext.currentWorkspaceId();request.mutationId=RemoteMutationDto::newMutationId();request.name=values.name;request.description=values.description;request.parentStorageId=values.parentStorageId;request.storageTypeId=values.storageTypeId;request.allowsInventory=values.allowsInventory;request.allowsCollection=values.allowsCollection;if(m_remoteDialogDetail){const auto&d=*m_remoteDialogDetail;request.storageId=d.storageId;request.expected={d.modifiedUtc.toUTC().toString(Qt::ISODateWithMs),d.parentStorageId,d.storageTypeId,d.name,d.description,d.sortOrder,d.active,d.allowsInventory,d.allowsCollection};}}
    if(m_retainedRequest&&!m_remoteMutations->isAvailableFor(operation)){dialog->setUnknownOutcome(true,QStringLiteral("Reconnect to BrickSuite Host before retrying this unchanged request."));return;}
    m_retainedRequest=request;m_retainedOperation=operation;m_remoteMutationPending=true;dialog->setPending(true);updateRemoteActionState();QPointer<StorageLocationDialog>guard(dialog);
    auto success=[this,guard](const RemoteStorageMutationDto::Result&){m_remoteMutationPending=false;m_retainedRequest.reset();m_retainedOperation.clear();if(guard)guard->completeSuccessfully();updateRemoteActionState();};
    auto failure=[this,guard](const RemoteMutationDto::Error&error){m_remoteMutationPending=false;if(!guard)return;if(error.outcome==RemoteMutationDto::Outcome::Unknown){guard->setUnknownOutcome(true,QStringLiteral("The outcome is unknown. Reconnect, then retry safely with the same mutation ID."));}else{m_retainedRequest.reset();m_retainedOperation.clear();if(error.code==QStringLiteral("STALE_VERSION")){guard->showError(QStringLiteral("Storage changed on the Host. Your edit was not applied."));guard->close();loadRemoteStorageTree();}else guard->showError(error.message.isEmpty()?QStringLiteral("The Host rejected the Storage change."):error.message);}updateRemoteActionState();};
    if(operation==QStringLiteral("storage.add"))m_remoteMutations->add(request,dialog,std::move(success),std::move(failure));else m_remoteMutations->edit(request,dialog,std::move(success),std::move(failure));
}

void StorageWidget::remoteSetActive(bool active)
{
    if(m_remoteMutationPending||!m_remoteMutations)return;
    if(m_retainedRequest&&m_retainedOperation==QStringLiteral("storage.setActive")){if(active!=m_retainedRequest->active||!m_remoteDialogDetail)return;submitRemoteSetActive(*m_remoteDialogDetail,active);return;}
    auto*item=m_tree->currentItem();if(!item)return;const qint64 id=item->data(0,Qt::UserRole).toLongLong();const int workspace=m_workspaceContext.currentWorkspaceId();const quint64 generation=m_actionGeneration;
    m_remoteReads->getStorage(workspace,id,this,[this,generation,workspace,active](AsyncReadResult<RemoteReadDto::StorageDetail>result){if(generation!=m_actionGeneration||workspace!=m_workspaceContext.currentWorkspaceId())return;if(!result.succeeded()){m_statusLabel->setText(result.message.isEmpty()?QStringLiteral("Unable to load the selected Storage location."):result.message);return;}const auto detail=*result.value;const QString verb=active?QStringLiteral("Reactivate"):QStringLiteral("Deactivate");auto*box=new QMessageBox(QMessageBox::Question,verb+QStringLiteral(" Storage Location"),QStringLiteral("%1 \"%2\"?").arg(verb,detail.name),QMessageBox::Yes|QMessageBox::No,this);box->setDefaultButton(QMessageBox::No);box->setAttribute(Qt::WA_DeleteOnClose);m_remoteConfirmation=box;m_remoteMutationPending=true;updateRemoteActionState();connect(box,&QMessageBox::finished,this,[this,box,detail,active](int result){if(m_remoteConfirmation==box)m_remoteConfirmation=nullptr;m_remoteMutationPending=false;if(result==QMessageBox::Yes&&m_remoteConnected&&detail.workspaceId==m_workspaceContext.currentWorkspaceId()){m_remoteDialogDetail=detail;submitRemoteSetActive(detail,active);}else updateRemoteActionState();});box->open();});
}

void StorageWidget::submitRemoteSetActive(const RemoteReadDto::StorageDetail&detail,bool active)
{
    RemoteStorageMutationDto::Request request;if(m_retainedRequest)request=*m_retainedRequest;else{request.workspaceId=detail.workspaceId;request.storageId=detail.storageId;request.mutationId=RemoteMutationDto::newMutationId();request.active=active;request.expected={detail.modifiedUtc.toUTC().toString(Qt::ISODateWithMs),detail.parentStorageId,detail.storageTypeId,detail.name,detail.description,detail.sortOrder,detail.active,detail.allowsInventory,detail.allowsCollection};m_retainedRequest=request;m_retainedOperation=QStringLiteral("storage.setActive");}
    m_remoteMutationPending=true;updateRemoteActionState();m_statusLabel->setText(active?QStringLiteral("Reactivating Storage on BrickSuite Host..."):QStringLiteral("Deactivating Storage on BrickSuite Host..."));
    m_remoteMutations->setActive(request,this,[this](const auto&){m_remoteMutationPending=false;m_retainedRequest.reset();m_retainedOperation.clear();m_remoteDialogDetail.reset();m_statusLabel->setText(QStringLiteral("Storage change committed on BrickSuite Host."));updateRemoteActionState();},[this](const RemoteMutationDto::Error&error){m_remoteMutationPending=false;if(error.outcome==RemoteMutationDto::Outcome::Unknown)m_statusLabel->setText(QStringLiteral("The outcome is unknown. Reconnect and choose the same action to retry safely."));else{m_retainedRequest.reset();m_retainedOperation.clear();m_remoteDialogDetail.reset();m_statusLabel->setText(error.message.isEmpty()?QStringLiteral("The Host rejected the Storage change."):error.message);if(error.code==QStringLiteral("STALE_VERSION"))loadRemoteStorageTree();}updateRemoteActionState();});
}

void StorageWidget::updateRemoteActionState()
{
    if(!m_remoteReads)return;
    auto*item=m_tree->currentItem();const bool selected=item;const bool active=selected&&item->data(0,Qt::UserRole+2).toBool();
    RemoteStorageActionEligibilityInput input;input.connected=m_remoteConnected;input.workspaceCurrent=m_workspaceContext.currentWorkspaceId()>0;input.stale=m_remoteStale;input.pending=m_remoteMutationPending;input.selected=selected;input.selectedActive=active;input.canGet=m_remoteReads->isAvailableFor("storage.get");input.canListTypes=m_remoteReads->isAvailableFor("storage.types.list");input.canAdd=m_remoteMutations&&m_remoteMutations->isAvailableFor("storage.add");input.canEdit=m_remoteMutations&&m_remoteMutations->isAvailableFor("storage.edit");input.canSetActive=m_remoteMutations&&m_remoteMutations->isAvailableFor("storage.setActive");const auto eligibility=remoteStorageActionEligibility(input);
    m_addButton->setEnabled(eligibility.add);m_editButton->setEnabled(eligibility.edit);m_deactivateButton->setEnabled(eligibility.deactivate);m_reactivateButton->setEnabled(eligibility.reactivate);
    if(m_retainedRequest&&m_retainedOperation==QStringLiteral("storage.setActive")&&selected
       &&item->data(0,Qt::UserRole).toLongLong()==m_retainedRequest->storageId&&m_remoteConnected&&!m_remoteStale&&!m_remoteMutationPending){m_deactivateButton->setEnabled(!m_retainedRequest->active);m_reactivateButton->setEnabled(m_retainedRequest->active);}
    const QString unavailable=!m_remoteConnected?QStringLiteral("Storage changes are unavailable while disconnected."):m_remoteStale?QStringLiteral("Refresh Storage before making changes."):QStringLiteral("The connected Host does not advertise this Storage operation.");
    m_addButton->setToolTip(eligibility.add?QString():unavailable);m_editButton->setToolTip(eligibility.edit?QString():selected?unavailable:QStringLiteral("Select a Storage location to edit."));
    m_deactivateButton->setToolTip(m_deactivateButton->isEnabled()?QString():unavailable);m_reactivateButton->setToolTip(m_reactivateButton->isEnabled()?QString():unavailable);
}

void StorageWidget::addLocation()
{
    if (m_remoteReads) { remoteAddLocation(); return; }
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

    auto* inventoryCheck = new QCheckBox("Allow Inventory", &dialog);
    inventoryCheck->setChecked(true);
    auto* collectionCheck = new QCheckBox("Allow Collection", &dialog);

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
    formLayout->addRow("Capabilities:", inventoryCheck);
    formLayout->addRow(QString(), collectionCheck);

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

    QSqlDatabase database = DatabaseManager::instance().database();
    if (!database.transaction())
    {
        QMessageBox::critical(this, "BrickSuite", "Unable to begin the Storage update.");
        return;
    }
    HostStorageMutationService service(database);
    HostStorageMutationService::AddRequest request;
    request.workspaceId = m_workspaceContext.currentWorkspaceId();
    request.parentStorageId = parentLocationId; request.storageTypeId = locationTypeId;
    request.name = name; request.allowsInventory = inventoryCheck->isChecked();
    request.allowsCollection = collectionCheck->isChecked();
    const auto result = service.add(request);
    if (!commitStorageMutation(database, result)) {
        QMessageBox::critical(this, "BrickSuite",
                              result.error.message.isEmpty()
                                  ? QStringLiteral("Unable to create the storage location.")
                                  : result.error.message);
        return;
    }

    loadStorageTree();
    emit storageLocationsChanged();
    emit hostStorageMutationCommitted(result.location.workspaceId(), result.location.id());
}

void StorageWidget::editLocation()
{
    if (m_remoteReads) { remoteEditLocation(); return; }
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

    const QList<StorageLocation> allLocations = locationRepository.getByWorkspaceIncludingInactive(
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

    auto* inventoryCheck = new QCheckBox("Allow Inventory", &dialog);
    inventoryCheck->setChecked(existing->allowsInventory());
    auto* collectionCheck = new QCheckBox("Allow Collection", &dialog);
    collectionCheck->setChecked(existing->allowsCollection());

    parentCombo->addItem("(Top Level)", 0);

    for (const StorageLocation& location : allLocations) {
        if (!location.isActive())
            continue;
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
    formLayout->addRow("Capabilities:", inventoryCheck);
    formLayout->addRow(QString(), collectionCheck);

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
    updated.setAllowsInventory(inventoryCheck->isChecked());
    updated.setAllowsCollection(collectionCheck->isChecked());

    QSqlDatabase database = DatabaseManager::instance().database();
    if (!database.transaction()) {
        QMessageBox::critical(this, "BrickSuite", "Unable to begin the Storage update.");
        return;
    }
    HostStorageMutationService service(database);
    HostStorageMutationService::EditRequest request;
    request.workspaceId = updated.workspaceId(); request.storageId = updated.id();
    request.parentStorageId = updated.parentLocationId();
    request.storageTypeId = updated.locationTypeId(); request.name = updated.name();
    request.description = updated.description(); request.allowsInventory = updated.allowsInventory();
    request.allowsCollection = updated.allowsCollection();
    request.expected = HostStorageMutationService::expectedState(*existing);
    const auto result = service.edit(request);
    if (!commitStorageMutation(database, result)) {
        QMessageBox::warning(this, "BrickSuite",
                             result.error.message.isEmpty()
                                 ? QStringLiteral("Unable to update the storage location.")
                                 : result.error.message);
        return;
    }

    loadStorageTree();
    emit storageLocationsChanged();
    emit hostStorageMutationCommitted(result.location.workspaceId(), result.location.id());
}

void StorageWidget::deactivateLocation()
{
    if (m_remoteReads) { remoteSetActive(false); return; }
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

    const auto existing = repository.getById(locationId);
    if (!existing) return;
    QSqlDatabase database = DatabaseManager::instance().database();
    if (!database.transaction()) {
        QMessageBox::critical(this, "BrickSuite", "Unable to begin the Storage update.");
        return;
    }
    HostStorageMutationService service(database);
    HostStorageMutationService::SetActiveRequest request;
    request.workspaceId = existing->workspaceId(); request.storageId = locationId;
    request.active = false; request.expected = HostStorageMutationService::expectedState(*existing);
    const auto result = service.setActive(request);
    if (!commitStorageMutation(database, result)) {
        QMessageBox::warning(this, "BrickSuite",
                             result.error.message.isEmpty()
                                 ? QStringLiteral("Unable to deactivate the storage location.")
                                 : result.error.message);
        return;
    }

    loadStorageTree();
    emit storageLocationsChanged();
    emit hostStorageMutationCommitted(m_workspaceContext.currentWorkspaceId(), locationId);
}

void StorageWidget::reactivateLocation()
{
    if (m_remoteReads) { remoteSetActive(true); return; }
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

    const auto existing = repository.getById(locationId);
    if (!existing) return;
    QSqlDatabase database = DatabaseManager::instance().database();
    if (!database.transaction()) {
        QMessageBox::critical(this, "BrickSuite", "Unable to begin the Storage update.");
        return;
    }
    HostStorageMutationService service(database);
    HostStorageMutationService::SetActiveRequest request;
    request.workspaceId = existing->workspaceId(); request.storageId = locationId;
    request.active = true; request.expected = HostStorageMutationService::expectedState(*existing);
    const auto result = service.setActive(request);
    if (!commitStorageMutation(database, result)) {
        QMessageBox::warning(this, "BrickSuite",
                             result.error.message.isEmpty()
                                 ? QStringLiteral("Unable to reactivate the storage location.")
                                 : result.error.message);
        return;
    }

    loadStorageTree();
    emit storageLocationsChanged();
    emit hostStorageMutationCommitted(m_workspaceContext.currentWorkspaceId(), locationId);
}
