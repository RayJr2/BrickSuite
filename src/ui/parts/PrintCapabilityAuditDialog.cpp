#include "../common/TooltipPolicy.h"
#include "../help/HelpManager.h"
#include "PrintCapabilityAuditDialog.h"

#include "../../database/DatabaseManager.h"
#include "../../services/parts/PartReferenceAuditSource.h"
#include "../../services/geometry/print/BatchPrintableModelService.h"
#include "../../settings/UserSettings.h"
#include "../../services/geometry/fit/FitCalibrationLibrary.h"

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
    HelpManager::setContextTopic(this,HelpTopic::PrintTroubleshooting);
    setWindowTitle(tr("Print Capability Audit"));resize(620,450);
    auto* layout=new QVBoxLayout(this);auto* form=new QFormLayout;
    m_mode=new QComboBox(this);m_mode->addItems({tr("Random Sample"),tr("Part List"),tr("Part Reference")});
    form->addRow(tr("Mode:"),m_mode);
    m_fitProfile=new QComboBox(this);m_fitProfile->setObjectName(QStringLiteral("auditFitProfile"));
    m_fitProfile->addItem(tr("Automatic / No explicit selection"),QString());
    for(const auto& profile:FitCalibrationLibrary().profiles())
        if(profile.compatible)m_fitProfile->addItem(profile.name,profile.identity);
    TooltipPolicy::explain(m_fitProfile, tr("An explicit Verified profile selects printing intent. Only evidence compatible with each source feature, role and Print Orientation applies. Automatic never guesses between profiles."));
    form->addRow(tr("Fit Profile:"),m_fitProfile);
    m_printOrientation=new QComboBox(this);m_printOrientation->setObjectName(QStringLiteral("auditPrintOrientation"));
    m_printOrientation->addItems({tr("Nominal"),tr("X +90 degrees"),tr("Y +90 degrees"),tr("Z +90 degrees")});
    TooltipPolicy::explain(m_printOrientation, tr("Explicit orthogonal rotation in print coordinates. Evidence must match the transformed feature axis; no automatic rotation is performed."));
    form->addRow(tr("Print Orientation:"),m_printOrientation);
    m_sampleCount=new QSpinBox(this);m_sampleCount->setRange(1,100000);m_sampleCount->setValue(25);
    form->addRow(tr("Sample count:"),m_sampleCount);
    m_seed=new QSpinBox(this);m_seed->setRange(0,2147483647);
    m_seed->setValue(int(QRandomGenerator::global()->bounded(2147483647u)));
    form->addRow(tr("Reproducible seed:"),m_seed);
    m_partList=new QPlainTextEdit(this);m_partList->setPlaceholderText(tr("Part IDs, one per line or separated by commas"));
    m_partList->setEnabled(false);form->addRow(tr("Part IDs:"),m_partList);
    m_noColor=new QCheckBox(tr("Exclude no-Color / sticker Parts"),this);
    m_noColor->setChecked(true);m_noColor->setEnabled(false);form->addRow(tr("Eligibility:"),m_noColor);
    m_excludeNonstandardIds=new QCheckBox(tr("Exclude composite/decorated Part IDs"),this);
    m_excludeNonstandardIds->setChecked(true);form->addRow(QString(),m_excludeNonstandardIds);
    m_excludeNoModel=new QCheckBox(tr("Exclude Parts without installed LDraw model"),this);
    form->addRow(QString(),m_excludeNoModel);
    auto* load=new QPushButton(tr("Load Part List..."),this);load->setEnabled(false);
    form->addRow(QString(),load);
    m_referenceCounts=new QLabel(this);m_referenceCounts->setWordWrap(true);
    m_referenceCounts->setObjectName(QStringLiteral("referenceCorpusCounts"));
    form->addRow(tr("Part Reference corpus:"),m_referenceCounts);
    m_referenceFirst=new QSpinBox(this);m_referenceFirst->setRange(1,100000);
    m_referenceFirst->setObjectName(QStringLiteral("referenceFirst"));
    m_referenceCount=new QSpinBox(this);m_referenceCount->setRange(0,100000);
    m_referenceCount->setSpecialValueText(tr("All remaining"));
    m_referenceCount->setObjectName(QStringLiteral("referenceCount"));
    form->addRow(tr("First unique Part:"),m_referenceFirst);
    form->addRow(tr("Parts in this run:"),m_referenceCount);
    m_refreshCorpus=new QPushButton(tr("Refresh current Part Reference"),this);
    m_loadPlan=new QPushButton(tr("Load saved Part Reference plan..."),this);
    form->addRow(QString(),m_refreshCorpus);form->addRow(QString(),m_loadPlan);
    connect(m_refreshCorpus,&QPushButton::clicked,this,[this]{refreshCorpus();});
    connect(m_loadPlan,&QPushButton::clicked,this,[this]{
        const auto path=QFileDialog::getOpenFileName(this,tr("Load saved corpus"),m_output->text(),
            tr("Part Reference plan (part-reference-plan.json)"));
        if(!path.isEmpty())refreshCorpus(path);
    });
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
    TooltipPolicy::explain(m_referenceFirst, tr("Global unique-Part sequence in the saved corpus; use a new run folder for each chunk."));
    TooltipPolicy::explain(m_referenceCount, tr("Zero means all remaining Parts. Explicit ranges allow bounded runs without changing corpus order."));
    TooltipPolicy::explain(m_loadPlan, tr("Continue in a new run folder using an earlier immutable corpus plan. This does not append to or automatically resume earlier CSV output."));
    TooltipPolicy::explain(m_seed, tr("Reproduce random sample order with the same seed and eligible catalog."));
    TooltipPolicy::explain(m_mode, tr("Random Sample shuffles eligible Parts; Part List tests explicit IDs; Part Reference audits every unique reference Part in saved order."));
    TooltipPolicy::explain(m_sampleCount, tr("Requested eligible sample size after exclusions; missing models need not consume slots."));
    TooltipPolicy::explain(m_excludeNoModel, tr("Filter missing installed models before random sampling. Explicit Part List IDs are still tested."));
    TooltipPolicy::explain(m_excludeNonstandardIds, tr("Restrict the random population to digits with at most one trailing letter; catalog identities are unchanged."));
    TooltipPolicy::explain(m_output, tr("Each run creates a new folder containing results and durable crash-attribution state."));
    TooltipPolicy::explain(m_stop, tr("Stop before the next Part after the active work finishes or cancels safely."));
    connect(m_mode,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,load](int index){
        updateMode();load->setEnabled(index==1);
        if(index==2&&m_corpus.parts.isEmpty())refreshCorpus();
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
    showIncompleteRuns();updateMode();
    connect(m_start,&QPushButton::clicked,this,[this]{start();});
    connect(m_stop,&QPushButton::clicked,this,[this]{stop();});
    connect(close,&QPushButton::clicked,this,[this]{reject();});
}

