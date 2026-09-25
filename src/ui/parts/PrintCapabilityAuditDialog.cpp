#include "PrintCapabilityAuditDialog.h"

#include "../../database/DatabaseManager.h"
#include "../../services/geometry/print/BatchPrintableModelService.h"
#include "../../settings/UserSettings.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTextStream>
#include <QVBoxLayout>
#include <QtConcurrent>

using namespace PrintGeometry;

PrintCapabilityAuditDialog::PrintCapabilityAuditDialog(QWidget* parent):QDialog(parent)
{
    setWindowTitle(tr("Print Capability Audit"));resize(620,450);
    auto* layout=new QVBoxLayout(this);auto* form=new QFormLayout;
    m_mode=new QComboBox(this);m_mode->addItems({tr("Random Sample"),tr("Part List")});
    form->addRow(tr("Mode:"),m_mode);
    m_sampleCount=new QSpinBox(this);m_sampleCount->setRange(1,100000);m_sampleCount->setValue(25);
    form->addRow(tr("Sample count:"),m_sampleCount);
    m_seed=new QSpinBox(this);m_seed->setRange(0,2147483647);
    m_seed->setValue(int(QRandomGenerator::global()->bounded(2147483647u)));
    form->addRow(tr("Reproducible seed:"),m_seed);
    m_partList=new QPlainTextEdit(this);m_partList->setPlaceholderText(tr("Part IDs, one per line or separated by commas"));
    m_partList->setEnabled(false);form->addRow(tr("Part IDs:"),m_partList);
    auto* noColor=new QCheckBox(tr("Exclude no-Color / sticker Parts (required)"),this);
    noColor->setChecked(true);noColor->setEnabled(false);form->addRow(tr("Eligibility:"),noColor);
    m_excludeNonstandardIds=new QCheckBox(tr("Exclude composite/decorated Part IDs"),this);
    m_excludeNonstandardIds->setChecked(true);form->addRow(QString(),m_excludeNonstandardIds);
    auto* load=new QPushButton(tr("Load Part List..."),this);load->setEnabled(false);
    form->addRow(QString(),load);
    const QString outputRoot=QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
        .filePath(QStringLiteral("BrickSuite/Print Capability Audits"));
    auto* outputRow=new QHBoxLayout;
    m_output=new QLineEdit(outputRoot,this);
    m_browseOutput=new QPushButton(tr("Browse..."),this);
    outputRow->addWidget(m_output);outputRow->addWidget(m_browseOutput);
    form->addRow(tr("Output folder:"),outputRow);
    layout->addLayout(form);
    m_counters=new QLabel(tr("Ready to audit. Sticker/no-Color records are excluded from printability denominators."),this);
    m_counters->setWordWrap(true);m_counters->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_counters);
    auto* buttons=new QHBoxLayout;m_start=new QPushButton(tr("Start"),this);
    m_stop=new QPushButton(tr("Stop"),this);m_stop->setEnabled(false);
    auto* close=new QPushButton(tr("Close"),this);
    buttons->addWidget(m_start);buttons->addWidget(m_stop);buttons->addStretch();buttons->addWidget(close);
    layout->addLayout(buttons);
    connect(m_mode,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,load](int index){
        m_sampleCount->setEnabled(index==0);m_seed->setEnabled(index==0);
        m_partList->setEnabled(index==1);load->setEnabled(index==1);
    });
    connect(load,&QPushButton::clicked,this,[this]{
        const QString path=QFileDialog::getOpenFileName(this,tr("Load Part IDs"),{},tr("Text or CSV (*.txt *.csv);;All files (*)"));
        if(path.isEmpty())return;QFile file(path);
        if(file.open(QIODevice::ReadOnly|QIODevice::Text))m_partList->setPlainText(QString::fromUtf8(file.readAll()));
        else QMessageBox::warning(this,tr("Print Capability Audit"),file.errorString());
    });
    connect(m_browseOutput,&QPushButton::clicked,this,[this]{
        const QString folder=QFileDialog::getExistingDirectory(this,tr("Choose audit output folder"),
                                                                m_output->text());
        if(!folder.isEmpty())m_output->setText(folder);
    });
    connect(m_start,&QPushButton::clicked,this,[this]{start();});
    connect(m_stop,&QPushButton::clicked,this,[this]{stop();});
    connect(close,&QPushButton::clicked,this,[this]{reject();});
}

