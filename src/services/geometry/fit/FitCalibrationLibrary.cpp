#include "FitCalibrationLibrary.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace PrintGeometry {
namespace {
void fail(QString* error, const QString& message) { if (error) *error = message; }
QString orientationName(FitPrintedOrientation value) {
    switch (value) {
    case FitPrintedOrientation::FeatureAxisParallelToBuildPlate: return "feature-axis-parallel-to-build-plate";
    case FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate: return "feature-axis-perpendicular-to-build-plate";
    case FitPrintedOrientation::OtherUnsupported: return "other-unsupported";
    default: return "unknown";
    }
}
FitPrintedOrientation orientationValue(const QString& value) {
    if (value == "feature-axis-parallel-to-build-plate") return FitPrintedOrientation::FeatureAxisParallelToBuildPlate;
    if (value == "feature-axis-perpendicular-to-build-plate") return FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;
    if (value == "other-unsupported") return FitPrintedOrientation::OtherUnsupported;
    return FitPrintedOrientation::Unknown;
}
QJsonObject processJson(const FitCalibrationProcess& process) {
    QJsonObject result{{"printerIdentity", process.printerIdentity}, {"materialIdentity", process.materialIdentity},
                       {"profileName", process.profileName}, {"actualPrintedOrientation", orientationName(process.actualPrintedOrientation)},
                       {"orientationNotes", process.orientationNotes}, {"dimensionalCompensationNotes", process.dimensionalCompensationNotes}};
    if (process.hasNozzleDiameter) result["nozzleDiameterMillimetres"] = process.nozzleDiameterMillimetres;
    if (process.hasLayerHeight) result["layerHeightMillimetres"] = process.layerHeightMillimetres;
    return result;
}
FitCalibrationProcess processFromJson(const QJsonObject& json) {
    FitCalibrationProcess result;
    result.printerIdentity = json.value("printerIdentity").toString();
    result.materialIdentity = json.value("materialIdentity").toString();
    result.profileName = json.value("profileName").toString();
    result.actualPrintedOrientation = orientationValue(json.value("actualPrintedOrientation").toString());
    result.orientationNotes = json.value("orientationNotes").toString();
    result.dimensionalCompensationNotes = json.value("dimensionalCompensationNotes").toString();
    result.hasNozzleDiameter = json.contains("nozzleDiameterMillimetres");
    result.nozzleDiameterMillimetres = json.value("nozzleDiameterMillimetres").toDouble();
    result.hasLayerHeight = json.contains("layerHeightMillimetres");
    result.layerHeightMillimetres = json.value("layerHeightMillimetres").toDouble();
    return result;
}
bool writeJson(const QString& path, const QJsonObject& json, QString* error) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(json).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) {
        fail(error, QStringLiteral("Could not atomically write %1.").arg(path)); return false;
    }
    return true;
}
bool readObject(const QString& path, QJsonObject* object, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { fail(error, file.errorString()); return false; }
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) {
        fail(error, QStringLiteral("Invalid JSON: %1").arg(parse.errorString())); return false;
    }
    *object = document.object(); return true;
}
QString fileForIdentity(const QString& directory, const QString& identity) {
    return QDir(directory).filePath(identity + ".json");
}
FitEvidenceState sessionState(const FitCalibrationSession& session) {
    if (session.hasFineExperiment && session.fineExperiment.state == FitEvidenceState::Verified) return FitEvidenceState::Verified;
    if (session.hasCoarseExperiment && session.coarseExperiment.state == FitEvidenceState::Verified) return FitEvidenceState::Verified;
    if (session.hasFineExperiment) return session.fineExperiment.state;
    if (session.hasCoarseExperiment) return session.coarseExperiment.state;
    return FitEvidenceState::Draft;
}
QString legacyProcessFingerprint(const FitCalibrationProcess& process) {
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(processJson(process)).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}
const FitCalibrationExperiment* representativeExperiment(const FitCalibrationSession& session) {
    return session.hasFineExperiment ? &session.fineExperiment : (session.hasCoarseExperiment ? &session.coarseExperiment : nullptr);
}
const FitCalibrationExperiment* verifiedExperiment(const FitCalibrationSession& session) {
    if (session.hasFineExperiment && session.fineExperiment.state == FitEvidenceState::Verified) return &session.fineExperiment;
    if (session.hasCoarseExperiment && session.coarseExperiment.state == FitEvidenceState::Verified) return &session.coarseExperiment;
    return nullptr;
}
bool sameFeature(const FitCalibrationSession& left, const FitCalibrationSession& right) {
    const auto* a = representativeExperiment(left); const auto* b = representativeExperiment(right);
    return a && b && a->featureFamily == b->featureFamily && a->featureRole == b->featureRole && a->correctionDimension == b->correctionDimension
        && left.process.actualPrintedOrientation == right.process.actualPrintedOrientation;
}
bool unresolvedSharedText(const QString& value, bool compensationContext) {
    const QString normalized = value.simplified().toLower();
    if (normalized.isEmpty() || normalized == QStringLiteral("unknown") || normalized == QStringLiteral("unspecified") ||
        normalized == QStringLiteral("not set") || normalized == QStringLiteral("not recorded") || normalized == QStringLiteral("n/a") ||
        (!compensationContext && normalized == QStringLiteral("none")) || normalized.startsWith(QStringLiteral("unknown ")) ||
        normalized.startsWith(QStringLiteral("select ")) || normalized.startsWith(QStringLiteral("enter "))) return true;
    return compensationContext && normalized.startsWith(QStringLiteral("print at 100% scale")) &&
        normalized.contains(QStringLiteral("record")) && normalized.contains(QStringLiteral("compensation"));
}
bool mergeSharedContext(FitCalibrationProcess* imported, const FitCalibrationProcess& selected, QString* error) {
    if (!imported) return false;
    auto mergeText = [&](QString* value, const QString& authority, const QString& label, bool compensationContext = false) {
        if (unresolvedSharedText(*value, compensationContext)) { *value = authority; return true; }
        if (!authority.trimmed().isEmpty() && value->trimmed() != authority.trimmed()) {
            fail(error, QStringLiteral("The imported %1 '%2' conflicts with the selected workspace value '%3'.")
                            .arg(label, value->trimmed(), authority.trimmed())); return false;
        }
        return true;
    };
    if (!mergeText(&imported->printerIdentity, selected.printerIdentity, QStringLiteral("printer")) ||
        !mergeText(&imported->materialIdentity, selected.materialIdentity, QStringLiteral("material")) ||
        !mergeText(&imported->profileName, selected.profileName, QStringLiteral("process/profile")) ||
        !mergeText(&imported->dimensionalCompensationNotes, selected.dimensionalCompensationNotes, QStringLiteral("slicer-compensation context"), true)) return false;
    if (!imported->hasNozzleDiameter) { imported->hasNozzleDiameter = selected.hasNozzleDiameter; imported->nozzleDiameterMillimetres = selected.nozzleDiameterMillimetres; }
    else if (selected.hasNozzleDiameter && std::abs(imported->nozzleDiameterMillimetres - selected.nozzleDiameterMillimetres) > 1e-9) {
        fail(error, QStringLiteral("The imported nozzle diameter conflicts with the selected workspace.")); return false;
    }
    if (!imported->hasLayerHeight) { imported->hasLayerHeight = selected.hasLayerHeight; imported->layerHeightMillimetres = selected.layerHeightMillimetres; }
    else if (selected.hasLayerHeight && std::abs(imported->layerHeightMillimetres - selected.layerHeightMillimetres) > 1e-9) {
        fail(error, QStringLiteral("The imported layer height conflicts with the selected workspace.")); return false;
    }
    return true;
}
}

