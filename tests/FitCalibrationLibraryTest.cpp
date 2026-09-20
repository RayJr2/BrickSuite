#include "../src/services/geometry/fit/FitCalibrationLibrary.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
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
    ok &= require(FitCalibrationLibrary::sessionDisplayName(loaded).contains("Round Technic Passage")&&!FitCalibrationLibrary::sessionDisplayName(loaded).contains(loaded.sessionIdentity),"managed-session presentation uses feature and stage context rather than an opaque UUID");
    auto inheritedChild=loaded.fineExperiment;inheritedChild.artifactIdentity="standard-stud-male-od-direct-verification-v2";inheritedChild.featureFamily="StandardStud";inheritedChild.featureRole="male";inheritedChild.correctionDimension=FitCorrectionDimension::Diameter;const auto continued=FitCalibrationLibrary::continuationSession(loaded,loaded.fineExperiment,inheritedChild);ok&=require(continued.sessionIdentity!=loaded.sessionIdentity&&continued.process.printerIdentity==loaded.process.printerIdentity&&continued.process.materialIdentity==loaded.process.materialIdentity&&continued.process.profileName==loaded.process.profileName&&continued.process.nozzleDiameterMillimetres==loaded.process.nozzleDiameterMillimetres&&continued.process.layerHeightMillimetres==loaded.process.layerHeightMillimetres&&continued.process.actualPrintedOrientation==loaded.process.actualPrintedOrientation&&continued.coarseExperiment.process.printerIdentity==loaded.process.printerIdentity&&continued.fineExperiment.process.dimensionalCompensationNotes==loaded.process.dimensionalCompensationNotes,"child continuation inherits the complete manufacturing context while retaining separate evidence records");ok&=require(FitCalibrationLibrary::sessionDisplayName(continued).contains("Standard Stud OD")&&FitCalibrationLibrary::sessionDisplayName(continued).contains("Verification")&&FitCalibrationLibrary::sessionDisplayName(continued).contains(loaded.process.printerIdentity),"child-session presentation identifies feature, dimension, stage, and manufacturing context");

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
    auto studSession=loaded;studSession.sessionIdentity="verified-stud-height-session";auto&studExperiment=studSession.fineExperiment;studExperiment.featureFamily="StandardStud";studExperiment.featureRole="male";studExperiment.correctionDimension=FitCorrectionDimension::Height;studExperiment.hasRegenerationPrototype=true;studExperiment.regenerationPrototype.evidenceContract="official-ldraw-standard-stud-v1";studExperiment.regenerationPrototype.family=FunctionalInterfaceFamily::StandardStud;for(int i=0;i<studExperiment.candidates.size();++i){auto&candidate=studExperiment.candidates[i];candidate.heightCorrectionMillimetres=.05*double(i-1);candidate.functionalHeightMillimetres=1.6+candidate.heightCorrectionMillimetres;}FitProfile studProfile;ok&=require(FitCalibrationLibrary::promoteVerifiedSession(studSession,"Verified stud height",&studProfile,&error),"Verified stud profile promotion: "+error);ok&=require(studProfile.corrections.size()==1&&studProfile.corrections.front().semantics=="male-stud-height"&&studProfile.corrections.front().correctionContractVersion=="male-stud-height-v1"&&std::abs(studProfile.corrections.front().valueMillimetres)<1e-9&&FitCalibrationLibrary::profileCompatibility(studProfile,&error),"stud height profile retains explicit dimensional semantics and is future-ready");const QString accumulatingIdentity=profile.profileIdentity;ok&=require(FitCalibrationLibrary::mergeVerifiedSession(studSession,&profile,&error),"same-process verified stud evidence joins an existing Fit Profile: "+error);ok&=require(profile.profileIdentity==accumulatingIdentity&&profile.corrections.size()==2&&profile.corrections[1].semantics=="male-stud-height","Fit Profile accumulates separate verified correction contracts without creating another manufacturing profile");auto mismatchedStud=studSession;mismatchedStud.process.materialIdentity="Different material";mismatchedStud.fineExperiment.process=mismatchedStud.process;ok&=require(!FitCalibrationLibrary::mergeVerifiedSession(mismatchedStud,&profile,&error),"different manufacturing context cannot join the existing Fit Profile");

    FitCalibrationLibrary workspaceLibrary(QDir(temporary.path()).filePath("workspaces"));auto technicFeature=loaded;technicFeature.sessionIdentity="workspace-technic";auto studOdFeature=studSession;studOdFeature.sessionIdentity="workspace-stud-od";studOdFeature.fineExperiment.correctionDimension=FitCorrectionDimension::Diameter;studOdFeature.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;studOdFeature.process.orientationNotes="Independent stud orientation";studOdFeature.fineExperiment.process=studOdFeature.process;auto studHeightFeature=studSession;studHeightFeature.sessionIdentity="workspace-stud-height";ok&=require(workspaceLibrary.saveSession(&technicFeature,&error)&&workspaceLibrary.saveSession(&studOdFeature,&error)&&workspaceLibrary.saveSession(&studHeightFeature,&error),"separate feature evidence records save");const auto workspaces=workspaceLibrary.workspaces(&issues);ok&=require(workspaces.size()==1&&workspaces.front().featureSessions.size()==3&&workspaces.front().displayName.contains("Bambu H2D")&&workspaces.front().displayName.contains("0.40 mm")&&workspaces.front().displayName.contains("0.20 mm"),"one manufacturing workspace contains Technic Hole, Stud OD, and Stud Height evidence");ok&=require(workspaces.front().process.actualPrintedOrientation==FitPrintedOrientation::Unknown&&studOdFeature.process.actualPrintedOrientation!=technicFeature.process.actualPrintedOrientation,"workspace context excludes orientation while feature orientation remains independent");ok&=require(FitCalibrationLibrary::manufacturingContextFingerprint(studOdFeature.process)==FitCalibrationLibrary::manufacturingContextFingerprint(technicFeature.process),"shared manufacturing context is reused despite feature orientation differences");

    FitCalibrationLibrary attachmentLibrary(QDir(temporary.path()).filePath("attachments"));auto attachmentTechnic=loaded;attachmentTechnic.sessionIdentity="attachment-technic";ok&=require(attachmentLibrary.saveSession(&attachmentTechnic,&error),"attachment workspace seed");auto selectedWorkspaces=attachmentLibrary.workspaces();ok&=require(selectedWorkspaces.size()==1,"selected attachment workspace");auto completeStud=studSession;completeStud.sessionIdentity="complete-stud";const QString completePath=QDir(temporary.path()).filePath("complete-stud.json");QFile completeFile(completePath);ok&=require(completeFile.open(QIODevice::WriteOnly)&&completeFile.write(QJsonDocument(FitCalibrationSessionJson::toJson(completeStud)).toJson())>0,"complete import fixture");completeFile.close();FitCalibrationSession attached;ok&=require(attachmentLibrary.importSessionIntoWorkspace(completePath,&selectedWorkspaces.front(),&attached,&error),"complete matching context joins selected workspace: "+error);ok&=require(attachmentLibrary.workspaces().size()==1&&attachmentLibrary.workspaces().front().featureSessions.size()==2,"complete matching import does not create another workspace");
    auto incompleteStud=studSession;incompleteStud.sessionIdentity="incomplete-stud";incompleteStud.process.printerIdentity.clear();incompleteStud.process.materialIdentity.clear();incompleteStud.process.profileName.clear();incompleteStud.process.hasNozzleDiameter=false;incompleteStud.process.hasLayerHeight=false;incompleteStud.process.dimensionalCompensationNotes.clear();incompleteStud.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;incompleteStud.process.orientationNotes="Imported feature orientation";incompleteStud.fineExperiment.process=incompleteStud.process;const QString incompletePath=QDir(temporary.path()).filePath("incomplete-stud.json");QFile incompleteFile(incompletePath);ok&=require(incompleteFile.open(QIODevice::WriteOnly)&&incompleteFile.write(QJsonDocument(FitCalibrationSessionJson::toJson(incompleteStud)).toJson())>0,"incomplete import fixture");incompleteFile.close();selectedWorkspaces=attachmentLibrary.workspaces();ok&=require(attachmentLibrary.importSessionIntoWorkspace(incompletePath,&selectedWorkspaces.front(),&attached,&error),"incomplete feature inherits selected workspace: "+error);const auto attachedWorkspaces=attachmentLibrary.workspaces();ok&=require(attachedWorkspaces.size()==1&&attachedWorkspaces.front().featureSessions.size()==2&&attached.process.printerIdentity=="Bambu H2D"&&attached.process.materialIdentity=="PETG"&&attached.process.hasNozzleDiameter&&attached.process.hasLayerHeight,"incomplete import avoids an Unknown workspace and inherits authoritative shared context");ok&=require(attached.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate&&attached.process.orientationNotes=="Imported feature orientation","imported feature orientation remains independent");ok&=require(attachedWorkspaces.front().featureSessions.front().fineExperiment.candidates[1].observations.size()==2,"existing Technic Hole evidence remains intact");
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
    const QString defaultRoot = FitCalibrationLibrary::defaultStorageRoot();
    ok &= require(QDir(defaultRoot).dirName() == "LegoCalibration" && QDir::isAbsolutePath(defaultRoot), "cross-platform default path resolution");
    return ok ? 0 : 1;
}
