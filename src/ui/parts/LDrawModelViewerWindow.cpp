#include "LDrawModelViewerWindow.h"

#include "LDrawViewportWidget.h"
#include "../common/SessionFileDialogDirectoryService.h"
#include "../help/HelpManager.h"
#include "../helpers/ColorComboHelper.h"
#include "../../repositories/ColorRepository.h"
#include "../../services/geometry/LDrawLibraryService.h"
#include "../../services/geometry/LDrawColorResolver.h"
#include "../../services/geometry/LDrawObjWriter.h"
#include "../../settings/UserSettings.h"

#include <QtConcurrentRun>
#include <QCloseEvent>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

LDrawModelViewerWindow::LDrawModelViewerWindow(QWidget* parent):QDialog(parent)
{
    setWindowTitle(tr("3D Model Viewer"));setModal(false);resize(1050,760);
    HelpManager::setContextTopic(this,HelpTopic::LDrawModels);
    const QByteArray geometry=UserSettings::instance().ldrawModelViewerGeometry();if(!geometry.isEmpty())restoreGeometry(geometry);
    auto*root=new QVBoxLayout(this);auto*toolbar=new QHBoxLayout;
    toolbar->addWidget(new QLabel(tr("LDraw model:"),this));m_candidate=new QComboBox(this);toolbar->addWidget(m_candidate);
    m_reload=new QPushButton(tr("Reload Model"),this);toolbar->addWidget(m_reload);
    toolbar->addWidget(new QLabel(tr("Projection:"),this));m_projection=new QComboBox(this);m_projection->addItems({tr("Perspective"),tr("Orthographic")});toolbar->addWidget(m_projection);
    toolbar->addWidget(new QLabel(tr("Render:"),this));m_renderMode=new QComboBox(this);m_renderMode->setObjectName(QStringLiteral("ldrawRenderModeCombo"));
    const QStringList renderLabels=partViewerRenderModeLabels();
    m_renderMode->addItems(renderLabels);
    m_renderMode->setItemData(0,int(PartViewerRenderMode::Solid));
    m_renderMode->setItemData(1,int(PartViewerRenderMode::SolidEdges));
    m_renderMode->setItemData(2,int(PartViewerRenderMode::Wireframe));
    m_renderMode->setCurrentIndex(m_renderMode->findData(int(PartViewerRenderMode::SolidEdges)));m_renderMode->setMinimumContentsLength(14);m_renderMode->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);toolbar->addWidget(m_renderMode);
    m_standardView=new QComboBox(this);m_standardView->addItems({tr("Isometric"),tr("Front"),tr("Back"),tr("Left"),tr("Right"),tr("Top"),tr("Bottom")});toolbar->addWidget(m_standardView);
    m_fit=new QPushButton(tr("Fit"),this);toolbar->addWidget(m_fit);m_resetView=new QPushButton(tr("Reset View"),this);toolbar->addWidget(m_resetView);
    auto*showAxes=new QCheckBox(tr("Show Axes"),this);showAxes->setChecked(true);toolbar->addWidget(showAxes);toolbar->addStretch();root->addLayout(toolbar);
    auto*colorRow=new QHBoxLayout;colorRow->addWidget(new QLabel(tr("Model Color:"),this));m_modelColor=new QComboBox(this);m_modelColor->setMinimumContentsLength(20);m_modelColor->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    int defaultIndex=-1;for(const auto&color:ColorRepository().getAll()){const int index=ColorComboHelper::addColorItem(m_modelColor,color.name(),color.rebrickableId(),color.rgb(),true);m_modelColor->setItemData(index,color.rgb(),Qt::UserRole+1);if(color.name().compare(QStringLiteral("Light Bluish Gray"),Qt::CaseInsensitive)==0)defaultIndex=index;}
    if(defaultIndex>=0)m_modelColor->setCurrentIndex(defaultIndex);else{const QColor fallback=LDrawColorResolver::defaultModelColor();m_modelColor->addItem(tr("Neutral Gray"),0);m_modelColor->setItemData(0,fallback.name(),Qt::UserRole+1);}
    colorRow->addWidget(m_modelColor);colorRow->addStretch();root->addLayout(colorRow);
    m_viewport=new LDrawViewportWidget(this);root->addWidget(m_viewport,1);
    auto*info=new QFormLayout;m_part=new QLabel(this);m_source=new QLabel(this);m_source->setTextInteractionFlags(Qt::TextSelectableByMouse);m_dimensions=new QLabel(this);m_counts=new QLabel(this);m_bfc=new QLabel(this);m_status=new QLabel(this);m_status->setWordWrap(true);
    info->addRow(tr("Part:"),m_part);info->addRow(tr("Source:"),m_source);info->addRow(tr("Dimensions:"),m_dimensions);info->addRow(tr("Geometry:"),m_counts);info->addRow(tr("BFC:"),m_bfc);info->addRow(tr("Status:"),m_status);info->addRow(tr("Printability:"),new QLabel(tr("Mesh repair / printability validation: Not performed"),this));root->addLayout(info);
    auto*actions=new QHBoxLayout;actions->addWidget(new QLabel(tr("Scale:"),this));m_scale=new QDoubleSpinBox(this);m_scale->setRange(1.0,1000.0);m_scale->setDecimals(2);m_scale->setSingleStep(0.5);m_scale->setSuffix(tr(" %"));m_scale->setValue(100.0);actions->addWidget(m_scale);auto*reset=new QPushButton(tr("Reset"),this);actions->addWidget(reset);actions->addStretch();m_export=new QPushButton(tr("Export OBJ..."),this);actions->addWidget(m_export);auto*buttons=new QDialogButtonBox(QDialogButtonBox::Help|QDialogButtonBox::Close,this);actions->addWidget(buttons);root->addLayout(actions);
    connect(m_candidate,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){if(m_candidate->currentIndex()<0)return;startLoad(LoadBehavior::ResetView);});
    connect(m_reload,&QPushButton::clicked,this,[this]{startLoad(LoadBehavior::PreserveView);});
    connect(m_projection,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int index){m_viewport->setProjection(index==0?PartViewerCamera::Projection::Perspective:PartViewerCamera::Projection::Orthographic);});
    connect(m_renderMode,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){m_viewport->setRenderMode(static_cast<PartViewerRenderMode>(m_renderMode->currentData().toInt()));});
    connect(m_standardView,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int index){m_viewport->setStandardView(static_cast<PartViewerCamera::View>(index));});
    connect(m_fit,&QPushButton::clicked,m_viewport,&LDrawViewportWidget::fitModel);
    connect(m_resetView,&QPushButton::clicked,m_viewport,&LDrawViewportWidget::resetView);
    connect(showAxes,&QCheckBox::toggled,m_viewport,&LDrawViewportWidget::setShowAxes);
    connect(m_modelColor,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int index){const QColor color(QStringLiteral("#")+m_modelColor->itemData(index,Qt::UserRole+1).toString().remove(QLatin1Char('#')));if(color.isValid())m_viewport->setModelColor(color);});
    connect(m_scale,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this](double value){m_state.setScalePercent(value);m_viewport->setUniformScale(float(value/100.0));updateDimensions();});
    connect(reset,&QPushButton::clicked,this,[this]{m_scale->setValue(100.0);});connect(m_export,&QPushButton::clicked,this,&LDrawModelViewerWindow::exportObj);
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::close);connect(buttons,&QDialogButtonBox::helpRequested,this,[this]{HelpManager::showTopic(HelpTopic::LDrawModels,this);});
    connect(m_viewport,&LDrawViewportWidget::renderingError,this,[this](const QString&message){m_renderingError=message;m_status->setText(message);m_projection->setEnabled(false);m_renderMode->setEnabled(false);m_standardView->setEnabled(false);m_fit->setEnabled(false);m_resetView->setEnabled(false);});
    connect(this,&QDialog::finished,this,[this](int){saveWindowGeometry();});
}

