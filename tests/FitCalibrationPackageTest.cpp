#include "../src/services/geometry/fit/FitCalibrationPackage.h"
#include "../src/services/geometry/fit/FitCalibrationFixtureLabel.h"
#include "../src/services/geometry/fit/FitCalibrationLibrary.h"
#include "../src/services/geometry/fit/StandardStudCalibrationArtifact.h"
#include "../src/services/geometry/fit/StudReceivingCalibrationArtifact.h"
#include "../src/services/geometry/fit/TechnicAxleHoleCalibrationArtifact.h"
#include "../src/services/geometry/ThreeMfWriter.h"
#include "../src/services/geometry/print/PrintMeshAnalysis.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QDir>
#include <QSaveFile>
#include <QSet>
#include <lib3mf_implicit.hpp>
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
    studDefinition.artifactIdentity = QStringLiteral("standard-stud-male-od-perpendicular-v1");
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
    PrintMesh engravedStud;
    QString appliedStudLabel;
    ok &= require(FitCalibrationFixtureLabel::recess(stud.mesh,
        QStringLiteral("Stud OD — Perpendicular"), QStringLiteral("Stud OD"),
        {6, 86, .5, 4.5}, &engravedStud, &appliedStudLabel, &error),
        "recessed Stud OD underside label: " + error);
    ok &= require(!engravedStud.faces.empty() && appliedStudLabel.contains("STUD OD") &&
                  analyzeSource(engravedStud).absoluteVolume < stud.analysis.absoluteVolume &&
                  std::abs(analyzeSource(engravedStud).bounds.maximum.z-stud.analysis.bounds.maximum.z)<1e-6,
                  "recessed label leaves stud functional height unchanged");
    auto newStudDefinition = studDefinition;
    newStudDefinition.artifactIdentity = StandardStudCalibrationArtifact::diameterArtifactIdentity();
    const auto newStud = StandardStudCalibrationArtifact::generate(
        StandardStudCalibrationArtifact::canonicalPrototype(), newStudDefinition);
    ok &= require(newStud.ok && sameMesh(newStud.mesh, engravedStud) &&
                  newStud.candidates.size() == stud.candidates.size(),
                  "new standalone Stud OD generation carries the same protected underside label");
    if (!stud.ok || !post.ok) return 1;
    for (int i = 0; i < 7; ++i) {
        ok &= require(std::abs(stud.candidates[i].functionalDiameterMillimetres-(4.5+.1*i))<1e-9,
                      "stud OD range remains 4.50–5.10 mm");
        ok &= require(std::abs(post.candidates[i].functionalDiameterMillimetres-(2.9+.1*i))<1e-9,
                      "PostWallCell OD range remains 2.90–3.50 mm");
    }

    FitCalibrationProcess process;
    process.printerIdentity = "Bambu H2D";
    process.materialIdentity = "PETG";
    process.profileName = "0.20mm Standard @BBL H2D";
    process.hasNozzleDiameter = true;
    process.nozzleDiameterMillimetres = .4;
    process.hasLayerHeight = true;
    process.layerHeightMillimetres = .2;
    process.dimensionalCompensationNotes = "None / Bambu Studio defaults";
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
    FitCalibrationPackageManifest pilot;
    QVector<FitCalibrationSession> pilotSessions;
    PrintMesh pilotMesh;
    ok &= require(FitCalibrationPackage::generateFourZonePilot(process, &pilot, &pilotSessions,
                                                                &pilotMesh, &error),
                  "four-zone pilot generation: " + error);
    if (pilot.zones.size() == 4) {
        FitCalibrationPackageManifest repeatedPilot;
        QVector<FitCalibrationSession> repeatedSessions;
        PrintMesh repeatedMesh;
        ok &= require(FitCalibrationPackage::generateFourZonePilot(process, &repeatedPilot,
            &repeatedSessions, &repeatedMesh, &error) && sameMesh(pilotMesh, repeatedMesh) &&
            QJsonDocument(FitCalibrationPackage::toJson(pilot)).toJson(QJsonDocument::Compact) ==
            QJsonDocument(FitCalibrationPackage::toJson(repeatedPilot)).toJson(QJsonDocument::Compact),
            "deterministic physical layout and manifest");
        ok &= require(pilotSessions.size() == 4 && pilot.zones[0].variant == "StandardStudOD" &&
                      pilot.zones[1].variant == "TubeWallCell" &&
                      pilot.zones[2].variant == "PostWallCell" &&
                      pilot.zones[3].variant == "TechnicAxleHoleArmWidthV2" &&
                      pilot.zones[3].correctionSemantic == "arm-width",
                      "four distinct active pilot contracts and sessions");
        ok &= require(pilot.zones[1].artifactIdentity.endsWith("pilot-v2") &&
                      pilot.zones[3].semanticContract == "official-ldraw-axlehole-arm-width-clearance-v2",
                      "versioned TubeWall marker and active axle-hole v2 evidence");
        ok &= require(pilot.zones[0].featureDisplayName == "Stud OD — Perpendicular" &&
                      pilot.zones[0].physicalLabel == "STUD OD - PERPENDICULAR" &&
                      pilot.zones[1].featureDisplayName.contains("TubeWallCell") &&
                      pilot.zones[1].physicalLabel == "CLUTCH TUBEWALL" &&
                      pilot.zones[2].physicalLabel == "CLUTCH POSTWALL" &&
                      pilot.zones[3].physicalLabel == "AXLE HOLE ARM WIDTH",
                      "physical underside labels derive from feature presentation with safe recorded fallbacks");
        for (int i = 0; i < 4; ++i) {
            ok &= require(FitCalibrationPackage::validateZone(pilot.zones[i], &error),
                          QString("pilot zone %1 physical marker: %2").arg(i).arg(error));
            ok &= require(pilotSessions[i].coarseExperiment.candidates.size() == 7 &&
                          pilotSessions[i].coarseExperiment.candidates[0].observations.isEmpty() &&
                          pilotSessions[i].sessionIdentity == pilot.zones[i].sessionIdentity &&
                          pilot.zones[i].sessionFile.endsWith("-session.json"),
                          "independent blank session evidence and exact zone link");
        }
        std::size_t vertexOffset = 0, faceCount = 0;
        for (const auto& zone : pilot.zones) {
            for (std::size_t vertex = 0; vertex < zone.mesh.vertices.size(); ++vertex) {
                const auto& source = zone.mesh.vertices[vertex];
                const auto& placed = pilotMesh.vertices[vertexOffset+vertex];
                ok &= require(std::abs(placed.x-source.x-zone.translation.x)<1e-9 &&
                              std::abs(placed.y-source.y-zone.translation.y)<1e-9 &&
                              std::abs(placed.z-source.z-zone.translation.z)<1e-9,
                              "packaged vertices preserve standalone geometry under translation");
            }
            vertexOffset += zone.mesh.vertices.size();
            faceCount += zone.mesh.faces.size();
        }
        ok &= require(vertexOffset == pilotMesh.vertices.size() &&
                      faceCount == pilotMesh.faces.size(),
                      "packaged geometry preserves every zone triangle");
        ok &= require(std::abs(pilot.zones[1].candidates.front().functionalValueMillimetres-6.1)<1e-9 &&
                      std::abs(pilot.zones[1].candidates.back().functionalValueMillimetres-6.7)<1e-9 &&
                      std::abs(pilot.zones[3].candidates.front().functionalValueMillimetres-1.6)<1e-9 &&
                      std::abs(pilot.zones[3].candidates.back().functionalValueMillimetres-2.2)<1e-9,
                      "TubeWall and axle-hole arm-width candidate ranges preserved");
        ok &= require(FitCalibrationPackage::validate(pilot, pilotSessions, &error),
                      "four-zone nonoverlap and session consistency: " + error);
        FitCalibrationPackageManifest pilotDecoded;
        const auto json = FitCalibrationPackage::toJson(pilot);
        ok &= require(FitCalibrationPackage::fromJson(QJsonDocument::fromJson(
            QJsonDocument(json).toJson()).object(), &pilotDecoded, &error),
            "four-zone manifest round trip: " + error);
        if (pilotDecoded.zones.size() == 4) {
            for (int i = 0; i < 4; ++i) pilotDecoded.zones[i].mesh = pilot.zones[i].mesh;
            ok &= require(FitCalibrationPackage::validate(pilotDecoded, pilotSessions, &error),
                          "round-tripped physical pilot validates: " + error);
        }
        auto overlapped = pilot;
        overlapped.zones[2].translation = pilot.zones[1].translation;
        ok &= require(!FitCalibrationPackage::validate(overlapped, pilotSessions, &error),
                      "pilot clearance collision rejected");
        auto missingPilotMarker = pilot;
        missingPilotMarker.zones[3].marker.materialWitness = {5, 17, 1.5};
        ok &= require(!FitCalibrationPackage::validate(missingPilotMarker, pilotSessions, &error),
                      "false axle-hole physical marker rejected");
        auto mixedPilot = pilot;
        mixedPilot.zones[3].manufacturingContextFingerprint = "wrong-context";
        ok &= require(!FitCalibrationPackage::validate(mixedPilot, pilotSessions, &error),
                      "four-zone mixed manufacturing context rejected");
        if (argc == 3 && QString::fromLocal8Bit(argv[1]) == "--pilot-output" && ok) {
            const QDir output(QString::fromLocal8Bit(argv[2]));
            ok &= require(output.exists(), "pilot output directory must already exist");
            ThreeMfWriter::Options options;
            options.objectName = "BrickSuite four-zone LEGO Fit calibration pilot";
            options.partIdentity = pilot.identity;
            options.modelColor = QColor("#A0A5A9");
            QVector<ThreeMfWriter::NamedMesh> objects;
            for (int i = 0; i < pilot.zones.size(); ++i)
                objects.push_back({QString("%1 %2").arg(i+1, 2, 10, QChar('0'))
                    .arg(pilot.zones[i].variant), pilot.zones[i].mesh,
                    pilot.zones[i].translation});
            ok &= require(ThreeMfWriter::writeCollection(objects,
                output.filePath("unified-lego-fit-pilot.3mf"), options, &error),
                "pilot 3MF export: " + error);
            if (ok) {
                Lib3MF::CWrapper wrapper;
                auto reopened = wrapper.CreateModel();
                reopened->QueryReader("3mf")->ReadFromFile(
                    output.filePath("unified-lego-fit-pilot.3mf").toStdString());
                auto meshes = reopened->GetMeshObjects();
                std::size_t reopenedVertices = 0, reopenedFaces = 0;
                QSet<QString> reopenedNames;
                while (meshes->MoveNext()) {
                    const auto object = meshes->GetCurrentMeshObject();
                    reopenedVertices += object->GetVertexCount();
                    reopenedFaces += object->GetTriangleCount();
                    reopenedNames.insert(QString::fromStdString(object->GetName()));
                }
                ok &= require(reopenedNames.size() == 4 &&
                    reopenedVertices == pilotMesh.vertices.size() &&
                    reopenedFaces == pilotMesh.faces.size() &&
                    reopenedNames.contains("01 StandardStudOD") &&
                    reopenedNames.contains("04 TechnicAxleHoleArmWidthV2"),
                    "independent lib3mf reopen preserves four named zone objects and geometry");
                auto items = reopened->GetBuildItems();
                int buildItemCount = 0;
                while (items->MoveNext()) ++buildItemCount;
                ok &= require(buildItemCount == 4, "pilot 3MF contains four printable build items");
            }
            const auto writeJson = [&](const QString& name, const QJsonObject& object) {
                QSaveFile file(output.filePath(name));
                return file.open(QIODevice::WriteOnly) &&
                    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) >= 0 &&
                    file.commit();
            };
            ok &= require(writeJson("unified-lego-fit-pilot-manifest.json", json),
                          "pilot manifest written");
            for (int i = 0; i < pilotSessions.size(); ++i)
                ok &= require(writeJson(pilot.zones[i].sessionFile,
                    FitCalibrationSessionJson::toJson(pilotSessions[i])),
                    "independent pilot session template written");
        }
    }
    return ok ? 0 : 1;
}
