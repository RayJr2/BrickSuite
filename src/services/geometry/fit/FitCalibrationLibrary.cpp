#include "FitCalibrationLibrary.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>
#include <algorithm>

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
    if (session.hasFineExperiment) return session.fineExperiment.state;
    if (session.hasCoarseExperiment) return session.coarseExperiment.state;
    return FitEvidenceState::Draft;
}
QString sessionName(const FitCalibrationSession& session) {
    QStringList parts;
    if (!session.process.printerIdentity.isEmpty()) parts << session.process.printerIdentity;
    if (!session.process.materialIdentity.isEmpty()) parts << session.process.materialIdentity;
    if (!session.process.profileName.isEmpty()) parts << session.process.profileName;
    return parts.isEmpty() ? session.sessionIdentity : parts.join(" / ");
}
}

QJsonObject FitProfileJson::toJson(const FitProfile& profile) {
    QJsonArray corrections;
    for (const auto& correction : profile.corrections) corrections.push_back(QJsonObject{
        {"featureFamily", correction.featureFamily}, {"featureRole", correction.featureRole},
        {"printedOrientation", correction.printedOrientation}, {"valueMillimetres", correction.valueMillimetres},
        {"units", correction.units}, {"semantics", correction.semantics},
        {"correctionContractVersion", correction.correctionContractVersion},
        {"semanticContractVersion", correction.semanticContractVersion},
        {"regeneratorAlgorithmVersion", correction.regeneratorAlgorithmVersion},
        {"calibrationArtifactIdentity", correction.calibrationArtifactIdentity}});
    return {{"formatVersion", CurrentFormatVersion}, {"documentType", "fit-profile"},
            {"profileIdentity", profile.profileIdentity}, {"name", profile.name},
            {"process", processJson(profile.process)}, {"processFingerprint", profile.processFingerprint},
            {"sourceSessionIdentity", profile.sourceSessionIdentity},
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
QString FitCalibrationLibrary::processFingerprint(const FitCalibrationProcess& process) {
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(processJson(process)).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
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
        identities.insert(session.sessionIdentity); result.push_back({session.sessionIdentity, sessionName(session), info.absoluteFilePath(), sessionState(session)});
    }
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
bool FitCalibrationLibrary::exportSession(const QString& identity, const QString& path, QString* error) const {
    FitCalibrationSession session; return loadSession(identity, &session, error) && writeJson(path, FitCalibrationSessionJson::toJson(session), error);
}

bool FitCalibrationLibrary::promoteVerifiedSession(const FitCalibrationSession& session, const QString& name, FitProfile* output, QString* error) {
    if (!output || session.sessionIdentity.isEmpty() || !session.hasFineExperiment || session.fineExperiment.state != FitEvidenceState::Verified) {
        fail(error, "Only an explicitly Verified fine-search calibration session can create a Fit Profile."); return false;
    }
    QString validation;
    if (!FitCalibrationEvidencePolicy::validate(session.fineExperiment, &validation)) { fail(error, validation); return false; }
    const auto& experiment = session.fineExperiment;
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
    correction.valueMillimetres = preferred->diameterCorrectionMillimetres;
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
bool FitCalibrationLibrary::saveProfile(FitProfile* profile, QString* error) {
    if (!profile || profile->corrections.isEmpty() || profile->sourceSessionIdentity.isEmpty()) { fail(error, "The Fit Profile is incomplete."); return false; }
    if (profile->profileIdentity.isEmpty()) profile->profileIdentity = newStableIdentity();
    return writeJson(fileForIdentity(profilesDirectory(), profile->profileIdentity), FitProfileJson::toJson(*profile), error);
}
bool FitCalibrationLibrary::loadProfile(const QString& identity, FitProfile* profile, QString* error) const {
    QJsonObject json; return readObject(fileForIdentity(profilesDirectory(), identity), &json, error) && FitProfileJson::fromJson(json, profile, error);
}
bool FitCalibrationLibrary::profileCompatibility(const FitProfile& profile, QString* reason) {
    if (profile.processFingerprint != processFingerprint(profile.process)) { fail(reason, "The stored manufacturing-process fingerprint no longer matches."); return false; }
    for (const auto& correction : profile.corrections) {
        if (correction.semanticContractVersion != currentSemanticContractVersion()) { fail(reason, "The functional semantic contract has changed."); return false; }
        if (correction.correctionContractVersion != "female-diameter-clearance-v1") { fail(reason, "The correction interpretation contract has changed."); return false; }
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