void LDrawModelViewerWindow::showPart(const LDrawModelViewerRequest& request)
{
    m_request=request;m_part->setText(QStringLiteral("%1 — %2").arg(request.partNumber,request.partName));
    int colorIndex=-1;if(request.initialRebrickableColorId)colorIndex=m_modelColor->findData(*request.initialRebrickableColorId);
    if(colorIndex<0){for(int i=0;i<m_modelColor->count();++i)if(m_modelColor->itemText(i).compare(QStringLiteral("Light Bluish Gray"),Qt::CaseInsensitive)==0){colorIndex=i;break;}}
    if(colorIndex<0)colorIndex=0;m_modelColor->setCurrentIndex(colorIndex);
    const QColor selected(QStringLiteral("#")+m_modelColor->itemData(colorIndex,Qt::UserRole+1).toString().remove(QLatin1Char('#')));if(selected.isValid())m_viewport->setModelColor(selected);
    {QSignalBlocker blocker(m_candidate);m_candidate->clear();m_candidate->addItems(request.candidates);m_candidate->setCurrentIndex(request.candidates.isEmpty()?-1:0);}
    m_candidate->setVisible(request.candidates.size()>1);startLoad(LoadBehavior::ResetView);
}

void LDrawModelViewerWindow::startLoad(LoadBehavior behavior)
{
    const QString id=m_candidate->currentText().trimmed();if(id.isEmpty()){m_status->setText(tr("No authoritative LDraw identity is available."));return;}
    const quint64 generation=behavior==LoadBehavior::PreserveView?m_state.beginReload():m_state.beginNewModelLoad();
    if(behavior==LoadBehavior::ResetView){
        QSignalBlocker scaleBlocker(m_scale);m_scale->setValue(100.0);m_viewport->setUniformScale(1.0f);
        QSignalBlocker projectionBlocker(m_projection);m_projection->setCurrentIndex(0);
    }
    m_status->setText(tr("Loading 3D model..."));m_export->setEnabled(false);m_reload->setEnabled(false);
    const QString root=UserSettings::instance().ldrawLibraryPath();auto*watcher=new QFutureWatcher<LDrawGeometry::Result>(this);QPointer<LDrawModelViewerWindow> self(this);
    connect(watcher,&QFutureWatcher<LDrawGeometry::Result>::finished,this,[self,watcher,generation,behavior]{const auto result=watcher->result();watcher->deleteLater();if(!self||!self->m_state.accepts(generation))return;self->m_reload->setEnabled(true);self->applyResult(result,behavior);});
    watcher->setFuture(QtConcurrent::run([root,id]{return LDrawLibraryService::loadPart(root,id);}));
}

