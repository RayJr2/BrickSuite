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

#include "../common/TooltipPolicy.h"
#include <QPlainTextEdit>
#include <QStandardItemModel>
#include "../../services/geometry/fit/FitCalibrationCapabilities.h"
#include "FitCalibrationDialog.h"
#include "../help/HelpManager.h"
#include "../../services/geometry/fit/FitCalibrationNamingCatalog.h"
#include "../../services/geometry/fit/FitCalibrationArtifactLocation.h"
#include "../../services/geometry/fit/FitCalibrationGenerationService.h"
#include <QFileInfo>
#include "../../services/geometry/LDrawLibraryService.h"
#include "../../settings/UserSettings.h"
#include "../common/SessionFileDialogDirectoryService.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QFormLayout>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProgressDialog>
#include <QtConcurrent>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
using namespace PrintGeometry;
namespace {
constexpr auto lastWorkspaceKey="LegoFitCalibration/LastWorkspaceIdentity";
QString resultText(FitObservation v){switch(v){case FitObservation::TooTight:return "Too Tight";case FitObservation::Acceptable:return "Acceptable";case FitObservation::Preferred:return "Preferred";case FitObservation::TooLoose:return "Too Loose";case FitObservation::UnableToEvaluate:return "Unable to Evaluate";default:return "Unevaluated";}}
QTableWidget* makeTable(QWidget*p){auto*t=new QTableWidget(p);t->setColumnCount(7);t->setHorizontalHeaderLabels({"Candidate","Correction (mm)","Diameter (mm)","Observations","Progress","Last Result","Preferred"});t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);t->setSelectionBehavior(QAbstractItemView::SelectRows);return t;}
QString featureName(const FitCalibrationSession&session){const auto*e=session.hasFineExperiment?&session.fineExperiment:(session.hasCoarseExperiment?&session.coarseExperiment:nullptr);return e?FitCalibrationLibrary::featureDisplayName(*e,session.process.actualPrintedOrientation):QStringLiteral("Empty feature");}
}
FitCalibrationDialog::FitCalibrationDialog(QWidget*p,const QString& managedRoot):QDialog(p),m_library(managedRoot){setWindowTitle("LEGO Fit Calibration");HelpManager::setContextTopic(this,HelpTopic::FitCalibration);resize(980,760);auto*root=new QVBoxLayout(this);auto*files=new QHBoxLayout;m_sessions=new QComboBox(this);m_sessions->setMinimumWidth(300);auto*workspaceActions=new QPushButton("Workspace...",this);auto*workspaceMenu=new QMenu(workspaceActions);auto*newWorkspaceAction=workspaceMenu->addAction("New Workspace...");auto*deleteWorkspaceAction=workspaceMenu->addAction("Delete Selected Workspace...");workspaceActions->setMenu(workspaceMenu);auto*load=new QPushButton("Import Session / Package...",this);auto*recover=new QPushButton("Recover Package...",this);m_newFamily=new QComboBox(this);m_newFamily->setObjectName(QStringLiteral("fitCalibrationNewFamily"));for(const auto&option:FitCalibrationFamilyCatalog::availableFamilies())m_newFamily->addItem(option.label,int(option.family));m_generationMode=new QComboBox(this);m_generationMode->addItems({"Single Calibration","Recommended Calibration Package","Custom Calibration Package"});auto*newCalibration=new QPushButton("Generate Calibration Fixture...",this);m_newGeneration=newCalibration;newCalibration->setObjectName(QStringLiteral("fitCalibrationNewAction"));m_save=new QPushButton("Save Managed",this);auto*saveAs=new QPushButton("Export Feature...",this);auto*profile=new QPushButton("Create / Update Fit Profile...",this);files->addWidget(new QLabel("Existing Workspaces:",this));files->addWidget(m_sessions,1);files->addWidget(workspaceActions);root->addLayout(files);
auto* generation=new QHBoxLayout;generation->addWidget(new QLabel("Generate:",this));generation->addWidget(m_generationMode);generation->addWidget(m_newFamily,1);generation->addWidget(newCalibration);root->addLayout(generation);
auto* packageActions=new QHBoxLayout;packageActions->addWidget(load);packageActions->addWidget(recover);packageActions->addStretch();packageActions->addWidget(m_save);packageActions->addWidget(saveAs);packageActions->addWidget(profile);root->addLayout(packageActions);m_identity=new QLabel("No calibration workspace loaded",this);QFont identityFont=m_identity->font();identityFont.setBold(true);m_identity->setFont(identityFont);root->addWidget(m_identity);m_autoSave=new QTimer(this);m_autoSave->setSingleShot(true);m_autoSave->setInterval(500);
auto*process=new QGroupBox("Manufacturing Context",this);auto*form=new QFormLayout(process);m_printer=new QLineEdit(process);m_material=new QLineEdit(process);m_profile=new QLineEdit(process);m_nozzle=new QDoubleSpinBox(process);m_nozzle->setRange(.1,2);m_nozzle->setDecimals(2);m_nozzle->setValue(.4);m_nozzle->setSuffix(" mm");m_layer=new QDoubleSpinBox(process);m_layer->setRange(.01,2);m_layer->setDecimals(3);m_layer->setValue(.2);m_layer->setSuffix(" mm");m_compensationNotes=new QLineEdit(process);form->addRow("Printer:",m_printer);form->addRow("Material:",m_material);form->addRow("Nozzle:",m_nozzle);form->addRow("Process/profile:",m_profile);form->addRow("Layer height:",m_layer);form->addRow("Slicer compensation:",m_compensationNotes);root->addWidget(process);
m_features=new QComboBox(this);root->addWidget(new QLabel("Feature calibration:",this));root->addWidget(m_features);auto*orientationBox=new QGroupBox("Feature Print Orientation",this);auto*orientationForm=new QFormLayout(orientationBox);m_orientation=new QComboBox(orientationBox);m_orientation->setObjectName("fitCalibrationActualOrientation");m_orientation->addItem("Not yet confirmed",int(FitPrintedOrientation::Unknown));m_orientation->addItem("Feature axis parallel to build plate",int(FitPrintedOrientation::FeatureAxisParallelToBuildPlate));m_orientation->addItem("Feature axis perpendicular to build plate",int(FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate));m_orientation->addItem("Other / unsupported",int(FitPrintedOrientation::OtherUnsupported));m_orientationNotes=new QLineEdit(orientationBox);orientationForm->addRow("Actual orientation:",m_orientation);orientationForm->addRow("Orientation notes:",m_orientationNotes);root->addWidget(orientationBox);
m_tabs=new QTabWidget(this);auto*coarse=new QWidget(m_tabs);auto*coarseLayout=new QVBoxLayout(coarse);m_coarseReview=new QComboBox(coarse);m_coarseReview->setObjectName(QStringLiteral("fitCalibrationCoarseHistory"));m_coarseContext=new QLabel(coarse);m_coarseTable=makeTable(coarse);coarseLayout->addWidget(m_coarseReview);coarseLayout->addWidget(m_coarseContext);coarseLayout->addWidget(m_coarseTable);auto*fine=new QWidget(m_tabs);auto*fineLayout=new QVBoxLayout(fine);m_fineContext=new QLabel(fine);m_fineTable=makeTable(fine);fineLayout->addWidget(m_fineContext);fineLayout->addWidget(m_fineTable);m_tabs->addTab(coarse,"Coarse Search");m_tabs->addTab(fine,"Fine Search / Verification");root->addWidget(m_tabs,1);
auto*entry=new QHBoxLayout;m_result=new QComboBox(this);for(auto v:{FitObservation::TooTight,FitObservation::Acceptable,FitObservation::Preferred,FitObservation::TooLoose,FitObservation::UnableToEvaluate})m_result->addItem(resultText(v),int(v));m_repeat=new QSpinBox(this);m_repeat->setObjectName("fitCalibrationObservationQuantity");m_repeat->setRange(1,99);m_measured=new QDoubleSpinBox(this);m_measured->setRange(0,99);m_measured->setDecimals(3);m_measured->setSpecialValueText("Not measured");m_notes=new QLineEdit(this);m_notes->setPlaceholderText("Observation notes");auto*add=new QPushButton("Add Observation",this);auto*preferred=new QPushButton("Select Preferred",this);m_generate=new QPushButton("Continue Calibration...",this);m_verify=new QPushButton("Mark Verified",this);entry->addWidget(m_result);entry->addWidget(m_repeat);entry->addWidget(m_measured);entry->addWidget(m_notes,1);entry->addWidget(add);entry->addWidget(preferred);entry->addWidget(m_generate);entry->addWidget(m_verify);root->addLayout(entry);
auto*bottom=new QHBoxLayout;m_guidance=new QLabel(this);m_guidance->setWordWrap(true);m_guidance->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);auto*buttons=new QDialogButtonBox(QDialogButtonBox::Close,this);bottom->addWidget(m_guidance,1);bottom->addWidget(buttons);root->addLayout(bottom);
TooltipPolicy::explain(m_sessions, tr("Switch manufacturing workspaces immediately. Unsaved changes must be saved or the switch cancelled."));
TooltipPolicy::explain(m_generationMode, tr("Generate one calibration, the recommended four-family core, or a custom package of independent sessions."));
TooltipPolicy::explain(m_save, tr("Save this feature’s observations and manufacturing context in the managed calibration library."));
TooltipPolicy::explain(load, tr("Import a portable single session or package companion. Existing newer managed evidence is preserved."));
TooltipPolicy::explain(recover, tr("Retry registration of a complete published package without regenerating fixtures or resetting observations."));
TooltipPolicy::explain(saveAs, tr("Export this feature’s portable session and available lineage; printable fixtures remain in their package."));
TooltipPolicy::explain(profile, tr("Contribute compatible, explicitly Verified feature evidence to this workspace’s Fit Profile."));
TooltipPolicy::explain(m_features, tr("Choose an independent feature session. Observations and continuation do not change sibling sessions."));
TooltipPolicy::explain(m_orientation, tr("Confirm the orientation you physically printed. The intended fixture label is not evidence. F1 opens calibration Help."));
TooltipPolicy::explain(m_orientationNotes, tr("Record actual placement, support or orientation details for this feature."));
TooltipPolicy::explain(add, tr("Record one physical trial for the selected candidate, including its repeat number."));
TooltipPolicy::explain(preferred, tr("Choose the best observed candidate. Preferred is provisional; it does not mean Verified."));
TooltipPolicy::explain(m_result, tr("Describe the selected candidate’s physical fit; do not infer a result from generated geometry."));
TooltipPolicy::explain(m_repeat, tr("Identify repeated physical trials for this candidate."));
TooltipPolicy::explain(m_measured, tr("Optional measured dimension in millimeters; leave Not measured when no measurement was taken."));
m_tabs->setTabToolTip(0,tr("Review the broad search and earlier range-extension evidence."));
m_tabs->setTabToolTip(1,tr("Record independent fine-search or verification trials; earlier Coarse evidence stays separate."));
connect(buttons,&QDialogButtonBox::rejected,this,[this]{if(m_dirty)saveSession();reject();});connect(m_sessions,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int index){if(!m_loading&&index>=0)activateWorkspace(m_sessions->itemData(index).toString());});connect(newWorkspaceAction,&QAction::triggered,this,&FitCalibrationDialog::newWorkspace);connect(deleteWorkspaceAction,&QAction::triggered,this,&FitCalibrationDialog::deleteSelectedWorkspace);connect(load,&QPushButton::clicked,this,&FitCalibrationDialog::loadSession);connect(recover,&QPushButton::clicked,this,&FitCalibrationDialog::recoverPackage);connect(m_generationMode,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int mode){m_newFamily->setVisible(mode==0);m_newGeneration->setText(mode==0?"Generate Calibration Fixture...":"Generate Calibration Package...");refreshCapabilities();});connect(m_newFamily,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{refreshCapabilities();});connect(newCalibration,&QPushButton::clicked,this,&FitCalibrationDialog::createSelectedCalibration);connect(m_save,&QPushButton::clicked,this,[this]{saveSession();});connect(saveAs,&QPushButton::clicked,this,&FitCalibrationDialog::exportSession);connect(profile,&QPushButton::clicked,this,&FitCalibrationDialog::createFitProfile);connect(m_autoSave,&QTimer::timeout,this,[this]{if(m_dirty)saveSession();});connect(add,&QPushButton::clicked,this,&FitCalibrationDialog::addObservation);connect(preferred,&QPushButton::clicked,this,&FitCalibrationDialog::selectPreferred);connect(m_generate,&QPushButton::clicked,this,&FitCalibrationDialog::generateFineSearch);connect(m_verify,&QPushButton::clicked,this,&FitCalibrationDialog::markVerified);connect(m_tabs,&QTabWidget::currentChanged,this,[this]{refresh();});connect(m_coarseReview,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{if(!m_loading)refresh();});connect(m_features,qOverload<int>(&QComboBox::currentIndexChanged),this,&FitCalibrationDialog::selectFeature);auto processChanged=[this]{if(m_loading||(!m_session.hasCoarseExperiment&&!m_session.hasFineExperiment))return;readProcess();m_dirty=true;m_savedConfirmation=false;scheduleManagedSave();refresh();};connect(m_printer,&QLineEdit::textChanged,this,processChanged);connect(m_material,&QLineEdit::textChanged,this,processChanged);connect(m_profile,&QLineEdit::textChanged,this,processChanged);connect(m_orientationNotes,&QLineEdit::textChanged,this,processChanged);connect(m_compensationNotes,&QLineEdit::textChanged,this,processChanged);connect(m_nozzle,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[processChanged](double){processChanged();});connect(m_layer,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[processChanged](double){processChanged();});connect(m_orientation,qOverload<int>(&QComboBox::currentIndexChanged),this,[processChanged](int){processChanged();});activateWorkspace(QSettings().value(lastWorkspaceKey).toString());}