QJsonObject FitProfileJson::toJson(const FitProfile& profile) {
    QJsonArray corrections;
    for (const auto& correction : profile.corrections) {
        QJsonObject item{
        {"featureFamily", correction.featureFamily}, {"featureRole", correction.featureRole},
        {"printedOrientation", correction.printedOrientation}, {"valueMillimetres", correction.valueMillimetres},
        {"units", correction.units}, {"semantics", correction.semantics},
        {"correctionContractVersion", correction.correctionContractVersion},
        {"semanticContractVersion", correction.semanticContractVersion},
        {"regeneratorAlgorithmVersion", correction.regeneratorAlgorithmVersion},
        {"calibrationArtifactIdentity", correction.calibrationArtifactIdentity}};
        if (correction.hasRequiredDiameterCorrection) {
            item["requiredDiameterCorrectionMillimetres"] = correction.requiredDiameterCorrectionMillimetres;
        }
        corrections.push_back(item);
    }
    return {{"formatVersion", CurrentFormatVersion}, {"documentType", "fit-profile"},
            {"profileIdentity", profile.profileIdentity}, {"name", profile.name},
            {"process", processJson(profile.process)}, {"processFingerprint", profile.processFingerprint},
            {"sourceSessionIdentity", profile.sourceSessionIdentity},
            {"verificationState", profile.verificationState == FitEvidenceState::Verified ? "verified" : "not-verified"},
            {"verifiedUtc", profile.verifiedUtc.toUTC().toString(Qt::ISODateWithMs)}, {"corrections", corrections}};
}

