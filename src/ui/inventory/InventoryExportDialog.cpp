#include "InventoryExportDialog.h"

#include "../../services/inventory/InventoryCsvWriter.h"
#include "../../services/inventory/InventoryExportService.h"
#include "../../settings/UserSettings.h"
#include "../help/HelpManager.h"
#include "../help/HelpTopic.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

InventoryExportDialog::InventoryExportDialog(QList<InventoryExportRow> rows,
    QString summary,QSet<QString> enrichedFields,Enricher enricher,QWidget*parent):QDialog(parent),m_rows(std::move(rows)),m_filterSummary(std::move(summary)),m_enrichedFields(std::move(enrichedFields)),m_enricher(std::move(enricher))
{
    setWindowTitle(QStringLiteral("Export Inventory CSV"));resize(1050,650);
    HelpManager::setContextTopic(this,HelpTopic::Inventory,QStringLiteral("export-csv"));
    auto*root=new QVBoxLayout(this);auto*description=new QLabel(
        QStringLiteral("Current filters: %1\n%2 matching Inventory record(s).")
            .arg(m_filterSummary).arg(m_rows.size()),this);description->setWordWrap(true);root->addWidget(description);
    auto*content=new QHBoxLayout;auto*left=new QVBoxLayout;left->addWidget(new QLabel("Export fields:",this));
    m_fields=new QListWidget(this);left->addWidget(m_fields);auto*buttons=new QHBoxLayout;
    auto*up=new QPushButton("Move Up",this);auto*down=new QPushButton("Move Down",this);
    buttons->addWidget(up);buttons->addWidget(down);left->addLayout(buttons);auto*reset=new QPushButton("Reset to Defaults",this);left->addWidget(reset);
    content->addLayout(left);m_preview=new QTableWidget(this);m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);content->addWidget(m_preview,1);root->addLayout(content,1);
    m_status=new QLabel(this);root->addWidget(m_status);auto*box=new QDialogButtonBox(QDialogButtonBox::Cancel|QDialogButtonBox::Help,this);
    m_export=new QPushButton("Export...",this);box->addButton(m_export,QDialogButtonBox::AcceptRole);root->addWidget(box);
    connect(up,&QPushButton::clicked,this,[this]{moveField(-1);});connect(down,&QPushButton::clicked,this,[this]{moveField(1);});
    connect(reset,&QPushButton::clicked,this,[this]{apply(InventoryExportService::defaultConfiguration());updatePreview();});
    connect(m_export,&QPushButton::clicked,this,&InventoryExportDialog::exportCsv);connect(box,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(box,&QDialogButtonBox::helpRequested,this,[this]{HelpManager::showTopic(HelpTopic::Inventory,QStringLiteral("export-csv"),this);});
    connect(m_fields,&QListWidget::itemChanged,this,[this]{updatePreview();});
    const auto&settings=UserSettings::instance();apply(InventoryExportService::normalizeConfiguration(
        settings.inventoryExportFieldOrder(),settings.inventoryExportEnabledFields()));updatePreview();
}

void InventoryExportDialog::apply(const InventoryExportConfiguration& c)
{
    const QSignalBlocker blocker(m_fields);
    m_fields->clear();QHash<QString,InventoryExportFieldDescriptor> known;for(const auto&f:InventoryExportService::fieldDescriptors())known.insert(f.id,f);
    for(const QString&id:c.fieldOrder){if(!known.contains(id))continue;auto*item=new QListWidgetItem(known.value(id).label,m_fields);item->setData(Qt::UserRole,id);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(c.enabledFields.contains(id)?Qt::Checked:Qt::Unchecked);}
}

InventoryExportConfiguration InventoryExportDialog::configuration()const
{InventoryExportConfiguration c;for(int i=0;i<m_fields->count();++i){auto*x=m_fields->item(i);const QString id=x->data(Qt::UserRole).toString();c.fieldOrder<<id;if(x->checkState()==Qt::Checked)c.enabledFields.insert(id);}return c;}
void InventoryExportDialog::moveField(int offset){const int row=m_fields->currentRow(),target=row+offset;if(row<0||target<0||target>=m_fields->count())return;auto*item=m_fields->takeItem(row);m_fields->insertItem(target,item);m_fields->setCurrentRow(target);updatePreview();}
void InventoryExportDialog::updatePreview(){const auto c=configuration();UserSettings::instance().setInventoryExportConfiguration(c.fieldOrder,QStringList(c.enabledFields.begin(),c.enabledFields.end()));const QSet<QString> expensive={"legoElementId","rebrickablePartId","brickLinkPartId","brickLinkColorId"};const QSet<QString> requested=c.enabledFields&expensive;const QSet<QString> missing=requested-m_enrichedFields;if(!missing.isEmpty()&&!m_enrichmentPending&&m_enricher){m_enrichmentPending=true;m_export->setEnabled(false);m_status->setText(QStringLiteral("Preparing selected Inventory fields..."));m_enricher(m_rows,requested,this,[this,requested](bool success,QList<InventoryExportRow> rows,QString error){m_enrichmentPending=false;if(!success){m_status->setText(error.isEmpty()?QStringLiteral("Unable to prepare the selected Inventory fields."):error);m_export->setEnabled(false);return;}m_rows=std::move(rows);m_enrichedFields.unite(requested);updatePreview();});return;}const auto p=InventoryExportService::project(m_rows,c,250);m_preview->clear();m_preview->setColumnCount(p.headers.size());m_preview->setHorizontalHeaderLabels(p.headers);m_preview->setRowCount(p.rows.size());for(int r=0;r<p.rows.size();++r)for(int col=0;col<p.rows.at(r).size();++col)m_preview->setItem(r,col,new QTableWidgetItem(p.rows.at(r).at(col)));m_preview->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);m_preview->horizontalHeader()->setStretchLastSection(true);m_status->setText(m_rows.size()>p.rows.size()?QStringLiteral("Previewing first %1 of %2 matching records.").arg(p.rows.size()).arg(m_rows.size()):QStringLiteral("Previewing all %1 matching records.").arg(m_rows.size()));m_export->setEnabled(!m_enrichmentPending&&!p.fields.isEmpty()&&!m_rows.isEmpty());}
void InventoryExportDialog::exportCsv(){const auto p=InventoryExportService::project(m_rows,configuration());const QString name=QFileDialog::getSaveFileName(this,"Export Inventory CSV","BrickSuite_Inventory.csv","CSV Files (*.csv)");if(name.isEmpty())return;const auto result=InventoryCsvWriter::write(name,p);if(!result.success)QMessageBox::critical(this,"Export Inventory",result.message);else{QMessageBox::information(this,"Export Inventory",result.message);accept();}}