void FitCalibrationDialog::createSelectedCalibration(){if(m_generationMode->currentIndex())createUnifiedPackage(m_generationMode->currentIndex()==1);else createCalibration(FitCalibrationFamily(m_newFamily->currentData().toInt()));}

FitCalibrationExperiment* FitCalibrationDialog::activeExperiment(){if(m_tabs->currentIndex()==0)return m_session.hasCoarseExperiment&&m_coarseReview->currentIndex()==m_coarseReview->count()-1?&m_session.coarseExperiment:nullptr;return m_session.hasFineExperiment?&m_session.fineExperiment:nullptr;}
QTableWidget* FitCalibrationDialog::activeTable()const{return m_tabs->currentIndex()==0?m_coarseTable:m_fineTable;}
void FitCalibrationDialog::readProcess(){if(m_loading)return;if(m_featureIndex>=0&&m_featureIndex<m_workspace.featureSessions.size())m_workspace.featureSessions[m_featureIndex]=m_session;const auto orientation=FitPrintedOrientation(m_orientation->currentData().toInt());const QString orientationNotes=m_orientationNotes->text();for(auto& feature:m_workspace.featureSessions){feature.process.printerIdentity=m_printer->text().trimmed();feature.process.materialIdentity=m_material->text().trimmed();feature.process.profileName=m_profile->text().trimmed();feature.process.hasNozzleDiameter=true;feature.process.nozzleDiameterMillimetres=m_nozzle->value();feature.process.hasLayerHeight=true;feature.process.layerHeightMillimetres=m_layer->value();feature.process.dimensionalCompensationNotes=m_compensationNotes->text();if(feature.sessionIdentity==m_session.sessionIdentity){feature.process.actualPrintedOrientation=orientation;feature.process.orientationNotes=orientationNotes;}if(feature.hasCoarseExperiment)feature.coarseExperiment.process=feature.process;if(feature.hasFineExperiment)feature.fineExperiment.process=feature.process;}if(m_featureIndex>=0&&m_featureIndex<m_workspace.featureSessions.size())m_session=m_workspace.featureSessions[m_featureIndex];if(!m_workspace.featureSessions.isEmpty()){m_workspace.process=m_workspace.featureSessions.front().process;m_workspace.process.actualPrintedOrientation=FitPrintedOrientation::Unknown;m_workspace.process.orientationNotes.clear();m_workspace.identity=FitCalibrationLibrary::manufacturingContextFingerprint(m_workspace.process);m_workspace.displayName=FitCalibrationLibrary::workspaceDisplayName(m_workspace.process);}}
void FitCalibrationDialog::refreshTable(QTableWidget*t,const FitCalibrationExperiment*e){const int selectedRow=t->currentRow();t->setRowCount(e?e->candidates.size():0);if(!e)return;const bool click=e->featureFamily==QStringLiteral("ClickHinge");const bool finger=e->featureFamily==QStringLiteral("InterleavedFingerHinge");const bool height=e->correctionDimension==FitCorrectionDimension::Height;const bool axle=e->featureFamily==QStringLiteral("TechnicAxle")||e->featureFamily==QStringLiteral("TechnicAxleHole");const bool armWidth=e->hasRegenerationPrototype&&e->regenerationPrototype.constructionRecipe==QStringLiteral("technic-axle-hole-arm-width-clearance-v2");const bool ballSocket=e->featureFamily==QStringLiteral("BallSocket");const bool wallPocket=e->hasRegenerationPrototype&&e->regenerationPrototype.constructionRecipe==QStringLiteral("stud-receiving-wall-pocket-square-v1");t->setHorizontalHeaderItem(1,new QTableWidgetItem(click?"Arrestor Reach Offset (mm)":finger?"Contact Bump Offset (mm)":height?"Height Correction (mm)":ballSocket?"Contact / Throat Offset (mm)":wallPocket?"Opening Correction (mm)":armWidth?"Arm Width Correction (mm)":axle?"Tip-to-Tip Correction (mm)":"Diameter Correction (mm)"));t->setHorizontalHeaderItem(2,new QTableWidgetItem(click?"Arrestor Radial Reach (mm)":finger?"Contact Bump Protrusion (mm)":height?"Height (mm)":ballSocket?"Contact Reference (mm)":wallPocket?"Opening Width (mm)":armWidth?"Arm Opening Width (mm)":axle?"Tip-to-Tip (mm)":"Diameter (mm)"));for(int r=0;r<e->candidates.size();++r){const auto&c=e->candidates[r];t->setItem(r,0,new QTableWidgetItem(QString::number(c.index)));t->setItem(r,1,new QTableWidgetItem(QString::number(FitCalibrationEvidencePolicy::candidateCorrection(*e,c),'f',3)));t->setItem(r,2,new QTableWidgetItem(QString::number(FitCalibrationEvidencePolicy::candidateFunctionalDimension(*e,c),'f',3)));t->setItem(r,3,new QTableWidgetItem(QString::number(FitCalibrationEvidencePolicy::observationCount(c))));t->setItem(r,4,new QTableWidgetItem(FitCalibrationEvidencePolicy::observationProgressText(*e,c)));t->setItem(r,5,new QTableWidgetItem(c.observations.isEmpty()?QString():resultText(c.observations.back().result)));t->setItem(r,6,new QTableWidgetItem(c.index==e->preferredCandidateIndex?"Yes":""));}if(selectedRow>=0&&selectedRow<t->rowCount())t->selectRow(selectedRow);}
void FitCalibrationDialog::refresh(){
const int reviewIndex=m_coarseReview->currentIndex();
const bool currentCoarse=m_session.hasCoarseExperiment&&reviewIndex==m_coarseReview->count()-1;
const FitCalibrationExperiment*coarse=currentCoarse?&m_session.coarseExperiment:
    reviewIndex>=0&&reviewIndex<m_coarseHistory.size()?&m_coarseHistory[reviewIndex]:nullptr;
refreshTable(m_coarseTable,coarse);refreshTable(m_fineTable,m_session.hasFineExperiment?&m_session.fineExperiment:nullptr);
const bool missingParent=!coarse&&m_session.hasFineExperiment&&
    !m_session.fineExperiment.parentArtifactIdentity.isEmpty();
m_tabs->setTabEnabled(0,coarse||missingParent);m_tabs->setTabEnabled(1,m_session.hasFineExperiment);
m_identity->setText(m_workspace.featureSessions.isEmpty()?(m_workspace.identity.isEmpty()?QStringLiteral("No calibration workspace loaded"):QStringLiteral("%1 — No feature calibration yet").arg(m_workspace.displayName)):QStringLiteral("%1 — %2").arg(FitCalibrationLibrary::workspaceDisplayName(m_session.process),featureName(m_session)));
m_coarseContext->setText(coarse?QStringLiteral("Artifact: %1 — Parent: %2%3").arg(coarse->artifactIdentity,
    coarse->parentArtifactIdentity.isEmpty()?QStringLiteral("none"):coarse->parentArtifactIdentity,
    currentCoarse?QString():QStringLiteral(" — Historical review (read-only)")):
    missingParent?QStringLiteral("Earlier evidence for parent artifact %1 is unavailable in the managed calibration history.")
        .arg(m_session.fineExperiment.parentArtifactIdentity):
        QStringLiteral("No coarse search is present in this session."));
m_fineContext->setText(m_session.hasFineExperiment?QStringLiteral("Artifact: %1 — Parent: %2 — Center: %3 mm").arg(m_session.fineExperiment.artifactIdentity,m_session.fineExperiment.parentArtifactIdentity).arg(FitCalibrationEvidencePolicy::candidateCorrection(m_session.fineExperiment,m_session.fineExperiment.candidates[m_session.fineExperiment.candidates.size()/2]),0,'f',3):QStringLiteral("Generate a fine search or direct verification artifact from a selected candidate."));
// Feature orientation belongs to a generated session, not an empty workspace.
const bool hasFeature=m_session.hasCoarseExperiment||m_session.hasFineExperiment;
m_orientation->setEnabled(hasFeature);m_orientationNotes->setEnabled(hasFeature);
m_save->setEnabled(hasFeature);auto*e=activeExperiment();FitCalibrationGenerationService::Request continuation;continuation.hasParent=true;continuation.parent=m_session;continuation.stage=FitCalibrationGenerationService::Stage::FineSearch;continuation.libraryRoot=UserSettings::instance().ldrawLibraryPath();
const auto unavailable=e?FitCalibrationGenerationService::unavailableReason(continuation):QString("Select a calibration first.");
m_generate->setEnabled(e&&unavailable.isEmpty());TooltipPolicy::explain(m_generate, unavailable.isEmpty()?tr("Generate a child search or verification fixture. Preserve parent evidence and start fresh child observations."):unavailable);refreshCapabilities();QString verificationReason;const bool ready=e&&e->state!=FitEvidenceState::Verified&&FitCalibrationEvidencePolicy::canMarkVerified(*e,&verificationReason);m_verify->setEnabled(ready);m_verify->setText(e&&e->state==FitEvidenceState::Verified?QStringLiteral("Verified"):QStringLiteral("Mark Verified"));TooltipPolicy::explain(m_verify, e&&e->state==FitEvidenceState::Verified?tr("This evidence was explicitly Verified; compatible corrections can contribute to a Fit Profile."):ready?tr("Explicitly verify sufficient physical evidence. Preferred alone is not verification."):verificationReason);updateGuidance();if(m_tabs->currentIndex()==0&&coarse&&!currentCoarse)m_guidance->setText(QStringLiteral("Historical coarse evidence is read-only. Select the latest stage or Fine Search / Verification to continue."));}
void FitCalibrationDialog::updateGuidance(){const auto*e=activeExperiment();QString text=e?FitCalibrationEvidencePolicy::guidanceText(*e):QStringLiteral("Load a calibration session to begin.");if(e&&e->state!=FitEvidenceState::Verified){QString reason;if(FitCalibrationEvidencePolicy::canMarkVerified(*e,&reason))text=QStringLiteral("Calibration evidence is sufficient. Mark Verified is available; Fine Search or additional verification is optional.");else if(e->preferredCandidateIndex>0)text=QStringLiteral("Mark Verified unavailable: %1").arg(reason);}else if(e&&e->state==FitEvidenceState::Verified&&e->preferredCandidateIndex>0){const auto it=std::find_if(e->candidates.cbegin(),e->candidates.cend(),[e](const auto&c){return c.index==e->preferredCandidateIndex;});if(it!=e->candidates.cend()){const double value=FitCalibrationEvidencePolicy::candidateCorrection(*e,*it);if(m_savedConfirmation&&!m_dirty)text=QStringLiteral("Verified calibration saved: %1 mm. Create / Update the Fit Profile to make this correction available to ManufacturingMesh and Auto Fit.").arg(value,0,'f',3);else if(!m_dirty)text=QStringLiteral("Verified calibration loaded: %1 mm. Create / Update the Fit Profile to make this correction available to ManufacturingMesh and Auto Fit.").arg(value,0,'f',3);}}m_guidance->setText(text);m_guidance->setToolTip(text);}
void FitCalibrationDialog::showSession(const FitCalibrationSession&s,const QString&path){FitCalibrationWorkspace workspace;workspace.identity=FitCalibrationLibrary::manufacturingContextFingerprint(s.process);workspace.displayName=FitCalibrationLibrary::workspaceDisplayName(s.process);workspace.process=s.process;workspace.featureSessions.push_back(s);showWorkspace(workspace,s.sessionIdentity);m_path=path;}
void FitCalibrationDialog::showWorkspace(const FitCalibrationWorkspace&workspace,const QString&preferredSession){m_loading=true;m_workspace=workspace;m_activeWorkspaceIdentity=workspace.identity;m_path.clear();m_dirty=false;m_savedConfirmation=false;m_printer->setText(workspace.process.printerIdentity);m_material->setText(workspace.process.materialIdentity);m_profile->setText(workspace.process.profileName);m_nozzle->setValue(workspace.process.hasNozzleDiameter?workspace.process.nozzleDiameterMillimetres:.4);m_layer->setValue(workspace.process.hasLayerHeight?workspace.process.layerHeightMillimetres:.2);m_compensationNotes->setText(workspace.process.dimensionalCompensationNotes);m_features->clear();int selected=0;for(int i=0;i<workspace.featureSessions.size();++i){const auto&s=workspace.featureSessions[i];m_features->addItem(featureName(s),s.sessionIdentity);if(s.sessionIdentity==preferredSession)selected=i;}m_features->setCurrentIndex(workspace.featureSessions.isEmpty()?-1:selected);if(workspace.featureSessions.isEmpty()){m_session={};m_featureIndex=-1;m_coarseHistory.clear();m_coarseReview->clear();m_orientation->setCurrentIndex(0);m_orientationNotes->clear();}const bool empty=workspace.featureSessions.isEmpty();for(auto*edit:{m_printer,m_material,m_profile,m_compensationNotes})edit->setReadOnly(empty);m_nozzle->setReadOnly(empty);m_layer->setReadOnly(empty);m_loading=false;refreshLibrary();if(workspace.featureSessions.isEmpty())refresh();else selectFeature(selected);rememberWorkspace();}
void FitCalibrationDialog::selectFeature(int index){if(m_loading||index<0||index>=m_workspace.featureSessions.size())return;if(m_dirty&&!saveSession()){m_features->setCurrentIndex(m_featureIndex);return;}m_loading=true;if(m_session.sessionIdentity!=m_workspace.featureSessions[index].sessionIdentity)m_repeat->setValue(1);m_featureIndex=index;m_session=m_workspace.featureSessions[index];m_coarseHistory=m_library.coarseReviewHistory(m_session);m_coarseReview->clear();for(int i=0;i<m_coarseHistory.size();++i)m_coarseReview->addItem(QStringLiteral("%1 — %2").arg(i==m_coarseHistory.size()-1?QStringLiteral("Latest coarse stage"):QStringLiteral("Earlier coarse stage %1").arg(i+1),m_coarseHistory[i].artifactIdentity));m_coarseReview->setCurrentIndex(m_coarseReview->count()-1);m_coarseReview->setVisible(m_coarseReview->count()>1);m_orientation->setCurrentIndex(m_orientation->findData(int(m_session.process.actualPrintedOrientation)));m_orientationNotes->setText(m_session.process.orientationNotes);m_tabs->setCurrentIndex(FitCalibrationEvidencePolicy::preferredSessionStage(m_session)==FitCalibrationStage::Fine?1:0);m_loading=false;refresh();}
bool FitCalibrationDialog::openSession(const QString&path){if(FitCalibrationGenerationService::isPackageCompanion(path)){if(m_dirty&&!saveSession())return false;runUnifiedPackage({},QFileInfo(path).absolutePath());return true;}FitCalibrationSession s;QString error;const FitCalibrationWorkspace*selected=m_workspace.featureSessions.isEmpty()?nullptr:&m_workspace;if(!m_library.importSessionIntoWorkspace(path,selected,&s,&error)){QMessageBox::warning(this,"Calibration",error);return false;}refreshLibrary();for(const auto&w:m_library.workspaces())if(w.identity==FitCalibrationLibrary::manufacturingContextFingerprint(s.process)){showWorkspace(w,s.sessionIdentity);return true;}showSession(s,QString());return true;}


