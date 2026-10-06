#include "PartOutSetDialog.h"
#include "../../database/DatabaseManager.h"
#include "../../repositories/SetCatalogRepository.h"
#include "../../repositories/StorageLocationRepository.h"
#include "../../repositories/StorageLocationTypeRepository.h"
#include "../common/TooltipPolicy.h"
#include "../help/HelpManager.h"
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent>

PartOutSetDialog::PartOutSetDialog(int setCatalogId, int workspaceId, bool remote, QWidget* parent)
    : QDialog(parent), m_setId(setCatalogId), m_workspaceId(workspaceId), m_remote(remote),
      m_databasePath(DatabaseManager::instance().databasePath()),
      m_operationId(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    setWindowTitle(tr("Part Out Set to Inventory")); resize(920,720);
    HelpManager::setContextTopic(this,HelpTopic::SetsCatalog,QStringLiteral("use-set-for-parts"));
    auto* layout=new QVBoxLayout(this);
    m_summary=new QLabel(this); m_summary->setWordWrap(true); layout->addWidget(m_summary);
    m_fields=new QWidget(this); auto* form=new QFormLayout(m_fields); layout->addWidget(m_fields);
    m_copies=new QSpinBox(this); m_copies->setObjectName("partOutCopies"); m_copies->setRange(1,CatalogSetPartOutService::MaximumCopies);
    m_condition=new QComboBox(this); m_condition->setObjectName("partOutCondition"); m_condition->addItems({"Used","New"});
    m_spares=new QCheckBox(tr("Include catalog spare Parts"),this); m_spares->setObjectName("partOutSpares"); m_spares->setChecked(true);
    m_mode=new QComboBox(this); m_mode->setObjectName("partOutDestinationMode");
    m_mode->addItems({tr("Create storage location for this Set"),tr("Use existing storage location")});
    m_existing=new QComboBox(this); m_existing->setObjectName("partOutExistingStorage"); m_existing->addItem(tr("Select Storage..."),0);
    m_type=new QComboBox(this); m_type->setObjectName("partOutStorageType");
    m_parent=new QComboBox(this); m_parent->setObjectName("partOutParent"); m_parent->addItem(tr("Top Level"),0);
    m_name=new QLineEdit(this); m_name->setObjectName("partOutStorageName");
    const auto set=SetCatalogRepository().getById(setCatalogId);
    if (set) {
        m_summary->setText(QString("%1 - %2").arg(set->setNumber(),set->name()));
        m_name->setText(QString("%1 - %2").arg(set->setNumber(),set->name()));
    }
    const auto types=StorageLocationTypeRepository().getActive(); int bin=-1;
    for (const auto& type:types) {
        m_type->addItem(type.name(),type.id());
        if (type.name().compare("Bin",Qt::CaseInsensitive)==0) bin=m_type->count()-1;
    }
    m_type->setCurrentIndex(bin);
    StorageLocationRepository storage;
    const auto locations=storage.getByWorkspaceIncludingInactive(workspaceId);
    QHash<int,StorageLocation> byId; for (const auto& location:locations) byId.insert(location.id(),location);
    for (const auto& location:locations) {
        if (!location.isActive()) continue;
        QStringList names; QSet<int> seen; int current=location.id(); bool valid=true;
        while (current>0) {
            if (seen.contains(current) || !byId.contains(current) || !byId.value(current).isActive()) { valid=false; break; }
            seen.insert(current); const auto item=byId.value(current); names.prepend(item.name()); current=item.parentLocationId();
        }
        if (!valid) continue;
        const QString path=names.join(" / ");
        if (storage.isValidInventoryDestination(workspaceId,location.id())) m_existing->addItem(path,location.id());
        if (storage.hasInventoryChecked(location.id())==StorageLocationRepository::CheckResult::No
            && storage.hasCollectionChecked(location.id())==StorageLocationRepository::CheckResult::No)
            m_parent->addItem(path,location.id());
    }
    form->addRow(tr("Number of copies:"),m_copies); form->addRow(tr("Condition:"),m_condition);
    form->addRow(m_spares); form->addRow(tr("Destination:"),m_mode); form->addRow(tr("Existing Storage:"),m_existing);
    form->addRow(tr("Name:"),m_name); form->addRow(tr("Storage type:"),m_type); form->addRow(tr("Parent location:"),m_parent);
    TooltipPolicy::explain(m_spares,tr("Include only spare rows stored in the catalog. No stored spare rows does not prove that none were supplied."));
    TooltipPolicy::explain(m_copies,tr("Multiply every selected composition quantity by the number of physical copies you are adding."));
    TooltipPolicy::explain(m_parent,tr("Choose an empty container parent. A location holding Inventory or Collection contents cannot become a parent here."));
    m_table=new QTableWidget(this); m_table->setObjectName("partOutPreview"); m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({"Part","Description","Color","Per Set","Spare","Copies","Total"});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers); m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch); layout->addWidget(m_table,1);
    m_totals=new QLabel(this); m_totals->setWordWrap(true); layout->addWidget(m_totals);
    auto* caveat=new QLabel(tr("Catalog quantities assume complete physical copies. Check used/incomplete Sets before adding stock. No Collection item will be created."),this);
    caveat->setWordWrap(true); layout->addWidget(caveat);
    m_status=new QLabel(this); m_status->setObjectName("partOutStatus"); m_status->setWordWrap(true); layout->addWidget(m_status);
    m_progress=new QProgressBar(this); m_progress->setRange(0,0); m_progress->hide(); layout->addWidget(m_progress);
    m_buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,this);
    m_buttons->button(QDialogButtonBox::Ok)->setText(tr("Part Out to Inventory")); layout->addWidget(m_buttons);
    connect(m_buttons,&QDialogButtonBox::accepted,this,&PartOutSetDialog::submit);
    connect(m_buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    m_previewTimer=new QTimer(this); m_previewTimer->setSingleShot(true); m_previewTimer->setInterval(100);
    connect(m_previewTimer,&QTimer::timeout,this,&PartOutSetDialog::preview);
    m_previewWatcher=new QFutureWatcher<CatalogSetPartOutService::Plan>(this);
    connect(m_previewWatcher,&QFutureWatcher<CatalogSetPartOutService::Plan>::finished,this,[this] {
        if (m_previewQueued) { m_previewQueued=false; preview(); return; }
        if (m_previewTimer->isActive()) return;
        showPlan(m_previewWatcher->result());
    });
    for (auto* combo:{m_condition,m_mode,m_existing,m_type,m_parent})
        connect(combo,&QComboBox::currentIndexChanged,this,&PartOutSetDialog::schedulePreview);
    connect(m_copies,&QSpinBox::valueChanged,this,&PartOutSetDialog::schedulePreview);
    connect(m_spares,&QCheckBox::toggled,this,&PartOutSetDialog::schedulePreview);
    connect(m_name,&QLineEdit::textChanged,this,&PartOutSetDialog::schedulePreview);
    qApp->installEventFilter(this);
    schedulePreview();
}

