#include "../src/services/geometry/fit/FitCalibrationPackage.h"
#include "../src/services/geometry/fit/FitCalibrationLibrary.h"
#include "../src/services/geometry/fit/StandardStudCalibrationArtifact.h"
#include "../src/services/geometry/fit/StudReceivingCalibrationArtifact.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QTextStream>
#include <cmath>

using namespace PrintGeometry;
namespace {
bool require(bool value, const QString& message)
{
    if (!value) QTextStream(stderr) << "FAIL: " << message << Qt::endl;
    return value;
}
bool sameMesh(const PrintMesh& a, const PrintMesh& b)
{
    if (a.faces != b.faces || a.vertices.size() != b.vertices.size()) return false;
    for (std::size_t i = 0; i < a.vertices.size(); ++i)
        if (a.vertices[i].x != b.vertices[i].x || a.vertices[i].y != b.vertices[i].y ||
            a.vertices[i].z != b.vertices[i].z) return false;
    return true;
}
FitCalibrationSession session(const QString& identity, const FitCalibrationExperiment& experiment,
                              const FitCalibrationProcess& process)
{
    FitCalibrationSession result;
    result.sessionIdentity = identity;
    result.process = process;
    result.hasCoarseExperiment = true;
    result.coarseExperiment = experiment;
    result.coarseExperiment.process = process;
    return result;
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    QString error;
    QVector<FitCandidateValue> values;
    ok &= require(FitCandidateSeries::generate("series-v1", .2, .05, 7, &values, &error), "seven-candidate series: " + error);
    ok &= require(values.size() == 7 && values.front().index == 1 &&
                  values.front().identity == "series-v1:candidate-1" &&
                  std::abs(values.front().correctionMillimetres-.05)<1e-9 &&
                  std::abs(values.back().correctionMillimetres-.35)<1e-9,
                  "candidate order, stable identity, and range");
    ok &= require(!FitCandidateSeries::generate("series-v1", 0, .1, 4, &values, &error) &&
                  values.size() == 7, "even series rejected without overwriting previous output");
    ok &= require(!FitCandidateSeries::generate("series-v1", 0, 0, 7, &values, &error),
                  "zero spacing rejected");

    StandardStudCalibrationArtifactDefinition studDefinition;
    studDefinition.artifactIdentity = StandardStudCalibrationArtifact::diameterArtifactIdentity();
    const auto stud = StandardStudCalibrationArtifact::generate(
        StandardStudCalibrationArtifact::canonicalPrototype(), studDefinition);
    ok &= require(stud.ok && stud.candidates.size() == 7, "standalone Standard Stud OD remains printable: " + stud.diagnostic);
    const auto studAgain = StandardStudCalibrationArtifact::generate(
        StandardStudCalibrationArtifact::canonicalPrototype(), studDefinition);
    ok &= require(studAgain.ok && sameMesh(stud.mesh, studAgain.mesh) &&
                  stud.analysis.triangles == studAgain.analysis.triangles,
                  "stud adapter leaves standalone mesh and topology deterministic");
    const auto post = StudReceivingCalibrationArtifact::generatePostWallCell();
    ok &= require(post.ok && post.candidates.size() == 7, "standalone PostWallCell remains printable: " + post.diagnostic);
    const auto postAgain = StudReceivingCalibrationArtifact::generatePostWallCell();
    ok &= require(postAgain.ok && sameMesh(post.mesh, postAgain.mesh) &&
                  post.analysis.triangles == postAgain.analysis.triangles,
                  "PostWallCell adapter leaves standalone mesh and topology deterministic");
    if (!stud.ok || !post.ok) return 1;
    for (int i = 0; i < 7; ++i) {
        ok &= require(std::abs(stud.candidates[i].functionalDiameterMillimetres-(4.5+.1*i))<1e-9,
                      "stud OD range remains 4.50–5.10 mm");
        ok &= require(std::abs(post.candidates[i].functionalDiameterMillimetres-(2.9+.1*i))<1e-9,
                      "PostWallCell OD range remains 2.90–3.50 mm");
    }

    FitCalibrationProcess process;
    process.printerIdentity = "Synthetic Printer";
    process.materialIdentity = "Synthetic PETG";
    process.profileName = "0.20 mm";
    process.hasNozzleDiameter = true;
    process.nozzleDiameterMillimetres = .4;
    process.hasLayerHeight = true;
    process.layerHeightMillimetres = .2;
    const auto context = FitCalibrationLibrary::manufacturingContextFingerprint(process);
    const auto studExperiment = StandardStudCalibrationArtifact::observationTemplate(stud, studDefinition);
    const auto postExperiment = StudReceivingCalibrationArtifact::observationTemplate(post);
    auto studZone = FitCalibrationPackage::standardStudOdZone(stud.mesh, stud.analysis.bounds,
        studExperiment, "stud-session", context, "stud-od.3mf");
    auto postZone = FitCalibrationPackage::postWallCellZone(post.mesh, post.analysis.bounds,
        postExperiment, "post-session", context, "post-wall.3mf");
    ok &= require(FitCalibrationPackage::validateZone(studZone, &error), "physical stud key marker: " + error);
    ok &= require(FitCalibrationPackage::validateZone(postZone, &error), "physical PostWallCell notch: " + error);
    ok &= require(studZone.identity.endsWith(":zone-v1") && postZone.identity.endsWith(":zone-v1") &&
                  studZone.semanticContract != postZone.semanticContract &&
                  std::abs(studZone.candidates.front().functionalValueMillimetres-4.5)<1e-9 &&
                  std::abs(postZone.candidates.front().functionalValueMillimetres-2.9)<1e-9 &&
                  studZone.candidates.front().identity != postZone.candidates.front().identity,
                  "versioned zones preserve separate functional contracts and candidate identities");

    const auto originalMarker = FitCalibrationPackage::placedMarker(studZone);
    studZone.translation = {50, 20, 0};
    const auto shiftedMarker = FitCalibrationPackage::placedMarker(studZone);
    ok &= require(std::abs(shiftedMarker.x-originalMarker.x-50)<1e-9 &&
                  std::abs(shiftedMarker.y-originalMarker.y-20)<1e-9 &&
                  studZone.marker.emptyWitness.x < studZone.candidates.front().position.x &&
                  FitCalibrationPackage::validateZone(studZone, &error),
                  "Candidate #1 marker mapping survives placement translation: " + error);

    auto missingMarker = studZone;
    missingMarker.marker.kind.clear();
    ok &= require(!FitCalibrationPackage::validateZone(missingMarker, &error), "undeclared marker rejected");
    missingMarker = postZone;
    missingMarker.marker.emptyWitness = missingMarker.marker.materialWitness;
    ok &= require(!FitCalibrationPackage::validateZone(missingMarker, &error), "false physical marker rejected");
    auto wrongBounds = postZone;
    wrongBounds.bounds.maximum.x += 1;
    ok &= require(!FitCalibrationPackage::validateZone(wrongBounds, &error), "incorrect physical bounds rejected");
    auto wrongMesh = postZone;
    wrongMesh.meshSha256 = QString(64, QChar('0'));
    ok &= require(!FitCalibrationPackage::validateZone(wrongMesh, &error), "descriptor cannot bind a different mesh");
    auto duplicateCandidates = studZone;
    duplicateCandidates.candidates[1].identity = duplicateCandidates.candidates[0].identity;
    ok &= require(!FitCalibrationPackage::validateZone(duplicateCandidates, &error), "duplicate candidate identity rejected");
    auto wrongZoneVersion = studZone;
    wrongZoneVersion.version = 2;
    ok &= require(!FitCalibrationPackage::validateZone(wrongZoneVersion, &error),
                  "zone version must agree with stable zone identity");

    FitCalibrationPackageManifest package;
    package.identity = "synthetic-package-v1";
    package.manufacturingContextFingerprint = context;
    package.zones = {studZone, postZone};
    QVector<FitCalibrationSession> sessions{session("stud-session", studExperiment, process),
                                             session("post-session", postExperiment, process)};
    FitCalibrationObservation studOnlyObservation;
    studOnlyObservation.result = FitObservation::TooTight;
    sessions[0].coarseExperiment.candidates[0].observations.push_back(studOnlyObservation);
    ok &= require(FitCalibrationPackage::validate(package, sessions, &error),
                  "two-zone package with independent managed sessions: " + error);
    ok &= require(sessions[0].coarseExperiment.candidates[0].observations.size() == 1 &&
                  sessions[1].coarseExperiment.candidates[0].observations.isEmpty(),
                  "package validation leaves independent feature evidence untouched");
    const auto manifestJson = FitCalibrationPackage::toJson(package);
    ok &= require(!manifestJson.contains("observations") && !manifestJson.contains("preferredCandidateIndex") &&
                  !manifestJson.contains("verificationState"), "manifest carries references, not evidence state");
    FitCalibrationPackageManifest decoded;
    ok &= require(FitCalibrationPackage::fromJson(QJsonDocument::fromJson(
        QJsonDocument(manifestJson).toJson()).object(), &decoded, &error), "manifest JSON round trip: " + error);
    ok &= require(decoded.zones.size() == 2 && decoded.zones[0].sessionIdentity == "stud-session" &&
                  decoded.zones[1].sessionIdentity == "post-session" &&
                  decoded.zones[0].marker.kind == "keyed-end" &&
                  decoded.zones[1].marker.kind == "wall-notch" &&
                  std::abs(decoded.zones[0].candidates.front().functionalValueMillimetres-4.5)<1e-9 &&
                  decoded.zones[0].translation.x == 50,
                  "zone descriptors, marker metadata, session references, and transforms round trip");
    decoded.zones[0].mesh = stud.mesh; decoded.zones[1].mesh = post.mesh;
    ok &= require(FitCalibrationPackage::validate(decoded, sessions, &error),
                  "round-tripped manifest validates when standalone meshes are attached: " + error);

    auto duplicateZone = package;
    duplicateZone.zones.push_back(studZone);
    ok &= require(!FitCalibrationPackage::validate(duplicateZone, sessions, &error), "duplicate zone rejected");
    auto overlap = package;
    overlap.zones[1].memberFile = overlap.zones[0].memberFile;
    overlap.zones[1].translation = {50, 20, 0};
    ok &= require(!FitCalibrationPackage::validate(overlap, sessions, &error), "same-member overlap rejected");
    overlap.zones[1].translation = {200, 20, 0};
    ok &= require(FitCalibrationPackage::validate(overlap, sessions, &error),
                  "same-member nonoverlapping placement accepted: " + error);
    auto mixed = package;
    mixed.zones[1].manufacturingContextFingerprint = "different-context";
    ok &= require(!FitCalibrationPackage::validate(mixed, sessions, &error), "mixed context rejected");
    auto incompatibleSessions = sessions;
    incompatibleSessions[1].process.materialIdentity = "Different Material";
    ok &= require(!FitCalibrationPackage::validate(package, incompatibleSessions, &error),
                  "linked session with different manufacturing context rejected");
    incompatibleSessions = sessions;
    incompatibleSessions[1].process.actualPrintedOrientation =
        FitPrintedOrientation::FeatureAxisParallelToBuildPlate;
    ok &= require(!FitCalibrationPackage::validate(package, incompatibleSessions, &error),
                  "incompatible physical print orientation rejected");
    auto missingSession = sessions;
    missingSession.removeLast();
    ok &= require(!FitCalibrationPackage::validate(package, missingSession, &error), "missing managed session rejected");
    auto wrongVersion = manifestJson;
    wrongVersion.insert("version", 2);
    ok &= require(!FitCalibrationPackage::fromJson(wrongVersion, &decoded, &error),
                  "unsupported package version rejected");
    return ok ? 0 : 1;
}