bool FitProfileJson::fromJson(const QJsonObject& json, FitProfile* output, QString* error) {
    if (!output || json.value("formatVersion").toInt() != CurrentFormatVersion ||
        json.value("documentType").toString() != "fit-profile" ||
        !json.value("profileIdentity").isString() || json.value("profileIdentity").toString().isEmpty() ||
        !json.value("name").isString() || !json.value("process").isObject() ||
        !json.value("sourceSessionIdentity").isString() || json.value("sourceSessionIdentity").toString().isEmpty() ||
        !json.value("corrections").isArray()) {
        fail(error, "The Fit Profile document is invalid or unsupported."); return false;
    }
    FitProfile profile;
    profile.profileIdentity = json.value("profileIdentity").toString(); profile.name = json.value("name").toString();
    profile.process = processFromJson(json.value("process").toObject());
    profile.processFingerprint = json.value("processFingerprint").toString();
    profile.sourceSessionIdentity = json.value("sourceSessionIdentity").toString();
    profile.verifiedUtc = QDateTime::fromString(json.value("verifiedUtc").toString(), Qt::ISODateWithMs);
    profile.verificationState = json.value("verificationState").toString("verified") == "verified" ? FitEvidenceState::Verified : FitEvidenceState::Draft;
    for (const auto& value : json.value("corrections").toArray()) {
        if (!value.isObject()) { fail(error, "A Fit Profile correction is malformed."); return false; }
        const auto item = value.toObject(); FitProfileCorrection correction;
        correction.featureFamily = item.value("featureFamily").toString(); correction.featureRole = item.value("featureRole").toString();
        correction.printedOrientation = item.value("printedOrientation").toString(); correction.valueMillimetres = item.value("valueMillimetres").toDouble();
        correction.units = item.value("units").toString(); correction.semantics = item.value("semantics").toString();
        correction.correctionContractVersion = item.value("correctionContractVersion").toString();
        correction.semanticContractVersion = item.value("semanticContractVersion").toString();
        correction.regeneratorAlgorithmVersion = item.value("regeneratorAlgorithmVersion").toString();
        correction.calibrationArtifactIdentity = item.value("calibrationArtifactIdentity").toString();
        if (item.contains("requiredDiameterCorrectionMillimetres")) {
            if (!item.value("requiredDiameterCorrectionMillimetres").isDouble()) {
                fail(error, "A Fit Profile correction dependency is malformed."); return false;
            }
            correction.hasRequiredDiameterCorrection = true;
            correction.requiredDiameterCorrectionMillimetres = item.value("requiredDiameterCorrectionMillimetres").toDouble();
        }
        if (correction.featureFamily.isEmpty() || correction.featureRole.isEmpty() || correction.printedOrientation.isEmpty() ||
            correction.units != "millimetres" || correction.semantics.isEmpty() || correction.calibrationArtifactIdentity.isEmpty()) {
            fail(error, "A Fit Profile correction is incomplete."); return false;
        }
        profile.corrections.push_back(correction);
    }
    if (profile.corrections.isEmpty()) { fail(error, "The Fit Profile contains no corrections."); return false; }
    *output = profile; return true;
}

FitCalibrationLibrary::FitCalibrationLibrary(QString storageRoot)
    : m_root(storageRoot.isEmpty() ? defaultStorageRoot() : QDir::cleanPath(storageRoot)) {}

