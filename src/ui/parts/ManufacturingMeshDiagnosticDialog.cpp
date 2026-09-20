#include "ManufacturingMeshDiagnosticDialog.h"

#include "../common/SessionFileDialogDirectoryService.h"
#include "../../services/geometry/fit/FitCalibrationLibrary.h"
#include "../../services/geometry/print/ManufacturingMeshDiagnosticExporter.h"
#include "../../services/geometry/print/ManufacturingMeshService.h"

#include <QtConcurrentRun>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>

using namespace PrintGeometry;

namespace {
constexpr auto ProofOrientation = FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;

const FitProfileCorrection* proofCorrection(const FitProfile& profile)
{
    const auto found = std::find_if(profile.corrections.cbegin(), profile.corrections.cend(), [](const auto& correction) {
        return correction.featureFamily == QStringLiteral("RoundTechnicPassage")
            && correction.featureRole == QStringLiteral("female")
            && correction.printedOrientation == QStringLiteral("feature-axis-perpendicular-to-build-plate");
    });
    return found == profile.corrections.cend() ? nullptr : &*found;
}

QString processDescription(const FitProfile& profile)
{
    const auto& process = profile.process;
    QStringList parts{process.printerIdentity, process.materialIdentity};
    if (process.hasNozzleDiameter)
        parts << QStringLiteral("%1 mm nozzle").arg(process.nozzleDiameterMillimetres, 0, 'f', 2);
    parts << process.profileName << QStringLiteral("feature axis perpendicular to build plate");
    parts.removeAll(QString());
    return parts.join(QStringLiteral(" · "));
}

QString signedMillimetres(double value)
{
    return QStringLiteral("%1%2").arg(value >= 0.0 ? QStringLiteral("+") : QString()).arg(value, 0, 'f', 3);
}
}

ManufacturingMeshDiagnosticDialog::ManufacturingMeshDiagnosticDialog(
    const LDrawGeometry::LDrawLoadResult& source, const PreparedMesh& prepared,
    double uniformScale, const QColor& modelColor, QWidget* parent)
    : QDialog(parent), m_source(source), m_prepared(prepared),
      m_uniformScale(uniformScale), m_modelColor(modelColor)
{
    setWindowTitle(tr("ManufacturingMesh Proof — Part 3700"));
    setModal(true);
    resize(680, 390);

    auto* root = new QVBoxLayout(this);
    auto* explanation = new QLabel(tr("Diagnostic proof only. Select a managed Verified Fit Profile to derive a separate ManufacturingMesh from the nominal Prepared Mesh."), this);
    explanation->setWordWrap(true);
    root->addWidget(explanation);

    auto* form = new QFormLayout;
    form->addRow(tr("Part:"), new QLabel(QStringLiteral("3700 — Technic Brick 1 x 2 with Hole"), this));
    form->addRow(tr("Nominal geometry:"), new QLabel(tr("Prepared Mesh (unchanged)"), this));
    m_profiles = new QComboBox(this);
    m_profiles->setMinimumContentsLength(38);
    form->addRow(tr("Verified Fit Profile:"), m_profiles);
    m_profileDetails = new QLabel(this);
    m_profileDetails->setWordWrap(true);
    m_profileDetails->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("Profile / correction:"), m_profileDetails);
    m_result = new QLabel(tr("Select a compatible profile, then generate the diagnostic ManufacturingMesh."), this);
    m_result->setWordWrap(true);
    m_result->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("Result:"), m_result);
    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_generate = new QPushButton(tr("Generate ManufacturingMesh"), this);
    m_export = new QPushButton(tr("Export ManufacturingMesh 3MF..."), this);
    m_export->setEnabled(false);
    buttons->addButton(m_generate, QDialogButtonBox::ActionRole);
    buttons->addButton(m_export, QDialogButtonBox::ActionRole);
    root->addWidget(buttons);

    connect(m_profiles, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { updateProfileDetails(); });
    connect(m_generate, &QPushButton::clicked, this, &ManufacturingMeshDiagnosticDialog::generate);
    connect(m_export, &QPushButton::clicked, this, &ManufacturingMeshDiagnosticDialog::exportMesh);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    populateProfiles();
}

void ManufacturingMeshDiagnosticDialog::populateProfiles()
{
    m_profiles->clear();
    m_profiles->addItem(tr("Select a compatible Verified Fit Profile..."), QString());
    FitCalibrationLibrary library;
    QVector<FitLibraryIssue> issues;
    for (const auto& summary : library.profiles(&issues)) {
        if (!summary.compatible)
            continue;
        FitProfile profile;
        QString error;
        if (!library.loadProfile(summary.identity, &profile, &error) || !proofCorrection(profile))
            continue;
        m_profiles->addItem(summary.name, summary.identity);
    }
    if (m_profiles->count() == 1)
        m_profileDetails->setText(tr("No compatible managed Verified profile provides a female RoundTechnicPassage correction for the perpendicular print orientation."));
    updateProfileDetails();
}