void LDrawModelViewerWindow::applyResult(const LDrawGeometry::Result&result,LoadBehavior behavior)
{
    if(!result.ok()){m_status->setText(result.error.message);m_source->setText(result.error.reference);return;}
    m_mesh=result.mesh;m_status->setText(m_renderingError.isEmpty()?tr("Geometry loaded successfully."):m_renderingError);m_source->setText(QStringLiteral("%1 (%2)").arg(m_mesh.sourceRelativePath,m_mesh.sourceProvenance));
    m_counts->setText(tr("%1 triangles, %2 hard edges, %3 conditional edges; %4 degenerate faces omitted").arg(m_mesh.triangles.size()).arg(m_mesh.hardEdges.size()).arg(m_mesh.conditionalEdges.size()).arg(m_mesh.degenerateFaces));
    m_bfc->setText(m_mesh.bfcCertified?tr("Certified source geometry encountered."):tr("No BFC certification was found in the loaded source."));
    m_viewport->setMesh(m_mesh,behavior==LoadBehavior::ResetView);m_export->setEnabled(!m_mesh.triangles.isEmpty());updateDimensions();
}

void LDrawModelViewerWindow::updateDimensions()
{
    if(!m_mesh.hasBounds){m_dimensions->clear();return;}const QVector3D original=m_mesh.dimensionsMm();const QVector3D scaled=original*float(m_state.scalePercent()/100.0);
    const auto text=[](const QVector3D&v){return QStringLiteral("%1 × %2 × %3 mm").arg(v.x(),0,'f',2).arg(v.y(),0,'f',2).arg(v.z(),0,'f',2);};
    m_dimensions->setText(qFuzzyCompare(m_state.scalePercent(),100.0)?text(original):tr("Original: %1    Scaled: %2").arg(text(original),text(scaled)));
}

void LDrawModelViewerWindow::exportObj()
{
    auto&directories=SessionFileDialogDirectoryService::instance();const QString id=m_candidate->currentText().trimmed();const QString path=QFileDialog::getSaveFileName(this,tr("Export Engineering OBJ"),directories.initialFilePath(FileDialogDirectoryCategory::SaveExport,id+QStringLiteral(".obj")),tr("Wavefront OBJ (*.obj)"));if(path.isEmpty())return;directories.rememberSelectedFile(FileDialogDirectoryCategory::SaveExport,path);
    LDrawObjWriter::Options options;options.uniformScale=m_state.scalePercent()/100.0;options.partNumber=m_request.partNumber;LDrawGeometry::Error error;
    if(!LDrawObjWriter::write(m_mesh,path,options,&error))QMessageBox::critical(this,tr("Export OBJ"),error.message);else QMessageBox::information(this,tr("Export OBJ"),tr("The engineering OBJ was exported at %1% scale.\n\nMesh repair / printability validation has not been performed. Review the exported model in your slicer before printing.").arg(m_state.scalePercent(),0,'f',2));
}

void LDrawModelViewerWindow::saveWindowGeometry(){UserSettings::instance().setLDrawModelViewerGeometry(saveGeometry());}
void LDrawModelViewerWindow::closeEvent(QCloseEvent*event){saveWindowGeometry();QDialog::closeEvent(event);}
