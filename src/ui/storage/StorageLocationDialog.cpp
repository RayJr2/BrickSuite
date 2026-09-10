#include "StorageLocationDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>

StorageLocationDialog::StorageLocationDialog(Mode mode,const Values&initial,
    const QList<Choice>&types,const QList<Choice>&parents,QWidget*parent)
    :QDialog(parent),m_mode(mode)
{
    setAttribute(Qt::WA_DeleteOnClose);setModal(true);
    setWindowTitle(mode==Mode::Add?QStringLiteral("Add Storage Location"):QStringLiteral("Edit Storage Location"));
    auto*form=new QFormLayout(this);m_name=new QLineEdit(initial.name,this);m_name->setObjectName("storageNameEdit");
    m_description=new QTextEdit(initial.description,this);m_description->setObjectName("storageDescriptionEdit");m_description->setMaximumHeight(90);
    m_parent=new QComboBox(this);m_parent->setObjectName("storageParentCombo");m_parent->addItem(QStringLiteral("(Top Level)"),QVariant::fromValue<qint64>(0));
    for(const auto&choice:parents)m_parent->addItem(choice.name,QVariant::fromValue(choice.id));
    m_type=new QComboBox(this);m_type->setObjectName("storageTypeCombo");for(const auto&choice:types)m_type->addItem(choice.name,QVariant::fromValue(choice.id));
    m_inventory=new QCheckBox(QStringLiteral("Allow Inventory"),this);m_inventory->setObjectName("storageAllowsInventory");m_inventory->setChecked(initial.allowsInventory);
    m_collection=new QCheckBox(QStringLiteral("Allow Collection"),this);m_collection->setObjectName("storageAllowsCollection");m_collection->setChecked(initial.allowsCollection);
    const int parentIndex=m_parent->findData(QVariant::fromValue(initial.parentStorageId));if(parentIndex>=0)m_parent->setCurrentIndex(parentIndex);
    const int typeIndex=m_type->findData(QVariant::fromValue(initial.storageTypeId));if(typeIndex>=0)m_type->setCurrentIndex(typeIndex);
    m_status=new QLabel(this);m_status->setObjectName("storageMutationStatus");m_status->setWordWrap(true);
    m_buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);m_buttons->setObjectName("storageDialogButtons");
    m_buttons->button(QDialogButtonBox::Ok)->setText(mode==Mode::Add?QStringLiteral("Add"):QStringLiteral("Save"));
    form->addRow(QStringLiteral("Name:"),m_name);form->addRow(QStringLiteral("Description:"),m_description);
    form->addRow(QStringLiteral("Parent:"),m_parent);form->addRow(QStringLiteral("Storage Type:"),m_type);
    form->addRow(QStringLiteral("Capabilities:"),m_inventory);form->addRow(QString(),m_collection);form->addRow(m_status);form->addRow(m_buttons);
    connect(m_buttons,&QDialogButtonBox::accepted,this,&StorageLocationDialog::requestSubmit);
    connect(m_buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
}
StorageLocationDialog::Values StorageLocationDialog::values()const{return{m_name->text().trimmed(),m_description->toPlainText(),m_parent->currentData().toLongLong(),m_type->currentData().toLongLong(),m_inventory->isChecked(),m_collection->isChecked()};}
void StorageLocationDialog::requestSubmit(){if(m_pending)return;if(!m_unknown&&values().name.isEmpty()){showError(QStringLiteral("Please enter a storage location name."));return;}emit submitRequested();}
void StorageLocationDialog::setPending(bool pending){m_pending=pending;const bool enabled=!pending&&!m_unknown;m_name->setEnabled(enabled);m_description->setEnabled(enabled);m_parent->setEnabled(enabled);m_type->setEnabled(enabled);m_inventory->setEnabled(enabled);m_collection->setEnabled(enabled);m_buttons->button(QDialogButtonBox::Ok)->setEnabled(!pending);m_buttons->button(QDialogButtonBox::Cancel)->setEnabled(true);if(pending)m_status->setText(QStringLiteral("Saving Storage on BrickSuite Host..."));}
void StorageLocationDialog::setUnknownOutcome(bool unknown,const QString&message){m_unknown=unknown;m_pending=false;m_name->setEnabled(!unknown);m_description->setEnabled(!unknown);m_parent->setEnabled(!unknown);m_type->setEnabled(!unknown);m_inventory->setEnabled(!unknown);m_collection->setEnabled(!unknown);auto*submit=m_buttons->button(QDialogButtonBox::Ok);submit->setEnabled(true);submit->setText(unknown?QStringLiteral("Retry Safely"):(m_mode==Mode::Add?QStringLiteral("Add"):QStringLiteral("Save")));m_status->setText(message);}
void StorageLocationDialog::showError(const QString&message){m_pending=false;m_unknown=false;m_name->setEnabled(true);m_description->setEnabled(true);m_parent->setEnabled(true);m_type->setEnabled(true);m_inventory->setEnabled(true);m_collection->setEnabled(true);auto*submit=m_buttons->button(QDialogButtonBox::Ok);submit->setEnabled(true);submit->setText(m_mode==Mode::Add?QStringLiteral("Add"):QStringLiteral("Save"));m_status->setText(message);}
void StorageLocationDialog::completeSuccessfully(){accept();}
