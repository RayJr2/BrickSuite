#include "RemoteInventoryMutationDialog.h"
#include "../../models/Color.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/ManufacturerRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../services/application/RemoteInventoryMutationApplicationService.h"
#include "../helpers/ColorComboHelper.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <algorithm>

RemoteInventoryMutationDialog::RemoteInventoryMutationDialog(const QString& operation,int workspaceId,
    RemoteInventoryMutationApplicationService& service,const QHash<int,QString>& storagePaths,
    const QStringList& hostManufacturerNames,
    std::optional<RemoteReadDto::InventoryDetail> detail,QWidget* parent)
    :QDialog(parent),m_operation(operation),m_workspaceId(workspaceId),m_service(service),
     m_hostManufacturerNames(hostManufacturerNames),m_detail(std::move(detail)){initialize(storagePaths);}
RemoteInventoryMutationDialog::RemoteInventoryMutationDialog(int workspaceId,RemoteInventoryMutationApplicationService& service,
    const QHash<int,QString>& storagePaths,const RemoteReadDto::LostInventoryRow& lost,QWidget* parent)
    :QDialog(parent),m_operation("inventory.markFound"),m_workspaceId(workspaceId),m_service(service),m_lost(lost){initialize(storagePaths);}

void RemoteInventoryMutationDialog::initialize(const QHash<int,QString>& paths)
{
    const bool edit=m_operation=="inventory.edit",move=m_operation=="inventory.move",correct=m_operation=="inventory.correct";
    const bool remove=m_operation=="inventory.remove",lost=m_operation=="inventory.markLost",found=m_operation=="inventory.markFound";
    setWindowTitle(edit?"Edit Inventory":move?"Move Inventory":correct?"Correct Inventory Entry":remove?"Remove Inventory Entry":lost?"Mark Inventory Lost":"Found / Return Inventory");
    resize(edit?520:move?550:560,edit?320:300);auto* form=new QFormLayout(this);
    m_partLabel=new QLabel(this);m_partLabel->setObjectName("partLabel");
    if(correct||remove){m_contextLabel=new QLabel(this);m_contextLabel->setObjectName("contextLabel");m_contextLabel->setWordWrap(true);}
    if(correct){m_replacementPart=new QLineEdit(this);m_replacementPart->setObjectName("replacementPartEdit");m_replacementPart->setPlaceholderText("Search by Part Number or Name");}
    if(edit){m_color=new QComboBox(this);m_color->setObjectName("colorCombo");m_manufacturer=new QComboBox(this);m_manufacturer->setObjectName("manufacturerCombo");}
    if(edit||found){m_condition=new QComboBox(this);m_condition->setObjectName("conditionCombo");m_ownership=new QComboBox(this);m_ownership->setObjectName("ownershipCombo");}
    if(move||found){m_storage=new QComboBox(this);m_storage->setObjectName("destinationCombo");}
    m_quantity=new QSpinBox(this);m_quantity->setRange(edit?0:1,RemoteInventoryMutationDto::MaximumQuantity);
    m_quantity->setObjectName("quantitySpin");
    if(correct||remove||lost||found){m_notes=new QLineEdit(this);m_notes->setObjectName("notesEdit");m_notes->setPlaceholderText(found?"Optional note about where the part was found.":"Optional note");}
    if(edit){m_showAllColors=new QCheckBox("Show all colors",this);m_showAllColors->setObjectName("showAllColorsCheck");m_showAllColors->setChecked(true);}
    if(m_condition)m_condition->addItems({"Used","New"});if(m_ownership)m_ownership->addItem("Owned");
    if(m_manufacturer)for(const QString& name:m_hostManufacturerNames)m_manufacturer->addItem(name,name);
    if(m_color)for(const auto& x:ColorRepository().getAll())ColorComboHelper::addColorItem(m_color,x.name(),x.id(),x.rgb());
    if(m_storage){QList<int> ids=paths.keys();std::sort(ids.begin(),ids.end());for(int id:ids)m_storage->addItem(paths.value(id),id);}
    QString number,name,colorName,storagePath,condition,ownership,manufacturer;int quantity=1,colorExternal=-1,storageId=0;
    if(m_detail){const auto&d=*m_detail;number=d.partNumber;name=d.partNameFallback;colorName=d.colorNameFallback;storagePath=d.storagePath;condition=d.condition;ownership=d.ownershipType;manufacturer=d.manufacturerDisplay;quantity=d.quantity;colorExternal=d.rebrickableColorId;storageId=int(d.storageId);}
    if(m_lost){const auto&d=*m_lost;number=d.partNumber;name=d.partNameFallback;colorName=d.colorNameFallback;storagePath=d.lastStoragePath;condition=d.condition;ownership=d.ownershipType;quantity=d.outstandingQuantity;colorExternal=d.rebrickableColorId;storageId=int(d.lastStorageId);}
    m_partLabel->setText(QString("%1 — %2").arg(number,name));const auto color=ColorRepository().getByRebrickableId(colorExternal);
    int i=-1;if(m_color&&color){i=m_color->findData(color->id());if(i>=0)m_color->setCurrentIndex(i);}if(m_storage){i=m_storage->findData(storageId);if(i>=0)m_storage->setCurrentIndex(i);}
    if(move){i=m_storage->findData(storageId);if(i>=0)m_storage->removeItem(i);}
    if(m_manufacturer){i=m_manufacturer->findText(manufacturer,Qt::MatchFixedString);if(i<0&&!manufacturer.isEmpty()){m_manufacturer->addItem(manufacturer+" (Host)",manufacturer);i=m_manufacturer->count()-1;}if(i>=0)m_manufacturer->setCurrentIndex(i);}if(m_condition){i=m_condition->findText(condition,Qt::MatchFixedString);if(i>=0)m_condition->setCurrentIndex(i);}if(m_ownership){i=m_ownership->findText(ownership,Qt::MatchFixedString);if(i>=0)m_ownership->setCurrentIndex(i);}
    const int allocated=m_detail?m_detail->allocatedQuantity:0;const int mutableQuantity=qMax(0,quantity-allocated);if(!edit)m_quantity->setMaximum(qMax(1,(remove?mutableQuantity:quantity)));m_quantity->setValue((edit||move||correct)?quantity:qMax(1,remove?mutableQuantity:1));if(m_contextLabel)m_contextLabel->setText(QString("Color: %1   |   Storage: %2   |   Available Qty: %3   |   Allocated to Builds: %4").arg(colorName,storagePath).arg(quantity).arg(allocated));
    if(edit){form->addRow("Part:",m_partLabel);form->addRow("Color:",m_color);form->addRow("Manufacturer:",m_manufacturer);form->addRow(QString(),m_showAllColors);form->addRow("Condition:",m_condition);form->addRow("Ownership:",m_ownership);form->addRow("Quantity:",m_quantity);}
    else if(move){form->addRow("Part:",m_partLabel);form->addRow("Current Storage:",new QLabel(storagePath,this));form->addRow("Available Quantity:",new QLabel(QString::number(quantity),this));form->addRow("Destination:",m_storage);form->addRow("Quantity to Move:",m_quantity);}
    else if(correct){form->addRow("Current Part:",m_partLabel);form->addRow("Current Inventory:",m_contextLabel);form->addRow("Correct Part:",m_replacementPart);form->addRow("Quantity to Correct:",m_quantity);form->addRow("Notes:",m_notes);}
    else if(remove){form->addRow("Part:",m_partLabel);form->addRow("Current Inventory:",m_contextLabel);form->addRow("Quantity to Remove:",m_quantity);form->addRow("Notes:",m_notes);}
    else if(lost){form->addRow("Part:",m_partLabel);form->addRow("Color:",new QLabel(colorName,this));form->addRow("Storage:",new QLabel(storagePath,this));form->addRow("Current Quantity:",new QLabel(QString::number(quantity),this));form->addRow("Quantity Lost:",m_quantity);form->addRow("Notes:",m_notes);}
    else{form->addRow("Part:",m_partLabel);form->addRow("Color:",new QLabel(colorName,this));form->addRow("Outstanding Lost:",new QLabel(QString::number(quantity),this));form->addRow("Quantity Found:",m_quantity);form->addRow("Return To:",m_storage);form->addRow("Condition:",m_condition);form->addRow("Ownership:",m_ownership);form->addRow("Notes:",m_notes);}
    m_status=new QLabel(this);form->addRow(m_status);m_buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);auto* button=m_buttons->button(QDialogButtonBox::Ok);button->setText(edit?"Save":move?"Move":correct?"Save":remove?"Remove":lost?"Mark Lost":"Return to Inventory");if(remove&&mutableQuantity==0){button->setEnabled(false);m_quantity->setEnabled(false);m_status->setText("All inventory in this record is allocated to active Builds.");}form->addRow(m_buttons);
    connect(m_buttons,&QDialogButtonBox::accepted,this,&RemoteInventoryMutationDialog::submit);connect(m_buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
}