QString FitCalibrationLibrary::defaultStorageRoot() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("LegoCalibration");
}
QString FitCalibrationLibrary::sessionsDirectory() const { return QDir(m_root).filePath("Sessions"); }
QString FitCalibrationLibrary::profilesDirectory() const { return QDir(m_root).filePath("Profiles"); }
QString FitCalibrationLibrary::newStableIdentity() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString FitCalibrationLibrary::currentSemanticContractVersion() { return "official-ldraw-peghole-pair-v1"; }
QString FitCalibrationLibrary::currentRegeneratorAlgorithmVersion() { return "functional-operand-regenerator-v1"; }
QString FitCalibrationLibrary::sessionDisplayName(const FitCalibrationSession&session){const FitCalibrationExperiment*experiment=session.hasFineExperiment?&session.fineExperiment:(session.hasCoarseExperiment?&session.coarseExperiment:nullptr);if(!experiment)return QStringLiteral("Empty calibration session");QString feature;if(experiment->featureFamily==QStringLiteral("StandardStud")){feature=QStringLiteral("Standard Stud");feature+=experiment->correctionDimension==FitCorrectionDimension::Height?QStringLiteral(" Height"):QStringLiteral(" OD");}else if(experiment->featureFamily==QStringLiteral("StudReceivingClutch"))feature=QStringLiteral("Stud Receiving Clutch — TubeWallCell OD");else if(experiment->featureFamily==QStringLiteral("FrictionlessTechnicPin"))feature=QStringLiteral("Frictionless Technic Pin — Envelope OD");else if(experiment->featureFamily==QStringLiteral("FrictionTechnicPin"))feature=QStringLiteral("Friction Technic Pin — Ridge Envelope OD");else feature=QStringLiteral("Round Technic Passage");const QString orientation=session.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate?QStringLiteral(" — Parallel"):session.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate?QStringLiteral(" — Perpendicular"):QString();QString stage;if(experiment->state==FitEvidenceState::Verified)stage=QStringLiteral("Verified");else if(experiment->artifactIdentity.contains(QStringLiteral("direct-verification")))stage=QStringLiteral("Verification");else if(experiment->artifactIdentity.contains(QStringLiteral("extension")))stage=QStringLiteral("Extended Search");else if(!experiment->parentArtifactIdentity.isEmpty())stage=QStringLiteral("Fine Search / Verification");else stage=QStringLiteral("Coarse Search");QString process;if(!session.process.printerIdentity.isEmpty()||!session.process.materialIdentity.isEmpty()){process=QStringLiteral(" — %1 / %2").arg(session.process.printerIdentity.isEmpty()?QStringLiteral("Unknown printer"):session.process.printerIdentity,session.process.materialIdentity.isEmpty()?QStringLiteral("Unknown material"):session.process.materialIdentity);}return QStringLiteral("%1%2 — %3%4").arg(feature,orientation,stage,process);}
FitCalibrationSession FitCalibrationLibrary::continuationSession(const FitCalibrationSession&parent,const FitCalibrationExperiment&source,FitCalibrationExperiment child){FitCalibrationSession result;result.sessionIdentity=newStableIdentity();result.process=parent.process;result.hasCoarseExperiment=true;result.coarseExperiment=source;result.coarseExperiment.process=result.process;result.hasFineExperiment=true;child.process=result.process;child.state=FitEvidenceState::Draft;child.preferredCandidateIndex=0;for(auto&candidate:child.candidates)candidate.observations.clear();result.fineExperiment=std::move(child);return result;}
QString FitCalibrationLibrary::processFingerprint(const FitCalibrationProcess& process) {
    return manufacturingContextFingerprint(process);
}
QString FitCalibrationLibrary::manufacturingContextFingerprint(const FitCalibrationProcess& process) {
    QJsonObject context{{"printerIdentity", process.printerIdentity}, {"materialIdentity", process.materialIdentity},
                        {"profileName", process.profileName}, {"dimensionalCompensationNotes", process.dimensionalCompensationNotes}};
    if (process.hasNozzleDiameter) context["nozzleDiameterMillimetres"] = process.nozzleDiameterMillimetres;
    if (process.hasLayerHeight) context["layerHeightMillimetres"] = process.layerHeightMillimetres;
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(context).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
}
QString FitCalibrationLibrary::workspaceDisplayName(const FitCalibrationProcess& process) {
    const QString nozzle = process.hasNozzleDiameter ? QString::number(process.nozzleDiameterMillimetres, 'f', 2) + QStringLiteral(" mm") : QStringLiteral("Unknown nozzle");
    const QString layer = process.hasLayerHeight ? QString::number(process.layerHeightMillimetres, 'f', 2) + QStringLiteral(" mm") : QStringLiteral("Unknown layer");
    return QStringLiteral("%1 / %2 / %3 / %4").arg(process.printerIdentity.isEmpty() ? QStringLiteral("Unknown printer") : process.printerIdentity,
                                                     process.materialIdentity.isEmpty() ? QStringLiteral("Unknown material") : process.materialIdentity,
                                                     nozzle, layer);
}