void ManufacturingMeshDiagnosticDialog::updateProfileDetails()
{
    m_manufacturingMesh.reset();
    m_export->setEnabled(false);
    m_result->setText(tr("Select a compatible profile, then generate the diagnostic ManufacturingMesh."));
    const QString identity = m_profiles->currentData().toString();
    m_generate->setEnabled(!m_generating && !identity.isEmpty());
    if (identity.isEmpty()) {
        if (m_profiles->count() > 1)
            m_profileDetails->setText(tr("No profile selected. Selection is explicit and applies only to this proof."));
        return;
    }
    FitProfile profile;
    QString error;
    if (!FitCalibrationLibrary().loadProfile(identity, &profile, &error)) {
        m_profileDetails->setText(error);
        m_generate->setEnabled(false);
        return;
    }
    const auto* correction = proofCorrection(profile);
    if (!correction) {
        m_profileDetails->setText(tr("The selected profile no longer contains the required correction."));
        m_generate->setEnabled(false);
        return;
    }
    m_profileDetails->setText(QStringLiteral("%1\nProfile: %2 · Source session: %3\n%4 / %5 · correction: %6 mm diameter")
        .arg(processDescription(profile), profile.profileIdentity, profile.sourceSessionIdentity,
             correction->featureFamily, correction->featureRole, signedMillimetres(correction->valueMillimetres)));
}

void ManufacturingMeshDiagnosticDialog::generate()
{
    const QString identity = m_profiles->currentData().toString();
    if (identity.isEmpty()) {
        QMessageBox::warning(this, tr("ManufacturingMesh Proof"), tr("Select a compatible Verified Fit Profile."));
        return;
    }
    FitProfile profile;
    QString error;
    if (!FitCalibrationLibrary().loadProfile(identity, &profile, &error)) {
        QMessageBox::warning(this, tr("ManufacturingMesh Proof"), error);
        return;
    }
    m_generating = true;
    m_generate->setEnabled(false);
    m_profiles->setEnabled(false);
    m_export->setEnabled(false);
    m_result->setText(tr("Generating a separate ManufacturingMesh..."));
    const auto source = m_source;
    const auto prepared = m_prepared;
    auto* watcher = new QFutureWatcher<ManufacturingMeshResult>(this);
    QPointer<ManufacturingMeshDiagnosticDialog> self(this);
    connect(watcher, &QFutureWatcher<ManufacturingMeshResult>::finished, this, [self, watcher] {
        const auto generated = watcher->result();
        watcher->deleteLater();
        if (!self)
            return;
        self->m_generating = false;
        self->m_profiles->setEnabled(true);
        self->m_generate->setEnabled(!self->m_profiles->currentData().toString().isEmpty());
        if (!generated.ok() || !generated.manufacturingMesh) {
            self->m_manufacturingMesh.reset();
            self->m_export->setEnabled(false);
            self->m_result->setText(self->tr("Generation rejected: %1").arg(generated.diagnostic));
            return;
        }
        self->m_manufacturingMesh = generated.manufacturingMesh;
        self->showResult(*generated.manufacturingMesh);
        self->m_export->setEnabled(true);
    });
    watcher->setFuture(QtConcurrent::run([source, prepared, profile] {
        return ManufacturingMeshService().generate(source, prepared, profile, ProofOrientation);
    }));
}

void ManufacturingMeshDiagnosticDialog::showResult(const ManufacturingMesh& mesh)
{
    m_result->setText(QStringLiteral("ManufacturingMesh generated separately from Prepared Mesh.\n"
                                     "Feature: RoundTechnicPassage / female\n"
                                     "Nominal diameter: %1 mm · Applied correction: %2 mm diameter · Manufacturing diameter: %3 mm\n"
                                     "ManufacturingMesh: %4")
        .arg(mesh.nominalDiameterMillimetres, 0, 'f', 3)
        .arg(signedMillimetres(mesh.diameterCorrectionMillimetres))
        .arg(mesh.manufacturingDiameterMillimetres, 0, 'f', 3)
        .arg(mesh.identity));
}

void ManufacturingMeshDiagnosticDialog::exportMesh()
{
    if (!m_manufacturingMesh) {
        QMessageBox::warning(this, tr("Export ManufacturingMesh"), tr("Generate a valid ManufacturingMesh before exporting."));
        return;
    }
    auto& directories = SessionFileDialogDirectoryService::instance();
    QString path = QFileDialog::getSaveFileName(this, tr("Export Diagnostic ManufacturingMesh"),
        directories.initialFilePath(FileDialogDirectoryCategory::SaveExport, QStringLiteral("3700-ManufacturingMesh.3mf")),
        tr("3MF Model (*.3mf)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".3mf"), Qt::CaseInsensitive))
        path += QStringLiteral(".3mf");
    directories.rememberSelectedFile(FileDialogDirectoryCategory::SaveExport, path);
    QString error;
    if (!ManufacturingMeshDiagnosticExporter::writeThreeMf(*m_manufacturingMesh, path, m_uniformScale, m_modelColor, &error)) {
        QMessageBox::critical(this, tr("Export ManufacturingMesh"), error);
        return;
    }
    QMessageBox::information(this, tr("Export ManufacturingMesh"), tr("The diagnostic compensated ManufacturingMesh was exported successfully."));
}
