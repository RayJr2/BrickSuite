#include "RoundTechnicPassageSemantic.h"

#include <QCryptographicHash>
#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace PrintGeometry { namespace {
constexpr double MillimetresPerLdu = 0.4;
constexpr double PairTolerance = 1e-5;
constexpr double InterfaceIntrusion = 0.1;

Point convert(const QVector3D& p)
{
    return {double(p.x()) * MillimetresPerLdu,
            double(-p.z()) * MillimetresPerLdu,
            double(-p.y()) * MillimetresPerLdu};
}

Point direction(const std::array<double, 12>& t, double x, double y, double z)
{
    QVector3D value(float(t[0] * x + t[1] * y + t[2] * z),
                    float(t[4] * x + t[5] * y + t[6] * z),
                    float(t[8] * x + t[9] * y + t[10] * z));
    value = QVector3D(value.x(), -value.z(), -value.y());
    if (!qFuzzyIsNull(value.length())) value.normalize();
    return {value.x(), value.y(), value.z()};
}

Point subtract(const Point& a, const Point& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Point cross(const Point& a, const Point& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double dot(const Point& a, const Point& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
double length(const Point& value) { return std::sqrt(dot(value, value)); }

struct Candidate {
    const LDrawGeometry::ReferenceRecord* reference = nullptr;
    QString path;
    Point origin;
    Point inwardAxis;
    Point profileU;
};

bool pairable(const Candidate& a, const Candidate& b, double* separation)
{
    const Point delta = subtract(b.origin, a.origin);
    const double distance = length(delta);
    if (distance <= PairTolerance || dot(a.inwardAxis, b.inwardAxis) > -1.0 + PairTolerance) return false;
    const Point axis{delta.x / distance, delta.y / distance, delta.z / distance};
    if (dot(axis, a.inwardAxis) < 1.0 - PairTolerance || dot(axis, b.inwardAxis) > -1.0 + PairTolerance) return false;
    if (length(cross(delta, a.inwardAxis)) > PairTolerance) return false;
    *separation = distance;
    return true;
}
}

QVector<FunctionalFeature> RoundTechnicPassageSemantic::recognize(const LDrawGeometry::LDrawLoadResult& source)
{
    QVector<FunctionalFeature> result;
    if (!source.sourceModel) return result;

    QVector<Candidate> candidates;
    for (const auto& reference : source.sourceModel->references) {
        if (reference.fileId < 0 || reference.fileId >= source.sourceModel->files.size()) continue;
        const auto& file = source.sourceModel->files[reference.fileId];
        if (file.classification != LDrawGeometry::SourceClassification::Primitive) continue;
        if (file.relativePath.compare(QStringLiteral("p/peghole.dat"), Qt::CaseInsensitive) != 0
            && file.relativePath.compare(QStringLiteral("peghole.dat"), Qt::CaseInsensitive) != 0) continue;
        const auto& t = reference.accumulatedTransform;
        Candidate candidate;
        candidate.reference = &reference;
        candidate.path = file.relativePath;
        candidate.origin = convert(QVector3D(float(t[3]), float(t[7]), float(t[11])));
        candidate.inwardAxis = direction(t, 0, 1, 0);
        candidate.profileU = direction(t, 1, 0, 0);
        candidates.push_back(candidate);
    }

    QVector<bool> consumed(candidates.size(), false);
    for (int i = 0; i < candidates.size(); ++i) {
        if (consumed[i]) continue;
        int match = -1;
        double matchDistance = 0.0;
        for (int j = i + 1; j < candidates.size(); ++j) {
            if (consumed[j]) continue;
            double distance = 0.0;
            if (!pairable(candidates[i], candidates[j], &distance)) continue;
            if (match >= 0) { match = -2; break; }
            match = j;
            matchDistance = distance;
        }
        if (match < 0) continue;

        const Candidate& first = candidates[i];
        const Candidate& second = candidates[match];
        const Point delta = subtract(second.origin, first.origin);
        const Point axis{delta.x / matchDistance, delta.y / matchDistance, delta.z / matchDistance};
        FunctionalFeature feature;
        feature.family = FunctionalInterfaceFamily::RoundTechnicPassage;
        feature.role = FunctionalInterfaceRole::Female;
        feature.materialSide = FunctionalMaterialSide::EmptyInsideMaterialOutside;
        feature.eligibility = FunctionalEligibility::Eligible;
        feature.confidence = SemanticConfidence::HighConfidence;
        feature.frame.origin = {(first.origin.x + second.origin.x) * 0.5,
                                (first.origin.y + second.origin.y) * 0.5,
                                (first.origin.z + second.origin.z) * 0.5};
        feature.frame.axis = axis;
        feature.frame.profileU = first.profileU;
        feature.frame.profileV = cross(axis, first.profileU);
        feature.frame.mirrored = first.reference->mirrored || second.reference->mirrored;
        feature.nominalRadiusMillimetres = 2.4;
        feature.nominalDiameterMillimetres = 4.8;
        feature.nominalAxialExtentMillimetres = matchDistance;
        feature.nominalEngagementExtentMillimetres = matchDistance;
        const double half = matchDistance * 0.5;
        feature.radialProfile = {{-half - InterfaceIntrusion, 3.0}, {-half, 3.0}, {-half, 2.4},
                                 {half, 2.4}, {half, 3.0}, {half + InterfaceIntrusion, 3.0}};
        feature.operandAction = FunctionalOperandAction::Subtract;
        feature.constructionRecipe = QStringLiteral("round-through-passage-v1");
        feature.evidenceContract = QStringLiteral("official-ldraw-peghole-pair-v1");
        feature.provenance = {{first.path, first.reference->id, first.reference->sourceLine, first.reference->inverted},
                              {second.path, second.reference->id, second.reference->sourceLine, second.reference->inverted}};
        QCryptographicHash hash(QCryptographicHash::Sha256);
        const int low = std::min(first.reference->id, second.reference->id);
        const int high = std::max(first.reference->id, second.reference->id);
        hash.addData(QString("%1|%2|%3|%4|%5|%6|%7|%8")
                         .arg(low).arg(high)
                         .arg(feature.frame.origin.x, 0, 'g', 17).arg(feature.frame.origin.y, 0, 'g', 17)
                         .arg(feature.frame.origin.z, 0, 'g', 17).arg(axis.x, 0, 'g', 17)
                         .arg(axis.y, 0, 'g', 17).arg(axis.z, 0, 'g', 17).toUtf8());
        feature.stableIdentity = QStringLiteral("round-passage:") + QString::fromLatin1(hash.result().toHex());
        feature.governingOperandIdentity = feature.stableIdentity + QStringLiteral(":operand");
        result.push_back(feature);
        consumed[i] = true;
        consumed[match] = true;
    }
    return result;
}

} // namespace PrintGeometry