bool FitCalibrationLibrary::saveSession(FitCalibrationSession* session, QString* error) {
    if (!session || (!session->hasCoarseExperiment && !session->hasFineExperiment)) { fail(error, "There is no calibration session to save."); return false; }
    if (session->sessionIdentity.isEmpty()) session->sessionIdentity = newStableIdentity();
    return writeJson(fileForIdentity(sessionsDirectory(), session->sessionIdentity), FitCalibrationSessionJson::toJson(*session), error);
}
bool FitCalibrationLibrary::loadSession(const QString& identity, FitCalibrationSession* session, QString* error) const {
    QJsonObject json; return readObject(fileForIdentity(sessionsDirectory(), identity), &json, error) && FitCalibrationSessionJson::fromJson(json, session, error);
}
QVector<FitSessionSummary> FitCalibrationLibrary::sessions(QVector<FitLibraryIssue>* issues) const {
    QVector<FitSessionSummary> result; QSet<QString> identities;
    for (const auto& info : QDir(sessionsDirectory()).entryInfoList({"*.json"}, QDir::Files, QDir::Name)) {
        QJsonObject json; FitCalibrationSession session; QString error;
        if (!readObject(info.absoluteFilePath(), &json, &error) || !FitCalibrationSessionJson::fromJson(json, &session, &error)) {
            if (issues) issues->push_back({info.absoluteFilePath(), error}); continue;
        }
        if (identities.contains(session.sessionIdentity)) { if (issues) issues->push_back({info.absoluteFilePath(), "Duplicate calibration session identity."}); continue; }
        identities.insert(session.sessionIdentity); result.push_back({session.sessionIdentity, sessionDisplayName(session), info.absoluteFilePath(), sessionState(session)});
    }
    return result;
}
QVector<FitCalibrationWorkspace> FitCalibrationLibrary::workspaces(QVector<FitLibraryIssue>* issues) const {
    QVector<FitCalibrationWorkspace> result;
    QHash<QString, int> byIdentity;
    for (const auto& summary : sessions(issues)) {
        FitCalibrationSession session; QString error;
        if (!loadSession(summary.identity, &session, &error)) { if (issues) issues->push_back({summary.path, error}); continue; }
        const QString identity = manufacturingContextFingerprint(session.process);
        int index = byIdentity.value(identity, -1);
        if (index < 0) {
            index = result.size(); byIdentity.insert(identity, index);
            result.push_back({identity, workspaceDisplayName(session.process), session.process, {}});
            result.back().process.actualPrintedOrientation = FitPrintedOrientation::Unknown;
            result.back().process.orientationNotes.clear();
        }
        auto matchingFeature = std::find_if(result[index].featureSessions.begin(), result[index].featureSessions.end(), [&](const auto& existing) {
            return sameFeature(existing, session);
        });
        auto rank = [](const FitCalibrationSession& item) {
            const auto* stage = item.hasFineExperiment ? &item.fineExperiment : &item.coarseExperiment;
            return (stage->state == FitEvidenceState::Verified ? 100 : 0) + (item.hasFineExperiment ? 20 : 0)
                + (stage->artifactIdentity.contains(QStringLiteral("direct-verification")) ? 4 : 0)
                + (stage->artifactIdentity.contains(QStringLiteral("extension")) ? 2 : 0)
                + (item.hasCoarseExperiment && !item.coarseExperiment.parentArtifactIdentity.isEmpty() ? 1 : 0);
        };
        if (matchingFeature == result[index].featureSessions.end()) result[index].featureSessions.push_back(session);
        else if (rank(session) > rank(*matchingFeature)) *matchingFeature = session;
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) { return left.displayName < right.displayName; });
    return result;
}
bool FitCalibrationLibrary::importSession(const QString& path, FitCalibrationSession* output, QString* error) {
    QJsonObject json; FitCalibrationSession session;
    if (!readObject(path, &json, error) || !FitCalibrationSessionJson::fromJson(json, &session, error)) return false;
    // Legacy artifact identity described the printable artifact, not a durable managed record.
    if (json.value("documentType").toString() != "fit-calibration-session" || session.sessionIdentity.isEmpty()) session.sessionIdentity = newStableIdentity();
    FitCalibrationSession existing;
    if (loadSession(session.sessionIdentity, &existing, nullptr)) { fail(error, "A managed calibration session with this stable identity already exists."); return false; }
    if (!saveSession(&session, error)) return false; if (output) *output = session; return true;
}
bool FitCalibrationLibrary::importSessionIntoWorkspace(const QString& path, const FitCalibrationWorkspace* selectedWorkspace,
                                                       FitCalibrationSession* output, QString* error) {
    QJsonObject json; FitCalibrationSession session;
    if (!readObject(path, &json, error) || !FitCalibrationSessionJson::fromJson(json, &session, error)) return false;
    if (json.value("documentType").toString() != "fit-calibration-session" || session.sessionIdentity.isEmpty()) session.sessionIdentity = newStableIdentity();
    FitCalibrationWorkspace matchingWorkspace;
    const bool completeContext = !session.process.printerIdentity.trimmed().isEmpty() && !session.process.materialIdentity.trimmed().isEmpty()
        && !session.process.profileName.trimmed().isEmpty() && session.process.hasNozzleDiameter && session.process.hasLayerHeight;
    const FitCalibrationWorkspace* targetWorkspace = selectedWorkspace;
    if (completeContext) {
        const QString identity = manufacturingContextFingerprint(session.process);
        const auto available = workspaces();
        const auto match = std::find_if(available.cbegin(), available.cend(), [&](const auto& workspace) { return workspace.identity == identity; });
        if (match != available.cend()) { matchingWorkspace = *match; targetWorkspace = &matchingWorkspace; }
    }
    if (targetWorkspace && !targetWorkspace->featureSessions.isEmpty()) {
        const auto orientation = session.process.actualPrintedOrientation;
        const auto orientationNotes = session.process.orientationNotes;
        if (!mergeSharedContext(&session.process, targetWorkspace->process, error)) return false;
        session.process.actualPrintedOrientation = orientation;
        session.process.orientationNotes = orientationNotes;
        if (session.hasCoarseExperiment) session.coarseExperiment.process = session.process;
        if (session.hasFineExperiment) session.fineExperiment.process = session.process;
        const auto existing = std::find_if(targetWorkspace->featureSessions.cbegin(), targetWorkspace->featureSessions.cend(),
                                           [&](const auto& feature) { return sameFeature(feature, session); });
        if (existing != targetWorkspace->featureSessions.cend()) session.sessionIdentity = existing->sessionIdentity;
    } else {
        FitCalibrationSession existing;
        if (loadSession(session.sessionIdentity, &existing, nullptr)) { fail(error, "A managed calibration session with this stable identity already exists."); return false; }
    }
    if (!saveSession(&session, error)) return false;
    if (output) *output = session;
    return true;
}
bool FitCalibrationLibrary::exportSession(const QString& identity, const QString& path, QString* error) const {
    FitCalibrationSession session; return loadSession(identity, &session, error) && writeJson(path, FitCalibrationSessionJson::toJson(session), error);
}