RemoteInventoryMutationDto::Request RemoteInventoryMutationDialog::request()const
{
    RemoteInventoryMutationDto::Request r;r.workspaceId=m_workspaceId;r.mutationId=m_mutationId.isEmpty()?RemoteMutationDto::newMutationId():m_mutationId;
    if(m_detail){const auto&d=*m_detail;r.inventoryRecordId=d.inventoryRecordId;r.expected={d.inventoryRecordId,d.quantity,d.storageId,d.modifiedUtc.toUTC().toString(Qt::ISODateWithMs),d.partNumber,d.rebrickableColorId,d.manufacturerDisplay,d.condition,d.ownershipType};r.partNumber=d.partNumber;r.colorExternalId=d.rebrickableColorId;r.manufacturerName=d.manufacturerDisplay;r.storageLocationId=d.storageId;}
    if(m_lost){const auto&d=*m_lost;r.partNumber=d.partNumber;r.colorExternalId=d.rebrickableColorId;r.condition=d.condition;r.ownershipType=d.ownershipType;}
    if(m_operation=="inventory.edit"){if(const auto c=ColorRepository().getById(m_color->currentData().toInt()))r.colorExternalId=c->rebrickableId();r.manufacturerName=m_manufacturer->currentData().toString();r.condition=m_condition->currentText();r.ownershipType=m_ownership->currentText();}
    if(m_operation=="inventory.correct")r.partNumber=m_replacementPart->text().trimmed().section(QStringLiteral(" — "),0,0);r.quantity=m_quantity->value();if(m_notes)r.notes=m_notes->text().trimmed();if(m_storage)r.destinationStorageLocationId=m_storage->currentData().toLongLong();
    if(m_operation=="inventory.markFound"){r.storageLocationId=r.destinationStorageLocationId;r.condition=m_condition->currentText();r.ownershipType=m_ownership->currentText();}return r;
}
void RemoteInventoryMutationDialog::setPending(bool pending){m_pending=pending;m_buttons->button(QDialogButtonBox::Ok)->setEnabled(!pending);const QList<QWidget*> fields{m_replacementPart,m_color,m_manufacturer,m_condition,m_ownership,m_storage,m_quantity,m_notes};for(QWidget*w:fields)if(w)w->setEnabled(!pending);}
void RemoteInventoryMutationDialog::submit(){if(m_pending)return;auto r=request();if(m_operation=="inventory.correct"){const auto part=PartRepository().getByPartNumber(r.partNumber);if(!part){QMessageBox::warning(this,"BrickSuite","Resolve a valid local catalog Part before saving the correction.");return;}r.partNumber=part->partNumber();}if((m_operation=="inventory.correct"&&r.partNumber.isEmpty())||(m_operation=="inventory.edit"&&(r.colorExternalId<0||r.quantity<=0))||((m_operation=="inventory.move"||m_operation=="inventory.markFound")&&r.destinationStorageLocationId<=0)){QMessageBox::warning(this,"BrickSuite","Complete all required fields.");return;}if(m_operation=="inventory.remove"||m_operation=="inventory.markLost"||m_operation=="inventory.markFound"){const QString verb=m_operation=="inventory.remove"?"remove":m_operation=="inventory.markLost"?"mark lost":"return to Inventory";if(QMessageBox::question(this,"BrickSuite",QString("%1 piece(s): %2?").arg(r.quantity).arg(verb),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;}m_mutationId=r.mutationId;setPending(true);m_status->setText("Saving to BrickSuite Host...");auto success=[this](const auto&){setPending(false);m_mutationId.clear();accept();};auto failure=[this](const RemoteMutationDto::Error&e){if(e.outcome==RemoteMutationDto::Outcome::Unknown){m_pending=false;m_buttons->button(QDialogButtonBox::Ok)->setEnabled(true);m_status->setText("The outcome is unknown. Retry safely to check the same mutation.");return;}setPending(false);m_mutationId.clear();m_status->setText(e.message);};if(m_operation=="inventory.edit")m_service.edit(r,this,success,failure);else if(m_operation=="inventory.move")m_service.move(r,this,success,failure);else if(m_operation=="inventory.correct")m_service.correct(r,this,success,failure);else if(m_operation=="inventory.remove")m_service.remove(r,this,success,failure);else if(m_operation=="inventory.markLost")m_service.markLost(r,this,success,failure);else m_service.markFound(r,this,success,failure);}
