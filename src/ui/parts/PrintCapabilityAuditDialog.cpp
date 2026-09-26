#include "PrintCapabilityAuditDialog.h"

#include "../../database/DatabaseManager.h"
#include "../../services/geometry/print/BatchPrintableModelService.h"
#include "../../settings/UserSettings.h"

#include <QComboBox>
#include <QCloseEvent>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
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

PrintCapabilityAuditDialog::PrintCapabilityAuditDialog(QWidget* parent,Runner runner):QDialog(parent),m_runner(std::move(runner))
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
    m_excludeNoModel=new QCheckBox(tr("Exclude Parts without installed LDraw model"),this);
    form->addRow(QString(),m_excludeNoModel);
    auto* load=new QPushButton(tr("Load Part List..."),this);load->setEnabled(false);
    form->addRow(QString(),load);
    const QString outputRoot=QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
        .filePath(QStringLiteral("BrickSuite/Print Capability Audits"));
    auto* outputRow=new QHBoxLayout;
    m_output=new QLineEdit(QSettings().value(QStringLiteral("printAudit/outputRoot"),outputRoot).toString(),this);
    m_browseOutput=new QPushButton(tr("Browse..."),this);
    outputRow->addWidget(m_output);outputRow->addWidget(m_browseOutput);
    form->addRow(tr("Output folder:"),outputRow);
    layout->addLayout(form);
    m_counters=new QLabel(tr("Ready to audit. Sticker/no-Color records are excluded from printability denominators."),this);
    m_counters->setWordWrap(true);m_counters->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_counters);
    m_recovery=new QLabel(this);m_recovery->setWordWrap(true);
    m_recovery->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(m_recovery);
    auto* buttons=new QHBoxLayout;m_start=new QPushButton(tr("Start"),this);
    m_stop=new QPushButton(tr("Stop"),this);m_stop->setEnabled(false);
    auto* close=new QPushButton(tr("Close"),this);
    buttons->addWidget(m_start);buttons->addWidget(m_stop);buttons->addStretch();buttons->addWidget(close);
    layout->addLayout(buttons);
    connect(m_mode,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,load](int index){
        m_excludeNoModel->setEnabled(index==0);
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
    connect(m_output,&QLineEdit::textChanged,this,[this]{showIncompleteRuns();});
    showIncompleteRuns();
    connect(m_start,&QPushButton::clicked,this,[this]{start();});
    connect(m_stop,&QPushButton::clicked,this,[this]{stop();});
    connect(close,&QPushButton::clicked,this,[this]{reject();});
}