bool FitCalibrationLibrary::promoteVerifiedSession(const FitCalibrationSession& session, const QString& name, FitProfile* output, QString* error) {
    const auto* verified = verifiedExperiment(session);
    if (!output || session.sessionIdentity.isEmpty() || !verified) {
        fail(error, "Only explicitly Verified calibration evidence can create or update a Fit Profile."); return false;
    }
    QString validation;
    if (!FitCalibrationEvidencePolicy::validate(*verified, &validation)) { fail(error, validation); return false; }
    const auto& experiment = *verified;
    auto evidenceProof = experiment; evidenceProof.state = FitEvidenceState::CandidateSelected; QString evidenceError;
    if (!FitCalibrationEvidencePolicy::markVerified(&evidenceProof, &evidenceError)) {
        fail(error, QStringLiteral("The stored Verified evidence no longer satisfies the verification policy: %1").arg(evidenceError)); return false;
    }
    const auto preferred = std::find_if(experiment.candidates.cbegin(), experiment.candidates.cend(), [&](const auto& candidate) { return candidate.index == experiment.preferredCandidateIndex; });
    if (preferred == experiment.candidates.cend() || session.process.printerIdentity.isEmpty() || session.process.materialIdentity.isEmpty() ||
        session.process.profileName.isEmpty() || !session.process.hasNozzleDiameter ||
        session.process.actualPrintedOrientation == FitPrintedOrientation::Unknown || session.process.actualPrintedOrientation == FitPrintedOrientation::OtherUnsupported) {
        fail(error, "Verified profile promotion requires a preferred correction and complete supported process identity."); return false;
    }
    FitProfile profile; profile.profileIdentity = newStableIdentity();
    profile.name = name.trimmed().isEmpty() ? QStringLiteral("%1 / %2 / %3 mm / LEGO Fit").arg(session.process.printerIdentity, session.process.materialIdentity).arg(session.process.nozzleDiameterMillimetres, 0, 'f', 2) : name.trimmed();
    profile.process = session.process; profile.processFingerprint = processFingerprint(session.process);
    profile.sourceSessionIdentity = session.sessionIdentity;
    FitProfileCorrection correction; correction.featureFamily = experiment.featureFamily; correction.featureRole = experiment.featureRole;
    correction.printedOrientation = orientationName(session.process.actualPrintedOrientation);
    correction.valueMillimetres = FitCalibrationEvidencePolicy::candidateCorrection(experiment, *preferred);
    if (experiment.featureFamily == QStringLiteral("StandardStud")) {
        correction.semantics = experiment.correctionDimension == FitCorrectionDimension::Height
            ? QStringLiteral("male-stud-height") : QStringLiteral("male-stud-diameter");
        correction.correctionContractVersion = experiment.correctionDimension == FitCorrectionDimension::Height
            ? QStringLiteral("male-stud-height-v1") : QStringLiteral("male-stud-diameter-v1");
        if (experiment.correctionDimension == FitCorrectionDimension::Height) {
            correction.hasRequiredDiameterCorrection = true;
            correction.requiredDiameterCorrectionMillimetres = experiment.fixedDiameterCorrectionMillimetres;
        }
    } else if (experiment.featureFamily == QStringLiteral("StudReceivingClutch")) {
        correction.semantics = QStringLiteral("female-stud-receiver-tube-od");
        correction.correctionContractVersion = QStringLiteral("female-stud-receiver-tube-od-v1");
    } else if (experiment.featureFamily == QStringLiteral("FrictionlessTechnicPin")) {
        correction.semantics = QStringLiteral("male-frictionless-technic-pin-envelope-diameter");
        correction.correctionContractVersion = QStringLiteral("male-frictionless-technic-pin-envelope-diameter-v1");
    } else if (experiment.featureFamily == QStringLiteral("FrictionTechnicPin")) {
        correction.semantics = QStringLiteral("male-friction-technic-pin-ridge-envelope-diameter");
        correction.correctionContractVersion = QStringLiteral("male-friction-technic-pin-ridge-envelope-diameter-v1");
    }
    correction.semanticContractVersion = experiment.hasRegenerationPrototype && !experiment.regenerationPrototype.evidenceContract.isEmpty()
        ? experiment.regenerationPrototype.evidenceContract : currentSemanticContractVersion();
    correction.regeneratorAlgorithmVersion = currentRegeneratorAlgorithmVersion();
    correction.calibrationArtifactIdentity = experiment.artifactIdentity;
    for (const auto& observation : preferred->observations)
        if (!profile.verifiedUtc.isValid() || observation.performedUtc > profile.verifiedUtc) profile.verifiedUtc = observation.performedUtc;
    if (!profile.verifiedUtc.isValid()) profile.verifiedUtc = experiment.performedUtc;
    if (!profile.verifiedUtc.isValid()) profile.verifiedUtc = QDateTime::currentDateTimeUtc();
    profile.corrections.push_back(correction); *output = profile; return true;
}
bool FitCalibrationLibrary::mergeVerifiedSession(const FitCalibrationSession&session,FitProfile*profile,QString*error){if(!profile){fail(error,"There is no Fit Profile to update.");return false;}FitProfile addition;if(!promoteVerifiedSession(session,profile->name,&addition,error))return false;if(manufacturingContextFingerprint(profile->process)!=manufacturingContextFingerprint(addition.process)){fail(error,"The verified calibration uses a different manufacturing process.");return false;}profile->processFingerprint=manufacturingContextFingerprint(profile->process);const auto&incoming=addition.corrections.front();const auto sameContract=[&](const FitProfileCorrection&existing){return existing.featureFamily==incoming.featureFamily&&existing.featureRole==incoming.featureRole&&existing.printedOrientation==incoming.printedOrientation&&existing.semantics==incoming.semantics&&existing.correctionContractVersion==incoming.correctionContractVersion;};auto existing=std::find_if(profile->corrections.begin(),profile->corrections.end(),sameContract);if(existing==profile->corrections.end())profile->corrections.push_back(incoming);else *existing=incoming;if(!profile->verifiedUtc.isValid()||addition.verifiedUtc>profile->verifiedUtc)profile->verifiedUtc=addition.verifiedUtc;return true;}
bool FitCalibrationLibrary::saveVerifiedWorkspaceProfile(const FitCalibrationWorkspace&workspace,const QString&profileName,FitProfile*output,QString*error){QVector<FitProfile>matching;for(const auto&summary:profiles()){FitProfile candidate;if(loadProfile(summary.identity,&candidate,nullptr)&&manufacturingContextFingerprint(candidate.process)==workspace.identity)matching.push_back(candidate);}if(matching.size()>1){fail(error,"More than one Fit Profile already uses this manufacturing context. BrickSuite will not guess which profile to update.");return false;}QVector<FitCalibrationSession>verified;for(const auto&session:workspace.featureSessions)if(verifiedExperiment(session))verified.push_back(session);if(verified.isEmpty()){fail(error,"This manufacturing workspace has no Verified calibration evidence.");return false;}FitProfile profile;if(matching.isEmpty()){if(!promoteVerifiedSession(verified.front(),profileName,&profile,error))return false;}else{profile=matching.front();if(!profileName.trimmed().isEmpty())profile.name=profileName.trimmed();}for(const auto&session:verified)if(!mergeVerifiedSession(session,&profile,error))return false;if(!saveProfile(&profile,error))return false;if(output)*output=profile;return true;}
bool FitCalibrationLibrary::saveProfile(FitProfile* profile, QString* error) {
    if (!profile || profile->corrections.isEmpty() || profile->sourceSessionIdentity.isEmpty()) { fail(error, "The Fit Profile is incomplete."); return false; }
    if (profile->profileIdentity.isEmpty()) profile->profileIdentity = newStableIdentity();
    return writeJson(fileForIdentity(profilesDirectory(), profile->profileIdentity), FitProfileJson::toJson(*profile), error);
}
bool FitCalibrationLibrary::loadProfile(const QString& identity, FitProfile* profile, QString* error) const {
    QJsonObject json;if(!readObject(fileForIdentity(profilesDirectory(),identity),&json,error)||!FitProfileJson::fromJson(json,profile,error))return false;
    for(auto&correction:profile->corrections){if(correction.semantics!=QStringLiteral("male-stud-height")||correction.hasRequiredDiameterCorrection)continue;bool found=false;double required=0;for(const auto&summary:sessions()){FitCalibrationSession session;if(!loadSession(summary.identity,&session,nullptr)||manufacturingContextFingerprint(session.process)!=manufacturingContextFingerprint(profile->process))continue;const FitCalibrationExperiment*experiment=session.hasFineExperiment?&session.fineExperiment:(session.hasCoarseExperiment?&session.coarseExperiment:nullptr);if(!experiment||experiment->artifactIdentity!=correction.calibrationArtifactIdentity||experiment->featureFamily!=QStringLiteral("StandardStud")||experiment->correctionDimension!=FitCorrectionDimension::Height)continue;if(found&&std::abs(required-experiment->fixedDiameterCorrectionMillimetres)>1e-9){found=false;break;}required=experiment->fixedDiameterCorrectionMillimetres;found=true;}if(found){correction.hasRequiredDiameterCorrection=true;correction.requiredDiameterCorrectionMillimetres=required;}}
    return true;
}
bool FitCalibrationLibrary::profileCompatibility(const FitProfile& profile, QString* reason) {
    if (profile.verificationState != FitEvidenceState::Verified) { fail(reason, "The Fit Profile is not Verified."); return false; }
    if (profile.processFingerprint != processFingerprint(profile.process) && profile.processFingerprint != legacyProcessFingerprint(profile.process)) { fail(reason, "The stored manufacturing-process fingerprint no longer matches."); return false; }
    for (const auto& correction : profile.corrections) {
        const bool roundPassage = correction.featureFamily == QStringLiteral("RoundTechnicPassage")
            && correction.featureRole == QStringLiteral("female")
            && correction.semanticContractVersion == currentSemanticContractVersion()
            && correction.correctionContractVersion == QStringLiteral("female-diameter-clearance-v1");
        const bool standardStud = correction.featureFamily == QStringLiteral("StandardStud")
            && correction.featureRole == QStringLiteral("male")
            && correction.semanticContractVersion == QStringLiteral("official-ldraw-standard-stud-v1")
            && ((correction.semantics == QStringLiteral("male-stud-diameter") && correction.correctionContractVersion == QStringLiteral("male-stud-diameter-v1"))
                || (correction.semantics == QStringLiteral("male-stud-height") && correction.correctionContractVersion == QStringLiteral("male-stud-height-v1")));
        const bool receivingClutch = correction.featureFamily == QStringLiteral("StudReceivingClutch")
            && correction.featureRole == QStringLiteral("female")
            && correction.semanticContractVersion == QStringLiteral("official-ldraw-stud4-tube-wall-cell-v1")
            && correction.semantics == QStringLiteral("female-stud-receiver-tube-od")
            && correction.correctionContractVersion == QStringLiteral("female-stud-receiver-tube-od-v1");
        const bool frictionlessPin = correction.featureFamily == QStringLiteral("FrictionlessTechnicPin")
            && correction.featureRole == QStringLiteral("male")
            && correction.semanticContractVersion == QStringLiteral("official-ldraw-connect-frictionless-pin-v1")
            && correction.semantics == QStringLiteral("male-frictionless-technic-pin-envelope-diameter")
            && correction.correctionContractVersion == QStringLiteral("male-frictionless-technic-pin-envelope-diameter-v1");
        const bool frictionPin = correction.featureFamily == QStringLiteral("FrictionTechnicPin")
            && correction.featureRole == QStringLiteral("male")
            && correction.semanticContractVersion == QStringLiteral("official-ldraw-confric5-friction-pin-v1")
            && correction.semantics == QStringLiteral("male-friction-technic-pin-ridge-envelope-diameter")
            && correction.correctionContractVersion == QStringLiteral("male-friction-technic-pin-ridge-envelope-diameter-v1");
        if (!roundPassage && !standardStud && !receivingClutch && !frictionlessPin && !frictionPin) { fail(reason, "The functional semantic or correction interpretation contract has changed."); return false; }
        if (correction.regeneratorAlgorithmVersion != currentRegeneratorAlgorithmVersion()) { fail(reason, "The functional regenerator version has changed."); return false; }
        if (correction.printedOrientation == "unknown" || correction.printedOrientation == "other-unsupported") { fail(reason, "The calibrated print orientation is unsupported."); return false; }
    }
    if (reason) *reason = "Compatible"; return true;
}
QVector<FitProfileSummary> FitCalibrationLibrary::profiles(QVector<FitLibraryIssue>* issues) const {
    QVector<FitProfileSummary> result; QSet<QString> identities;
    for (const auto& info : QDir(profilesDirectory()).entryInfoList({"*.json"}, QDir::Files, QDir::Name)) {
        QJsonObject json; FitProfile profile; QString error;
        if (!readObject(info.absoluteFilePath(), &json, &error) || !FitProfileJson::fromJson(json, &profile, &error)) { if (issues) issues->push_back({info.absoluteFilePath(), error}); continue; }
        if (identities.contains(profile.profileIdentity)) { if (issues) issues->push_back({info.absoluteFilePath(), "Duplicate Fit Profile identity."}); continue; }
        identities.insert(profile.profileIdentity); QString compatibility;
        bool compatible = profileCompatibility(profile, &compatibility);
        FitCalibrationSession source;
        if (compatible && !loadSession(profile.sourceSessionIdentity, &source, nullptr)) {
            compatible = false; compatibility = "The source calibration session is missing.";
        }
        result.push_back({profile.profileIdentity, profile.name, info.absoluteFilePath(), compatible, compatibility});
    }
    return result;
}
} // namespace PrintGeometry