CatalogSetPartOutService::Request PartOutSetDialog::request() const
{
    CatalogSetPartOutService::Request r;
    r.workspaceId=m_workspaceId; r.setCatalogId=m_setId; r.copies=m_copies->value(); r.includeSpares=m_spares->isChecked();
    r.condition=m_condition->currentText(); r.createStorage=m_mode->currentIndex()==0;
    r.storageId=r.createStorage?0:m_existing->currentData().toInt();
    r.storageName=r.createStorage?m_name->text().trimmed():QString();
    r.storageTypeId=r.createStorage?m_type->currentData().toInt():0;
    r.parentStorageId=r.createStorage?m_parent->currentData().toInt():0;
    r.operationId=m_operationId; return r;
}
void PartOutSetDialog::schedulePreview()
{
    if (m_submitting) return;
    m_plan.success=false; m_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    const bool create=m_mode->currentIndex()==0;
    m_existing->setEnabled(!create); m_name->setEnabled(create); m_type->setEnabled(create); m_parent->setEnabled(create);
    m_status->setText(tr("Checking composition and destination..."));
    if (m_previewWatcher->isRunning()) m_previewQueued=true;
    m_previewTimer->start();
}
void PartOutSetDialog::preview()
{
    if (m_submitting) return;
    if (m_remote) { CatalogSetPartOutService::Plan plan; plan.message=tr("Parting out a catalog Set is currently available on the Host/local database only."); showPlan(plan); return; }
    if (m_previewWatcher->isRunning()) { m_previewQueued=true; return; }
    m_previewQueued=false;
    const auto r=request(); const auto path=m_databasePath;
    m_previewWatcher->setFuture(QtConcurrent::run([path,r]{return CatalogSetPartOutService::previewFile(path,r);}));
}
void PartOutSetDialog::showPlan(const CatalogSetPartOutService::Plan& plan)
{
    m_plan=plan; m_table->setRowCount(plan.rows.size());
    for (int i=0;i<plan.rows.size();++i) {
        const auto& row=plan.rows.at(i);
        const QStringList cells{row.partNumber,row.partName,row.colorName,QString::number(row.perSet),row.spare?tr("Yes"):tr("No"),
            QString::number(m_copies->value()),row.spare&&!m_spares->isChecked()?tr("Excluded"):QString::number(row.total)};
        for (int c=0;c<cells.size();++c) m_table->setItem(i,c,new QTableWidgetItem(cells.at(c)));
    }
    m_totals->setText(tr("Required Parts: %1 | Catalog Spares: %2 | Total to Inventory: %3\n%4")
        .arg(plan.requiredPieces).arg(plan.sparePieces).arg(plan.totalPieces)
        .arg(plan.sparePieces==0?tr("No catalog spare rows are present."):QString()));
    m_status->setText(plan.success?plan.source+"\n"+plan.warnings.join('\n'):plan.message);
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(plan.success&&!m_remote);
}
void PartOutSetDialog::submit()
{
    if (m_submitting || !m_plan.success || m_previewTimer->isActive() || m_previewWatcher->isRunning() || m_remote) return;
    const auto r=request(); const QString fingerprint=m_plan.fingerprint;
    if (QMessageBox::question(this,tr("Part Out Set to Inventory"),
        tr("Add %1 pieces to %2?\nNo Collection item will be created.\n\n%3")
            .arg(m_plan.totalPieces).arg(m_plan.destination,m_plan.warnings.join('\n')),
        QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes) return;
    m_submitting=true; m_fields->setEnabled(false); m_buttons->setEnabled(false); m_progress->show();
    m_status->setText(tr("Adding Inventory and history atomically. Please keep BrickSuite open until completion."));
    auto* watcher=new QFutureWatcher<CatalogSetPartOutService::Result>(this);
    connect(watcher,&QFutureWatcher<CatalogSetPartOutService::Result>::finished,this,[this,watcher] {
        const auto result=watcher->result(); watcher->deleteLater();
        m_submitting=false; m_progress->hide(); m_buttons->setEnabled(true); m_fields->setEnabled(true);
        if (result.success) {
            emit inventoryCommitted(result.workspaceId,result.storageId,result.storageCreated);
            QMessageBox::information(this,tr("Set Part-Out Complete"),result.message);
            accept();
        } else {
            m_plan.success=false; m_buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
            QMessageBox::warning(this,tr("Set Part-Out Failed"),result.message);
            schedulePreview();
        }
    });
    const auto path=m_databasePath;
    watcher->setFuture(QtConcurrent::run([path,r,fingerprint]{return CatalogSetPartOutService::executeFile(path,r,fingerprint);}));
}
void PartOutSetDialog::done(int result)
{ if (!m_submitting) QDialog::done(result); }
bool PartOutSetDialog::eventFilter(QObject* watched,QEvent* event)
{
    if (m_submitting && event->type()==QEvent::Quit) { event->ignore(); return true; }
    if (m_submitting && event->type()==QEvent::Close) {
        if (auto* window=qobject_cast<QWidget*>(watched); window && window->isWindow()) { event->ignore(); return true; }
    }
    return QDialog::eventFilter(watched,event);
}