void PrintCapabilityAuditDialog::updateMode()
{
    m_fitProfile->setEnabled(!m_running&&!m_discovering);
    m_printOrientation->setEnabled(!m_running&&!m_discovering);
    const bool reference=m_mode->currentIndex()==2,random=m_mode->currentIndex()==0;
    m_sampleCount->setEnabled(!m_running&&random);m_seed->setEnabled(!m_running&&random);
    m_partList->setEnabled(!m_running&&m_mode->currentIndex()==1);
    m_excludeNoModel->setEnabled(!m_running&&random);
    m_excludeNonstandardIds->setEnabled(!m_running&&!reference);
    m_noColor->setChecked(!reference);
    m_referenceFirst->setEnabled(!m_running&&!m_discovering&&reference);
    m_referenceCount->setEnabled(!m_running&&!m_discovering&&reference);
    m_refreshCorpus->setEnabled(!m_running&&!m_discovering&&reference);
    m_loadPlan->setEnabled(!m_running&&!m_discovering&&reference);
    m_start->setEnabled(!m_running&&!m_discovering&&(!reference||m_corpus.ok()));
}

void PrintCapabilityAuditDialog::refreshCorpus(const QString& savedPlan)
{
    if(m_running||m_discovering)return;
    m_discovering=true;updateMode();m_referenceCounts->setText(tr("Discovering Part Reference corpus and installed models..."));
    const auto database=DatabaseManager::instance().databasePath();
    const auto library=UserSettings::instance().ldrawLibraryPath();
    auto* watcher=new QFutureWatcher<BatchPrintCorpus>(this);QPointer<PrintCapabilityAuditDialog> self(this);
    connect(watcher,&QFutureWatcher<BatchPrintCorpus>::finished,this,[self,watcher,savedPlan]{
        if(!self){watcher->deleteLater();return;}
        try{self->m_corpus=watcher->result();}catch(...){self->m_corpus={};self->m_corpus.diagnostic=QStringLiteral("Corpus discovery failed.");}
        watcher->deleteLater();self->m_discovering=false;
        self->m_continuationPlan=!savedPlan.isEmpty();
        if(!self->m_corpus.ok())self->m_referenceCounts->setText(self->m_corpus.diagnostic);
        else {
            int models=0;for(const auto& part:self->m_corpus.parts)models+=!part.resolvedModel.isEmpty();
            const auto& plan=self->m_corpus.plan;
            self->m_referenceCounts->setText(self->tr("%1 positions; %2 unique Parts; %3 duplicate memberships; %4 with installed models; %5 without models. %6 Fingerprint: %7")
                .arg(plan.value("manifestPositionCount").toInt()).arg(self->m_corpus.parts.size())
                .arg(plan.value("duplicateMembershipCount").toInt()).arg(models).arg(self->m_corpus.parts.size()-models)
                .arg(savedPlan.isEmpty()?self->tr("Current definitions; no eligibility exclusions."):self->tr("Saved ordered corpus; no eligibility exclusions."))
                .arg(plan.value("corpusFingerprint").toString()));
            self->m_referenceFirst->setMaximum(self->m_corpus.parts.size());self->m_referenceFirst->setValue(1);
            self->m_referenceCount->setValue(0);
        }
        self->updateMode();
    });
    m_discoveryFuture=QtConcurrent::run([database,library,savedPlan]{
        return savedPlan.isEmpty()?PartReferenceAuditSource::load(database,library):
            BatchPrintableModelService::readPartReferencePlan(savedPlan,library);
    });
    watcher->setFuture(m_discoveryFuture);
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
    if(m_running||m_discovering)return;
    const bool random=m_mode->currentIndex()==0,reference=m_mode->currentIndex()==2;
    const QStringList ids=m_partList->toPlainText().split(QRegularExpression(QStringLiteral("[\\s,;]+")),Qt::SkipEmptyParts);
    if(!random&&!reference&&ids.isEmpty()){
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
    options.partReference=reference;
    options.continuationPlan=reference&&m_continuationPlan;
    options.selectedFitProfileIdentity=m_fitProfile->currentData().toString();
    switch(m_printOrientation->currentIndex()){
    case 1:options.printOrientation.rotate(PrintOrientation::Rotation::XPositive);break;
    case 2:options.printOrientation.rotate(PrintOrientation::Rotation::YPositive);break;
    case 3:options.printOrientation.rotate(PrintOrientation::Rotation::ZPositive);break;
    default:break;
    }
    QVector<BatchPrintablePart> referenceParts;
    if(reference){
        if(!m_corpus.ok()){m_running=false;updateMode();return;}
        options.corpusPlan=m_corpus.plan;
        const int first=m_referenceFirst->value()-1;
        const int count=m_referenceCount->value()?m_referenceCount->value():m_corpus.parts.size()-first;
        referenceParts=m_corpus.parts.mid(first,count);
    }
    updateMode();
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
        self->showIncompleteRuns();self->updateMode();
        if(!result.ok){self->m_counters->setText(self->tr("Audit failed: %1").arg(result.diagnostic));return;}
        const auto fits=BatchPrintableModelService::summarizeFit(result.results);
        const QString fitSummary=self->tr(" Fit results: %1 validated fitted exports, %2 partial; %3 Verified-zero, %4 nonzero; %5 nominal fallbacks; %6 ManufacturingMesh failures. Details: %7/summary.json")
            .arg(fits.value("fittedPrintSuccess").toObject().value("numerator").toInt())
            .arg(fits.value("partialFittedSuccess").toInt()).arg(fits.value("verifiedZeroSuccess").toInt())
            .arg(fits.value("nonzeroFittedSuccess").toInt()).arg(fits.value("nominalFallback").toInt())
            .arg(fits.value("fittedManufacturingFailures").toInt()).arg(result.runDirectory);
        if(!result.referenceSummary.isEmpty()){
            const auto& summary=result.referenceSummary;
            self->m_counters->setText(self->tr("%1. Part Reference range: %2 unique Parts; %3 completed; %4 with models; %5 without models. Native successes %6 (%7%); override recoveries %8; practical successes %9 (%10%). Source coverage %11; resource limit %12; other failures %13. Elapsed %14 s. CSV: %15. Global/catalog summaries: %16/summary.json")
                .arg(result.stopped?self->tr("Stopped"):self->tr("Complete"))
                .arg(summary.value("uniqueParts").toInt()).arg(summary.value("completedParts").toInt())
                .arg(summary.value("modelBearingParts").toInt()).arg(summary.value("noModelParts").toInt())
                .arg(summary.value("nativeSuccesses").toInt()).arg(summary.value("nativeSuccessPercent").toDouble(),0,'f',1)
                .arg(summary.value("overrideRecoveries").toInt()).arg(summary.value("practicalSuccesses").toInt())
                .arg(summary.value("practicalSuccessPercent").toDouble(),0,'f',1)
                .arg(summary.value("sourceCoverageFailures").toInt()).arg(summary.value("resourceLimitFailures").toInt())
                .arg(summary.value("otherFailures").toInt()).arg(summary.value("elapsedMilliseconds").toDouble()/1000,0,'f',1)
                .arg(result.csvPath,result.runDirectory));
            self->m_counters->setText(self->m_counters->text()+fitSummary);return;
        }
        const auto& t=result.totals;
        const int noColor=result.population.catalogTotal?result.population.excludedNoColor:t.skippedNoColor;
        const int stickers=result.population.catalogTotal?result.population.excludedStickerCategory:t.skippedStickerCategory;
        const int nonstandard=result.population.catalogTotal?result.population.excludedNonstandardId:t.skippedNonstandardIds;
        self->m_counters->setText(self->tr("%1. Eligible pool %2; sampled %3; attempted %4; practically printable %5; excluded no Color %6; Sticker category %7; nonstandard IDs %8; no model %9; native preparation failures %10; ManufacturingMesh failures %11; export/reopen failures %12. Model availability %13%; native print-service success %14%; catalog-to-printable %15%. CSV: %16. Native successes %17; override-assisted recoveries %18; practical printable success %19% (model-bearing denominator %20).")
            .arg(result.stopped?self->tr("Stopped"):self->tr("Complete"))
            .arg(result.population.catalogTotal?result.population.eligibleTotal:t.eligible)
            .arg(result.population.catalogTotal?result.population.actualSampled:t.input)
            .arg(t.attempted).arg(t.successful).arg(noColor).arg(stickers)
            .arg(nonstandard).arg(t.noModel).arg(t.prepareFailures)
            .arg(t.manufacturingFailures).arg(t.exportFailures)
            .arg(t.modelAvailabilityPercent(),0,'f',1).arg(t.printServicePercent(),0,'f',1)
            .arg(t.catalogToPrintablePercent(),0,'f',1).arg(result.csvPath)
            .arg(t.nativeSuccessful).arg(t.overrideRecoveries)
            .arg(t.practicalPrintablePercent(),0,'f',1).arg(t.modelAvailable));
        self->m_counters->setText(self->m_counters->text()+fitSummary);
    });
    options.phaseProgress=[self](int sequence,const QString& part,const QString& phase){
        if(self)QMetaObject::invokeMethod(self,[self,sequence,part,phase]{
            if(self&&self->m_running)self->showPhase(sequence,part,phase);
        },Qt::QueuedConnection);
    };
    const auto runner=m_runner;
    m_future=QtConcurrent::run([databasePath,random,ids,count,options,cancellation,self,runner,referenceParts]{
        const auto progress=[self](const BatchPrintResult& row,const BatchPrintTotals&){
            if(self)QMetaObject::invokeMethod(self,[self,row]{
                if(self&&self->m_running)self->showPhase(row.sequence,row.partNumber,
                    QStringLiteral("completed: ")+BatchPrintableModelService::categoryCode(row.category));
            },Qt::QueuedConnection);
        };
        if(runner)return runner(options,cancellation.get(),progress);
        if(options.partReference)return BatchPrintableModelService().run(referenceParts,options,cancellation.get(),progress);
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
    if(m_discovering)m_discoveryFuture.waitForFinished();
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
