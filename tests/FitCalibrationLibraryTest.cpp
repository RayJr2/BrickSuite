#include "../src/services/geometry/fit/FitCalibrationLibrary.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTextStream>
#include <cmath>

using namespace PrintGeometry;

namespace {
bool require(bool condition, const QString& message) {
    if (!condition) QTextStream(stderr) << "FAIL: " << message << Qt::endl;
    return condition;
}
FitCalibrationSession verifiedSession() {
    FitCalibrationSession session; session.sessionIdentity = "stable-session";
    session.process.printerIdentity = "Bambu H2D"; session.process.materialIdentity = "PETG";
    session.process.profileName = "0.20 mm Standard"; session.process.hasNozzleDiameter = true;
    session.process.nozzleDiameterMillimetres = .4; session.process.hasLayerHeight = true;
    session.process.layerHeightMillimetres = .2;
    session.process.actualPrintedOrientation = FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;
    session.process.orientationNotes = "Physical feature axis perpendicular";
    session.process.dimensionalCompensationNotes = "Defaults; no explicit compensation";
    session.hasFineExperiment = true; auto& experiment = session.fineExperiment;
    experiment.artifactIdentity = "round-technic-female-perpendicular-verification-v2";
    experiment.parentArtifactIdentity = "round-technic-female-perpendicular-v1";
    experiment.featureFamily = "RoundTechnicPassage"; experiment.featureRole = "female";
    experiment.modeledOrientationIdentity = "feature-axis-perpendicular-to-build-plate";
    experiment.process = session.process; experiment.preferredCandidateIndex = 2;
    experiment.state = FitEvidenceState::Verified;
    for (int index = 1; index <= 3; ++index) {
        FitCalibrationCandidate candidate; candidate.index = index;
        candidate.diameterCorrectionMillimetres = .175 + .025 * (index - 1);
        candidate.functionalDiameterMillimetres = 4.8 + candidate.diameterCorrectionMillimetres;
        FitCalibrationObservation observation; observation.repeatNumber = 1;
        observation.result = index == 2 ? FitObservation::Preferred : FitObservation::Acceptable;
        observation.notes = QStringLiteral("physical result %1").arg(index);
        observation.performedUtc = QDateTime::fromString("2026-09-20T12:00:00.000Z", Qt::ISODateWithMs);
        candidate.observations.push_back(observation);
        if (index == 2) { observation.repeatNumber = 2; candidate.observations.push_back(observation); }
        experiment.candidates.push_back(candidate);
    }
    return session;
}
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv); bool ok = true; QTemporaryDir temporary;
    ok &= require(temporary.isValid(), "temporary storage root");
    FitCalibrationLibrary library(temporary.path()); QVector<FitLibraryIssue> issues;
    ok &= require(library.storageRoot() == QDir::cleanPath(temporary.path()) && library.sessions(&issues).isEmpty() && issues.isEmpty(), "override and empty library");
    auto session = verifiedSession(); QString error;
    ok &= require(FitCalibrationEvidencePolicy::preferredSessionStage(session) == FitCalibrationStage::Fine, "Verified session selects the Fine Search stage");
    auto fineProgress = session; fineProgress.fineExperiment.state = FitEvidenceState::Experimental; fineProgress.fineExperiment.preferredCandidateIndex = 0;
    ok &= require(FitCalibrationEvidencePolicy::preferredSessionStage(fineProgress) == FitCalibrationStage::Fine, "fine-search progress selects the Fine Search stage");
    auto coarseOnly = session; coarseOnly.hasFineExperiment = false; coarseOnly.hasCoarseExperiment = true; coarseOnly.coarseExperiment = session.fineExperiment; coarseOnly.coarseExperiment.parentArtifactIdentity.clear();
    ok &= require(FitCalibrationEvidencePolicy::preferredSessionStage(coarseOnly) == FitCalibrationStage::Coarse, "coarse-only session selects the Coarse Search stage");
    ok &= require(library.saveSession(&session, &error), "atomic managed session save: " + error);
    auto summaries = library.sessions(&issues);
    ok &= require(summaries.size() == 1 && summaries.front().identity == "stable-session" && summaries.front().displayName.contains("Bambu H2D"), "session discovery and meaningful name");
    FitCalibrationSession loaded;
    ok &= require(library.loadSession(session.sessionIdentity, &loaded, &error) && loaded.fineExperiment.candidates[1].observations.size() == 2 && loaded.process.orientationNotes == session.process.orientationNotes, "session reload preserves physical evidence");

    FitProfile profile;
    ok &= require(FitCalibrationLibrary::promoteVerifiedSession(loaded, "H2D PETG LEGO Fit", &profile, &error), "Verified promotion: " + error);
    ok &= require(!profile.profileIdentity.isEmpty() && profile.sourceSessionIdentity == session.sessionIdentity && profile.corrections.size() == 1 &&
                  profile.corrections.front().featureFamily == "RoundTechnicPassage" && profile.corrections.front().featureRole == "female" &&
                  std::abs(profile.corrections.front().valueMillimetres - .2) < 1e-9 && profile.corrections.front().semantics == "female-diameter-clearance" &&
                  profile.corrections.front().printedOrientation == "feature-axis-perpendicular-to-build-plate", "exact correction and applicability semantics");
    ok &= require(library.saveProfile(&profile, &error), "profile save"); const auto stableProfileIdentity = profile.profileIdentity;
    FitProfile loadedProfile;
    ok &= require(library.loadProfile(stableProfileIdentity, &loadedProfile, &error) && loadedProfile.profileIdentity == stableProfileIdentity &&
                  loadedProfile.process.printerIdentity == "Bambu H2D" && loadedProfile.process.materialIdentity == "PETG" &&
                  FitCalibrationLibrary::profileCompatibility(loadedProfile, &error), "profile round trip and compatibility");
    auto stale = loadedProfile; stale.corrections.front().regeneratorAlgorithmVersion = "future-v2";
    ok &= require(!FitCalibrationLibrary::profileCompatibility(stale, &error) && error.contains("regenerator"), "deterministic version staleness");
    auto incomplete = loaded; incomplete.fineExperiment.state = FitEvidenceState::CandidateSelected; FitProfile rejected;
    ok &= require(!FitCalibrationLibrary::promoteVerifiedSession(incomplete, "Must reject", &rejected, &error), "non-Verified promotion rejected");

    const QString exported = QDir(temporary.path()).filePath("portable.json");
    ok &= require(library.exportSession(session.sessionIdentity, exported, &error), "portable export");
    FitCalibrationLibrary importedLibrary(QDir(temporary.path()).filePath("imported")); FitCalibrationSession imported;
    ok &= require(importedLibrary.importSession(exported, &imported, &error) && imported.sessionIdentity == session.sessionIdentity &&
                  imported.fineExperiment.candidates[1].observations.size() == 2, "v3 portable import preserves identity and evidence");
    FitCalibrationLibrary creationLibrary(QDir(temporary.path()).filePath("created")); auto created = verifiedSession(); created.sessionIdentity.clear();
    ok &= require(creationLibrary.saveSession(&created, &error) && !created.sessionIdentity.isEmpty(), "managed creation assigns stable identity");
    const QString createdIdentity = created.sessionIdentity; created.process.orientationNotes = "updated";
    ok &= require(creationLibrary.saveSession(&created, &error) && created.sessionIdentity == createdIdentity && creationLibrary.sessions().size() == 1, "managed updates retain stable identity");

    const QString malformed = QDir(library.sessionsDirectory()).filePath("malformed.json"); QFile malformedFile(malformed);
    ok &= require(malformedFile.open(QIODevice::WriteOnly) && malformedFile.write("{broken") > 0, "malformed fixture"); malformedFile.close();
    issues.clear(); summaries = library.sessions(&issues);
    ok &= require(summaries.size() == 1 && issues.size() == 1, "malformed session isolated");
    const QString duplicate = QDir(library.sessionsDirectory()).filePath("duplicate.json"); QFile::copy(exported, duplicate);
    issues.clear(); summaries = library.sessions(&issues);
    ok &= require(summaries.size() == 1 && issues.size() == 2, "duplicate stable identity isolated");
    const QString badProfile = QDir(library.profilesDirectory()).filePath("bad.json"); QFile badProfileFile(badProfile);
    ok &= require(badProfileFile.open(QIODevice::WriteOnly) && badProfileFile.write("[]") > 0, "corrupted profile fixture"); badProfileFile.close();
    QVector<FitLibraryIssue> profileIssues; const auto profiles = library.profiles(&profileIssues);
    ok &= require(profiles.size() == 1 && profiles.front().compatible && profileIssues.size() == 1, "corrupted profile isolated");

    QJsonObject legacy{{"formatVersion",1},{"artifactIdentity","legacy-v1"},{"featureFamily","RoundTechnicPassage"},{"featureRole","female"},
        {"orientationIdentity","legacy"},{"printerIdentity","Legacy Printer"},{"materialIdentity","PLA"},
        {"candidates",QJsonArray{QJsonObject{{"index",1},{"diameterCorrectionMillimetres",.1},{"functionalDiameterMillimetres",4.9},{"observation","acceptable"},{"repeatNumber",1},{"notes","kept"}}}}};
    const QString legacyPath = QDir(temporary.path()).filePath("legacy-v1.json"); QFile legacyFile(legacyPath);
    ok &= require(legacyFile.open(QIODevice::WriteOnly) && legacyFile.write(QJsonDocument(legacy).toJson()) > 0, "legacy fixture"); legacyFile.close();
    FitCalibrationSession legacyImported;
    ok &= require(importedLibrary.importSession(legacyPath, &legacyImported, &error) && legacyImported.sessionIdentity != "legacy-v1" &&
                  legacyImported.coarseExperiment.candidates.front().observations.front().notes == "kept", "legacy v1 upgrade gets managed identity without evidence loss");
    auto legacyV2Experiment = session.fineExperiment; legacyV2Experiment.artifactIdentity = "legacy-v2";
    const QString legacyV2Path = QDir(temporary.path()).filePath("legacy-v2.json"); QFile legacyV2File(legacyV2Path);
    ok &= require(legacyV2File.open(QIODevice::WriteOnly) && legacyV2File.write(QJsonDocument(FitCalibrationExperimentJson::toJson(legacyV2Experiment)).toJson()) > 0, "legacy v2 fixture"); legacyV2File.close();
    FitCalibrationSession legacyV2Imported;
    ok &= require(importedLibrary.importSession(legacyV2Path, &legacyV2Imported, &error) && legacyV2Imported.hasFineExperiment &&
                  legacyV2Imported.fineExperiment.candidates[1].observations.size() == 2 && legacyV2Imported.sessionIdentity != "legacy-v2", "legacy v2 upgrade preserves repeat evidence and receives managed identity");
    const QString defaultRoot = FitCalibrationLibrary::defaultStorageRoot();
    ok &= require(QDir(defaultRoot).dirName() == "LegoCalibration" && QDir::isAbsolutePath(defaultRoot), "cross-platform default path resolution");
    return ok ? 0 : 1;
}