void FitCalibrationDialog::loadSession(){auto&dirs=SessionFileDialogDirectoryService::instance();const auto path=QFileDialog::getOpenFileName(this,"Open Calibration Session",dirs.initialDirectory(FileDialogDirectoryCategory::OpenImport,FitCalibrationArtifactLocation::directory()),"Calibration JSON (*.json)");if(path.isEmpty())return;dirs.rememberSelectedFile(FileDialogDirectoryCategory::OpenImport,path);openSession(path);}
bool FitCalibrationDialog::writeSession(const QString&path){QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(QJsonDocument(FitCalibrationSessionJson::toJson(m_session)).toJson(QJsonDocument::Indented))<0||!f.commit()){QMessageBox::warning(this,"Calibration","The calibration session could not be saved.");return false;}m_dirty=false;m_savedConfirmation=true;updateGuidance();return true;}
bool FitCalibrationDialog::saveSession(){if(!m_session.hasCoarseExperiment&&!m_session.hasFineExperiment)return false;readProcess();QString error;for(auto&feature:m_workspace.featureSessions)if(!m_library.saveSession(&feature,&error)){QMessageBox::warning(this,"Calibration",error);return false;}if(m_featureIndex>=0&&m_featureIndex<m_workspace.featureSessions.size())m_session=m_workspace.featureSessions[m_featureIndex];m_dirty=false;m_savedConfirmation=true;m_activeWorkspaceIdentity=m_workspace.identity;refreshLibrary();rememberWorkspace();updateGuidance();return true;}
void FitCalibrationDialog::scheduleManagedSave(){if((m_session.hasCoarseExperiment||m_session.hasFineExperiment)&&m_autoSave)m_autoSave->start();}
void FitCalibrationDialog::refreshLibrary(){const QSignalBlocker blocker(m_sessions);const QString selected=m_activeWorkspaceIdentity;QVector<FitLibraryIssue> issues;const auto available=m_library.workspaces(&issues);m_sessions->clear();for(const auto&item:available)m_sessions->addItem(item.displayName,item.identity);const int index=m_sessions->findData(selected);m_sessions->setCurrentIndex(index);TooltipPolicy::explain(m_sessions, tr("Switch manufacturing workspaces immediately. Save pending edits or Cancel to stay in the current workspace."));if(!issues.isEmpty())m_sessions->setToolTip(QStringLiteral("%1 managed record(s) could not be loaded. Valid records remain available.").arg(issues.size()));}
void FitCalibrationDialog::rememberWorkspace()
{
    QSettings settings;
    if(m_activeWorkspaceIdentity.isEmpty())settings.remove(lastWorkspaceKey);
    else settings.setValue(lastWorkspaceKey,m_activeWorkspaceIdentity);
}
void FitCalibrationDialog::activateWorkspace(const QString& requestedIdentity)
{
    // Capture the request before saving: saveSession repopulates the combo.
    const QString identity=requestedIdentity;
    m_autoSave->stop();
    if(m_dirty&&(QMessageBox::question(this,"Unsaved Calibration Changes",
        "Save pending calibration edits before changing workspaces?",
        QMessageBox::Save|QMessageBox::Cancel,QMessageBox::Save)!=QMessageBox::Save||!saveSession())){
        const QSignalBlocker blocker(m_sessions);
        m_sessions->setCurrentIndex(m_sessions->findData(m_activeWorkspaceIdentity));
        return;
    }
    const auto available=m_library.workspaces();
    for(const auto& workspace:available)if(workspace.identity==identity){showWorkspace(workspace);return;}
    // workspaces() already supplies the library's deterministic display ordering.
    showWorkspace(available.isEmpty()?FitCalibrationWorkspace():available.front());
}
void FitCalibrationDialog::newWorkspace()
{
    if (m_dirty && !saveSession()) return;
    QDialog dialog(this); dialog.setWindowTitle("New Calibration Workspace");
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    QLineEdit printer(&dialog), material(&dialog), profile(&dialog), compensation(&dialog);
    QDoubleSpinBox nozzle(&dialog), layer(&dialog);
    nozzle.setRange(.1, 2); nozzle.setDecimals(2); nozzle.setValue(.4); nozzle.setSuffix(" mm");
    layer.setRange(.01, 2); layer.setDecimals(3); layer.setValue(.2); layer.setSuffix(" mm");
    form->addRow("Printer:", &printer); form->addRow("Material:", &material);
    form->addRow("Nozzle:", &nozzle); form->addRow("Process/profile:", &profile);
    form->addRow("Layer height:", &layer); form->addRow("Slicer compensation:", &compensation);
    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    FitCalibrationProcess process;
    process.printerIdentity = printer.text().trimmed(); process.materialIdentity = material.text().trimmed();
    process.profileName = profile.text().trimmed(); process.dimensionalCompensationNotes = compensation.text().trimmed();
    process.hasNozzleDiameter = true; process.nozzleDiameterMillimetres = nozzle.value();
    process.hasLayerHeight = true; process.layerHeightMillimetres = layer.value();
    FitCalibrationWorkspace created; QString error;
    if (!m_library.createWorkspace(process, &created, &error)) {
        QMessageBox::warning(this, "New Calibration Workspace", error); return;
    }
    activateWorkspace(created.identity);
}
void FitCalibrationDialog::deleteSelectedWorkspace()
{
    const QString identity = m_sessions->currentData().toString();
    if (identity.isEmpty()) return;
    if (m_dirty && !saveSession()) return;
    const auto available = m_library.workspaces();
    const auto it = std::find_if(available.cbegin(), available.cend(), [&](const auto& item) { return item.identity == identity; });
    if (it == available.cend()) { refreshLibrary(); return; }
    const QString warning = QStringLiteral("Delete the selected workspace '%1'?\n\n"
        "Its managed calibration sessions, historical lineage, manufacturing context, and associated Fit Profile "
        "will be removed from the active library. A recoverable backup is written first. "
        "Other workspaces are unaffected. External calibration packages are not modified.").arg(it->displayName);
    if (QMessageBox::question(this, "Delete Calibration Workspace", warning,
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
    QString backup, error;
    if (!m_library.deleteWorkspace(identity, &backup, &error)) {
        QMessageBox::warning(this, "Delete Calibration Workspace", error); return;
    }
    activateWorkspace({});
    QMessageBox::information(this, "Calibration Workspace Deleted",
        QStringLiteral("The selected workspace was removed from the active library.\nBackup: %1").arg(backup));
}
void FitCalibrationDialog::exportSession(){if((m_dirty||m_session.sessionIdentity.isEmpty())&&!saveSession())return;auto&dirs=SessionFileDialogDirectoryService::instance();const QString path=QFileDialog::getSaveFileName(this,"Export Calibration Session",dirs.initialFilePath(FileDialogDirectoryCategory::SaveExport,m_session.sessionIdentity+"-session.json"),"Calibration JSON (*.json)");if(path.isEmpty())return;dirs.rememberSelectedFile(FileDialogDirectoryCategory::SaveExport,path);QString error;if(!m_library.exportSession(m_session.sessionIdentity,path,&error))QMessageBox::warning(this,"Calibration",error);}
void FitCalibrationDialog::createFitProfile(){readProcess();if(m_dirty&&!saveSession())return;bool accepted=false;const QString suggested=QStringLiteral("%1 / %2 / %3 mm / LEGO Fit").arg(m_session.process.printerIdentity,m_session.process.materialIdentity).arg(m_session.process.nozzleDiameterMillimetres,0,'f',2);const QString name=QInputDialog::getText(this,"Create Fit Profile","Profile name:",QLineEdit::Normal,suggested,&accepted).trimmed();if(!accepted)return;FitProfile profile;QString error;if(!m_library.saveVerifiedWorkspaceProfile(m_workspace,name,&profile,&error)){QMessageBox::warning(this,"Fit Profile",error);return;}QMessageBox::information(this,"Fit Profile",QStringLiteral("Verified Fit Profile '%1' now contains all compatible Verified corrections in this manufacturing workspace. Auto Fit applies only corrections supported by the selected Part's recognized functional geometry.").arg(profile.name));}
void FitCalibrationDialog::addObservation(){auto*e=activeExperiment();auto*t=activeTable();const int row=t->currentRow();if(!e||row<0){QMessageBox::information(this,"Calibration","Select a candidate first.");return;}FitCalibrationObservation o;o.result=FitObservation(m_result->currentData().toInt());o.repeatNumber=m_repeat->value();o.hasMeasuredDiameter=m_measured->value()>0;o.measuredDiameterMillimetres=m_measured->value();o.notes=m_notes->text();o.performedUtc=QDateTime::currentDateTimeUtc();QString error;if(!FitCalibrationEvidencePolicy::addObservation(e,e->candidates[row].index,o,&error))QMessageBox::warning(this,"Calibration",error);else{m_dirty=true;m_savedConfirmation=false;scheduleManagedSave();refresh();}}
void FitCalibrationDialog::selectPreferred(){auto*e=activeExperiment();auto*t=activeTable();const int row=t->currentRow();QString error;if(!e||row<0||!FitCalibrationEvidencePolicy::selectPreferredCandidate(e,row<0||!e?0:e->candidates[row].index,&error))QMessageBox::warning(this,"Calibration",error.isEmpty()?"Select a candidate first.":error);else{m_dirty=true;m_savedConfirmation=false;scheduleManagedSave();refresh();}}
void FitCalibrationDialog::markVerified(){readProcess();auto*e=activeExperiment();QString error;if(!e||e->state==FitEvidenceState::Verified||!FitCalibrationEvidencePolicy::markVerified(e,&error)){if(e&&e->state==FitEvidenceState::Verified)return;QMessageBox::warning(this,"Calibration",error.isEmpty()?"Open the fine-search stage first.":error);}else{m_dirty=true;m_savedConfirmation=false;saveSession();refresh();}}

void FitCalibrationDialog::refreshCapabilities()
{
    auto* model=qobject_cast<QStandardItemModel*>(m_newFamily->model());if(!model)return;
    const auto options=FitCalibrationFamilyCatalog::availableFamilies();
    for(int row=0;row<options.size();++row){
        bool available=false;QStringList reasons;
        for(const auto& capability:FitCalibrationCapabilities::entries())if(capability.family==options[row].identity){
            FitCalibrationGenerationService::Request request;request.family=capability.family;request.variant=capability.variant;
            request.workspace=m_workspace;request.libraryRoot=UserSettings::instance().ldrawLibraryPath();
            const auto reason=FitCalibrationGenerationService::unavailableReason(request);
            available|=reason.isEmpty();if(!reason.isEmpty())reasons.push_back(reason);
        }
        model->item(row)->setEnabled(available);model->item(row)->setToolTip(reasons.join("\n"));
    }
    const auto* item=model->item(m_newFamily->currentIndex());
    m_newGeneration->setEnabled(m_generationMode->currentIndex()!=0?!m_workspace.identity.isEmpty():item&&item->isEnabled());
    TooltipPolicy::explain(m_newGeneration, m_generationMode->currentIndex()==0&&item&&!item->toolTip().isEmpty()?item->toolTip():tr("Review and publish fixtures with portable companions. Complex fixtures can take several minutes."));
}

void FitCalibrationDialog::createCalibration(FitCalibrationFamily family)
{
    if(m_dirty&&!saveSession())return;
    if(m_workspace.identity.isEmpty()){
        QMessageBox::information(this,"Calibration","Create or select a manufacturing workspace first.");return;
    }
    readProcess();FitCalibrationGenerationService::Request request;request.workspace=m_workspace;
    for(const auto& option:FitCalibrationFamilyCatalog::availableFamilies())if(option.family==family)request.family=option.identity;
    request.libraryRoot=UserSettings::instance().ldrawLibraryPath();
    QVector<FitCalibrationCapability> choices;
    QStringList labels;
    for(const auto& capability:FitCalibrationCapabilities::entries())if(capability.family==request.family){
        auto probe=request;probe.variant=capability.variant;
        const auto reason=FitCalibrationGenerationService::unavailableReason(probe);
        if(reason.isEmpty()){choices.push_back(capability);labels.push_back(capability.variantName.isEmpty()?capability.name:capability.variantName);}
    }
    if(choices.isEmpty()){
        QMessageBox::information(this,"Calibration unavailable",FitCalibrationGenerationService::unavailableReason(request));return;
    }
    int selected=0;bool accepted=false;
    if(choices.size()>1){
        const auto choice=QInputDialog::getItem(this,"Calibration","Variant:",labels,0,false,&accepted);
        if(!accepted)return;selected=labels.indexOf(choice);
    }
    const auto capability=choices[selected];request.variant=capability.variant;
    request.orientation=capability.orientations.front();
    if(capability.orientations.size()>1){
        QStringList orientations;for(const auto o:capability.orientations)orientations.push_back(o==FitPrintedOrientation::FeatureAxisParallelToBuildPlate?"Parallel to build plate":"Perpendicular to build plate");
        const auto choice=QInputDialog::getItem(this,"Calibration","Intended fixture orientation:",orientations,0,false,&accepted);
        if(!accepted)return;request.orientation=capability.orientations[orientations.indexOf(choice)];
    }
    runGeneration(request);
}

void FitCalibrationDialog::generateFineSearch()
{
    if(m_dirty&&!saveSession())return;
    if(m_tabs->currentIndex()==0&&m_session.hasFineExperiment){
        QMessageBox::information(this,"Calibration","Continue from Fine Search / Verification to retain existing later-stage evidence.");return;
    }
    readProcess();const auto* source=activeExperiment();
    if(!source||!FitCalibrationEvidencePolicy::continuationAvailable(*source))return;
    FitCalibrationGenerationService::Request request;request.hasParent=true;request.parent=m_session;
    request.workspace=m_workspace;request.stage=FitCalibrationGenerationService::Stage::FineSearch;
    const auto plan=FitCalibrationEvidencePolicy::nextSearchPlan(*source);
    if(plan.boundary==FitPreferredBoundary::None){
        QStringList stages;for(const auto& stage:FitCalibrationCapabilities::find(source->featureFamily).stages)
            if(stage=="Direct Verification"||stage=="Fine Search")stages.push_back(stage);
        bool accepted=false;const auto choice=QInputDialog::getItem(this,"Continue Calibration","Next step:",
            stages,stages.indexOf("Direct Verification"),false,&accepted);
        if(!accepted)return;
        if(choice.startsWith("Direct"))request.stage=FitCalibrationGenerationService::Stage::Verification;
    }
    if(request.stage==FitCalibrationGenerationService::Stage::FineSearch){
        bool accepted=false;
        request.candidateSpacing=QInputDialog::getDouble(this,"Continue Calibration","Candidate spacing (mm):",
            plan.candidateSpacingMillimetres,FitCalibrationCapabilities::find(source->featureFamily).minimumSpacing,FitCalibrationCapabilities::find(source->featureFamily).maximumSpacing,3,&accepted);if(!accepted)return;
        request.candidateCount=QInputDialog::getInt(this,"Continue Calibration","Odd candidate count:",
            std::min(plan.candidateCount,FitCalibrationCapabilities::find(source->featureFamily).maximumCandidates),3,FitCalibrationCapabilities::find(source->featureFamily).maximumCandidates,2,&accepted);if(!accepted)return;
    }
    runGeneration(request);
}

void FitCalibrationDialog::runGeneration(FitCalibrationGenerationService::Request request)
{
    request.libraryRoot=UserSettings::instance().ldrawLibraryPath();
    const auto unavailable=FitCalibrationGenerationService::unavailableReason(request);
    if(!unavailable.isEmpty()){QMessageBox::information(this,"Calibration unavailable",unavailable);return;}
    const QString managedRoot=m_library.storageRoot();
    QFutureWatcher<FitCalibrationGenerationService::Result> watcher;
    const auto progressText=[this](const QString& stage){return stage+tr("\n\nComplex calibration fixtures can take several minutes.\nPlease keep BrickSuite open until generation completes.");};
    QProgressDialog progress(progressText(tr("Generating and publishing calibration package...")),QString(),0,0,this);
    progress.setWindowModality(Qt::ApplicationModal);progress.setMinimumDuration(0);
    connect(&watcher,&QFutureWatcher<FitCalibrationGenerationService::Result>::finished,&progress,&QProgressDialog::close);
    const FitCalibrationGenerationService::Observer status=[&progress,progressText](FitCalibrationGenerationService::Checkpoint checkpoint,const QString&,QString*){
        QString stage;
        switch(checkpoint){
        case FitCalibrationGenerationService::Checkpoint::GeometryWritten:stage=QObject::tr("Writing calibration companion...");break;
        case FitCalibrationGenerationService::Checkpoint::BeforeReopen:stage=QObject::tr("Verifying calibration package...");break;
        case FitCalibrationGenerationService::Checkpoint::Published:stage=QObject::tr("Registering calibration session...");break;
        case FitCalibrationGenerationService::Checkpoint::Registered:return true;
        }
        QMetaObject::invokeMethod(&progress,[&progress,progressText,stage]{progress.setLabelText(progressText(stage));},Qt::QueuedConnection);
        return true;
    };
    watcher.setFuture(QtConcurrent::run([request,managedRoot,status]{return FitCalibrationGenerationService({},managedRoot,status).generate(request);}));
    progress.exec();watcher.waitForFinished();showGenerationResult(watcher.result());
}

void FitCalibrationDialog::recoverPackage()
{
    if(m_dirty&&!saveSession())return;
    const auto path=QFileDialog::getOpenFileName(this,"Recover Published Calibration Package",
        FitCalibrationArtifactLocation::directory(),"Package record (publication.json);;Calibration companion (*-session.json)");
    if(path.isEmpty())return;
    const QString directory=QFileInfo(path).absolutePath(),managedRoot=m_library.storageRoot();
    if(FitCalibrationGenerationService::isPackageCompanion(QDir(directory).filePath("package-session.json"))){runUnifiedPackage({},directory);return;}
    QFutureWatcher<FitCalibrationGenerationService::Result> watcher;
    QProgressDialog progress("Checking package and registering managed session...",QString(),0,0,this);
    progress.setWindowModality(Qt::ApplicationModal);progress.setMinimumDuration(0);
    connect(&watcher,&QFutureWatcher<FitCalibrationGenerationService::Result>::finished,&progress,&QProgressDialog::close);
    watcher.setFuture(QtConcurrent::run([directory,managedRoot]{return FitCalibrationGenerationService({},managedRoot).recover(directory);}));
    progress.exec();watcher.waitForFinished();showGenerationResult(watcher.result());
}

void FitCalibrationDialog::showGenerationResult(const FitCalibrationGenerationService::Result& result)
{
    if(!result.ok()){
        QMessageBox::warning(this,"Calibration",result.diagnostic+(result.published?
            tr("\nThe complete package is saved at %1. Use Recover Package to retry managed registration; do not regenerate it.").arg(result.directory):QString()));return;
    }
    auto workspace=m_workspace;workspace.identity=FitCalibrationLibrary::manufacturingContextFingerprint(result.session.process);
    workspace.process=result.session.process;workspace.displayName=FitCalibrationLibrary::workspaceDisplayName(workspace.process);
    if(!m_workspace.identity.isEmpty()&&m_workspace.identity!=workspace.identity)workspace.featureSessions.clear();
    bool found=false;for(auto& session:workspace.featureSessions){
        const auto* previous=session.hasFineExperiment?&session.fineExperiment:session.hasCoarseExperiment?&session.coarseExperiment:nullptr;
        if(session.sessionIdentity==result.session.sessionIdentity||
           (result.session.hasFineExperiment&&previous&&previous->artifactIdentity==result.session.coarseExperiment.artifactIdentity)){
            // Replace only the displayed stage; the parent's managed record is retained.
            session=result.session;found=true;break;
        }
    }
    if(!found)workspace.featureSessions.push_back(result.session);
    showWorkspace(workspace,result.session.sessionIdentity);
    SessionFileDialogDirectoryService::instance().rememberSelectedFile(FileDialogDirectoryCategory::OpenImport,result.companionPath);
    const auto& experiment=result.session.hasFineExperiment?result.session.fineExperiment:result.session.coarseExperiment;
    QMessageBox::information(this,"Calibration Package Ready",tr("Printable fixture:\n%1\n\nManaged companion:\n%2\n\nPrint at 100% scale. %3\nRecord new physical observations before verification.")
        .arg(result.fixturePath,result.companionPath,experiment.process.orientationNotes));
}

void FitCalibrationDialog::createUnifiedPackage(bool recommendedPackage)
{
    if(m_dirty&&!saveSession())return;
    if(m_workspace.identity.isEmpty()){QMessageBox::information(this,"Calibration","Create or select a manufacturing workspace first.");return;}
    readProcess();const auto libraryRoot=UserSettings::instance().ldrawLibraryPath();
    FitCalibrationGenerationService::PackageRequest request;
    const auto describe=[](const FitCalibrationGenerationService::PackageRequest& r){
        const auto plan=FitCalibrationGenerationService::planPackage(r);
        if(!plan.ok())return plan.diagnostic;
        QString text=QString("%1 calibrations selected\n%2 printable fixtures will be generated\n").arg(r.selections.size()).arg(plan.fixtures.size());
        for(const auto& fixture:plan.fixtures){text+="\n"+fixture.name+(fixture.combinedCore?" — shared four-zone fixture":" — separate fixture");
            for(int index:fixture.selections){const auto& selection=r.selections[index];const auto c=FitCalibrationCapabilities::find(selection.family,selection.variant);
                text+="\n  "+c.name+(c.variantName.isEmpty()?QString():" — "+c.variantName);}}
        return text;
    };
    if(recommendedPackage){
        request=FitCalibrationGenerationService::recommended(m_workspace,libraryRoot);
        const auto plan=FitCalibrationGenerationService::planPackage(request);
        if(!plan.ok()){QMessageBox::information(this,"Calibration unavailable",plan.diagnostic);return;}
        if(QMessageBox::question(this,"Recommended Calibration Package",describe(request)+"\n\nGenerate this package?")!=QMessageBox::Yes)return;
    }else{
        QDialog selection(this);selection.setWindowTitle("Custom Calibration Package");selection.resize(850,650);
        auto* layout=new QVBoxLayout(&selection);auto* table=new QTableWidget(&selection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setColumnCount(4);table->setHorizontalHeaderLabels({"Calibration","Variant","Print orientation","Availability"});
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);layout->addWidget(table);
        QVector<FitCalibrationGenerationService::Request> choices;
        for(const auto& c:FitCalibrationCapabilities::entries())if(c.initial)for(const auto orientation:c.orientations){
            FitCalibrationGenerationService::Request r;r.family=c.family;r.variant=c.variant;r.orientation=orientation;r.workspace=m_workspace;r.libraryRoot=libraryRoot;
            const auto unavailable=FitCalibrationGenerationService::unavailableReason(r);const int row=table->rowCount();table->insertRow(row);choices.push_back(r);
            auto* item=new QTableWidgetItem(c.name);item->setFlags(Qt::ItemIsUserCheckable|(unavailable.isEmpty()?Qt::ItemIsEnabled:Qt::NoItemFlags));item->setCheckState(Qt::Unchecked);table->setItem(row,0,item);
            table->setItem(row,1,new QTableWidgetItem(c.variantName));table->setItem(row,2,new QTableWidgetItem(orientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate?"Parallel":"Perpendicular"));
            table->setItem(row,3,new QTableWidgetItem(unavailable.isEmpty()?"Available":unavailable));
        }
        auto* summary=new QPlainTextEdit(&selection);summary->setReadOnly(true);summary->setMaximumHeight(190);layout->addWidget(summary);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&selection);buttons->button(QDialogButtonBox::Ok)->setText("Generate Package");layout->addWidget(buttons);
        const auto update=[&]{request.selections.clear();for(int row=0;row<table->rowCount();++row)if(table->item(row,0)->checkState()==Qt::Checked)request.selections.push_back(choices[row]);
            summary->setPlainText(describe(request));buttons->button(QDialogButtonBox::Ok)->setEnabled(FitCalibrationGenerationService::planPackage(request).ok());};
        connect(table,&QTableWidget::itemChanged,&selection,[&]{update();});connect(buttons,&QDialogButtonBox::accepted,&selection,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&selection,&QDialog::reject);
        update();if(selection.exec()!=QDialog::Accepted)return;
    }
    runUnifiedPackage(request);
}

void FitCalibrationDialog::runUnifiedPackage(const FitCalibrationGenerationService::PackageRequest& request,const QString& recoveryDirectory)
{
    const auto text=[](const QString& stage){return stage+QString("\n\nComplex calibration fixtures can take several minutes.\nPlease keep BrickSuite open until generation completes.");};
    QProgressDialog progress(text("Planning calibration package..."),QString(),0,0,this);progress.setWindowModality(Qt::ApplicationModal);progress.setMinimumDuration(0);
    QFutureWatcher<FitCalibrationGenerationService::PackageResult> watcher;
    const auto report=[&progress,text](const QString& stage){QMetaObject::invokeMethod(&progress,[&progress,text,stage]{progress.setLabelText(text(stage));},Qt::QueuedConnection);};
    connect(&watcher,&QFutureWatcher<FitCalibrationGenerationService::PackageResult>::finished,&progress,&QProgressDialog::close);
    const auto root=m_library.storageRoot();watcher.setFuture(QtConcurrent::run([request,recoveryDirectory,root,report]{
        FitCalibrationGenerationService service({},root);return recoveryDirectory.isEmpty()?service.generatePackage(request,report):service.recoverPackage(recoveryDirectory,report);}));
    progress.exec();watcher.waitForFinished();const auto result=watcher.result();
    if(!result.ok()){QMessageBox::warning(this,"Calibration Package",result.diagnostic+(result.published?
        QString("\n%1 sessions registered. The complete package is at %2. Use Recover Package to retry safely.").arg(result.registeredCount).arg(result.directory):QString()));return;}
    refreshLibrary();const auto context=FitCalibrationLibrary::manufacturingContextFingerprint(result.sessions.front().process);
    for(const auto& workspace:m_library.workspaces())if(workspace.identity==context){showWorkspace(workspace,result.sessions.front().sessionIdentity);break;}
    SessionFileDialogDirectoryService::instance().rememberSelectedFile(FileDialogDirectoryCategory::OpenImport,result.companionPath);
    QMessageBox::information(this,"Calibration Package Ready",QString("%1 independent calibrations registered.\n\nPrintable fixtures:\n%2\n\nPackage companion:\n%3\n\nPrint at 100% scale. Select a feature calibration to record its own physical observations; siblings remain independent.")
        .arg(result.sessions.size()).arg(result.fixturePaths.join("\n"),result.companionPath));
}
