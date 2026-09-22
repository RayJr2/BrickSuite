#include "StudReceivingPostSemantic.h"

#include <QCryptographicHash>
#include <QSet>
#include <QVector3D>
#include <cmath>

namespace PrintGeometry { namespace {
constexpr double MillimetresPerLdu = 0.4;
constexpr double NominalPostRadiusLdu = 4.0;
constexpr double Tolerance = 1e-5;

Point converted(const QVector3D& value)
{
    return {double(value.x()) * MillimetresPerLdu,
            double(-value.z()) * MillimetresPerLdu,
            double(-value.y()) * MillimetresPerLdu};
}

Point transformedDirection(const std::array<double, 12>& transform, double x, double y, double z)
{
    QVector3D value(float(transform[0] * x + transform[1] * y + transform[2] * z),
                    float(transform[4] * x + transform[5] * y + transform[6] * z),
                    float(transform[8] * x + transform[9] * y + transform[10] * z));
    value = QVector3D(value.x(), -value.z(), -value.y());
    if (!qFuzzyIsNull(value.length())) value.normalize();
    return {value.x(), value.y(), value.z()};
}

double vectorLength(const std::array<double, 12>& transform, double x, double y, double z)
{
    const double tx = transform[0] * x + transform[1] * y + transform[2] * z;
    const double ty = transform[4] * x + transform[5] * y + transform[6] * z;
    const double tz = transform[8] * x + transform[9] * y + transform[10] * z;
    return std::sqrt(tx * tx + ty * ty + tz * tz);
}

QSet<int> ancestors(int referenceId, const LDrawGeometry::LDrawSourceModel& source)
{
    QSet<int> result;
    int current = referenceId;
    while (current >= 0 && current < source.references.size() && !result.contains(current)) {
        result.insert(current);
        current = source.references[current].parentId;
    }
    return result;
}

bool isStructuralShellReference(const LDrawGeometry::ReferenceRecord& reference,
                                const LDrawGeometry::LDrawSourceModel& source,
                                const QSet<int>& postAncestors)
{
    if (reference.fileId < 0 || reference.fileId >= source.files.size()) return false;
    const QString path = source.files[reference.fileId].relativePath;
    if (!path.endsWith(QStringLiteral("box5.dat"), Qt::CaseInsensitive)
        && !path.endsWith(QStringLiteral("box4t.dat"), Qt::CaseInsensitive)) return false;
    const auto shellAncestors = ancestors(reference.id, source);
    for (int ancestor : postAncestors) if (shellAncestors.contains(ancestor)) return true;
    return false;
}

bool hasPostWallContext(int referenceId, const LDrawGeometry::LDrawSourceModel& source)
{
    const auto postAncestors = ancestors(referenceId, source);
    bool innerShell = false;
    bool outerShell = false;
    for (const auto& reference : source.references) {
        if (!isStructuralShellReference(reference, source, postAncestors)) continue;
        const QString path = source.files[reference.fileId].relativePath;
        innerShell = innerShell || path.endsWith(QStringLiteral("box5.dat"), Qt::CaseInsensitive);
        outerShell = outerShell || path.endsWith(QStringLiteral("box4t.dat"), Qt::CaseInsensitive);
    }
    return innerShell && outerShell;
}
}

QVector<FunctionalFeature> StudReceivingPostSemantic::recognize(const LDrawGeometry::LDrawLoadResult& source)
{
    QVector<FunctionalFeature> result;
    if (!source.sourceModel) return result;
    const auto& model = *source.sourceModel;
    for (const auto& reference : model.references) {
        if (reference.fileId < 0 || reference.fileId >= model.files.size()) continue;
        const auto& file = model.files[reference.fileId];
        if (file.classification != LDrawGeometry::SourceClassification::Primitive) continue;
        if (file.relativePath.compare(QStringLiteral("p/stud3.dat"), Qt::CaseInsensitive) != 0
            && file.relativePath.compare(QStringLiteral("stud3.dat"), Qt::CaseInsensitive) != 0) continue;
        if (!hasPostWallContext(reference.id, model)) continue;

        const auto& transform = reference.accumulatedTransform;
        const double radialX = vectorLength(transform, 1, 0, 0);
        const double radialZ = vectorLength(transform, 0, 0, 1);
        const double axialScale = vectorLength(transform, 0, 1, 0);
        if (radialX <= Tolerance || radialZ <= Tolerance || axialScale <= Tolerance
            || std::abs(radialX - radialZ) > Tolerance) continue;

        FunctionalFeature feature;
        feature.family = FunctionalInterfaceFamily::StudReceivingClutch;
        feature.role = FunctionalInterfaceRole::Female;
        feature.materialSide = FunctionalMaterialSide::MaterialInside;
        feature.eligibility = FunctionalEligibility::Eligible;
        feature.confidence = SemanticConfidence::HighConfidence;
        feature.operandAction = FunctionalOperandAction::Unite;
        feature.frame.origin = converted(QVector3D(float(transform[3]), float(transform[7]), float(transform[11])));
        feature.frame.axis = transformedDirection(transform, 0, -1, 0);
        feature.frame.profileU = transformedDirection(transform, 1, 0, 0);
        feature.frame.profileV = transformedDirection(transform, 0, 0, 1);
        feature.frame.mirrored = reference.mirrored;
        feature.nominalRadiusMillimetres = NominalPostRadiusLdu * radialX * MillimetresPerLdu;
        feature.nominalDiameterMillimetres = feature.nominalRadiusMillimetres * 2.0;
        feature.nominalAxialExtentMillimetres = 4.0 * axialScale * MillimetresPerLdu;
        feature.nominalEngagementExtentMillimetres = feature.nominalAxialExtentMillimetres;
        feature.radialProfile = {{0.0, feature.nominalRadiusMillimetres},
                                 {feature.nominalAxialExtentMillimetres, feature.nominalRadiusMillimetres}};
        feature.constructionRecipe = QStringLiteral("stud-receiving-post-wall-cell-v1");
        feature.evidenceContract = QStringLiteral("official-ldraw-stud3-post-wall-cell-v1");
        feature.provenance.push_back({file.relativePath, reference.id, reference.sourceLine, reference.inverted});
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(QStringLiteral("%1|%2|%3|%4|%5|%6")
                         .arg(reference.id).arg(reference.sourceLine)
                         .arg(feature.frame.origin.x, 0, 'g', 17)
                         .arg(feature.frame.origin.y, 0, 'g', 17)
                         .arg(feature.frame.origin.z, 0, 'g', 17)
                         .arg(feature.nominalAxialExtentMillimetres, 0, 'g', 17).toUtf8());
        feature.stableIdentity = QStringLiteral("stud-receiving-post-wall-cell:")
            + QString::fromLatin1(hash.result().toHex());
        feature.governingOperandIdentity = feature.stableIdentity + QStringLiteral(":operand");
        result.push_back(feature);
    }
    return result;
}

} // namespace PrintGeometry
