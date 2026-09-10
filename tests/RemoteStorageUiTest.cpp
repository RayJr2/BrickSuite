#include "../src/ui/storage/StorageLocationDialog.h"
#include "../src/ui/storage/RemoteStorageActionEligibility.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <cstdio>

namespace { bool require(bool value,const char*message){if(!value)std::fprintf(stderr,"FAILED: %s\n",message);return value;} }

int main(int argc,char**argv)
{
    QApplication app(argc,argv);bool ok=true;
    RemoteStorageActionEligibilityInput input;input.connected=true;input.workspaceCurrent=true;input.stale=false;input.selected=true;input.selectedActive=true;input.canGet=true;input.canListTypes=true;
    input.canAdd=true;auto eligibility=remoteStorageActionEligibility(input);ok&=require(eligibility.add&&!eligibility.edit&&!eligibility.deactivate,"storage.add enables Add only");
    input.canAdd=false;input.canEdit=true;eligibility=remoteStorageActionEligibility(input);ok&=require(!eligibility.add&&eligibility.edit&&!eligibility.deactivate,"storage.edit enables Edit only");
    input.canEdit=false;input.canSetActive=true;eligibility=remoteStorageActionEligibility(input);ok&=require(!eligibility.edit&&eligibility.deactivate&&!eligibility.reactivate,"setActive maps active row to Deactivate");
    input.selectedActive=false;eligibility=remoteStorageActionEligibility(input);ok&=require(!eligibility.deactivate&&eligibility.reactivate,"setActive maps inactive row to Reactivate");
    input.canGet=false;eligibility=remoteStorageActionEligibility(input);ok&=require(!eligibility.edit&&!eligibility.deactivate&&!eligibility.reactivate,"read capabilities do not imply writes and detail is required");
    input.canGet=true;input.pending=true;eligibility=remoteStorageActionEligibility(input);ok&=require(!eligibility.add&&!eligibility.edit&&!eligibility.deactivate&&!eligibility.reactivate,"pending mutation disables all actions");

    const QList<StorageLocationDialog::Choice> types{{17,QStringLiteral("Cabinet")},{23,QStringLiteral("Bin")}};
    const QList<StorageLocationDialog::Choice> parents{{41,QStringLiteral("Room / Shelf")}};
    StorageLocationDialog::Values initial{QStringLiteral("Drawer"),QStringLiteral("Middle"),41,23,true,true};
    auto*dialog=new StorageLocationDialog(StorageLocationDialog::Mode::Edit,initial,types,parents);
    ok&=require(dialog->windowTitle()==QStringLiteral("Edit Storage Location"),"Edit title");
    auto*type=dialog->findChild<QComboBox*>("storageTypeCombo");auto*parent=dialog->findChild<QComboBox*>("storageParentCombo");auto*buttons=dialog->findChild<QDialogButtonBox*>("storageDialogButtons");
    ok&=require(type&&type->currentData().toLongLong()==23&&parent&&parent->currentData().toLongLong()==41,"Host type and parent IDs retained");
    ok&=require(buttons&&buttons->button(QDialogButtonBox::Ok)->text()==QStringLiteral("Save"),"Edit button wording");
    ok&=require(!dialog->findChild<QWidget*>("mutationId")&&!dialog->findChild<QWidget*>("modifiedUtc"),"protocol state hidden");
    dialog->open();QApplication::processEvents();ok&=require(dialog->isVisible(),"asynchronous open path displays dialog");
    dialog->setPending(true);ok&=require(!buttons->button(QDialogButtonBox::Ok)->isEnabled(),"submit disabled while pending");
    dialog->setUnknownOutcome(true,QStringLiteral("unknown"));ok&=require(buttons->button(QDialogButtonBox::Ok)->text()==QStringLiteral("Retry Safely")&&!type->isEnabled()&&!parent->isEnabled(),"Unknown outcome locks payload and exposes safe retry");
    dialog->close();QApplication::processEvents();
    auto*add=new StorageLocationDialog(StorageLocationDialog::Mode::Add,{},types,parents);buttons=add->findChild<QDialogButtonBox*>("storageDialogButtons");ok&=require(buttons->button(QDialogButtonBox::Ok)->text()==QStringLiteral("Add"),"Add button wording");add->open();add->close();QApplication::processEvents();
    return ok?0:1;
}