void PrintCapabilityAuditDialog::start()
{
    if(m_running)return;
    const bool random=m_mode->currentIndex()==0;
    const QStringList ids=m_partList->toPlainText().split(QRegularExpression(QStringLiteral("[\\s,;]+")),Qt::SkipEmptyParts);
    if(!random&&ids.isEmpty()){
        QMessageBox::information(this,tr("Print Capability Audit"),tr("Enter or load at least one Part ID."));return;
    }
    if(m_output->text().trimmed().isEmpty()){
        QMessageBox::information(this,tr("Print Capability Audit"),tr("Choose an output folder."));return;
    }
    m_cancellation=std::make_shared<CancellationState>();m_running=true;
    m_start->setEnabled(false);m_stop->setEnabled(true);m_mode->setEnabled(false);
    m_excludeNonstandardIds->setEnabled(false);m_output->setReadOnly(true);m_browseOutput->setEnabled(false);
    m_partList->setReadOnly(true);m_counters->setText(tr("Loading catalog Parts..."));
    BatchPrintOptions options;options.libraryRoot=UserSettings::instance().ldrawLibraryPath();
    options.outputRoot=m_output->text().trimmed();options.seed=quint32(m_seed->value());
    options.excludeNonstandardIds=m_excludeNonstandardIds->isChecked();
    options.randomSample=random;options.requestedEligibleCount=random?m_sampleCount->value():0;
    options.autoFitEnabled=UserSettings::instance().autoFitEnabled();
    const QString databasePath=DatabaseManager::instance().databasePath();
    const int count=m_sampleCount->value();const auto cancellation=m_cancellation;
    auto* watcher=new QFutureWatcher<BatchPrintRun>(this);QPointer<PrintCapabilityAuditDialog> self(this);
    connect(watcher,&QFutureWatcher<BatchPrintRun>::finished,this,[self,watcher]{
        const auto result=watcher->result();watcher->deleteLater();if(!self)return;
        self->m_running=false;self->m_start->setEnabled(true);self->m_stop->setEnabled(false);
        self->m_mode->setEnabled(true);self->m_partList->setReadOnly(false);
        self->m_excludeNonstandardIds->setEnabled(true);self->m_output->setReadOnly(false);
        self->m_browseOutput->setEnabled(true);
        if(!result.ok){self->m_counters->setText(self->tr("Audit could not start: %1").arg(result.diagnostic));return;}
        const auto& t=result.totals;
        const int noColor=result.population.catalogTotal?result.population.excludedNoColor:t.skippedNoColor;
        const int stickers=result.population.catalogTotal?result.population.excludedStickerCategory:t.skippedStickerCategory;
        const int nonstandard=result.population.catalogTotal?result.population.excludedNonstandardId:t.skippedNonstandardIds;
        self->m_counters->setText(self->tr("%1. Eligible pool %2; sampled %3; attempted %4; successful %5; excluded no Color %6; Sticker category %7; nonstandard IDs %8; no model %9; preparation failures %10; ManufacturingMesh failures %11; export/reopen failures %12. Model availability %13%; print-service success %14%; catalog-to-printable %15%. CSV: %16")
            .arg(result.stopped?self->tr("Stopped"):self->tr("Complete"))
            .arg(result.population.catalogTotal?result.population.eligibleTotal:t.eligible)
            .arg(result.population.catalogTotal?result.population.actualSampled:t.input)
            .arg(t.attempted).arg(t.successful).arg(noColor).arg(stickers)
            .arg(nonstandard).arg(t.noModel).arg(t.prepareFailures)
            .arg(t.manufacturingFailures).arg(t.exportFailures)
            .arg(t.modelAvailabilityPercent(),0,'f',1).arg(t.printServicePercent(),0,'f',1)
            .arg(t.catalogToPrintablePercent(),0,'f',1).arg(result.csvPath));
    });
    watcher->setFuture(QtConcurrent::run([databasePath,random,ids,count,options,cancellation,self]{
        QString error;const auto catalog=BatchPrintableModelService::loadCatalog(databasePath,&error);
        if(!error.isEmpty()){BatchPrintRun failed;failed.diagnostic=error;return failed;}
        BatchPrintPopulation population;
        const auto entries=random?BatchPrintableModelService::randomSample(catalog,count,options.seed,
                                                                           options.excludeNonstandardIds,&population):
            BatchPrintableModelService::explicitParts(catalog,ids);
        BatchPrintOptions runOptions=options;runOptions.population=population;
        return BatchPrintableModelService().run(entries,runOptions,cancellation.get(),
            [self,population](const BatchPrintResult& row,const BatchPrintTotals& totals){
                if(!self)return;
                QMetaObject::invokeMethod(self,[self,row,totals,population]{
                    if(!self)return;
                    self->m_counters->setText(self->tr("Part %1: %2 — eligible pool %3, successful %4, excluded no Color %5, Sticker category %6, nonstandard IDs %7, no model %8, preparation %9, ManufacturingMesh %10, export/reopen %11")
                        .arg(row.partNumber,BatchPrintableModelService::categoryCode(row.category))
                        .arg(population.catalogTotal?population.eligibleTotal:totals.eligible)
                        .arg(totals.successful)
                        .arg(population.catalogTotal?population.excludedNoColor:totals.skippedNoColor)
                        .arg(population.catalogTotal?population.excludedStickerCategory:totals.skippedStickerCategory)
                        .arg(population.catalogTotal?population.excludedNonstandardId:totals.skippedNonstandardIds)
                        .arg(totals.noModel).arg(totals.prepareFailures).arg(totals.manufacturingFailures)
                        .arg(totals.exportFailures));
                },Qt::QueuedConnection);
            });
    }));
}

void PrintCapabilityAuditDialog::stop()
{
    if(!m_running||!m_cancellation)return;
    m_cancellation->cancel();m_stop->setEnabled(false);
    m_counters->setText(tr("Stop requested. The active Part will finish or reach a safe cancellation point; CSV will close cleanly."));
}

void PrintCapabilityAuditDialog::reject()
{
    if(m_running){stop();return;}
    QDialog::reject();
}
