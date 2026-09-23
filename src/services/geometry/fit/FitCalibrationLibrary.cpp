#include "FitCalibrationLibrary.h"
#include "FitCalibrationNamingCatalog.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <tuple>

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
        && (a->featureFamily != QStringLiteral("StudReceivingClutch") ||
            (a->hasRegenerationPrototype && b->hasRegenerationPrototype &&
             a->regenerationPrototype.evidenceContract == b->regenerationPrototype.evidenceContract))
        && left.process.actualPrintedOrientation == right.process.actualPrintedOrientation;
}
bool mergeImportedHistory(FitCalibrationSession* existing, const FitCalibrationSession& incoming)
{
    auto oldStages = *existing;
    auto newStages = incoming;
    oldStages.history.clear();
    newStages.history.clear();
    if (FitCalibrationSessionJson::toJson(oldStages) != FitCalibrationSessionJson::toJson(newStages)) return false;
    for (const auto& stage : incoming.history) {
        const auto found = std::find_if(existing->history.cbegin(), existing->history.cend(), [&](const auto& saved) {
            return saved.artifactIdentity == stage.artifactIdentity;
        });
        if (found == existing->history.cend()) existing->history.push_back(stage);
        else if (FitCalibrationExperimentJson::toJson(*found) != FitCalibrationExperimentJson::toJson(stage)) return false;
    }
    return true;
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
QString canonicalProcessLabel(QString value) {
    value = value.simplified().toCaseFolded();
    const qsizetype annotation = value.indexOf(QStringLiteral(" @"));
    if (annotation >= 0) value.truncate(annotation);
    static const QRegularExpression millimetres(QStringLiteral("(\\d(?:[\\d.]*)?)\\s*mm\\b"));
    value.replace(millimetres, QStringLiteral("\\1 mm"));
    return value.simplified();
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
        !mergeText(&imported->materialIdentity, selected.materialIdentity, QStringLiteral("material"))) return false;
    if (!imported->hasNozzleDiameter) { imported->hasNozzleDiameter = selected.hasNozzleDiameter; imported->nozzleDiameterMillimetres = selected.nozzleDiameterMillimetres; }
    else if (selected.hasNozzleDiameter && std::abs(imported->nozzleDiameterMillimetres - selected.nozzleDiameterMillimetres) > 1e-9) {
        fail(error, QStringLiteral("The imported nozzle diameter conflicts with the selected workspace.")); return false;
    }
    if (!imported->hasLayerHeight) { imported->hasLayerHeight = selected.hasLayerHeight; imported->layerHeightMillimetres = selected.layerHeightMillimetres; }
    else if (selected.hasLayerHeight && std::abs(imported->layerHeightMillimetres - selected.layerHeightMillimetres) > 1e-9) {
        fail(error, QStringLiteral("The imported layer height conflicts with the selected workspace.")); return false;
    }
    if (unresolvedSharedText(imported->profileName, false)) imported->profileName = selected.profileName;
    else if (!selected.profileName.trimmed().isEmpty() && imported->profileName.trimmed() != selected.profileName.trimmed()) {
        if (FitCalibrationLibrary::processIdentityFingerprint(*imported) != FitCalibrationLibrary::processIdentityFingerprint(selected)) {
            fail(error, QStringLiteral("The imported process/profile '%1' conflicts with the selected workspace value '%2'.")
                            .arg(imported->profileName.trimmed(), selected.profileName.trimmed()));
            return false;
        }
        imported->profileName = selected.profileName;
    }
    if (!mergeText(&imported->dimensionalCompensationNotes, selected.dimensionalCompensationNotes,
                   QStringLiteral("slicer-compensation context"), true)) return false;
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
        if (correction.hasFixedTipToTipCorrection) {
            item["fixedTipToTipCorrectionMillimetres"] = correction.fixedTipToTipCorrectionMillimetres;
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
        if (item.contains("fixedTipToTipCorrectionMillimetres")) {
            if (!item.value("fixedTipToTipCorrectionMillimetres").isDouble()) {
                fail(error, "A Fit Profile fixed tip-to-tip context is malformed."); return false;
            }
            correction.hasFixedTipToTipCorrection = true;
            correction.fixedTipToTipCorrectionMillimetres = item.value("fixedTipToTipCorrectionMillimetres").toDouble();
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
QString FitCalibrationLibrary::workspacesDirectory() const { return QDir(m_root).filePath("Workspaces"); }
QString FitCalibrationLibrary::newStableIdentity() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString FitCalibrationLibrary::currentSemanticContractVersion() { return "official-ldraw-peghole-pair-v1"; }
QString FitCalibrationLibrary::currentRegeneratorAlgorithmVersion() { return "functional-operand-regenerator-v1"; }
QString FitCalibrationLibrary::sessionDisplayName(const FitCalibrationSession& session) { const FitCalibrationExperiment* experiment=session.hasFineExperiment?&session.fineExperiment:(session.hasCoarseExperiment?&session.coarseExperiment:nullptr);if(!experiment)return QStringLiteral("Empty calibration session");const QString feature=FitCalibrationNamingCatalog::canonical(*experiment,session.process.actualPrintedOrientation);QString stage;if(experiment->state==FitEvidenceState::Verified)stage=QStringLiteral("Verified");else if(experiment->artifactIdentity.contains(QStringLiteral("direct-verification")))stage=QStringLiteral("Verification");else if(experiment->artifactIdentity.contains(QStringLiteral("extension")))stage=QStringLiteral("Extended Search");else if(!experiment->parentArtifactIdentity.isEmpty())stage=QStringLiteral("Fine Search / Verification");else stage=QStringLiteral("Coarse Search");QString process;if(!session.process.printerIdentity.isEmpty()||!session.process.materialIdentity.isEmpty()){process=QStringLiteral(" — %1 / %2").arg(session.process.printerIdentity.isEmpty()?QStringLiteral("Unknown printer"):session.process.printerIdentity,session.process.materialIdentity.isEmpty()?QStringLiteral("Unknown material"):session.process.materialIdentity);}return QStringLiteral("%1 — %2%3").arg(feature,stage,process);}
FitCalibrationSession FitCalibrationLibrary::continuationSession(const FitCalibrationSession& parent,
    const FitCalibrationExperiment& source, FitCalibrationExperiment child)
{
    FitCalibrationSession result;
    result.sessionIdentity = newStableIdentity();
    result.process = parent.process;
    result.history = parent.history;
    const auto retain = [&](const FitCalibrationExperiment& stage) {
        if (stage.artifactIdentity.isEmpty() || stage.artifactIdentity == source.artifactIdentity) return;
        const auto found = std::find_if(result.history.cbegin(), result.history.cend(), [&](const auto& earlier) {
            return earlier.artifactIdentity == stage.artifactIdentity;
        });
        if (found == result.history.cend()) result.history.push_back(stage);
    };
    if (parent.hasCoarseExperiment) retain(parent.coarseExperiment);
    if (parent.hasFineExperiment) retain(parent.fineExperiment);
    result.hasCoarseExperiment = true;
    result.coarseExperiment = source;
    result.coarseExperiment.process = result.process;
    result.hasFineExperiment = true;
    child.process = result.process;
    child.state = FitEvidenceState::Draft;
    child.preferredCandidateIndex = 0;
    for (auto& candidate : child.candidates) candidate.observations.clear();
    result.fineExperiment = std::move(child);
    return result;
}
QString FitCalibrationLibrary::featureDisplayName(const FitCalibrationExperiment& experiment,
                                                  FitPrintedOrientation orientation)
{
    return FitCalibrationNamingCatalog::canonical(experiment, orientation);
}
QString FitCalibrationLibrary::processFingerprint(const FitCalibrationProcess& process) {
    return manufacturingContextFingerprint(process);
}
QString FitCalibrationLibrary::processIdentityFingerprint(const FitCalibrationProcess& process) {
    QJsonObject identity{{"printerIdentity", process.printerIdentity.simplified().toCaseFolded()},
                         {"materialIdentity", process.materialIdentity.simplified().toCaseFolded()},
                         {"profileIdentity", canonicalProcessLabel(process.profileName)}};
    if (process.hasNozzleDiameter) identity["nozzleDiameterMillimetres"] = process.nozzleDiameterMillimetres;
    if (process.hasLayerHeight) identity["layerHeightMillimetres"] = process.layerHeightMillimetres;
    return QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(identity).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
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
    if (!writeJson(fileForIdentity(sessionsDirectory(), session->sessionIdentity), FitCalibrationSessionJson::toJson(*session), error)) return false;
    // Once evidence exists, the session is the workspace's authoritative context.
    const QString emptyRecord = fileForIdentity(workspacesDirectory(), manufacturingContextFingerprint(session->process));
    if (QFileInfo::exists(emptyRecord) && !QFile::remove(emptyRecord)) {
        fail(error, QStringLiteral("Session saved, but the empty-workspace record could not be retired: %1").arg(emptyRecord));
        return false;
    }
    return true;
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
    for (const auto& info : QDir(workspacesDirectory()).entryInfoList({"*.json"}, QDir::Files, QDir::Name)) {
        QJsonObject json; QString error;
        if (!readObject(info.absoluteFilePath(), &json, &error)) {
            if (issues) issues->push_back({info.absoluteFilePath(), error});
            continue;
        }
        const auto process = processFromJson(json.value("process").toObject());
        const QString identity = manufacturingContextFingerprint(process);
        if (json.value("formatVersion").toInt() != 1 || json.value("identity").toString() != identity ||
            info.baseName() != identity || byIdentity.contains(identity)) {
            if (issues) issues->push_back({info.absoluteFilePath(), "Invalid or duplicate managed workspace record."});
            continue;
        }
        byIdentity.insert(identity, result.size());
        result.push_back({identity, workspaceDisplayName(process), process, {}});
    }
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
            int observations = 0;
            for (const auto& candidate : stage->candidates) observations += candidate.observations.size();
            return std::make_tuple(stage->state == FitEvidenceState::Verified,
                item.hasFineExperiment,
                stage->artifactIdentity.contains(QStringLiteral("direct-verification")),
                stage->artifactIdentity.contains(QStringLiteral("extension")),
                item.history.size(), observations);
        };
        if (matchingFeature == result[index].featureSessions.end()) result[index].featureSessions.push_back(session);
        else if (rank(session) > rank(*matchingFeature)) *matchingFeature = session;
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) { return left.displayName < right.displayName; });
    return result;
}
bool FitCalibrationLibrary::createWorkspace(const FitCalibrationProcess& process,
                                            FitCalibrationWorkspace* created, QString* error)
{
    if (process.printerIdentity.trimmed().isEmpty() || process.materialIdentity.trimmed().isEmpty() ||
        process.profileName.trimmed().isEmpty() || !process.hasNozzleDiameter ||
        process.nozzleDiameterMillimetres <= 0 || !process.hasLayerHeight ||
        process.layerHeightMillimetres <= 0) {
        fail(error, "Enter a printer, material, nozzle, process/profile, and layer height."); return false;
    }
    FitCalibrationProcess context = process;
    context.actualPrintedOrientation = FitPrintedOrientation::Unknown;
    context.orientationNotes.clear();
    const QString identity = manufacturingContextFingerprint(context);
    for (const auto& workspace : workspaces()) {
        if (workspace.identity == identity) {
            fail(error, "This manufacturing workspace already exists. Resume it instead."); return false;
        }
    }
    const QString path = fileForIdentity(workspacesDirectory(), identity);
    if (QFileInfo::exists(path)) { fail(error, "A workspace record already exists at this location."); return false; }
    if (!writeJson(path, {{"formatVersion", 1}, {"identity", identity}, {"process", processJson(context)}}, error)) return false;
    if (created) *created = {identity, workspaceDisplayName(context), context, {}};
    return true;
}
bool FitCalibrationLibrary::deleteWorkspace(const QString& identity, QString* backupPath, QString* error)
{
    if (identity.isEmpty()) { fail(error, "Select a managed workspace first."); return false; }
    const auto available = workspaces();
    if (std::none_of(available.cbegin(), available.cend(), [&](const auto& workspace) { return workspace.identity == identity; })) {
        fail(error, "The selected workspace is no longer available."); return false;
    }
    struct Record { QString path; QByteArray bytes; };
    QVector<Record> records;
    auto collect = [&](const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) { fail(error, QStringLiteral("Could not read %1 for backup.").arg(path)); return false; }
        records.push_back({path, file.readAll()}); return true;
    };
    const QString workspacePath = fileForIdentity(workspacesDirectory(), identity);
    if (QFileInfo::exists(workspacePath) && !collect(workspacePath)) return false;
    for (const auto& summary : sessions()) {
        FitCalibrationSession session;
        if (!loadSession(summary.identity, &session, error)) return false;
        if (manufacturingContextFingerprint(session.process) == identity && !collect(summary.path)) return false;
    }
    for (const auto& summary : profiles()) {
        FitProfile profile;
        if (!loadProfile(summary.identity, &profile, error)) return false;
        if (manufacturingContextFingerprint(profile.process) == identity && !collect(summary.path)) return false;
    }
    if (records.isEmpty()) { fail(error, "The selected workspace has no managed records to remove."); return false; }
    QJsonArray saved;
    for (const auto& record : records) {
        const QString relative = QDir(m_root).relativeFilePath(record.path);
        saved.append(QJsonObject{{"path", relative}, {"dataBase64", QString::fromLatin1(record.bytes.toBase64())}});
    }
    const QString backup = QDir(m_root).filePath(QStringLiteral("Backups/workspace-%1-%2.json")
        .arg(identity, newStableIdentity()));
    if (!writeJson(backup, {{"formatVersion", 1}, {"workspaceIdentity", identity}, {"records", saved}}, error)) return false;
    QJsonObject check;
    if (!readObject(backup, &check, error) || check.value("records").toArray().size() != records.size()) {
        fail(error, "The workspace backup could not be verified; no active records were removed."); return false;
    }
    const auto checkedRecords = check.value("records").toArray();
    for (int i = 0; i < records.size(); ++i) {
        if (QByteArray::fromBase64(checkedRecords[i].toObject().value("dataBase64").toString().toLatin1()) != records[i].bytes) {
            fail(error, "The workspace backup did not match the active records; no active records were removed."); return false;
        }
    }
    if (backupPath) *backupPath = backup;
    for (int i = 0; i < records.size(); ++i) {
        if (QFile::remove(records[i].path)) continue;
        for (int previous = 0; previous < i; ++previous) {
            QSaveFile restore(records[previous].path);
            if (restore.open(QIODevice::WriteOnly)) {
                restore.write(records[previous].bytes);
                restore.commit();
            }
        }
        fail(error, QStringLiteral("Could not remove %1. A recoverable backup is at %2.")
            .arg(records[i].path, backup));
        return false;
    }
    return true;
}
QVector<FitCalibrationExperiment> FitCalibrationLibrary::coarseReviewHistory(
    const FitCalibrationSession& current) const
{
    QVector<FitCalibrationExperiment> reverseHistory;
    const auto* representative = representativeExperiment(current);
    if (!representative) return reverseHistory;
    const QString context = manufacturingContextFingerprint(current.process);
    // Embedded snapshots are authoritative for this child. Older managed records
    // remain a compatibility fallback for v3 sessions with no embedded history.
    QVector<FitCalibrationExperiment> available = current.history;
    const int embeddedCount = available.size();
    const auto observationCount = [](const FitCalibrationExperiment& stage) {
        int count = 0;
        for (const auto& candidate : stage.candidates) count += candidate.observations.size();
        return count;
    };
    for (const auto& summary : sessions()) {
        FitCalibrationSession candidate;
        if (!loadSession(summary.identity, &candidate, nullptr) ||
            manufacturingContextFingerprint(candidate.process) != context ||
            candidate.process.actualPrintedOrientation != current.process.actualPrintedOrientation)
            continue;
        const auto add = [&](const FitCalibrationExperiment& experiment) {
            if (experiment.featureFamily == representative->featureFamily &&
                experiment.featureRole == representative->featureRole &&
                experiment.correctionDimension == representative->correctionDimension &&
                (experiment.featureFamily != QStringLiteral("StudReceivingClutch") ||
                 (experiment.hasRegenerationPrototype && representative->hasRegenerationPrototype &&
                  experiment.regenerationPrototype.evidenceContract ==
                      representative->regenerationPrototype.evidenceContract)) &&
                !experiment.artifactIdentity.isEmpty()) {
                const auto existing = std::find_if(available.begin(), available.end(), [&](const auto& stage) {
                    return stage.artifactIdentity == experiment.artifactIdentity;
                });
                if (existing == available.end()) available.push_back(experiment);
                else if (existing - available.begin() >= embeddedCount &&
                         observationCount(experiment) > observationCount(*existing)) *existing = experiment;
            }
        };
        if (candidate.hasCoarseExperiment) add(candidate.coarseExperiment);
        if (candidate.hasFineExperiment) add(candidate.fineExperiment);
    }
    FitCalibrationExperiment stage;
    if (current.hasCoarseExperiment) stage = current.coarseExperiment;
    else {
        const auto parent = current.fineExperiment.parentArtifactIdentity;
        const auto found = std::find_if(available.cbegin(), available.cend(), [&](const auto& item) {
            return item.artifactIdentity == parent;
        });
        if (found == available.cend()) return reverseHistory;
        stage = *found;
    }
    QSet<QString> visited;
    while (!stage.artifactIdentity.isEmpty() && !visited.contains(stage.artifactIdentity)) {
        visited.insert(stage.artifactIdentity);
        reverseHistory.push_back(stage);
        const auto found = std::find_if(available.cbegin(), available.cend(), [&](const auto& item) {
            return item.artifactIdentity == stage.parentArtifactIdentity;
        });
        if (found == available.cend()) break;
        stage = *found;
    }
    std::reverse(reverseHistory.begin(), reverseHistory.end());
    return reverseHistory;
}
bool FitCalibrationLibrary::importSession(const QString& path, FitCalibrationSession* output, QString* error) {
    QJsonObject json; FitCalibrationSession session;
    if (!readObject(path, &json, error) || !FitCalibrationSessionJson::fromJson(json, &session, error)) return false;
    // Legacy artifact identity described the printable artifact, not a durable managed record.
    if (json.value("documentType").toString() != "fit-calibration-session" || session.sessionIdentity.isEmpty()) session.sessionIdentity = newStableIdentity();
    FitCalibrationSession existing;
    if (loadSession(session.sessionIdentity, &existing, nullptr)) {
        const int oldHistorySize = existing.history.size();
        if (!mergeImportedHistory(&existing, session)) {
            fail(error, "A different managed calibration session already uses this stable identity."); return false;
        }
        if (existing.history.size() > oldHistorySize && !saveSession(&existing, error)) return false;
        if (output) *output = existing;
        return true;
    }
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
        auto match = std::find_if(available.cbegin(), available.cend(), [&](const auto& workspace) { return workspace.identity == identity; });
        if (match == available.cend()) {
            const QString processIdentity = processIdentityFingerprint(session.process);
            match = std::find_if(available.cbegin(), available.cend(), [&](const auto& workspace) {
                return processIdentityFingerprint(workspace.process) == processIdentity;
            });
        }
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
    }
    FitCalibrationSession existing;
    if (loadSession(session.sessionIdentity, &existing, nullptr)) {
        const int oldHistorySize = existing.history.size();
        if (!mergeImportedHistory(&existing, session)) {
            fail(error, "A different managed calibration session already uses this stable identity."); return false;
        }
        if (existing.history.size() > oldHistorySize && !saveSession(&existing, error)) return false;
        if (output) *output = existing;
        return true;
    }
    auto comparable = [](FitCalibrationSession candidate) {
        candidate.sessionIdentity.clear();
        return FitCalibrationSessionJson::toJson(candidate);
    };
    for (const auto& summary : sessions()) {
        FitCalibrationSession prior;
        if (loadSession(summary.identity, &prior, nullptr) &&
            comparable(prior) == comparable(session)) {
            if (output) *output = prior;
            return true;
        }
    }
    if (!saveSession(&session, error)) return false;
    if (output) *output = session;
    return true;
}
bool FitCalibrationLibrary::exportSession(const QString& identity, const QString& path, QString* error) const {
    FitCalibrationSession session;
    if (!loadSession(identity, &session, error)) return false;
    const auto lineage = coarseReviewHistory(session);
    for (const auto& stage : lineage) {
        if ((session.hasCoarseExperiment && stage.artifactIdentity == session.coarseExperiment.artifactIdentity) ||
            (session.hasFineExperiment && stage.artifactIdentity == session.fineExperiment.artifactIdentity)) continue;
        const auto found = std::find_if(session.history.cbegin(), session.history.cend(), [&](const auto& earlier) {
            return earlier.artifactIdentity == stage.artifactIdentity;
        });
        if (found == session.history.cend()) session.history.push_back(stage);
    }
    return writeJson(path, FitCalibrationSessionJson::toJson(session), error);
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
    if (experiment.featureFamily == QStringLiteral("StandardBar") &&
        session.process.actualPrintedOrientation != FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate) {
        fail(error, "This family currently has only a perpendicular physical calibration fixture."); return false;
    }
    if (experiment.featureFamily == QStringLiteral("BallJoint") &&
        session.process.actualPrintedOrientation != FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate &&
        session.process.actualPrintedOrientation != FitPrintedOrientation::FeatureAxisParallelToBuildPlate) {
        fail(error, "Ball Joint calibration requires a supported perpendicular or parallel fixture orientation."); return false;
    }
    if (experiment.featureFamily == QStringLiteral("CClipBarReceiver") &&
        session.process.actualPrintedOrientation != FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate &&
        session.process.actualPrintedOrientation != FitPrintedOrientation::FeatureAxisParallelToBuildPlate) {
        fail(error, "C-Clip calibration requires a supported perpendicular or parallel fixture orientation."); return false;
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
        const bool wallPocket = experiment.hasRegenerationPrototype
            && experiment.regenerationPrototype.constructionRecipe == QStringLiteral("stud-receiving-wall-pocket-square-v1");
        const bool antiStudBore = experiment.hasRegenerationPrototype
            && experiment.regenerationPrototype.constructionRecipe == QStringLiteral("stud-receiving-antistud-bore-v1");
        const bool postWall = experiment.hasRegenerationPrototype
            && experiment.regenerationPrototype.constructionRecipe == QStringLiteral("stud-receiving-post-wall-cell-v1");
        correction.semantics = antiStudBore ? QStringLiteral("female-stud-receiver-antistud-bore-diameter")
            : wallPocket ? QStringLiteral("female-stud-receiver-wall-pocket-opening-width")
            : postWall ? QStringLiteral("female-stud-receiver-post-od")
                       : QStringLiteral("female-stud-receiver-tube-od");
        correction.correctionContractVersion = antiStudBore ? QStringLiteral("female-stud-receiver-antistud-bore-diameter-v1")
            : wallPocket ? QStringLiteral("female-stud-receiver-wall-pocket-opening-width-v1")
            : postWall ? QStringLiteral("female-stud-receiver-post-od-v1")
                       : QStringLiteral("female-stud-receiver-tube-od-v1");
    } else if (experiment.featureFamily == QStringLiteral("StandardBar")) {
        correction.semantics = QStringLiteral("male-standard-bar-diameter");
        correction.correctionContractVersion = QStringLiteral("male-standard-bar-diameter-v1");
    } else if (experiment.featureFamily == QStringLiteral("CClipBarReceiver")) {
        correction.semantics = QStringLiteral("female-c-clip-contact-arc-and-throat-clearance");
        correction.correctionContractVersion = QStringLiteral("female-c-clip-contact-arc-and-throat-clearance-v1");
    } else if (experiment.featureFamily == QStringLiteral("BallJoint")) {
        correction.semantics = QStringLiteral("male-ball-joint-spherical-diameter");
        correction.correctionContractVersion = QStringLiteral("male-ball-joint-spherical-diameter-v1");
    } else if (experiment.featureFamily == QStringLiteral("FrictionlessTechnicPin")) {
        correction.semantics = QStringLiteral("male-frictionless-technic-pin-envelope-diameter");
        correction.correctionContractVersion = QStringLiteral("male-frictionless-technic-pin-envelope-diameter-v1");
    } else if (experiment.featureFamily == QStringLiteral("FrictionTechnicPin")) {
        correction.semantics = QStringLiteral("male-friction-technic-pin-ridge-envelope-diameter");
        correction.correctionContractVersion = QStringLiteral("male-friction-technic-pin-ridge-envelope-diameter-v1");
    } else if (experiment.featureFamily == QStringLiteral("TechnicAxle")) {
        correction.semantics = QStringLiteral("male-technic-axle-tip-to-tip-envelope");
        correction.correctionContractVersion = QStringLiteral("male-technic-axle-tip-to-tip-envelope-v1");
    } else if (experiment.featureFamily == QStringLiteral("TechnicAxleHole")) {
        const bool armWidth = experiment.hasRegenerationPrototype && experiment.regenerationPrototype.constructionRecipe == QStringLiteral("technic-axle-hole-arm-width-clearance-v2");
        correction.semantics = armWidth ? QStringLiteral("female-technic-axle-hole-arm-width-clearance") : QStringLiteral("female-technic-axle-hole-tip-to-tip-clearance");
        correction.correctionContractVersion = armWidth ? QStringLiteral("female-technic-axle-hole-arm-width-clearance-v2") : QStringLiteral("female-technic-axle-hole-tip-to-tip-clearance-v1");
        if (armWidth) {
            correction.hasFixedTipToTipCorrection = true;
            correction.fixedTipToTipCorrectionMillimetres = experiment.fixedDiameterCorrectionMillimetres;
        }
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
bool FitCalibrationLibrary::mergeVerifiedSession(const FitCalibrationSession&session,FitProfile*profile,QString*error){if(!profile){fail(error,"There is no Fit Profile to update.");return false;}FitProfile addition;if(!promoteVerifiedSession(session,profile->name,&addition,error))return false;if(manufacturingContextFingerprint(profile->process)!=manufacturingContextFingerprint(addition.process)){fail(error,"The verified calibration uses a different manufacturing process.");return false;}profile->processFingerprint=manufacturingContextFingerprint(profile->process);const auto&incoming=addition.corrections.front();const auto sameContract=[&](const FitProfileCorrection&existing){return existing.featureFamily==incoming.featureFamily&&existing.featureRole==incoming.featureRole&&existing.printedOrientation==incoming.printedOrientation&&existing.semantics==incoming.semantics&&existing.correctionContractVersion==incoming.correctionContractVersion&&existing.semanticContractVersion==incoming.semanticContractVersion;};auto existing=std::find_if(profile->corrections.begin(),profile->corrections.end(),sameContract);if(existing==profile->corrections.end())profile->corrections.push_back(incoming);else *existing=incoming;if(!profile->verifiedUtc.isValid()||addition.verifiedUtc>profile->verifiedUtc)profile->verifiedUtc=addition.verifiedUtc;return true;}
bool FitCalibrationLibrary::saveVerifiedWorkspaceProfile(const FitCalibrationWorkspace&workspace,const QString&profileName,FitProfile*output,QString*error){QVector<FitProfile>matching;for(const auto&summary:profiles()){FitProfile candidate;if(loadProfile(summary.identity,&candidate,nullptr)&&manufacturingContextFingerprint(candidate.process)==workspace.identity)matching.push_back(candidate);}if(matching.size()>1){fail(error,"More than one Fit Profile already uses this manufacturing context. BrickSuite will not guess which profile to update.");return false;}QVector<FitCalibrationSession>verified;for(const auto&session:workspace.featureSessions)if(verifiedExperiment(session))verified.push_back(session);if(verified.isEmpty()){fail(error,"This manufacturing workspace has no Verified calibration evidence.");return false;}FitProfile profile;if(matching.isEmpty()){if(!promoteVerifiedSession(verified.front(),profileName,&profile,error))return false;}else{profile=matching.front();if(!profileName.trimmed().isEmpty())profile.name=profileName.trimmed();}for(const auto&session:verified)if(!mergeVerifiedSession(session,&profile,error))return false;if(!saveProfile(&profile,error))return false;if(output)*output=profile;return true;}
bool FitCalibrationLibrary::saveProfile(FitProfile* profile, QString* error) {
    if (!profile || profile->corrections.isEmpty() || profile->sourceSessionIdentity.isEmpty()) { fail(error, "The Fit Profile is incomplete."); return false; }
    if (profile->profileIdentity.isEmpty()) profile->profileIdentity = newStableIdentity();
    return writeJson(fileForIdentity(profilesDirectory(), profile->profileIdentity), FitProfileJson::toJson(*profile), error);
}
bool FitCalibrationLibrary::loadProfile(const QString& identity, FitProfile* profile, QString* error) const {
    QJsonObject json;if(!readObject(fileForIdentity(profilesDirectory(),identity),&json,error)||!FitProfileJson::fromJson(json,profile,error))return false;
    for(auto&correction:profile->corrections){if(correction.semantics!=QStringLiteral("male-stud-height")||correction.hasRequiredDiameterCorrection)continue;bool found=false;double required=0;for(const auto&summary:sessions()){FitCalibrationSession session;if(!loadSession(summary.identity,&session,nullptr)||manufacturingContextFingerprint(session.process)!=manufacturingContextFingerprint(profile->process))continue;const FitCalibrationExperiment*experiment=session.hasFineExperiment?&session.fineExperiment:(session.hasCoarseExperiment?&session.coarseExperiment:nullptr);if(!experiment||experiment->artifactIdentity!=correction.calibrationArtifactIdentity||experiment->featureFamily!=QStringLiteral("StandardStud")||experiment->correctionDimension!=FitCorrectionDimension::Height)continue;if(found&&std::abs(required-experiment->fixedDiameterCorrectionMillimetres)>1e-9){found=false;break;}required=experiment->fixedDiameterCorrectionMillimetres;found=true;}if(found){correction.hasRequiredDiameterCorrection=true;correction.requiredDiameterCorrectionMillimetres=required;}}
    for(auto&correction:profile->corrections){if(correction.semantics!=QStringLiteral("female-technic-axle-hole-arm-width-clearance")||correction.hasFixedTipToTipCorrection)continue;for(const auto&summary:sessions()){FitCalibrationSession session;if(!loadSession(summary.identity,&session,nullptr)||manufacturingContextFingerprint(session.process)!=manufacturingContextFingerprint(profile->process))continue;const FitCalibrationExperiment*experiment=session.hasFineExperiment?&session.fineExperiment:(session.hasCoarseExperiment?&session.coarseExperiment:nullptr);if(!experiment||experiment->artifactIdentity!=correction.calibrationArtifactIdentity||experiment->featureFamily!=QStringLiteral("TechnicAxleHole")||!experiment->hasRegenerationPrototype||experiment->regenerationPrototype.constructionRecipe!=QStringLiteral("technic-axle-hole-arm-width-clearance-v2"))continue;correction.hasFixedTipToTipCorrection=true;correction.fixedTipToTipCorrectionMillimetres=experiment->fixedDiameterCorrectionMillimetres;break;}}
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
            && ((correction.semanticContractVersion == QStringLiteral("official-ldraw-stud4-tube-wall-cell-v1")
                 && correction.semantics == QStringLiteral("female-stud-receiver-tube-od")
                 && correction.correctionContractVersion == QStringLiteral("female-stud-receiver-tube-od-v1"))
                || (correction.semanticContractVersion == QStringLiteral("official-ldraw-stud3-post-wall-cell-v1")
                    && correction.semantics == QStringLiteral("female-stud-receiver-post-od")
                    && correction.correctionContractVersion == QStringLiteral("female-stud-receiver-post-od-v1"))
                || ((correction.semanticContractVersion == QStringLiteral("official-ldraw-box5-wall-pocket-plate-v1")
                     || correction.semanticContractVersion == QStringLiteral("official-ldraw-box5-wall-pocket-brick-v1"))
                    && correction.semantics == QStringLiteral("female-stud-receiver-wall-pocket-opening-width")
                    && correction.correctionContractVersion == QStringLiteral("female-stud-receiver-wall-pocket-opening-width-v1"))
                || (correction.semanticContractVersion == QStringLiteral("official-ldraw-stud4o-antistud-bore-v1")
                    && correction.semantics == QStringLiteral("female-stud-receiver-antistud-bore-diameter")
                    && correction.correctionContractVersion == QStringLiteral("female-stud-receiver-antistud-bore-diameter-v1")));
        const bool frictionlessPin = correction.featureFamily == QStringLiteral("FrictionlessTechnicPin")
            && correction.featureRole == QStringLiteral("male")
            && correction.semanticContractVersion == QStringLiteral("official-ldraw-connect-frictionless-pin-v1")
            && correction.semantics == QStringLiteral("male-frictionless-technic-pin-envelope-diameter")
            && correction.correctionContractVersion == QStringLiteral("male-frictionless-technic-pin-envelope-diameter-v1");
        const bool standardBar = correction.featureFamily == QStringLiteral("StandardBar")
            && correction.featureRole == QStringLiteral("male")
            && (correction.printedOrientation == QStringLiteral("axis-perpendicular-to-build-plate")
                || correction.printedOrientation == QStringLiteral("feature-axis-perpendicular-to-build-plate"))
            && correction.semanticContractVersion == QStringLiteral("official-ldraw-capped-standard-bar-v1")
            && correction.semantics == QStringLiteral("male-standard-bar-diameter")
            && correction.correctionContractVersion == QStringLiteral("male-standard-bar-diameter-v1");
        const bool cClip = correction.featureFamily == QStringLiteral("CClipBarReceiver")
            && correction.featureRole == QStringLiteral("female")
            && (correction.printedOrientation == QStringLiteral("axis-perpendicular-to-build-plate")
                || correction.printedOrientation == QStringLiteral("feature-axis-perpendicular-to-build-plate")
                || correction.printedOrientation == QStringLiteral("feature-axis-parallel-to-build-plate"))
            && correction.semanticContractVersion == QStringLiteral("official-ldraw-clip6-bar-receiver-v1")
            && correction.semantics == QStringLiteral("female-c-clip-contact-arc-and-throat-clearance")
            && correction.correctionContractVersion == QStringLiteral("female-c-clip-contact-arc-and-throat-clearance-v1");
        const bool ballJoint = correction.featureFamily == QStringLiteral("BallJoint")
            && correction.featureRole == QStringLiteral("male")
            && (correction.printedOrientation == QStringLiteral("feature-axis-perpendicular-to-build-plate")
                || correction.printedOrientation == QStringLiteral("feature-axis-parallel-to-build-plate"))
            && correction.semanticContractVersion == QStringLiteral("official-ldraw-joint8ball-sphere-v1")
            && correction.semantics == QStringLiteral("male-ball-joint-spherical-diameter")
            && correction.correctionContractVersion == QStringLiteral("male-ball-joint-spherical-diameter-v1");
        const bool frictionPin = correction.featureFamily == QStringLiteral("FrictionTechnicPin")
            && correction.featureRole == QStringLiteral("male")
            && correction.semanticContractVersion == QStringLiteral("official-ldraw-confric5-friction-pin-v1")
            && correction.semantics == QStringLiteral("male-friction-technic-pin-ridge-envelope-diameter")
            && correction.correctionContractVersion == QStringLiteral("male-friction-technic-pin-ridge-envelope-diameter-v1");
        const bool technicAxle = correction.featureFamily == QStringLiteral("TechnicAxle")
            && correction.featureRole == QStringLiteral("male")
            && correction.semanticContractVersion == QStringLiteral("official-ldraw-axle-cross-profile-v1")
            && correction.semantics == QStringLiteral("male-technic-axle-tip-to-tip-envelope")
            && correction.correctionContractVersion == QStringLiteral("male-technic-axle-tip-to-tip-envelope-v1");
        const bool technicAxleHole = correction.featureFamily == QStringLiteral("TechnicAxleHole")
            && correction.featureRole == QStringLiteral("female")
            && correction.semanticContractVersion == QStringLiteral("official-ldraw-axlehole-cross-profile-v1")
            && correction.semantics == QStringLiteral("female-technic-axle-hole-tip-to-tip-clearance")
            && correction.correctionContractVersion == QStringLiteral("female-technic-axle-hole-tip-to-tip-clearance-v1");
        const bool technicAxleHoleArmWidth = correction.featureFamily == QStringLiteral("TechnicAxleHole")
            && correction.featureRole == QStringLiteral("female")
            && correction.semanticContractVersion == QStringLiteral("official-ldraw-axlehole-arm-width-clearance-v2")
            && correction.semantics == QStringLiteral("female-technic-axle-hole-arm-width-clearance")
            && correction.correctionContractVersion == QStringLiteral("female-technic-axle-hole-arm-width-clearance-v2");
        if (!roundPassage && !standardStud && !receivingClutch && !standardBar && !cClip && !ballJoint && !frictionlessPin && !frictionPin && !technicAxle && !technicAxleHole && !technicAxleHoleArmWidth) { fail(reason, "The functional semantic or correction interpretation contract has changed."); return false; }
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
