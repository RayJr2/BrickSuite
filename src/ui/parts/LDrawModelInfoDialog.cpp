#include "LDrawModelInfoDialog.h"

#include "../help/HelpManager.h"
#include "../../settings/UserSettings.h"
#include "../../services/geometry/LDrawLibraryService.h"
#include "../../services/geometry/LDrawObjWriter.h"

#include <QtConcurrentRun>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

LDrawModelInfoDialog::LDrawModelInfoDialog(const QString& ldrawId, QWidget* parent)
    : QDialog(parent), m_ldrawId(ldrawId)
{
    setWindowTitle(tr("3D Model Information"));
    resize(620, 360);
    HelpManager::setContextTopic(this, HelpTopic::LDrawModels);
    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    form->addRow(tr("LDraw identity:"), new QLabel(m_ldrawId, this));
    m_source = new QLabel(this); m_source->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_status = new QLabel(this); m_status->setWordWrap(true);
    m_dimensions = new QLabel(this);
    m_counts = new QLabel(this);
    m_bfc = new QLabel(this); m_bfc->setWordWrap(true);
    form->addRow(tr("Source:"), m_source);
    form->addRow(tr("Validation:"), m_status);
    form->addRow(tr("Dimensions:"), m_dimensions);
    form->addRow(tr("Geometry:"), m_counts);
    form->addRow(tr("BFC:"), m_bfc);
    form->addRow(tr("Printability:"), new QLabel(tr("Not performed"), this));
    root->addLayout(form);
    root->addStretch();
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Help | QDialogButtonBox::Close, this);
    m_reload = buttons->addButton(tr("Reload"), QDialogButtonBox::ActionRole);
    m_export = buttons->addButton(tr("Export OBJ..."), QDialogButtonBox::ActionRole);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::helpRequested, this, [this]{ HelpManager::showTopic(HelpTopic::LDrawModels, this); });
    connect(m_reload, &QPushButton::clicked, this, &LDrawModelInfoDialog::startLoad);
    connect(m_export, &QPushButton::clicked, this, &LDrawModelInfoDialog::exportObj);
    startLoad();
}

void LDrawModelInfoDialog::startLoad()
{
    if (m_watcher) return;
    m_status->setText(tr("Loading model geometry..."));
    m_source->clear(); m_dimensions->clear(); m_counts->clear(); m_bfc->clear();
    m_reload->setEnabled(false); m_export->setEnabled(false);
    const QString root = UserSettings::instance().ldrawLibraryPath();
    const QString id = m_ldrawId;
    m_watcher = new QFutureWatcher<LDrawGeometry::Result>(this);
    connect(m_watcher, &QFutureWatcher<LDrawGeometry::Result>::finished, this, [this] {
        const auto value = m_watcher->result();
        m_watcher->deleteLater(); m_watcher = nullptr;
        m_reload->setEnabled(true); applyResult(value);
    });
    m_watcher->setFuture(QtConcurrent::run([root, id] { return LDrawLibraryService::loadPart(root, id); }));
}

void LDrawModelInfoDialog::applyResult(const LDrawGeometry::Result& result)
{
    if (!result.ok()) {
        m_status->setText(result.error.message);
        m_source->setText(result.error.reference);
        return;
    }
    m_mesh = result.mesh;
    m_status->setText(tr("Geometry loaded successfully."));
    m_source->setText(m_mesh.sourceRelativePath);
    const auto ldu=m_mesh.dimensionsLdu(), mm=m_mesh.dimensionsMm();
    m_dimensions->setText(tr("%1 × %2 × %3 LDU  (%4 × %5 × %6 mm)")
        .arg(ldu.x(),0,'f',2).arg(ldu.y(),0,'f',2).arg(ldu.z(),0,'f',2)
        .arg(mm.x(),0,'f',2).arg(mm.y(),0,'f',2).arg(mm.z(),0,'f',2));
    m_counts->setText(tr("%1 triangles, %2 hard edges, %3 conditional edges; %4 degenerate faces omitted")
        .arg(m_mesh.triangles.size()).arg(m_mesh.hardEdges.size())
        .arg(m_mesh.conditionalEdges.size()).arg(m_mesh.degenerateFaces));
    m_bfc->setText(m_mesh.bfcCertified ? tr("Certified source geometry encountered.")
                                        : tr("No BFC certification was found in the loaded source."));
    m_export->setEnabled(!m_mesh.triangles.isEmpty());
}

void LDrawModelInfoDialog::exportObj()
{
    const QString path=QFileDialog::getSaveFileName(this,tr("Export Engineering OBJ"),
        m_ldrawId+QStringLiteral(".obj"),tr("Wavefront OBJ (*.obj)"));
    if(path.isEmpty()) return;
    LDrawGeometry::Error error;
    if(!LDrawObjWriter::write(m_mesh,path,&error)) QMessageBox::critical(this,tr("Export OBJ"),error.message);
    else QMessageBox::information(this,tr("Export OBJ"),tr("The engineering OBJ was exported successfully."));
}
