#include "FitCalibrationPackage.h"

#include "FitCalibrationLibrary.h"
#include "../print/PrintMeshAnalysis.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QIODevice>
#include <QJsonArray>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>

namespace PrintGeometry { namespace {
bool fail(QString* error, const QString& message)
{
    if (error) *error = message;
    return false;
}

bool finite(const Point& p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
QJsonArray pointJson(const Point& p) { return {p.x, p.y, p.z}; }
bool readPoint(const QJsonValue& value, Point* out)
{
    if (!out || !value.isArray()) return false;
    const auto array = value.toArray();
    if (array.size() != 3) return false;
    for (const auto& item : array) if (!item.isDouble()) return false;
    const Point point{array[0].toDouble(), array[1].toDouble(), array[2].toDouble()};
    if (!finite(point)) return false;
    *out = point;
    return true;
}
QJsonObject boundsJson(const MeshBounds& b)
{
    return {{"minimum", pointJson(b.minimum)}, {"maximum", pointJson(b.maximum)}};
}
bool readBounds(const QJsonValue& value, MeshBounds* out)
{
    if (!out || !value.isObject()) return false;
    const auto object = value.toObject();
    MeshBounds bounds;
    if (!readPoint(object.value("minimum"), &bounds.minimum) ||
        !readPoint(object.value("maximum"), &bounds.maximum)) return false;
    bounds.valid = bounds.minimum.x < bounds.maximum.x &&
                   bounds.minimum.y < bounds.maximum.y && bounds.minimum.z < bounds.maximum.z;
    if (!bounds.valid) return false;
    *out = bounds;
    return true;
}

Point subtract(const Point& a, const Point& b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
Point cross(const Point& a, const Point& b)
{
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
double dot(const Point& a, const Point& b) { return a.x*b.x+a.y*b.y+a.z*b.z; }

// Odd/even ray intersections against a closed, validated triangle mesh. Three
// non-axis-aligned rays avoid depending on an individual coplanar triangle.
bool inside(const PrintMesh& mesh, const Point& point)
{
    const Point rays[]{{1, .319, .127}, {.217, 1, .413}, {.371, .193, 1}};
    int votes = 0;
    for (const auto& ray : rays) {
        int hits = 0;
        for (const auto& face : mesh.faces) {
            const auto& a = mesh.vertices[face[0]];
            const auto& b = mesh.vertices[face[1]];
            const auto& c = mesh.vertices[face[2]];
            const Point edge1 = subtract(b, a), edge2 = subtract(c, a);
            const Point p = cross(ray, edge2);
            const double determinant = dot(edge1, p);
            if (std::abs(determinant) < 1e-12) continue;
            const double inverse = 1.0/determinant;
            const Point t = subtract(point, a);
            const double u = dot(t, p)*inverse;
            if (u < 1e-9 || u > 1.0-1e-9) continue;
            const Point q = cross(t, edge1);
            const double v = dot(ray, q)*inverse;
            if (v < 1e-9 || u+v > 1.0-1e-9) continue;
            if (dot(edge2, q)*inverse > 1e-9) ++hits;
        }
        if (hits % 2) ++votes;
    }
    return votes >= 2;
}

bool sameBounds(const MeshBounds& a, const MeshBounds& b)
{
    if (!a.valid || !b.valid) return false;
    constexpr double tolerance = 1e-5;
    return std::abs(a.minimum.x-b.minimum.x)<tolerance &&
           std::abs(a.minimum.y-b.minimum.y)<tolerance &&
           std::abs(a.minimum.z-b.minimum.z)<tolerance &&
           std::abs(a.maximum.x-b.maximum.x)<tolerance &&
           std::abs(a.maximum.y-b.maximum.y)<tolerance &&
           std::abs(a.maximum.z-b.maximum.z)<tolerance;
}

QString meshHash(const PrintMesh& mesh)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << quint64(mesh.vertices.size()) << quint64(mesh.faces.size());
    for (const auto& vertex : mesh.vertices) stream << vertex.x << vertex.y << vertex.z;
    for (const auto& face : mesh.faces) stream << quint32(face[0]) << quint32(face[1]) << quint32(face[2]);
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

bool placementsOverlap(const FitCalibrationZone& a, const FitCalibrationZone& b)
{
    const auto interval = [](double a0, double a1, double b0, double b1) {
        return a0 < b1 && b0 < a1;
    };
    const double gap = std::max(a.clearanceMillimetres, b.clearanceMillimetres);
    return interval(a.bounds.minimum.x+a.translation.x-gap, a.bounds.maximum.x+a.translation.x+gap,
                    b.bounds.minimum.x+b.translation.x, b.bounds.maximum.x+b.translation.x) &&
           interval(a.bounds.minimum.y+a.translation.y-gap, a.bounds.maximum.y+a.translation.y+gap,
                    b.bounds.minimum.y+b.translation.y, b.bounds.maximum.y+b.translation.y);
}

FitCalibrationZone makeZone(const PrintMesh& mesh, const MeshBounds& bounds,
                            const FitCalibrationExperiment& experiment,
                            const QString& sessionIdentity, const QString& context,
                            const QString& file, const QString& variant)
{
    FitCalibrationZone zone;
    zone.identity = experiment.artifactIdentity + QStringLiteral(":zone-v1");
    zone.featureFamily = experiment.featureFamily;
    zone.variant = variant;
    zone.correctionSemantic = experiment.correctionDimension == FitCorrectionDimension::Height
        ? QStringLiteral("height") : QStringLiteral("diameter");
    zone.semanticContract = experiment.hasRegenerationPrototype
        ? experiment.regenerationPrototype.evidenceContract : QString();
    zone.artifactIdentity = experiment.artifactIdentity;
    zone.sessionIdentity = sessionIdentity;
    zone.manufacturingContextFingerprint = context;
    zone.modeledOrientationIdentity = experiment.modeledOrientationIdentity;
    zone.intendedPrintOrientation = FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;
    zone.bounds = bounds;
    zone.meshSha256 = meshHash(mesh);
    zone.clearanceMillimetres = 2.0;
    zone.memberFile = file;
    zone.mesh = mesh;
    const double center = experiment.correctionDimension == FitCorrectionDimension::Height
        ? experiment.centerHeightCorrectionMillimetres : experiment.centerDiameterCorrectionMillimetres;
    QVector<FitCandidateValue> series;
    if (!FitCandidateSeries::generate(experiment.artifactIdentity, center,
                                      experiment.candidateSpacingMillimetres,
                                      experiment.candidates.size(), &series)) return zone;
    for (int i = 0; i < series.size(); ++i)
        if (experiment.candidates[i].index != series[i].index ||
            std::abs(FitCalibrationEvidencePolicy::candidateCorrection(experiment, experiment.candidates[i]) -
                     series[i].correctionMillimetres) > 1e-8) return zone;
    for (int i = 0; i < series.size(); ++i)
        series[i].functionalValueMillimetres =
            FitCalibrationEvidencePolicy::candidateFunctionalDimension(experiment, experiment.candidates[i]);
    zone.candidates = std::move(series);
    return zone;
}
} // namespace

bool FitCandidateSeries::generate(const QString& artifactIdentity, double center,
                                  double spacing, int count, QVector<FitCandidateValue>* values,
                                  QString* error)
{
    if (!values || artifactIdentity.trimmed().isEmpty() || count < 3 || count > 9 ||
        count % 2 == 0 || !std::isfinite(center) || !std::isfinite(spacing) || spacing <= 0)
        return fail(error, QStringLiteral("A nonempty artifact identity, odd candidate count from 3 to 9, finite center, and positive spacing are required."));
    QVector<FitCandidateValue> result;
    for (int i = 0; i < count; ++i) {
        const double correction = center + (i-count/2)*spacing;
        if (!std::isfinite(correction)) return fail(error, QStringLiteral("The candidate range is not finite."));
        result.push_back({i+1, QStringLiteral("%1:candidate-%2").arg(artifactIdentity).arg(i+1), correction, 0, {}});
    }
    *values = result;
    if (error) error->clear();
    return true;
}

FitCalibrationZone FitCalibrationPackage::standardStudOdZone(const PrintMesh& mesh,
    const MeshBounds& bounds, const FitCalibrationExperiment& experiment,
    const QString& sessionIdentity, const QString& context, const QString& file)
{
    auto zone = makeZone(mesh, bounds, experiment, sessionIdentity, context, file, QStringLiteral("StandardStudOD"));
    zone.marker = {QStringLiteral("keyed-end"), {.5, 5, 2.7}, {.5, 5, 1.5},
                   {bounds.maximum.x-.5, 5, 2.7}, 1};
    for (int i = 0; i < zone.candidates.size(); ++i) zone.candidates[i].position = {8.0+12.0*i, 5, 3};
    return zone;
}

FitCalibrationZone FitCalibrationPackage::postWallCellZone(const PrintMesh& mesh,
    const MeshBounds& bounds, const FitCalibrationExperiment& experiment,
    const QString& sessionIdentity, const QString& context, const QString& file)
{
    auto zone = makeZone(mesh, bounds, experiment, sessionIdentity, context, file, QStringLiteral("PostWallCell"));
    zone.marker = {QStringLiteral("wall-notch"), {2, .4, 4.2}, {4, .4, 4.2},
                   {bounds.maximum.x-2, .4, 4.2}, 1};
    for (int i = 0; i < zone.candidates.size(); ++i) zone.candidates[i].position = {9.0+18.0*i, 4, 1.6};
    return zone;
}

bool FitCalibrationPackage::validateZone(const FitCalibrationZone& zone, QString* error)
{
    if (zone.version < 1 || zone.identity !=
        QStringLiteral("%1:zone-v%2").arg(zone.artifactIdentity).arg(zone.version) ||
        zone.featureFamily.isEmpty() ||
        zone.variant.isEmpty() || zone.correctionSemantic.isEmpty() || zone.semanticContract.isEmpty() ||
        zone.artifactIdentity.isEmpty() || zone.sessionIdentity.isEmpty() ||
        zone.manufacturingContextFingerprint.isEmpty() || zone.modeledOrientationIdentity.isEmpty() ||
        zone.intendedPrintOrientation == FitPrintedOrientation::Unknown ||
        zone.intendedPrintOrientation == FitPrintedOrientation::OtherUnsupported ||
        zone.memberFile.isEmpty() || zone.meshSha256.size() != 64 ||
        !std::isfinite(zone.clearanceMillimetres) ||
        zone.clearanceMillimetres < 0 || !finite(zone.translation))
        return fail(error, QStringLiteral("The calibration zone has incomplete identity, orientation, placement, or contract metadata."));
    if (zone.candidates.size() < 3 || zone.candidates.size() > 9 || zone.candidates.size() % 2 == 0)
        return fail(error, QStringLiteral("The zone needs an odd candidate series from 3 to 9."));
    QSet<QString> candidateIds;
    for (int i = 0; i < zone.candidates.size(); ++i) {
        const auto& candidate = zone.candidates[i];
        if (candidate.index != i+1 || candidate.identity !=
            QStringLiteral("%1:candidate-%2").arg(zone.artifactIdentity).arg(i+1) ||
            candidateIds.contains(candidate.identity) || !std::isfinite(candidate.correctionMillimetres) ||
            !std::isfinite(candidate.functionalValueMillimetres) || candidate.functionalValueMillimetres <= 0 ||
            !finite(candidate.position) ||
            (i > 0 && candidate.position.x <= zone.candidates[i-1].position.x))
            return fail(error, QStringLiteral("Candidate order, identity, value, or physical position is invalid."));
        candidateIds.insert(candidate.identity);
    }
    if (zone.marker.candidateIndex != 1 || zone.marker.kind.isEmpty() ||
        !finite(zone.marker.emptyWitness) || !finite(zone.marker.materialWitness) ||
        !finite(zone.marker.ordinaryEndWitness) ||
        zone.marker.emptyWitness.x >= zone.candidates.front().position.x ||
        zone.marker.materialWitness.x >= zone.candidates.front().position.x ||
        zone.marker.ordinaryEndWitness.x <= zone.candidates.back().position.x)
        return fail(error, QStringLiteral("A physical marker on the Candidate #1 side is required."));
    const auto analysis = analyzeSource(zone.mesh);
    if (!validatePreparedMesh(analysis).ok() || !sameBounds(zone.bounds, analysis.bounds) ||
        zone.meshSha256 != meshHash(zone.mesh))
        return fail(error, QStringLiteral("The zone mesh is invalid or its recorded bounds differ from the physical mesh."));
    if (inside(zone.mesh, zone.marker.emptyWitness) ||
        !inside(zone.mesh, zone.marker.materialWitness) ||
        !inside(zone.mesh, zone.marker.ordinaryEndWitness))
        return fail(error, QStringLiteral("The declared Candidate #1 marker is absent from the physical mesh."));
    if (error) error->clear();
    return true;
}

bool FitCalibrationPackage::validate(const FitCalibrationPackageManifest& manifest,
    const QVector<FitCalibrationSession>& sessions, QString* error)
{
    if (manifest.identity.isEmpty() || manifest.version != FitCalibrationPackageManifest::CurrentVersion ||
        manifest.manufacturingContextFingerprint.isEmpty() || manifest.zones.isEmpty())
        return fail(error, QStringLiteral("The calibration package identity, version, context, or zone list is invalid."));
    QSet<QString> zoneIds, artifactIds, sessionIds, candidateIds;
    for (int zoneIndex = 0; zoneIndex < manifest.zones.size(); ++zoneIndex) {
        const auto& zone = manifest.zones[zoneIndex];
        if (!validateZone(zone, error)) return false;
        for (int earlier = 0; earlier < zoneIndex; ++earlier)
            if (zone.memberFile == manifest.zones[earlier].memberFile &&
                placementsOverlap(zone, manifest.zones[earlier]))
                return fail(error, QStringLiteral("Zones in one physical member overlap or lack the required clearance."));
        if (zone.manufacturingContextFingerprint != manifest.manufacturingContextFingerprint)
            return fail(error, QStringLiteral("The package contains mixed manufacturing contexts."));
        if (zoneIds.contains(zone.identity) || artifactIds.contains(zone.artifactIdentity) ||
            sessionIds.contains(zone.sessionIdentity))
            return fail(error, QStringLiteral("Package zone, artifact, and session references must be unique."));
        zoneIds.insert(zone.identity); artifactIds.insert(zone.artifactIdentity); sessionIds.insert(zone.sessionIdentity);
        for (const auto& candidate : zone.candidates) {
            if (candidateIds.contains(candidate.identity)) return fail(error, QStringLiteral("Duplicate candidate identity in package."));
            candidateIds.insert(candidate.identity);
        }
        const auto linked = std::find_if(sessions.cbegin(), sessions.cend(), [&](const auto& session) {
            return session.sessionIdentity == zone.sessionIdentity;
        });
        if (linked == sessions.cend() ||
            FitCalibrationLibrary::manufacturingContextFingerprint(linked->process) != manifest.manufacturingContextFingerprint)
            return fail(error, QStringLiteral("A zone has no compatible independent managed session."));
        if (linked->process.actualPrintedOrientation != FitPrintedOrientation::Unknown &&
            linked->process.actualPrintedOrientation != zone.intendedPrintOrientation)
            return fail(error, QStringLiteral("A zone conflicts with its recorded feature print orientation."));
        const auto matches = [&](const FitCalibrationExperiment& experiment) {
            if (experiment.artifactIdentity != zone.artifactIdentity ||
                experiment.featureFamily != zone.featureFamily ||
                experiment.modeledOrientationIdentity != zone.modeledOrientationIdentity ||
                !experiment.hasRegenerationPrototype ||
                experiment.regenerationPrototype.evidenceContract != zone.semanticContract ||
                (experiment.correctionDimension == FitCorrectionDimension::Height
                    ? QStringLiteral("height") : QStringLiteral("diameter")) != zone.correctionSemantic ||
                experiment.candidates.size() != zone.candidates.size()) return false;
            for (int i = 0; i < zone.candidates.size(); ++i)
                if (experiment.candidates[i].index != zone.candidates[i].index ||
                    std::abs(FitCalibrationEvidencePolicy::candidateCorrection(experiment, experiment.candidates[i]) -
                             zone.candidates[i].correctionMillimetres) > 1e-8 ||
                    std::abs(FitCalibrationEvidencePolicy::candidateFunctionalDimension(experiment, experiment.candidates[i]) -
                             zone.candidates[i].functionalValueMillimetres) > 1e-8) return false;
            return true;
        };
        if ((!linked->hasCoarseExperiment || !matches(linked->coarseExperiment)) &&
            (!linked->hasFineExperiment || !matches(linked->fineExperiment)))
            return fail(error, QStringLiteral("The zone does not match its referenced experiment."));
    }
    if (error) error->clear();
    return true;
}

QJsonObject FitCalibrationPackage::zoneToJson(const FitCalibrationZone& zone)
{
    QJsonArray candidates;
    for (const auto& candidate : zone.candidates)
        candidates.append(QJsonObject{{"index", candidate.index}, {"identity", candidate.identity},
                                      {"correctionMillimetres", candidate.correctionMillimetres},
                                      {"functionalValueMillimetres", candidate.functionalValueMillimetres},
                                      {"position", pointJson(candidate.position)}});
    const QJsonObject marker{{"kind", zone.marker.kind}, {"candidateIndex", zone.marker.candidateIndex},
        {"emptyWitness", pointJson(zone.marker.emptyWitness)},
        {"materialWitness", pointJson(zone.marker.materialWitness)},
        {"ordinaryEndWitness", pointJson(zone.marker.ordinaryEndWitness)}};
    return {{"identity", zone.identity}, {"version", zone.version},
        {"featureFamily", zone.featureFamily}, {"variant", zone.variant},
        {"correctionSemantic", zone.correctionSemantic}, {"semanticContract", zone.semanticContract},
        {"artifactIdentity", zone.artifactIdentity}, {"sessionIdentity", zone.sessionIdentity},
        {"manufacturingContextFingerprint", zone.manufacturingContextFingerprint},
        {"modeledOrientationIdentity", zone.modeledOrientationIdentity},
        {"intendedPrintOrientation", int(zone.intendedPrintOrientation)},
        {"candidates", candidates}, {"bounds", boundsJson(zone.bounds)},
        {"meshSha256", zone.meshSha256},
        {"clearanceMillimetres", zone.clearanceMillimetres}, {"marker", marker},
        {"memberFile", zone.memberFile}, {"translation", pointJson(zone.translation)}};
}

bool FitCalibrationPackage::zoneFromJson(const QJsonObject& json, FitCalibrationZone* out, QString* error)
{
    if (!out) return fail(error, QStringLiteral("A destination zone is required."));
    FitCalibrationZone zone;
    const auto string = [&](const char* key) { return json.value(QLatin1String(key)).toString(); };
    zone.identity = string("identity"); zone.version = json.value("version").toInt();
    zone.featureFamily = string("featureFamily"); zone.variant = string("variant");
    zone.correctionSemantic = string("correctionSemantic"); zone.semanticContract = string("semanticContract");
    zone.artifactIdentity = string("artifactIdentity"); zone.sessionIdentity = string("sessionIdentity");
    zone.manufacturingContextFingerprint = string("manufacturingContextFingerprint");
    zone.modeledOrientationIdentity = string("modeledOrientationIdentity");
    zone.intendedPrintOrientation = FitPrintedOrientation(json.value("intendedPrintOrientation").toInt());
    zone.clearanceMillimetres = json.value("clearanceMillimetres").toDouble(std::numeric_limits<double>::quiet_NaN());
    zone.memberFile = string("memberFile");
    zone.meshSha256 = string("meshSha256");
    if (!readBounds(json.value("bounds"), &zone.bounds) || !readPoint(json.value("translation"), &zone.translation) ||
        !json.value("marker").isObject() || !json.value("candidates").isArray())
        return fail(error, QStringLiteral("The calibration zone has invalid bounds, marker, placement, or candidates."));
    const auto marker = json.value("marker").toObject();
    zone.marker.kind = marker.value("kind").toString();
    zone.marker.candidateIndex = marker.value("candidateIndex").toInt();
    if (!readPoint(marker.value("emptyWitness"), &zone.marker.emptyWitness) ||
        !readPoint(marker.value("materialWitness"), &zone.marker.materialWitness) ||
        !readPoint(marker.value("ordinaryEndWitness"), &zone.marker.ordinaryEndWitness))
        return fail(error, QStringLiteral("The Candidate #1 marker witnesses are invalid."));
    for (const auto& value : json.value("candidates").toArray()) {
        if (!value.isObject()) return fail(error, QStringLiteral("A candidate is invalid."));
        const auto candidateJson = value.toObject();
        FitCandidateValue candidate;
        candidate.index = candidateJson.value("index").toInt();
        candidate.identity = candidateJson.value("identity").toString();
        candidate.correctionMillimetres = candidateJson.value("correctionMillimetres").toDouble(std::numeric_limits<double>::quiet_NaN());
        candidate.functionalValueMillimetres = candidateJson.value("functionalValueMillimetres").toDouble(std::numeric_limits<double>::quiet_NaN());
        if (!readPoint(candidateJson.value("position"), &candidate.position))
            return fail(error, QStringLiteral("A candidate position is invalid."));
        zone.candidates.push_back(candidate);
    }
    if (zone.identity.isEmpty() || zone.version < 1 || zone.featureFamily.isEmpty() ||
        zone.artifactIdentity.isEmpty() || zone.sessionIdentity.isEmpty() ||
        zone.manufacturingContextFingerprint.isEmpty() || zone.memberFile.isEmpty() ||
        zone.meshSha256.size() != 64 ||
        !std::isfinite(zone.clearanceMillimetres))
        return fail(error, QStringLiteral("The calibration zone is incomplete."));
    *out = zone; // Mesh is intentionally supplied separately from the portable manifest.
    if (error) error->clear();
    return true;
}

QJsonObject FitCalibrationPackage::toJson(const FitCalibrationPackageManifest& manifest)
{
    QJsonArray zones;
    for (const auto& zone : manifest.zones) zones.append(zoneToJson(zone));
    return {{"format", QStringLiteral("BrickSuiteFitCalibrationPackage")},
            {"version", manifest.version}, {"identity", manifest.identity},
            {"manufacturingContextFingerprint", manifest.manufacturingContextFingerprint},
            {"zones", zones}};
}

bool FitCalibrationPackage::fromJson(const QJsonObject& json, FitCalibrationPackageManifest* out, QString* error)
{
    if (!out || json.value("format").toString() != QStringLiteral("BrickSuiteFitCalibrationPackage") ||
        json.value("version").toInt() != FitCalibrationPackageManifest::CurrentVersion ||
        !json.value("zones").isArray())
        return fail(error, QStringLiteral("The calibration package format or version is unsupported."));
    FitCalibrationPackageManifest manifest;
    manifest.identity = json.value("identity").toString();
    manifest.version = json.value("version").toInt();
    manifest.manufacturingContextFingerprint = json.value("manufacturingContextFingerprint").toString();
    for (const auto& value : json.value("zones").toArray()) {
        if (!value.isObject()) return fail(error, QStringLiteral("A package zone is invalid."));
        FitCalibrationZone zone;
        if (!zoneFromJson(value.toObject(), &zone, error)) return false;
        manifest.zones.push_back(std::move(zone));
    }
    if (manifest.identity.isEmpty() || manifest.manufacturingContextFingerprint.isEmpty() || manifest.zones.isEmpty())
        return fail(error, QStringLiteral("The package identity, context, or zones are missing."));
    *out = std::move(manifest);
    if (error) error->clear();
    return true;
}

Point FitCalibrationPackage::placedMarker(const FitCalibrationZone& zone)
{
    return {zone.marker.emptyWitness.x+zone.translation.x,
            zone.marker.emptyWitness.y+zone.translation.y,
            zone.marker.emptyWitness.z+zone.translation.z};
}
} // namespace PrintGeometry