void PrintCapabilityAuditDialog::showIncompleteRuns()
{
    if(m_running)return;
    const QDir root(m_output->text().trimmed());
    QStringList paths{root.filePath(QStringLiteral("run-state.json"))};
    for(const auto& folder:root.entryList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Time))
        paths.append(root.filePath(folder+QStringLiteral("/run-state.json")));
    QStringList notices;
    for(const auto& path:paths){
        QFile file(path);if(!file.open(QIODevice::ReadOnly))continue;
        const auto state=QJsonDocument::fromJson(file.readAll()).object();
        const auto status=state.value(QStringLiteral("status")).toString();
        if(status==QStringLiteral("completed"))continue;
        notices.append(tr("Prior run %1: sequence %2, Part %3 (%4), completed %5. State: %6")
            .arg(state.value(QStringLiteral("runId")).toString())
            .arg(state.value(QStringLiteral("currentSampleSequence")).toInt())
            .arg(state.value(QStringLiteral("currentPartNumber")).toString(),
                 status+QStringLiteral(" / ")+state.value(QStringLiteral("currentPhase")).toString(
                     state.value(QStringLiteral("currentPartStatus")).toString()))
            .arg(state.value(QStringLiteral("completedCount")).toInt()).arg(path));
    }
    m_recovery->setText(notices.join(QLatin1Char('\n')));
    m_recovery->setVisible(!notices.isEmpty());
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
    QSettings().setValue(QStringLiteral("printAudit/outputRoot"),m_output->text().trimmed());
    m_excludeNoModel->setEnabled(false);
    m_cancellation=std::make_shared<CancellationState>();m_runState=std::make_shared<BatchPrintRunState>();
    m_running=true;m_closePending=false;m_stop->setText(tr("Stop"));
    m_currentSequence=0;m_currentPart.clear();m_currentPhase=tr("Loading catalog");
    m_start->setEnabled(false);m_stop->setEnabled(true);m_mode->setEnabled(false);
    m_excludeNonstandardIds->setEnabled(false);m_output->setReadOnly(true);m_browseOutput->setEnabled(false);
    m_partList->setReadOnly(true);m_counters->setText(tr("Loading catalog Parts..."));
    BatchPrintOptions options;options.libraryRoot=UserSettings::instance().ldrawLibraryPath();
    options.outputRoot=m_output->text().trimmed();options.seed=quint32(m_seed->value());
    options.excludeNonstandardIds=m_excludeNonstandardIds->isChecked();
    options.excludeNoModel=random&&m_excludeNoModel->isChecked();
    options.randomSample=random;options.requestedEligibleCount=random?m_sampleCount->value():0;
    options.autoFitEnabled=UserSettings::instance().autoFitEnabled();
    options.runState=m_runState;
    const QString databasePath=DatabaseManager::instance().databasePath();
    const int count=m_sampleCount->value();const auto cancellation=m_cancellation;
    auto* watcher=new QFutureWatcher<BatchPrintRun>(this);QPointer<PrintCapabilityAuditDialog> self(this);
    connect(watcher,&QFutureWatcher<BatchPrintRun>::finished,this,[self,watcher]{
        BatchPrintRun result;
        try{result=watcher->result();}catch(...){result.diagnostic=QStringLiteral("Audit worker failed; inspect its last durable checkpoint.");}
        watcher->deleteLater();if(!self)return;
        self->m_running=false;
        if(self->m_closePending){self->QDialog::reject();return;}
        self->m_start->setEnabled(true);self->m_stop->setEnabled(false);
        self->m_mode->setEnabled(true);self->m_partList->setReadOnly(false);
        self->m_excludeNonstandardIds->setEnabled(true);self->m_output->setReadOnly(false);
        self->m_browseOutput->setEnabled(true);
        self->m_excludeNoModel->setEnabled(self->m_mode->currentIndex()==0);
        self->showIncompleteRuns();
        if(!result.ok){self->m_counters->setText(self->tr("Audit failed: %1").arg(result.diagnostic));return;}
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
    options.phaseProgress=[self](int sequence,const QString& part,const QString& phase){
        if(self)QMetaObject::invokeMethod(self,[self,sequence,part,phase]{
            if(self&&self->m_running)self->showPhase(sequence,part,phase);
        },Qt::QueuedConnection);
    };
    const auto runner=m_runner;
    m_future=QtConcurrent::run([databasePath,random,ids,count,options,cancellation,self,runner]{
        const auto progress=[self](const BatchPrintResult& row,const BatchPrintTotals&){
            if(self)QMetaObject::invokeMethod(self,[self,row]{
                if(self&&self->m_running)self->showPhase(row.sequence,row.partNumber,
                    QStringLiteral("completed: ")+BatchPrintableModelService::categoryCode(row.category));
            },Qt::QueuedConnection);
        };
        if(runner)return runner(options,cancellation.get(),progress);
        QString error;const auto catalog=BatchPrintableModelService::loadCatalog(databasePath,&error);
        if(!error.isEmpty()){BatchPrintRun failed;failed.diagnostic=error;return failed;}
        BatchPrintPopulation population;
        const auto entries=random?BatchPrintableModelService::randomSample(catalog,count,options.seed,
                                                                           options.excludeNonstandardIds,&population,
                                                                           options.excludeNoModel,options.libraryRoot):
            BatchPrintableModelService::explicitParts(catalog,ids);
        BatchPrintOptions runOptions=options;runOptions.population=population;
        return BatchPrintableModelService().run(entries,runOptions,cancellation.get(),progress);
    });
    watcher->setFuture(m_future);
}

PrintCapabilityAuditDialog::~PrintCapabilityAuditDialog()
{
    // Parent/application teardown is also a safe boundary. Keep the QObject
    // alive until callbacks can no longer be issued by the worker.
    if(m_running&&m_cancellation){m_cancellation->cancel();m_future.waitForFinished();}
}

void PrintCapabilityAuditDialog::showPhase(int sequence,const QString& part,const QString& phase)
{
    m_currentSequence=sequence;m_currentPart=part;m_currentPhase=phase;
    const QString prefix=m_closePending?tr("Stopping / closing"):
        (m_cancellation&&m_cancellation->isCancelled()?tr("Stopping"):tr("Auditing"));
    m_counters->setText(tr("%1 — sequence %2, Part %3: %4")
        .arg(prefix).arg(sequence).arg(part,phase));
}

void PrintCapabilityAuditDialog::stop()
{
    if(!m_running||!m_cancellation)return;
    m_cancellation->cancel();m_stop->setEnabled(false);m_stop->setText(tr("Stopping..."));
    QString error;
    if(m_runState&&!m_runState->requestStop(&error)){
        m_counters->setText(error);return;
    }
    showPhase(m_currentSequence,m_currentPart,m_currentPhase);
}

void PrintCapabilityAuditDialog::reject()
{
    if(m_running){m_closePending=true;stop();return;}
    QDialog::reject();
}

void PrintCapabilityAuditDialog::closeEvent(QCloseEvent* event)
{
    if(m_running){event->ignore();reject();return;}
    QDialog::closeEvent(event);
}
