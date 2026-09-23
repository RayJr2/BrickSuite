#include "../src/services/geometry/fit/FitCalibrationLibrary.h"
#include "../src/services/geometry/fit/FitCalibrationNamingCatalog.h"
#include "../src/services/geometry/fit/FitCalibrationArtifactLocation.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QTemporaryDir>
#include <QTextStream>
#include <algorithm>
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
    QSet<QString> canonicalNames, abbreviations, slugs;
    for (const auto& name : FitCalibrationNamingCatalog::entries()) {
        const QString canonical = QString::fromUtf8(name.canonical);
        const QString abbreviated = QString::fromLatin1(name.abbreviated);
        const QString slug = QString::fromLatin1(name.slug);
        ok &= require(!canonicalNames.contains(canonical) && !abbreviations.contains(abbreviated) &&
                      !slugs.contains(slug), "canonical names, label abbreviations, and new-file slugs are unique");
        canonicalNames.insert(canonical); abbreviations.insert(abbreviated); slugs.insert(slug);
    }
    ok &= require(canonicalNames.size() == 16 &&
                  QString::fromUtf8(FitCalibrationNamingCatalog::forKey(FitCalibrationNameKey::CClipBarReceiverClearance).canonical)
                      .contains(QStringLiteral("C-Clip / Bar Receiver")) &&
                  QString::fromUtf8(FitCalibrationNamingCatalog::forKey(FitCalibrationNameKey::ClutchAntiStudBore).canonical)
                      .contains(QStringLiteral("Anti-Stud Bore")) &&
                  QString::fromUtf8(FitCalibrationNamingCatalog::forKey(FitCalibrationNameKey::ClutchWallPocketBrick).canonical)
                      .contains(QStringLiteral("Wall Pocket")) &&
                  QString::fromUtf8(FitCalibrationNamingCatalog::forKey(FitCalibrationNameKey::LegacyAxleHoleTip).canonical) ==
                      QStringLiteral("Technic Axle Hole — Tip Clearance — Perpendicular (legacy)"),
                  "all current features and obsolete axle-hole evidence have explicit names");
    ok &= require(FitCalibrationNamingCatalog::suggestedFileName(FitCalibrationNameKey::StudOd) ==
                      "standard-stud-od-perpendicular-coarse.3mf" &&
                  FitCalibrationNamingCatalog::suggestedFileName(FitCalibrationNameKey::StudOd, "verification") ==
                      "standard-stud-od-perpendicular-verification.3mf",
                  "new-file slugs keep stage separate from stable artifact identities");
    const QString fixturePath = FitCalibrationArtifactLocation::fixturePath(
        FitCalibrationNameKey::StudOd, "coarse", 2, temporary.path());
    ok &= require(fixturePath.startsWith(QDir::cleanPath(temporary.path())) &&
                  fixturePath.endsWith("standard-stud-od-perpendicular-coarse-v2.3mf") &&
                  FitCalibrationArtifactLocation::companionPath(fixturePath).endsWith(
                      "standard-stud-od-perpendicular-coarse-v2-session.json") &&
                  QFileInfo(FitCalibrationArtifactLocation::companionPath(fixturePath)).absolutePath() ==
                      QFileInfo(fixturePath).absolutePath(),
                  "redirectable cross-platform fixture and Import Session pair share one directory");
    FitCalibrationExperiment namingExperiment;
    namingExperiment.featureFamily = "RoundTechnicPassage";
    ok &= require(FitCalibrationLibrary::featureDisplayName(namingExperiment, FitPrintedOrientation::FeatureAxisParallelToBuildPlate) ==
                      "Technic Hole — Round Passage — Parallel", "parallel Technic Hole catalog lookup");
    namingExperiment.featureFamily = "TechnicAxleHole";
    namingExperiment.artifactIdentity = "technic-axle-hole-tip-clearance-perpendicular-coarse-v1";
    ok &= require(FitCalibrationLibrary::featureDisplayName(namingExperiment, FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate) ==
                      "Technic Axle Hole — Tip Clearance — Perpendicular (legacy)",
                  "historical tip-clearance evidence remains explicitly legacy");
    ok &= require(library.storageRoot() == QDir::cleanPath(temporary.path()) && library.sessions(&issues).isEmpty() && issues.isEmpty(), "override and empty library");
    {
        FitCalibrationLibrary managed(QDir(temporary.path()).filePath("workspace-management"));
        auto process = verifiedSession().process;
        FitCalibrationWorkspace created;
        QString workspaceError;
        ok &= require(managed.createWorkspace(process, &created, &workspaceError), "create empty managed workspace: " + workspaceError);
        ok &= require(created.featureSessions.isEmpty() && managed.workspaces().size() == 1,
                      "empty workspace is immediately discoverable");
        FitCalibrationLibrary reopened(managed.storageRoot());
        ok &= require(reopened.workspaces().size() == 1 && reopened.workspaces().front().identity == created.identity,
                      "empty workspace survives library reopen");
        ok &= require(!managed.createWorkspace(process, nullptr, &workspaceError) && managed.workspaces().size() == 1,
                      "duplicate manufacturing context is rejected");
        auto other = process; other.materialIdentity = "PLA";
        FitCalibrationWorkspace retained;
        ok &= require(managed.createWorkspace(other, &retained, &workspaceError), "independent workspace creation");
        auto ownedSession = verifiedSession(); ownedSession.sessionIdentity = "workspace-owned-session";
        ok &= require(managed.saveSession(&ownedSession, &workspaceError), "managed workspace session persists");
        FitProfile ownedProfile;
        ok &= require(FitCalibrationLibrary::promoteVerifiedSession(ownedSession, "Managed workspace profile", &ownedProfile, &workspaceError) &&
                      managed.saveProfile(&ownedProfile, &workspaceError), "associated profile persists");
        {
            FitCalibrationLibrary blocked(QDir(temporary.path()).filePath("blocked-workspace-deletion"));
            FitCalibrationWorkspace blockedWorkspace;
            ok &= require(blocked.createWorkspace(process, &blockedWorkspace, &workspaceError), "workspace for backup-failure test");
            QFile obstruction(QDir(blocked.storageRoot()).filePath("Backups"));
            ok &= require(obstruction.open(QIODevice::WriteOnly), "create backup-directory obstruction");
            obstruction.close();
            QString rejectedBackup;
            ok &= require(!blocked.deleteWorkspace(blockedWorkspace.identity, &rejectedBackup, &workspaceError) &&
                          blocked.workspaces().size() == 1, "backup failure leaves selected workspace intact");
        }
        QString backup;
        ok &= require(managed.deleteWorkspace(created.identity, &backup, &workspaceError),
                      "selected workspace deletion: " + workspaceError);
        ok &= require(QFile::exists(backup) && managed.workspaces().size() == 1 &&
                      managed.workspaces().front().identity == retained.identity && managed.sessions().isEmpty() &&
                      managed.profiles().isEmpty(),
                      "deletion backs up selected context and profile while retaining unrelated workspace");
        auto legacyUnknown = verifiedSession(); legacyUnknown.sessionIdentity = "legacy-unknown-context";
        legacyUnknown.process.printerIdentity.clear(); legacyUnknown.process.materialIdentity.clear();
        legacyUnknown.process.profileName.clear();
        legacyUnknown.fineExperiment.process = legacyUnknown.process;
        ok &= require(managed.saveSession(&legacyUnknown, &workspaceError), "legacy Unknown session fixture persists");
        const QString unknownIdentity = FitCalibrationLibrary::manufacturingContextFingerprint(legacyUnknown.process);
        ok &= require(managed.deleteWorkspace(unknownIdentity, &backup, &workspaceError) && QFile::exists(backup) &&
                      managed.workspaces().size() == 1 && managed.workspaces().front().identity == retained.identity,
                      "legacy Unknown workspace can be removed without changing the valid workspace");
    }
    auto session = verifiedSession(); QString error;
    ok &= require(FitCalibrationEvidencePolicy::preferredSessionStage(session) == FitCalibrationStage::Fine, "Verified session selects the Fine Search stage");
    auto fineProgress = session; fineProgress.fineExperiment.state = FitEvidenceState::Experimental; fineProgress.fineExperiment.preferredCandidateIndex = 0;
    ok &= require(FitCalibrationEvidencePolicy::preferredSessionStage(fineProgress) == FitCalibrationStage::Fine, "fine-search progress selects the Fine Search stage");
    auto coarseOnly = session; coarseOnly.hasFineExperiment = false; coarseOnly.hasCoarseExperiment = true; coarseOnly.coarseExperiment = session.fineExperiment; coarseOnly.coarseExperiment.parentArtifactIdentity.clear();
    ok &= require(FitCalibrationEvidencePolicy::preferredSessionStage(coarseOnly) == FitCalibrationStage::Coarse, "coarse-only session selects the Coarse Search stage");
    FitCalibrationLibrary reviewLibrary(QDir(temporary.path()).filePath("historical-review"));
    auto studRoot = coarseOnly;
    studRoot.sessionIdentity = "stud-root";
    studRoot.coarseExperiment.artifactIdentity = "standard-stud-od-coarse-v1";
    studRoot.coarseExperiment.featureFamily = "StandardStud";
    studRoot.coarseExperiment.featureRole = "male";
    studRoot.coarseExperiment.state = FitEvidenceState::Experimental;
    studRoot.coarseExperiment.preferredCandidateIndex = 0;
    auto studExtension = studRoot;
    studExtension.sessionIdentity = "stud-extension";
    studExtension.coarseExperiment.artifactIdentity = "standard-stud-od-extension-v2";
    studExtension.coarseExperiment.parentArtifactIdentity = studRoot.coarseExperiment.artifactIdentity;
    studExtension.coarseExperiment.preferredCandidateIndex = 2;
    auto studVerified = studExtension;
    studVerified.sessionIdentity = "stud-verified";
    studVerified.hasFineExperiment = true;
    studVerified.fineExperiment = session.fineExperiment;
    studVerified.fineExperiment.artifactIdentity = "standard-stud-od-direct-verification-v3";
    studVerified.fineExperiment.parentArtifactIdentity = studExtension.coarseExperiment.artifactIdentity;
    studVerified.fineExperiment.featureFamily = "StandardStud";
    studVerified.fineExperiment.featureRole = "male";
    for (int i = 0; i < studVerified.fineExperiment.candidates.size(); ++i)
        studVerified.fineExperiment.candidates[i].functionalDiameterMillimetres = 5.10 + .05 * i;
    ok &= require(reviewLibrary.saveSession(&studRoot, &error) &&
                  reviewLibrary.saveSession(&studExtension, &error) &&
                  reviewLibrary.saveSession(&studVerified, &error),
                  "synthetic Stud OD lineage persists independently");
    const auto studHistory = reviewLibrary.coarseReviewHistory(studVerified);
    ok &= require(studHistory.size() == 2 &&
                  studHistory[0].artifactIdentity == studRoot.coarseExperiment.artifactIdentity &&
                  studHistory[1].artifactIdentity == studExtension.coarseExperiment.artifactIdentity &&
                  studHistory[0].candidates[1].observations.size() == 2 &&
                  studHistory[1].preferredCandidateIndex == 2 &&
                  studVerified.fineExperiment.state == FitEvidenceState::Verified &&
                  std::abs(studVerified.fineExperiment.candidates[0].functionalDiameterMillimetres-5.10) < 1e-9 &&
                  std::abs(studVerified.fineExperiment.candidates[1].functionalDiameterMillimetres-5.15) < 1e-9 &&
                  std::abs(studVerified.fineExperiment.candidates[2].functionalDiameterMillimetres-5.20) < 1e-9 &&
                  studVerified.fineExperiment.artifactIdentity != studHistory.back().artifactIdentity,
                  "Verified Stud OD retains separate coarse, extension, and verification review");
    auto richerRoot = studRoot;
    richerRoot.sessionIdentity = "stud-root-with-more-evidence";
    auto additionalObservation = richerRoot.coarseExperiment.candidates[1].observations.front();
    additionalObservation.repeatNumber = 3;
    richerRoot.coarseExperiment.candidates[1].observations.push_back(additionalObservation);
    ok &= require(reviewLibrary.saveSession(&richerRoot, &error) &&
                  reviewLibrary.coarseReviewHistory(studVerified).front().candidates[1].observations.size() == 3,
                  "legacy duplicate artifact lookup prefers the saved stage with more observations");
    FitCalibrationLibrary durableLibrary(QDir(temporary.path()).filePath("durable-lineage"));
    auto durableRoot = studRoot;
    durableRoot.sessionIdentity = "durable-stud-root";
    durableRoot.coarseExperiment.candidates[1].observations[0].notes = "physical coarse observation";
    ok &= require(durableLibrary.saveSession(&durableRoot, &error), "observed coarse stage saved");
    auto extensionStage = studExtension.coarseExperiment;
    extensionStage.candidates.clear();
    for (int index = 1; index <= 7; ++index) {
        FitCalibrationCandidate candidate;candidate.index = index;
        candidate.functionalDiameterMillimetres = 5.10 + .05 * (index - 1);
        candidate.diameterCorrectionMillimetres = candidate.functionalDiameterMillimetres - 4.8;
        extensionStage.candidates.push_back(candidate);
    }
    auto firstContinuation = durableRoot;
    firstContinuation.hasFineExperiment = true;
    firstContinuation.fineExperiment = extensionStage;
    ok &= require(firstContinuation.coarseExperiment.candidates[1].observations.size() == 2 &&
                  firstContinuation.fineExperiment.candidates[1].observations.isEmpty() &&
                  durableLibrary.saveSession(&firstContinuation, &error),
                  "boundary extension starts empty and retains observed coarse evidence");
    FitCalibrationObservation extensionObservation;
    extensionObservation.result = FitObservation::Preferred;
    extensionObservation.repeatNumber = 1;
    firstContinuation.fineExperiment.candidates[1].observations.push_back(extensionObservation);
    firstContinuation.fineExperiment.preferredCandidateIndex = 2;
    ok &= require(durableLibrary.saveSession(&firstContinuation, &error), "extension observation saved");
    auto verificationStage = studVerified.fineExperiment;
    verificationStage.parentArtifactIdentity = extensionStage.artifactIdentity;
    auto verifiedChild = FitCalibrationLibrary::continuationSession(
        firstContinuation, firstContinuation.fineExperiment, verificationStage);
    ok &= require(verifiedChild.sessionIdentity != firstContinuation.sessionIdentity &&
                  verifiedChild.history.size() == 1 &&
                  FitCalibrationSessionJson::toJson(verifiedChild).value("formatVersion").toInt() ==
                      FitCalibrationSessionJson::CurrentFormatVersion &&
                  verifiedChild.history[0].artifactIdentity == durableRoot.coarseExperiment.artifactIdentity &&
                  verifiedChild.history[0].candidates[1].observations[0].notes == "physical coarse observation" &&
                  verifiedChild.coarseExperiment.candidates[1].observations.size() == 1 &&
                  verifiedChild.fineExperiment.candidates[0].observations.isEmpty(),
                  "new verification child snapshots full earlier lineage and starts with fresh evidence");
    verifiedChild.fineExperiment = studVerified.fineExperiment;
    verifiedChild.fineExperiment.parentArtifactIdentity = extensionStage.artifactIdentity;
    ok &= require(durableLibrary.saveSession(&verifiedChild, &error), "Verified continuation saved");
    FitCalibrationSession reloadedChild, reloadedRoot;
    ok &= require(durableLibrary.loadSession(verifiedChild.sessionIdentity, &reloadedChild, &error) &&
                  durableLibrary.loadSession(firstContinuation.sessionIdentity, &reloadedRoot, &error) &&
                  reloadedRoot.coarseExperiment.candidates[1].observations[0].notes == "physical coarse observation" &&
                  reloadedChild.history.size() == 1 &&
                  reloadedChild.history[0].candidates[1].observations.size() == 2 &&
                  reloadedChild.coarseExperiment.candidates[1].observations.size() == 1 &&
                  reloadedChild.fineExperiment.state == FitEvidenceState::Verified,
                  "restart reload preserves all observed stages and final Verified evidence");
    const auto durableHistory = durableLibrary.coarseReviewHistory(reloadedChild);
    ok &= require(durableHistory.size() == 2 &&
                  durableHistory[0].artifactIdentity == durableRoot.coarseExperiment.artifactIdentity &&
                  durableHistory[1].artifactIdentity == extensionStage.artifactIdentity,
                  "historical review resolves embedded coarse and extension evidence");
    const QString chainExport = QDir(temporary.path()).filePath("durable-chain-export.json");
    ok &= require(durableLibrary.exportSession(reloadedChild.sessionIdentity, chainExport, &error),
                  "portable full-lineage export");
    FitCalibrationLibrary importedChain(QDir(temporary.path()).filePath("imported-chain"));
    FitCalibrationSession restoredChain;
    ok &= require(importedChain.importSession(chainExport, &restoredChain, &error) &&
                  restoredChain.history.size() == 1 &&
                  restoredChain.history[0].candidates[1].observations.size() == 2 &&
                  importedChain.coarseReviewHistory(restoredChain).size() == 2 &&
                  restoredChain.fineExperiment.state == FitEvidenceState::Verified,
                  "portable export/import restores complete observed lineage without parent files");
    FitCalibrationLibrary monotonicLibrary(QDir(temporary.path()).filePath("monotonic-import"));
    auto incompleteCopy = verifiedChild;
    incompleteCopy.history.clear();
    ok &= require(monotonicLibrary.saveSession(&incompleteCopy, &error) &&
                  monotonicLibrary.importSession(chainExport, &restoredChain, &error) &&
                  restoredChain.history.size() == 1 &&
                  monotonicLibrary.sessions().size() == 1,
                  "same-ID import can add missing historical evidence without duplicating current stages");
    const QString parentExport = QDir(temporary.path()).filePath("durable-parent-export.json");
    const auto importedWorkspace = importedChain.workspaces();
    FitCalibrationSession separatelyImportedParent;
    ok &= require(durableLibrary.exportSession(durableRoot.sessionIdentity, parentExport, &error) &&
                  !importedWorkspace.isEmpty() &&
                  importedChain.importSessionIntoWorkspace(parentExport, &importedWorkspace.front(),
                                                           &separatelyImportedParent, &error) &&
                  separatelyImportedParent.sessionIdentity != restoredChain.sessionIdentity &&
                  importedChain.sessions().size() == 2 &&
                  separatelyImportedParent.coarseExperiment.candidates[1].observations.size() == 2 &&
                  importedChain.workspaces().front().featureSessions.size() == 1 &&
                  importedChain.workspaces().front().featureSessions.front().fineExperiment.state == FitEvidenceState::Verified,
                  "same-feature import preserves distinct observed parent and verified child records");
    auto legacyMissing = verifiedChild;
    legacyMissing.sessionIdentity = "legacy-missing-parent";
    legacyMissing.history.clear();
    legacyMissing.hasCoarseExperiment = false;
    auto legacyJson = FitCalibrationSessionJson::toJson(legacyMissing);
    legacyJson.remove("history");
    FitCalibrationSession decodedLegacy;
    FitCalibrationLibrary missingLibrary(QDir(temporary.path()).filePath("missing-lineage"));
    ok &= require(FitCalibrationSessionJson::fromJson(legacyJson, &decodedLegacy, &error) &&
                  decodedLegacy.history.isEmpty() &&
                  missingLibrary.coarseReviewHistory(decodedLegacy).isEmpty() &&
                  decodedLegacy.fineExperiment.state == FitEvidenceState::Verified,
                  "missing legacy parent remains unavailable without fabricated observations");
    auto axleRoot = studRoot;
    axleRoot.sessionIdentity = "axle-root";
    axleRoot.coarseExperiment.artifactIdentity = "technic-axle-coarse-v1";
    axleRoot.coarseExperiment.featureFamily = "TechnicAxle";
    auto axleExtension = axleRoot;
    axleExtension.sessionIdentity = "axle-extension";
    axleExtension.coarseExperiment.artifactIdentity = "technic-axle-extension-v2";
    axleExtension.coarseExperiment.parentArtifactIdentity = axleRoot.coarseExperiment.artifactIdentity;
    ok &= require(reviewLibrary.saveSession(&axleRoot, &error) &&
                  reviewLibrary.saveSession(&axleExtension, &error),
                  "second-family range-extension lineage persists");
    const auto axleHistory = reviewLibrary.coarseReviewHistory(axleExtension);
    ok &= require(axleHistory.size() == 2 &&
                  axleHistory.front().artifactIdentity == axleRoot.coarseExperiment.artifactIdentity &&
                  axleHistory.back().artifactIdentity == axleExtension.coarseExperiment.artifactIdentity,
                  "Technic Axle coarse extension remains independently retrievable");
    axleExtension.history.push_back(axleRoot.coarseExperiment);
    auto axleChild = FitCalibrationLibrary::continuationSession(axleExtension,
        axleExtension.coarseExperiment, axleExtension.coarseExperiment);
    axleChild.fineExperiment.artifactIdentity = "technic-axle-verification-v3";
    axleChild.fineExperiment.parentArtifactIdentity = axleExtension.coarseExperiment.artifactIdentity;
    ok &= require(axleChild.history.size() == 1 &&
                  axleChild.history[0].artifactIdentity == axleRoot.coarseExperiment.artifactIdentity &&
                  axleChild.coarseExperiment.artifactIdentity == axleExtension.coarseExperiment.artifactIdentity &&
                  axleChild.fineExperiment.candidates[1].observations.isEmpty(),
                  "second-family continuation retains source and clears child evidence");
    const auto selectedDurableWorkspace = durableLibrary.workspaces();
    const auto durableParentBeforeImport = reloadedRoot.coarseExperiment.candidates[1].observations.size();
    FitCalibrationSession duplicateImport;
    ok &= require(!selectedDurableWorkspace.isEmpty() &&
                  durableLibrary.importSessionIntoWorkspace(chainExport, &selectedDurableWorkspace.front(),
                                                            &duplicateImport, &error) &&
                  duplicateImport.sessionIdentity == verifiedChild.sessionIdentity,
                  "reimporting an identical full-lineage session is idempotent");
    auto conflictingImport = verifiedChild;
    conflictingImport.fineExperiment.candidates[0].observations.clear();
    const QString conflictChainPath = QDir(temporary.path()).filePath("conflicting-chain.json");
    QFile conflictChainFile(conflictChainPath);
    ok &= require(conflictChainFile.open(QIODevice::WriteOnly) &&
                  conflictChainFile.write(QJsonDocument(FitCalibrationSessionJson::toJson(conflictingImport)).toJson()) > 0,
                  "conflicting import fixture created");
    conflictChainFile.close();
    ok &= require(!durableLibrary.importSessionIntoWorkspace(conflictChainPath,
                  &selectedDurableWorkspace.front(), &duplicateImport, &error) &&
                  error.contains("stable identity") &&
                  durableLibrary.loadSession(firstContinuation.sessionIdentity, &reloadedRoot, &error) &&
                  reloadedRoot.coarseExperiment.candidates[1].observations.size() == durableParentBeforeImport,
                  "same-ID conflicting import cannot overwrite the sole observed parent");
    ok &= require(library.saveSession(&session, &error), "atomic managed session save: " + error);
    auto summaries = library.sessions(&issues);
    ok &= require(summaries.size() == 1 && summaries.front().identity == "stable-session" && summaries.front().displayName.contains("Bambu H2D"), "session discovery and meaningful name");
    FitCalibrationSession loaded;
    ok &= require(library.loadSession(session.sessionIdentity, &loaded, &error) && loaded.fineExperiment.candidates[1].observations.size() == 2 && loaded.process.orientationNotes == session.process.orientationNotes, "session reload preserves physical evidence");
    ok &= require(FitCalibrationLibrary::sessionDisplayName(loaded).contains("Technic Hole — Round Passage")&&!FitCalibrationLibrary::sessionDisplayName(loaded).contains(loaded.sessionIdentity),"managed-session presentation uses feature and stage context rather than an opaque UUID");
    auto inheritedChild=loaded.fineExperiment;inheritedChild.artifactIdentity="standard-stud-male-od-direct-verification-v2";inheritedChild.featureFamily="StandardStud";inheritedChild.featureRole="male";inheritedChild.correctionDimension=FitCorrectionDimension::Diameter;const auto continued=FitCalibrationLibrary::continuationSession(loaded,loaded.fineExperiment,inheritedChild);ok&=require(continued.sessionIdentity!=loaded.sessionIdentity&&continued.process.printerIdentity==loaded.process.printerIdentity&&continued.process.materialIdentity==loaded.process.materialIdentity&&continued.process.profileName==loaded.process.profileName&&continued.process.nozzleDiameterMillimetres==loaded.process.nozzleDiameterMillimetres&&continued.process.layerHeightMillimetres==loaded.process.layerHeightMillimetres&&continued.process.actualPrintedOrientation==loaded.process.actualPrintedOrientation&&continued.coarseExperiment.process.printerIdentity==loaded.process.printerIdentity&&continued.fineExperiment.process.dimensionalCompensationNotes==loaded.process.dimensionalCompensationNotes&&FitCalibrationLibrary::processIdentityFingerprint(continued.process)==FitCalibrationLibrary::processIdentityFingerprint(loaded.process),"child continuation inherits the authoritative manufacturing-process identity and complete context while retaining separate evidence records");ok&=require(FitCalibrationLibrary::sessionDisplayName(continued).contains("Standard Stud — OD")&&FitCalibrationLibrary::sessionDisplayName(continued).contains("Verification")&&FitCalibrationLibrary::sessionDisplayName(continued).contains(loaded.process.printerIdentity),"child-session presentation identifies feature, dimension, stage, and manufacturing context");

    FitProfile profile;
    ok &= require(FitCalibrationLibrary::promoteVerifiedSession(loaded, "H2D PETG LEGO Fit", &profile, &error), "Verified promotion: " + error);
    ok &= require(!profile.profileIdentity.isEmpty() && profile.sourceSessionIdentity == session.sessionIdentity && profile.corrections.size() == 1 &&
                  profile.corrections.front().featureFamily == "RoundTechnicPassage" && profile.corrections.front().featureRole == "female" &&
                  std::abs(profile.corrections.front().valueMillimetres - .2) < 1e-9 && profile.corrections.front().semantics == "female-diameter-clearance" &&
                  profile.corrections.front().printedOrientation == "feature-axis-perpendicular-to-build-plate", "exact correction and applicability semantics");
    auto verifiedCoarse=loaded;verifiedCoarse.sessionIdentity="verified-coarse-technic-session";verifiedCoarse.hasFineExperiment=false;verifiedCoarse.hasCoarseExperiment=true;verifiedCoarse.coarseExperiment=loaded.fineExperiment;verifiedCoarse.coarseExperiment.parentArtifactIdentity.clear();verifiedCoarse.coarseExperiment.artifactIdentity="round-technic-female-parallel-coarse-v1";verifiedCoarse.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;verifiedCoarse.process.orientationNotes="Passage axis parallel to build plate";verifiedCoarse.coarseExperiment.process=verifiedCoarse.process;FitProfile coarseProfile;ok&=require(FitCalibrationLibrary::promoteVerifiedSession(verifiedCoarse,"Verified coarse Technic",&coarseProfile,&error),"Verified coarse evidence promotes without a fine-search stage: "+error);ok&=require(coarseProfile.corrections.size()==1&&coarseProfile.corrections.front().printedOrientation=="feature-axis-parallel-to-build-plate"&&std::abs(coarseProfile.corrections.front().valueMillimetres-.2)<1e-9,"Verified parallel coarse correction retains +0.200 mm and orientation semantics");auto unverifiedCoarse=verifiedCoarse;unverifiedCoarse.coarseExperiment.state=FitEvidenceState::CandidateSelected;FitProfile rejectedCoarse;ok&=require(!FitCalibrationLibrary::promoteVerifiedSession(unverifiedCoarse,"Must reject",&rejectedCoarse,&error)&&error.contains("Verified"),"non-Verified coarse evidence remains ineligible for promotion");auto coarseAccumulation=profile;ok&=require(FitCalibrationLibrary::mergeVerifiedSession(verifiedCoarse,&coarseAccumulation,&error),"Verified coarse correction updates the existing manufacturing-context profile: "+error);ok&=require(coarseAccumulation.corrections.size()==2&&coarseAccumulation.corrections[0].printedOrientation=="feature-axis-perpendicular-to-build-plate"&&coarseAccumulation.corrections[1].printedOrientation=="feature-axis-parallel-to-build-plate","coarse promotion accumulates both orientation-specific corrections without overwrite");
    auto parallelSession=loaded;parallelSession.sessionIdentity="verified-parallel-technic-session";parallelSession.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;parallelSession.process.orientationNotes="Passage axis parallel to build plate";parallelSession.coarseExperiment.process=parallelSession.process;parallelSession.fineExperiment.process=parallelSession.process;parallelSession.fineExperiment.artifactIdentity="round-technic-female-parallel-verification-v2";auto orientationProfile=profile;ok&=require(FitCalibrationLibrary::mergeVerifiedSession(parallelSession,&orientationProfile,&error),"parallel Technic evidence joins the same manufacturing profile: "+error);ok&=require(orientationProfile.corrections.size()==2&&orientationProfile.corrections[0].printedOrientation=="feature-axis-perpendicular-to-build-plate"&&orientationProfile.corrections[1].printedOrientation=="feature-axis-parallel-to-build-plate"&&orientationProfile.corrections[0].calibrationArtifactIdentity!=orientationProfile.corrections[1].calibrationArtifactIdentity,"perpendicular and parallel RoundTechnicPassage corrections coexist without overwrite");auto unverifiedParallel=parallelSession;unverifiedParallel.fineExperiment.state=FitEvidenceState::CandidateSelected;FitProfile unverifiedParallelProfile;ok&=require(!FitCalibrationLibrary::promoteVerifiedSession(unverifiedParallel,"Must remain experimental",&unverifiedParallelProfile,&error),"unverified parallel evidence cannot enter a production Fit Profile");FitCalibrationLibrary orientationLibrary(QDir(temporary.path()).filePath("orientation-workspace"));auto perpendicularSession=loaded;perpendicularSession.sessionIdentity="perpendicular-technic-session";ok&=require(orientationLibrary.saveSession(&perpendicularSession,&error)&&orientationLibrary.saveSession(&parallelSession,&error),"orientation-specific Technic sessions persist independently");const auto orientationWorkspaces=orientationLibrary.workspaces();ok&=require(orientationWorkspaces.size()==1,"orientation-specific evidence shares one manufacturing workspace");ok&=require(!orientationWorkspaces.isEmpty()&&orientationWorkspaces.front().featureSessions.size()==2,"workspace retains both orientation-specific Technic sessions");ok&=require(FitCalibrationLibrary::sessionDisplayName(perpendicularSession).contains("Perpendicular")&&FitCalibrationLibrary::sessionDisplayName(parallelSession).contains("Parallel"),"orientation-specific Technic evidence is visibly distinguished");FitProfile persistedOrientationProfile;if(!orientationWorkspaces.isEmpty())ok&=require(orientationLibrary.saveVerifiedWorkspaceProfile(orientationWorkspaces.front(),"Orientation-specific LEGO Fit",&persistedOrientationProfile,&error),"orientation workspace profile saves: "+error);ok&=require(persistedOrientationProfile.corrections.size()==2,"workspace profile persists both verified orientation-specific entries");
    ok &= require(library.saveProfile(&profile, &error), "profile save"); const auto stableProfileIdentity = profile.profileIdentity;
    FitProfile loadedProfile;
    ok &= require(library.loadProfile(stableProfileIdentity, &loadedProfile, &error) && loadedProfile.profileIdentity == stableProfileIdentity &&
                  loadedProfile.process.printerIdentity == "Bambu H2D" && loadedProfile.process.materialIdentity == "PETG" &&
                  FitCalibrationLibrary::profileCompatibility(loadedProfile, &error), "profile round trip and compatibility");
    auto stale = loadedProfile; stale.corrections.front().regeneratorAlgorithmVersion = "future-v2";
    ok &= require(!FitCalibrationLibrary::profileCompatibility(stale, &error) && error.contains("regenerator"), "deterministic version staleness");
    auto incomplete = loaded; incomplete.fineExperiment.state = FitEvidenceState::CandidateSelected; FitProfile rejected;
    ok &= require(!FitCalibrationLibrary::promoteVerifiedSession(incomplete, "Must reject", &rejected, &error), "non-Verified promotion rejected");
    auto studSession=loaded;studSession.sessionIdentity="verified-stud-height-session";auto&studExperiment=studSession.fineExperiment;studExperiment.featureFamily="StandardStud";studExperiment.featureRole="male";studExperiment.correctionDimension=FitCorrectionDimension::Height;studExperiment.hasRegenerationPrototype=true;studExperiment.regenerationPrototype.evidenceContract="official-ldraw-standard-stud-v1";studExperiment.regenerationPrototype.family=FunctionalInterfaceFamily::StandardStud;for(int i=0;i<studExperiment.candidates.size();++i){auto&candidate=studExperiment.candidates[i];candidate.heightCorrectionMillimetres=.05*double(i-1);candidate.functionalHeightMillimetres=1.6+candidate.heightCorrectionMillimetres;}FitProfile studProfile;ok&=require(FitCalibrationLibrary::promoteVerifiedSession(studSession,"Verified stud height",&studProfile,&error),"Verified stud profile promotion: "+error);ok&=require(studProfile.corrections.size()==1&&studProfile.corrections.front().semantics=="male-stud-height"&&studProfile.corrections.front().correctionContractVersion=="male-stud-height-v1"&&std::abs(studProfile.corrections.front().valueMillimetres)<1e-9&&FitCalibrationLibrary::profileCompatibility(studProfile,&error),"stud height profile retains explicit dimensional semantics and is future-ready");const QString accumulatingIdentity=profile.profileIdentity;ok&=require(FitCalibrationLibrary::mergeVerifiedSession(studSession,&profile,&error),"same-process verified stud evidence joins an existing Fit Profile: "+error);ok&=require(profile.profileIdentity==accumulatingIdentity&&profile.corrections.size()==2&&profile.corrections[1].semantics=="male-stud-height","Fit Profile accumulates separate verified correction contracts without creating another manufacturing profile");auto studOdSession=studSession;studOdSession.sessionIdentity="verified-stud-od-session";studOdSession.fineExperiment.correctionDimension=FitCorrectionDimension::Diameter;for(int i=0;i<studOdSession.fineExperiment.candidates.size();++i){auto&candidate=studOdSession.fineExperiment.candidates[i];candidate.diameterCorrectionMillimetres=.05*double(i-1);candidate.functionalDiameterMillimetres=4.8+candidate.diameterCorrectionMillimetres;}ok&=require(FitCalibrationLibrary::mergeVerifiedSession(studOdSession,&profile,&error),"same-process verified stud OD evidence joins the existing Fit Profile: "+error);ok&=require(profile.profileIdentity==accumulatingIdentity&&profile.corrections.size()==3&&profile.corrections[0].semantics=="female-diameter-clearance"&&profile.corrections[1].semantics=="male-stud-height"&&profile.corrections[2].semantics=="male-stud-diameter"&&FitCalibrationLibrary::profileCompatibility(profile,&error),"one compatible Fit Profile retains independent Technic Hole, Stud Height, and Stud OD corrections");auto mismatchedStud=studSession;mismatchedStud.process.materialIdentity="Different material";mismatchedStud.fineExperiment.process=mismatchedStud.process;ok&=require(!FitCalibrationLibrary::mergeVerifiedSession(mismatchedStud,&profile,&error),"different manufacturing context cannot join the existing Fit Profile");

    FitCalibrationLibrary workspaceLibrary(QDir(temporary.path()).filePath("workspaces"));auto technicFeature=loaded;technicFeature.sessionIdentity="workspace-technic";auto studOdFeature=studSession;studOdFeature.sessionIdentity="workspace-stud-od";studOdFeature.fineExperiment.correctionDimension=FitCorrectionDimension::Diameter;studOdFeature.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;studOdFeature.process.orientationNotes="Independent stud orientation";studOdFeature.fineExperiment.process=studOdFeature.process;auto studHeightFeature=studSession;studHeightFeature.sessionIdentity="workspace-stud-height";ok&=require(workspaceLibrary.saveSession(&technicFeature,&error)&&workspaceLibrary.saveSession(&studOdFeature,&error)&&workspaceLibrary.saveSession(&studHeightFeature,&error),"separate feature evidence records save");const auto workspaces=workspaceLibrary.workspaces(&issues);ok&=require(workspaces.size()==1&&workspaces.front().featureSessions.size()==3&&workspaces.front().displayName.contains("Bambu H2D")&&workspaces.front().displayName.contains("0.40 mm")&&workspaces.front().displayName.contains("0.20 mm"),"one manufacturing workspace contains Technic Hole, Stud OD, and Stud Height evidence");ok&=require(workspaces.front().process.actualPrintedOrientation==FitPrintedOrientation::Unknown&&studOdFeature.process.actualPrintedOrientation!=technicFeature.process.actualPrintedOrientation,"workspace context excludes orientation while feature orientation remains independent");ok&=require(FitCalibrationLibrary::manufacturingContextFingerprint(studOdFeature.process)==FitCalibrationLibrary::manufacturingContextFingerprint(technicFeature.process),"shared manufacturing context is reused despite feature orientation differences");
    FitProfile workspaceProfile;ok&=require(workspaceLibrary.saveVerifiedWorkspaceProfile(workspaces.front(),"Workspace LEGO Fit",&workspaceProfile,&error),"profile creation collects every Verified correction in the manufacturing workspace: "+error);ok&=require(workspaceProfile.corrections.size()==3&&workspaceProfile.profileIdentity.size()>0,"workspace profile contains Technic Hole, Stud OD, and Stud Height corrections");const QString workspaceProfileIdentity=workspaceProfile.profileIdentity;const bool updatedWorkspaceProfile=workspaceLibrary.saveVerifiedWorkspaceProfile(workspaces.front(),"Updated Workspace LEGO Fit",&workspaceProfile,&error);ok&=require(updatedWorkspaceProfile,"recreating a workspace profile succeeds: "+error);ok&=require(workspaceProfile.profileIdentity==workspaceProfileIdentity&&workspaceProfile.name=="Updated Workspace LEGO Fit","recreating a workspace profile preserves its stable identity and updates its presentation name");ok&=require(workspaceLibrary.profiles().size()==1,"recreating a workspace profile does not create separate feature profiles");

    FitCalibrationLibrary attachmentLibrary(QDir(temporary.path()).filePath("attachments"));auto attachmentTechnic=loaded;attachmentTechnic.sessionIdentity="attachment-technic";ok&=require(attachmentLibrary.saveSession(&attachmentTechnic,&error),"attachment workspace seed");auto selectedWorkspaces=attachmentLibrary.workspaces();ok&=require(selectedWorkspaces.size()==1,"selected attachment workspace");auto completeStud=studSession;completeStud.sessionIdentity="complete-stud";const QString completePath=QDir(temporary.path()).filePath("complete-stud.json");QFile completeFile(completePath);ok&=require(completeFile.open(QIODevice::WriteOnly)&&completeFile.write(QJsonDocument(FitCalibrationSessionJson::toJson(completeStud)).toJson())>0,"complete import fixture");completeFile.close();FitCalibrationSession attached;ok&=require(attachmentLibrary.importSessionIntoWorkspace(completePath,&selectedWorkspaces.front(),&attached,&error),"complete matching context joins selected workspace: "+error);ok&=require(attachmentLibrary.workspaces().size()==1&&attachmentLibrary.workspaces().front().featureSessions.size()==2,"complete matching import does not create another workspace");
    FitCalibrationLibrary processIdentityLibrary(QDir(temporary.path()).filePath("process-identity"));auto labeledWorkspaceSession=loaded;labeledWorkspaceSession.sessionIdentity="labeled-workspace";labeledWorkspaceSession.process.profileName="0.20mm Standard @BBL H2D";labeledWorkspaceSession.coarseExperiment.process=labeledWorkspaceSession.process;labeledWorkspaceSession.fineExperiment.process=labeledWorkspaceSession.process;ok&=require(processIdentityLibrary.saveSession(&labeledWorkspaceSession,&error),"display-labeled workspace seed");const auto labeledWorkspaces=processIdentityLibrary.workspaces();auto legacyLabelSession=studSession;legacyLabelSession.sessionIdentity="axle-extension-label-variation";legacyLabelSession.process.profileName="0.20 mm Standard";legacyLabelSession.coarseExperiment.process=legacyLabelSession.process;legacyLabelSession.fineExperiment.process=legacyLabelSession.process;const auto legacyCandidates=legacyLabelSession.fineExperiment.candidates;const QString legacyLabelPath=QDir(temporary.path()).filePath("axle-extension-label-variation.json");QFile legacyLabelFile(legacyLabelPath);ok&=require(legacyLabelFile.open(QIODevice::WriteOnly)&&legacyLabelFile.write(QJsonDocument(FitCalibrationSessionJson::toJson(legacyLabelSession)).toJson())>0,"legacy process-label fixture");legacyLabelFile.close();ok&=require(labeledWorkspaces.size()==1&&FitCalibrationLibrary::processIdentityFingerprint(legacyLabelSession.process)==FitCalibrationLibrary::processIdentityFingerprint(labeledWorkspaces.front().process),"canonical process identity ignores only unit spacing and printer display annotation");ok&=require(processIdentityLibrary.importSessionIntoWorkspace(legacyLabelPath,&labeledWorkspaces.front(),&attached,&error),"compatible process display-label variation imports: "+error);ok&=require(attached.process.profileName=="0.20mm Standard @BBL H2D"&&attached.fineExperiment.candidates.size()==legacyCandidates.size()&&processIdentityLibrary.workspaces().size()==1&&processIdentityLibrary.workspaces().front().featureSessions.size()==2,"compatible child evidence adopts the authoritative selected process label without duplicate workspace or evidence loss");auto genuineProcessMismatch=legacyLabelSession;genuineProcessMismatch.sessionIdentity="genuine-process-mismatch";genuineProcessMismatch.process.profileName="0.12 mm Fine";genuineProcessMismatch.coarseExperiment.process=genuineProcessMismatch.process;genuineProcessMismatch.fineExperiment.process=genuineProcessMismatch.process;const QString genuineMismatchPath=QDir(temporary.path()).filePath("genuine-process-mismatch.json");QFile genuineMismatchFile(genuineMismatchPath);ok&=require(genuineMismatchFile.open(QIODevice::WriteOnly)&&genuineMismatchFile.write(QJsonDocument(FitCalibrationSessionJson::toJson(genuineProcessMismatch)).toJson())>0,"genuine process mismatch fixture");genuineMismatchFile.close();ok&=require(!processIdentityLibrary.importSessionIntoWorkspace(genuineMismatchPath,&labeledWorkspaces.front(),&attached,&error)&&error.contains("process/profile")&&processIdentityLibrary.workspaces().size()==1,"genuinely different process identity is rejected atomically");
    auto incompleteStud=studSession;incompleteStud.sessionIdentity="incomplete-stud";incompleteStud.process.printerIdentity.clear();incompleteStud.process.materialIdentity.clear();incompleteStud.process.profileName.clear();incompleteStud.process.hasNozzleDiameter=false;incompleteStud.process.hasLayerHeight=false;incompleteStud.process.dimensionalCompensationNotes.clear();incompleteStud.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;incompleteStud.process.orientationNotes="Imported feature orientation";incompleteStud.fineExperiment.process=incompleteStud.process;const QString incompletePath=QDir(temporary.path()).filePath("incomplete-stud.json");QFile incompleteFile(incompletePath);ok&=require(incompleteFile.open(QIODevice::WriteOnly)&&incompleteFile.write(QJsonDocument(FitCalibrationSessionJson::toJson(incompleteStud)).toJson())>0,"incomplete import fixture");incompleteFile.close();selectedWorkspaces=attachmentLibrary.workspaces();ok&=require(attachmentLibrary.importSessionIntoWorkspace(incompletePath,&selectedWorkspaces.front(),&attached,&error),"incomplete feature inherits selected workspace: "+error);const auto attachedWorkspaces=attachmentLibrary.workspaces();ok&=require(attachedWorkspaces.size()==1&&attachedWorkspaces.front().featureSessions.size()==3&&attached.process.printerIdentity=="Bambu H2D"&&attached.process.materialIdentity=="PETG"&&attached.process.hasNozzleDiameter&&attached.process.hasLayerHeight,"incomplete import avoids an Unknown workspace, inherits shared context, and preserves its distinct feature orientation");ok&=require(attached.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate&&attached.process.orientationNotes=="Imported feature orientation","imported feature orientation remains independent");ok&=require(attachedWorkspaces.front().featureSessions.front().fineExperiment.candidates[1].observations.size()==2,"existing Technic Hole evidence remains intact");
    auto conflictingStud=incompleteStud;conflictingStud.sessionIdentity="conflicting-stud";conflictingStud.process.materialIdentity="PLA";conflictingStud.fineExperiment.process=conflictingStud.process;const QString conflictPath=QDir(temporary.path()).filePath("conflicting-stud.json");QFile conflictFile(conflictPath);ok&=require(conflictFile.open(QIODevice::WriteOnly)&&conflictFile.write(QJsonDocument(FitCalibrationSessionJson::toJson(conflictingStud)).toJson())>0,"conflict import fixture");conflictFile.close();selectedWorkspaces=attachmentLibrary.workspaces();ok&=require(!attachmentLibrary.importSessionIntoWorkspace(conflictPath,&selectedWorkspaces.front(),&attached,&error)&&error.contains("conflicts"),"conflicting shared context is rejected clearly");ok&=require(attachmentLibrary.workspaces().size()==1,"conflicting import writes no workspace");

    auto otherContext=attachmentTechnic;otherContext.sessionIdentity="other-context";otherContext.process.materialIdentity="ABS";otherContext.fineExperiment.process=otherContext.process;ok&=require(attachmentLibrary.saveSession(&otherContext,&error),"second manufacturing workspace fixture");const auto twoWorkspaces=attachmentLibrary.workspaces();const auto otherWorkspace=std::find_if(twoWorkspaces.cbegin(),twoWorkspaces.cend(),[](const auto&workspace){return workspace.process.materialIdentity=="ABS";});ok&=require(otherWorkspace!=twoWorkspaces.cend()&&attachmentLibrary.importSessionIntoWorkspace(completePath,&*otherWorkspace,&attached,&error),"complete import follows its matching workspace instead of the unrelated selection: "+error);ok&=require(attachmentLibrary.workspaces().size()==2,"complete matching import does not create an Unknown or third workspace");

    const auto currentWorkspaces=attachmentLibrary.workspaces();const auto bambuWorkspace=std::find_if(currentWorkspaces.cbegin(),currentWorkspaces.cend(),[](const auto&workspace){return workspace.process.materialIdentity=="PETG";});auto placeholderStud=studSession;placeholderStud.sessionIdentity="placeholder-stud";placeholderStud.process.printerIdentity="Unknown printer";placeholderStud.process.materialIdentity="Unspecified";placeholderStud.process.profileName="Not recorded";placeholderStud.process.hasNozzleDiameter=false;placeholderStud.process.hasLayerHeight=false;placeholderStud.process.dimensionalCompensationNotes="Print at 100% scale and record all slicer dimensional compensation settings.";placeholderStud.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;placeholderStud.process.orientationNotes="Recorded physical stud orientation";placeholderStud.fineExperiment.process=placeholderStud.process;const auto originalCandidates=placeholderStud.fineExperiment.candidates;const QString placeholderPath=QDir(temporary.path()).filePath("placeholder-stud.json");QFile placeholderFile(placeholderPath);ok&=require(placeholderFile.open(QIODevice::WriteOnly)&&placeholderFile.write(QJsonDocument(FitCalibrationSessionJson::toJson(placeholderStud)).toJson())>0,"placeholder import fixture");placeholderFile.close();ok&=require(bambuWorkspace!=currentWorkspaces.cend()&&attachmentLibrary.importSessionIntoWorkspace(placeholderPath,&*bambuWorkspace,&attached,&error),"legacy instructional context inherits selected workspace: "+error);ok&=require(attached.process.printerIdentity=="Bambu H2D"&&attached.process.materialIdentity=="PETG"&&attached.process.profileName=="0.20 mm Standard"&&attached.process.dimensionalCompensationNotes=="Defaults; no explicit compensation","placeholder and unknown shared fields normalize to authoritative workspace context");ok&=require(attached.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate&&attached.process.orientationNotes=="Recorded physical stud orientation"&&attached.fineExperiment.parentArtifactIdentity==placeholderStud.fineExperiment.parentArtifactIdentity&&attached.fineExperiment.candidates.size()==originalCandidates.size()&&std::abs(attached.fineExperiment.candidates[1].functionalHeightMillimetres-originalCandidates[1].functionalHeightMillimetres)<1e-9,"feature-specific orientation, lineage, and candidates survive normalization");ok&=require(attachmentLibrary.workspaces().size()==2,"placeholder import creates no Unknown workspace");

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
    auto dependentHeightSession=studSession;dependentHeightSession.sessionIdentity="dependent-height-profile";dependentHeightSession.fineExperiment.fixedDiameterCorrectionMillimetres=.35;FitProfile dependentHeightProfile;ok&=require(FitCalibrationLibrary::promoteVerifiedSession(dependentHeightSession,"Dependent stud height",&dependentHeightProfile,&error),"Stud Height dependency promotion: "+error);ok&=require(dependentHeightProfile.corrections.size()==1&&dependentHeightProfile.corrections.front().hasRequiredDiameterCorrection&&std::abs(dependentHeightProfile.corrections.front().requiredDiameterCorrectionMillimetres-.35)<1e-9,"Stud Height profile correction retains the fixed Verified OD context");FitProfile restoredDependentHeight;ok&=require(FitProfileJson::fromJson(FitProfileJson::toJson(dependentHeightProfile),&restoredDependentHeight,&error)&&restoredDependentHeight.corrections.front().hasRequiredDiameterCorrection&&std::abs(restoredDependentHeight.corrections.front().requiredDiameterCorrectionMillimetres-.35)<1e-9,"Stud Height OD dependency survives Fit Profile JSON round trip");auto legacyHeightJson=FitProfileJson::toJson(dependentHeightProfile);auto legacyCorrections=legacyHeightJson.value("corrections").toArray();auto legacyHeightCorrection=legacyCorrections.at(0).toObject();legacyHeightCorrection.remove("requiredDiameterCorrectionMillimetres");legacyCorrections[0]=legacyHeightCorrection;legacyHeightJson["corrections"]=legacyCorrections;FitProfile legacyHeightProfile;ok&=require(FitProfileJson::fromJson(legacyHeightJson,&legacyHeightProfile,&error)&&!legacyHeightProfile.corrections.front().hasRequiredDiameterCorrection,"legacy Height correction remains readable but explicitly lacks authoritative OD dependency context");
    FitCalibrationLibrary dependencyRecoveryLibrary(QDir(temporary.path()).filePath("dependency-recovery"));ok&=require(dependencyRecoveryLibrary.saveSession(&dependentHeightSession,&error),"managed Stud Height evidence saved for legacy profile recovery");auto legacyStoredProfile=dependentHeightProfile;legacyStoredProfile.corrections.front().hasRequiredDiameterCorrection=false;legacyStoredProfile.corrections.front().requiredDiameterCorrectionMillimetres=0;ok&=require(dependencyRecoveryLibrary.saveProfile(&legacyStoredProfile,&error),"legacy dependency-less profile fixture saved");FitProfile recoveredDependentHeight;ok&=require(dependencyRecoveryLibrary.loadProfile(legacyStoredProfile.profileIdentity,&recoveredDependentHeight,&error)&&recoveredDependentHeight.corrections.front().hasRequiredDiameterCorrection&&std::abs(recoveredDependentHeight.corrections.front().requiredDiameterCorrectionMillimetres-.35)<1e-9,"existing managed profile recovers Stud Height dependency from its matching evidence artifact without rewriting source data");
    auto receiverSession=studSession;receiverSession.sessionIdentity="verified-tube-wall-cell-session";receiverSession.fineExperiment.featureFamily="StudReceivingClutch";receiverSession.fineExperiment.featureRole="female";receiverSession.fineExperiment.correctionDimension=FitCorrectionDimension::Diameter;receiverSession.fineExperiment.regenerationPrototype.family=FunctionalInterfaceFamily::StudReceivingClutch;receiverSession.fineExperiment.regenerationPrototype.role=FunctionalInterfaceRole::Female;receiverSession.fineExperiment.regenerationPrototype.materialSide=FunctionalMaterialSide::MaterialInside;receiverSession.fineExperiment.regenerationPrototype.evidenceContract="official-ldraw-stud4-tube-wall-cell-v1";receiverSession.fineExperiment.regenerationPrototype.constructionRecipe="stud-receiving-tube-wall-cell-v1";FitProfile receiverProfile;ok&=require(FitCalibrationLibrary::promoteVerifiedSession(receiverSession,"Future TubeWallCell profile",&receiverProfile,&error),"Verified TubeWallCell evidence can be retained independently: "+error);ok&=require(receiverProfile.corrections.size()==1&&receiverProfile.corrections.front().semantics=="female-stud-receiver-tube-od"&&receiverProfile.corrections.front().correctionContractVersion=="female-stud-receiver-tube-od-v1"&&FitCalibrationLibrary::profileCompatibility(receiverProfile,&error),"future TubeWallCell correction round-trips as an independent compatible Fit Profile contract");
    ok&=require(FitCalibrationLibrary::sessionDisplayName(receiverSession).contains("Stud Receiving Clutch")&&FitCalibrationLibrary::sessionDisplayName(receiverSession).contains("Tube Wall Cell"),"managed workspace presents TubeWallCell as a distinct feature calibration");
    auto wallPocketSession = receiverSession;
    wallPocketSession.sessionIdentity = "synthetic-wall-pocket-session";
    wallPocketSession.fineExperiment.artifactIdentity = "wall-pocket-plate-verification-test";
    wallPocketSession.fineExperiment.regenerationPrototype.constructionRecipe = "stud-receiving-wall-pocket-square-v1";
    wallPocketSession.fineExperiment.regenerationPrototype.evidenceContract = "official-ldraw-box5-wall-pocket-plate-v1";
    wallPocketSession.fineExperiment.regenerationPrototype.materialSide = FunctionalMaterialSide::EmptyInsideMaterialOutside;
    wallPocketSession.fineExperiment.regenerationPrototype.operandAction = FunctionalOperandAction::Subtract;
    FitProfile wallPocketProfile;
    ok &= require(FitCalibrationLibrary::promoteVerifiedSession(wallPocketSession,"Synthetic WallPocket profile",&wallPocketProfile,&error) &&
                  wallPocketProfile.corrections.size()==1 &&
                  wallPocketProfile.corrections.front().semantics=="female-stud-receiver-wall-pocket-opening-width" &&
                  FitCalibrationLibrary::profileCompatibility(wallPocketProfile,&error),
                  "WallPocket evidence promotes through a distinct Verified profile contract: "+error);
    ok &= require(FitCalibrationLibrary::sessionDisplayName(wallPocketSession).contains("Wall Pocket"),
                  "managed workspace names WallPocket separately from TubeWallCell and PostWallCell");
    auto antiStudSession=wallPocketSession;
    antiStudSession.sessionIdentity="synthetic-antistud-bore-session";
    antiStudSession.fineExperiment.artifactIdentity="antistud-bore-verification-test";
    antiStudSession.fineExperiment.regenerationPrototype.constructionRecipe="stud-receiving-antistud-bore-v1";
    antiStudSession.fineExperiment.regenerationPrototype.evidenceContract="official-ldraw-stud4o-antistud-bore-v1";
    FitProfile antiStudProfile;
    ok&=require(FitCalibrationLibrary::promoteVerifiedSession(antiStudSession,"Synthetic AntiStudBore profile",&antiStudProfile,&error) &&
                antiStudProfile.corrections.size()==1 &&
                antiStudProfile.corrections.front().semantics=="female-stud-receiver-antistud-bore-diameter" &&
                FitCalibrationLibrary::profileCompatibility(antiStudProfile,&error),
                "synthetic AntiStudBore evidence has a distinct compatible profile contract: "+error);
    ok&=require(FitCalibrationLibrary::sessionDisplayName(antiStudSession).contains("Anti-Stud Bore"),
                "managed workspace names AntiStudBore separately from other clutch mechanisms");
    auto platePocketSession = wallPocketSession;
    platePocketSession.sessionIdentity = "synthetic-plate-wall-pocket-session";
    platePocketSession.fineExperiment.artifactIdentity = "wall-pocket-plate-verification-test";
    platePocketSession.fineExperiment.regenerationPrototype.evidenceContract =
        "official-ldraw-box5-wall-pocket-plate-v1";
    auto brickPocketSession = wallPocketSession;
    brickPocketSession.sessionIdentity = "synthetic-brick-wall-pocket-session";
    brickPocketSession.fineExperiment.artifactIdentity = "wall-pocket-brick-verification-test";
    brickPocketSession.fineExperiment.regenerationPrototype.evidenceContract =
        "official-ldraw-box5-wall-pocket-brick-v1";
    FitProfile depthProfile = wallPocketProfile;
    depthProfile.corrections.front().semanticContractVersion =
        "official-ldraw-box5-wall-pocket-brick-v1";
    ok &= require(FitCalibrationLibrary::mergeVerifiedSession(platePocketSession,&depthProfile,&error) &&
                  depthProfile.corrections.size()==2 &&
                  depthProfile.corrections[0].semanticContractVersion != depthProfile.corrections[1].semanticContractVersion &&
                  FitCalibrationLibrary::profileCompatibility(depthProfile,&error),
                  "brick and plate WallPocket evidence remain separate corrections in one managed profile: "+error);
    FitCalibrationLibrary depthLibrary(QDir(temporary.path()).filePath("wall-pocket-depth-workspace"));
    ok &= require(depthLibrary.saveSession(&brickPocketSession,&error) &&
                  depthLibrary.saveSession(&platePocketSession,&error),
                  "both depth-specific WallPocket sessions persist independently: "+error);
    bool bothDepthsVisible = false;
    for (const auto& workspace : depthLibrary.workspaces())
        if (workspace.featureSessions.size()==2) bothDepthsVisible = true;
    ok &= require(bothDepthsVisible,
                  "managed workspace keeps brick-depth and plate-depth WallPocket evidence visible separately");
    bool mixedDepthHistory = false;
    for (const auto& stage : depthLibrary.coarseReviewHistory(brickPocketSession))
        mixedDepthHistory |= stage.artifactIdentity == platePocketSession.fineExperiment.artifactIdentity;
    ok &= require(!mixedDepthHistory,
                  "brick-depth review history does not borrow plate-depth physical evidence");
    auto postSession=receiverSession;postSession.sessionIdentity="verified-post-wall-cell-session";postSession.fineExperiment.artifactIdentity="stud-receiving-clutch-post-wall-cell-verification-v2";postSession.fineExperiment.regenerationPrototype.nominalRadiusMillimetres=1.6;postSession.fineExperiment.regenerationPrototype.nominalDiameterMillimetres=3.2;postSession.fineExperiment.regenerationPrototype.protectedInnerRadiusMillimetres=0;postSession.fineExperiment.regenerationPrototype.evidenceContract="official-ldraw-stud3-post-wall-cell-v1";postSession.fineExperiment.regenerationPrototype.constructionRecipe="stud-receiving-post-wall-cell-v1";FitProfile postProfile;ok&=require(FitCalibrationLibrary::promoteVerifiedSession(postSession,"Future PostWallCell profile",&postProfile,&error),"Verified PostWallCell evidence can be retained as its own correction contract: "+error);ok&=require(postProfile.corrections.size()==1&&postProfile.corrections.front().semantics=="female-stud-receiver-post-od"&&postProfile.corrections.front().correctionContractVersion=="female-stud-receiver-post-od-v1"&&FitCalibrationLibrary::profileCompatibility(postProfile,&error),"PostWallCell correction round-trips independently from TubeWallCell");ok&=require(FitCalibrationLibrary::sessionDisplayName(postSession).contains("Post Wall Cell"),"managed workspace identifies PostWallCell as a distinct receiving-clutch variant");
    auto unverifiedPost=postSession;unverifiedPost.fineExperiment.state=FitEvidenceState::Draft;FitProfile unavailablePost;ok&=require(!FitCalibrationLibrary::promoteVerifiedSession(unverifiedPost,"Unverified PostWallCell",&unavailablePost,&error)&&unavailablePost.corrections.isEmpty(),"unverified PostWallCell evidence remains unavailable to Fit Profile and ManufacturingMesh selection");
    auto pinSession=studSession;pinSession.sessionIdentity="verified-frictionless-pin-session";pinSession.fineExperiment.featureFamily="FrictionlessTechnicPin";pinSession.fineExperiment.featureRole="male";pinSession.fineExperiment.correctionDimension=FitCorrectionDimension::Diameter;pinSession.fineExperiment.regenerationPrototype.family=FunctionalInterfaceFamily::FrictionlessTechnicPin;pinSession.fineExperiment.regenerationPrototype.role=FunctionalInterfaceRole::Male;pinSession.fineExperiment.regenerationPrototype.evidenceContract="official-ldraw-connect-frictionless-pin-v1";pinSession.fineExperiment.regenerationPrototype.constructionRecipe="frictionless-technic-pin-connect-v1";FitProfile pinProfile;ok&=require(FitCalibrationLibrary::promoteVerifiedSession(pinSession,"Future frictionless pin profile",&pinProfile,&error)&&pinProfile.corrections.front().semantics=="male-frictionless-technic-pin-envelope-diameter"&&FitCalibrationLibrary::profileCompatibility(pinProfile,&error),"future Verified frictionless-pin evidence retains an independent compatible Fit Profile contract");
    const QString defaultRoot = FitCalibrationLibrary::defaultStorageRoot();
    ok &= require(QDir(defaultRoot).dirName() == "LegoCalibration" && QDir::isAbsolutePath(defaultRoot), "cross-platform default path resolution");
    auto frictionSession=pinSession;frictionSession.sessionIdentity="verified-friction-pin-session";frictionSession.fineExperiment.featureFamily="FrictionTechnicPin";frictionSession.fineExperiment.regenerationPrototype.family=FunctionalInterfaceFamily::FrictionTechnicPin;frictionSession.fineExperiment.regenerationPrototype.evidenceContract="official-ldraw-confric5-friction-pin-v1";frictionSession.fineExperiment.regenerationPrototype.constructionRecipe="friction-technic-pin-confric5-ridge-v1";FitProfile frictionProfile;ok&=require(FitCalibrationLibrary::promoteVerifiedSession(frictionSession,"Future friction pin profile",&frictionProfile,&error)&&frictionProfile.corrections.front().semantics=="male-friction-technic-pin-ridge-envelope-diameter"&&FitCalibrationLibrary::profileCompatibility(frictionProfile,&error),"future Verified friction-pin evidence retains a distinct compatible Fit Profile contract");
    auto axleSession=pinSession;axleSession.sessionIdentity="verified-technic-axle-session";axleSession.fineExperiment.featureFamily="TechnicAxle";axleSession.fineExperiment.featureRole="male";axleSession.fineExperiment.regenerationPrototype.family=FunctionalInterfaceFamily::TechnicAxle;axleSession.fineExperiment.regenerationPrototype.role=FunctionalInterfaceRole::Male;axleSession.fineExperiment.regenerationPrototype.materialSide=FunctionalMaterialSide::MaterialInside;axleSession.fineExperiment.regenerationPrototype.evidenceContract="official-ldraw-axle-cross-profile-v1";axleSession.fineExperiment.regenerationPrototype.constructionRecipe="technic-axle-cross-profile-v1";FitProfile axleProfile;ok&=require(FitCalibrationLibrary::promoteVerifiedSession(axleSession,"Future ordinary axle profile",&axleProfile,&error)&&axleProfile.corrections.front().semantics=="male-technic-axle-tip-to-tip-envelope"&&axleProfile.corrections.front().correctionContractVersion=="male-technic-axle-tip-to-tip-envelope-v1"&&FitCalibrationLibrary::profileCompatibility(axleProfile,&error),"future Verified ordinary axle evidence retains an independent compatible Fit Profile contract");auto axleRoundTrip=FitProfile{};ok&=require(FitProfileJson::fromJson(FitProfileJson::toJson(axleProfile),&axleRoundTrip,&error)&&axleRoundTrip.corrections.front().featureFamily=="TechnicAxle","ordinary axle Fit Profile contract survives JSON round trip");
    auto axleHoleSession=axleSession;axleHoleSession.sessionIdentity="verified-technic-axle-hole-session";axleHoleSession.fineExperiment.featureFamily="TechnicAxleHole";axleHoleSession.fineExperiment.featureRole="female";axleHoleSession.fineExperiment.regenerationPrototype.family=FunctionalInterfaceFamily::TechnicAxleHole;axleHoleSession.fineExperiment.regenerationPrototype.role=FunctionalInterfaceRole::Female;axleHoleSession.fineExperiment.regenerationPrototype.materialSide=FunctionalMaterialSide::EmptyInsideMaterialOutside;axleHoleSession.fineExperiment.regenerationPrototype.evidenceContract="official-ldraw-axlehole-cross-profile-v1";axleHoleSession.fineExperiment.regenerationPrototype.constructionRecipe="technic-axle-hole-cross-profile-v1";FitProfile axleHoleProfile;ok&=require(FitCalibrationLibrary::promoteVerifiedSession(axleHoleSession,"Future ordinary axle-hole profile",&axleHoleProfile,&error)&&axleHoleProfile.corrections.front().semantics=="female-technic-axle-hole-tip-to-tip-clearance"&&FitCalibrationLibrary::profileCompatibility(axleHoleProfile,&error),"future Verified ordinary axle-hole evidence remains distinct from the male axle correction");
    auto unverifiedAxleHole = axleHoleSession;
    unverifiedAxleHole.fineExperiment.state = FitEvidenceState::CandidateSelected;
    FitProfile unavailableAxleHoleProfile;
    ok &= require(!FitCalibrationLibrary::promoteVerifiedSession(unverifiedAxleHole, "Unverified axle-hole profile", &unavailableAxleHoleProfile, &error),
                  "unverified Technic axle-hole evidence remains unavailable to Fit Profiles and ManufacturingMesh");
    auto armWidthSession = axleHoleSession;
    armWidthSession.sessionIdentity = "verified-technic-axle-hole-arm-width-session";
    armWidthSession.fineExperiment.artifactIdentity = "technic-axle-hole-arm-width-perpendicular-coarse-v2";
    armWidthSession.fineExperiment.regenerationPrototype.constructionRecipe = "technic-axle-hole-arm-width-clearance-v2";
    armWidthSession.fineExperiment.regenerationPrototype.evidenceContract = "official-ldraw-axlehole-arm-width-clearance-v2";
    armWidthSession.fineExperiment.fixedDiameterCorrectionMillimetres = .30;
    FitProfile armWidthProfile;
    ok &= require(FitCalibrationLibrary::promoteVerifiedSession(armWidthSession, "Future axle-hole arm-width profile", &armWidthProfile, &error) &&
                  armWidthProfile.corrections.front().semantics == "female-technic-axle-hole-arm-width-clearance" &&
                  armWidthProfile.corrections.front().hasFixedTipToTipCorrection &&
                  std::abs(armWidthProfile.corrections.front().fixedTipToTipCorrectionMillimetres - .30) < 1e-9 &&
                  FitCalibrationLibrary::profileCompatibility(armWidthProfile, &error),
                  "corrected axle-hole arm-width evidence has a distinct future-ready Fit Profile contract");
    ok &= require(FitCalibrationLibrary::mergeVerifiedSession(armWidthSession, &axleProfile, &error) &&
                  axleProfile.corrections.size() == 2 && FitCalibrationLibrary::profileCompatibility(axleProfile, &error),
                  "Verified male axle and female arm-width corrections coexist independently in one Fit Profile");
    auto incompleteAxleHoleContext = axleProfile;
    incompleteAxleHoleContext.corrections.back().hasFixedTipToTipCorrection = false;
    ok &= require(FitCalibrationLibrary::profileCompatibility(incompleteAxleHoleContext, &error),
                  "missing dependent axle-hole context does not hide independent corrections in a multi-family Fit Profile");
    auto armWidthRoundTrip = FitProfile{};
    ok &= require(FitProfileJson::fromJson(FitProfileJson::toJson(armWidthProfile), &armWidthRoundTrip, &error) &&
                  armWidthRoundTrip.corrections.front().hasFixedTipToTipCorrection &&
                  std::abs(armWidthRoundTrip.corrections.front().fixedTipToTipCorrectionMillimetres - .30) < 1e-9,
                  "female v2 fixed tip-to-tip calibration context survives Fit Profile JSON round trip");
    return ok ? 0 : 1;
}
