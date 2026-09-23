#include "StudReceivingWallPocketSemantic.h"

#include "PrintMeshAnalysis.h"

#include <QCryptographicHash>
#include <QVector3D>
#include <cmath>

namespace PrintGeometry { namespace {
constexpr double MillimetresPerLdu = .4;
constexpr double Tolerance = 1e-5;

double length(const std::array<double, 12>& t, int column)
{
    return std::sqrt(t[column]*t[column] + t[4+column]*t[4+column] + t[8+column]*t[8+column]);
}
Point converted(double x, double y, double z)
{
    return {x*MillimetresPerLdu, -z*MillimetresPerLdu, -y*MillimetresPerLdu};
}
Point direction(const std::array<double, 12>& t, int column)
{
    const double size = length(t, column);
    const auto p = converted(t[column], t[4+column], t[8+column]);
    return {p.x/(size*MillimetresPerLdu), p.y/(size*MillimetresPerLdu), p.z/(size*MillimetresPerLdu)};
}
Point cross(const Point& a, const Point& b)
{
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
double dot(const Point& a, const Point& b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
bool named(const LDrawGeometry::LDrawSourceModel& model, const LDrawGeometry::ReferenceRecord& ref,
           const char* name)
{
    return ref.fileId >= 0 && ref.fileId < model.files.size() &&
        (model.files[ref.fileId].relativePath.compare(QString::fromLatin1(name), Qt::CaseInsensitive) == 0 ||
         model.files[ref.fileId].relativePath.compare(QStringLiteral("p/") + QString::fromLatin1(name), Qt::CaseInsensitive) == 0);
}
bool descendsFrom(int child, int parent, const LDrawGeometry::LDrawSourceModel& model)
{
    while (child >= 0 && child < model.references.size()) {
        if (child == parent) return true;
        child = model.references[child].parentId;
    }
    return false;
}
bool samePosition(const std::array<double, 12>& a, const std::array<double, 12>& b)
{
    return std::abs(a[3]-b[3]) < Tolerance && std::abs(a[7]-b[7]) < Tolerance &&
           std::abs(a[11]-b[11]) < Tolerance;
}
bool validFrame(const Point& axis, const Point& u, const Point& v)
{
    return std::abs(dot(axis, u)) < Tolerance && std::abs(dot(axis, v)) < Tolerance &&
           std::abs(dot(u, v)) < Tolerance && std::abs(dot(axis, cross(u, v))-1.0) < Tolerance;
}
bool fail(QString* diagnostic, const QString& message)
{
    if (diagnostic) *diagnostic = message;
    return false;
}
} // namespace

QVector<FunctionalFeature> StudReceivingWallPocketSemantic::recognize(const LDrawGeometry::LDrawLoadResult& source)
{
    QVector<FunctionalFeature> found;
    if (!source.ok() || !source.sourceModel) return found;
    const auto& model = *source.sourceModel;
    for (const auto& pocket : model.references) {
        // BFC INVERTNEXT may be canceled in the accumulated parity by a mirrored transform.
        if (!named(model, pocket, "box5.dat") || pocket.inverted == pocket.mirrored || pocket.parentId < 0) continue;
        const auto& t = pocket.accumulatedTransform;
        const double halfU = length(t, 0), depth = length(t, 1), halfV = length(t, 2);
        if (std::abs(halfU-6.0) > Tolerance || std::abs(halfV-6.0) > Tolerance ||
            (std::abs(depth-4.0) > Tolerance && std::abs(depth-20.0) > Tolerance)) continue;
        const auto axis = direction(t, 1), u = direction(t, 0), v = direction(t, 2);
        if (!validFrame(axis, u, v) && !validFrame(axis, u, {-v.x, -v.y, -v.z})) continue;

        const LDrawGeometry::ReferenceRecord* shell = nullptr;
        bool conflictingPrimitive = false;
        for (const auto& sibling : model.references) {
            if (sibling.parentId != pocket.parentId || sibling.id == pocket.id) continue;
            if (named(model, sibling, "stud3.dat") || named(model, sibling, "stud4.dat") ||
                named(model, sibling, "4-4cyli.dat")) conflictingPrimitive = true;
            if (!named(model, sibling, "box4t.dat") || !samePosition(t, sibling.accumulatedTransform)) continue;
            const auto& s = sibling.accumulatedTransform;
            if (std::abs(length(s, 0)-10.0) < Tolerance &&
                std::abs(length(s, 2)-10.0) < Tolerance &&
                std::abs(length(s, 1)-(depth+4.0)) < Tolerance &&
                dot(axis, direction(s, 1)) > 1.0-Tolerance) {
                if (shell) { conflictingPrimitive = true; break; }
                shell = &sibling;
            }
        }
        if (!shell || conflictingPrimitive) continue;
        int certifiedPocketTriangles = 0;
        for (const auto& surface : model.surfaces) {
            if (descendsFrom(surface.referenceId, pocket.id, model)) {
                if (!surface.certified) { certifiedPocketTriangles = 0; break; }
                ++certifiedPocketTriangles;
            }
        }
        if (certifiedPocketTriangles < 10) continue;

        FunctionalFeature feature;
        feature.family = FunctionalInterfaceFamily::StudReceivingClutch;
        feature.role = FunctionalInterfaceRole::Female;
        feature.materialSide = FunctionalMaterialSide::EmptyInsideMaterialOutside;
        feature.eligibility = FunctionalEligibility::Eligible;
        feature.confidence = SemanticConfidence::HighConfidence;
        feature.operandAction = FunctionalOperandAction::Subtract;
        feature.frame.origin = converted(t[3], t[7], t[11]);
        feature.frame.axis = axis;
        feature.frame.profileU = u;
        feature.frame.profileV = cross(axis, u);
        feature.frame.mirrored = pocket.mirrored;
        feature.nominalRadiusMillimetres = 6.0*MillimetresPerLdu;
        feature.nominalDiameterMillimetres = 12.0*MillimetresPerLdu;
        feature.nominalAxialExtentMillimetres = depth*MillimetresPerLdu;
        feature.nominalEngagementExtentMillimetres = feature.nominalAxialExtentMillimetres;
        feature.constructionRecipe = QStringLiteral("stud-receiving-wall-pocket-square-v1");
        feature.evidenceContract = depth < 10.0
            ? QStringLiteral("official-ldraw-box5-wall-pocket-plate-v1")
            : QStringLiteral("official-ldraw-box5-wall-pocket-brick-v1");
        feature.radialProfile = {{0.0, feature.nominalRadiusMillimetres},
                                 {feature.nominalAxialExtentMillimetres, feature.nominalRadiusMillimetres}};
        feature.provenance = {{model.files[pocket.fileId].relativePath, pocket.id, pocket.sourceLine, pocket.inverted},
                              {model.files[shell->fileId].relativePath, shell->id, shell->sourceLine, shell->inverted}};
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(QStringLiteral("%1|%2|%3|%4|%5")
            .arg(pocket.id).arg(pocket.sourceLine)
            .arg(feature.frame.origin.x, 0, 'g', 17)
            .arg(feature.frame.origin.y, 0, 'g', 17)
            .arg(feature.frame.origin.z, 0, 'g', 17).toUtf8());
        feature.stableIdentity = QStringLiteral("stud-receiving-wall-pocket:") +
                                 QString::fromLatin1(hash.result().toHex());
        feature.governingOperandIdentity = feature.stableIdentity + QStringLiteral(":body-walls");
        found.push_back(feature);
    }
    return found;
}

bool StudReceivingWallPocketSemantic::adjustPrepared(const PrintMesh& nominal,
    const FunctionalFeature& pocket, double correction, PrintMesh* adjusted, QString* diagnostic)
{
    if (!adjusted || pocket.family != FunctionalInterfaceFamily::StudReceivingClutch ||
        pocket.role != FunctionalInterfaceRole::Female ||
        pocket.constructionRecipe != QStringLiteral("stud-receiving-wall-pocket-square-v1") ||
        (pocket.evidenceContract != QStringLiteral("official-ldraw-box5-wall-pocket-plate-v1") &&
         pocket.evidenceContract != QStringLiteral("official-ldraw-box5-wall-pocket-brick-v1")) ||
        !std::isfinite(correction) || std::abs(correction) > 1.0 ||
        pocket.nominalRadiusMillimetres + correction*.5 <= 0.0)
        return fail(diagnostic, QStringLiteral("The WallPocket correction or semantic contract is invalid."));
    if (correction == 0.0) { *adjusted = nominal; if (diagnostic) diagnostic->clear(); return true; }
    PrintMesh result = nominal;
    const auto& f = pocket.frame;
    const double half = pocket.nominalRadiusMillimetres;
    int moved = 0;
    for (auto& point : result.vertices) {
        const Point relative{point.x-f.origin.x, point.y-f.origin.y, point.z-f.origin.z};
        const double axial = dot(relative, f.axis), u = dot(relative, f.profileU), v = dot(relative, f.profileV);
        if (axial < -1e-4 || axial > pocket.nominalAxialExtentMillimetres+1e-4 ||
            std::abs(u) > half+1e-4 || std::abs(v) > half+1e-4) continue;
        const double du = std::abs(std::abs(u)-half) < 1e-4 ? std::copysign(correction*.5, u) : 0.0;
        const double dv = std::abs(std::abs(v)-half) < 1e-4 ? std::copysign(correction*.5, v) : 0.0;
        if (du == 0.0 && dv == 0.0) continue;
        point.x += f.profileU.x*du + f.profileV.x*dv;
        point.y += f.profileU.y*du + f.profileV.y*dv;
        point.z += f.profileU.z*du + f.profileV.z*dv;
        ++moved;
    }
    if (moved < 8) return fail(diagnostic, QStringLiteral("The prepared mesh does not retain all certified WallPocket corner surfaces."));
    const auto before = analyzeSource(nominal), after = analyzeSource(result);
    if (!validatePreparedMesh(after).ok() ||
        maximumBoundsDeviation(before.bounds, after.bounds) > 1e-6)
        return fail(diagnostic, QStringLiteral("The corrected WallPocket failed topology or exterior-bounds validation."));
    *adjusted = std::move(result);
    if (diagnostic) *diagnostic = QStringLiteral("Four certified square pocket walls adjusted; pocket floor, shell, and exterior preserved.");
    return true;
}

} // namespace PrintGeometry
