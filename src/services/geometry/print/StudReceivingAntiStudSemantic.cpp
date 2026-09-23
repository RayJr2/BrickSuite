#include "StudReceivingAntiStudSemantic.h"

#include "PrintMeshAnalysis.h"

#include <QCryptographicHash>
#include <QSet>
#include <cmath>

namespace PrintGeometry { namespace {
constexpr double MmPerLdu = .4;
constexpr double Tolerance = 1e-5;

Point converted(const QVector3D& p) { return {double(p.x())*MmPerLdu, double(p.z())*MmPerLdu, -double(p.y())*MmPerLdu}; }
Point add(const Point& a, const Point& b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Point subtract(const Point& a, const Point& b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Point scaled(const Point& p, double factor) { return {p.x*factor,p.y*factor,p.z*factor}; }
double dot(const Point& a, const Point& b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
double length(const Point& p) { return std::sqrt(dot(p,p)); }
Point cross(const Point& a, const Point& b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
Point column(const std::array<double,12>& t, int c) { return {t[c]*MmPerLdu,t[8+c]*MmPerLdu,-t[4+c]*MmPerLdu}; }
Point position(const std::array<double,12>& t, double localY)
{
    return {(t[3]+t[1]*localY)*MmPerLdu,(t[11]+t[9]*localY)*MmPerLdu,
            -(t[7]+t[5]*localY)*MmPerLdu};
}
bool antistud(const LDrawGeometry::LDrawSourceModel& model, const LDrawGeometry::ReferenceRecord& ref)
{
    if (ref.fileId < 0 || ref.fileId >= model.files.size()) return false;
    const auto path = model.files[ref.fileId].relativePath;
    return path.compare(QStringLiteral("p/stud4o.dat"),Qt::CaseInsensitive)==0 ||
           path.compare(QStringLiteral("stud4o.dat"),Qt::CaseInsensitive)==0;
}
bool descendsFrom(int child, int parent, const LDrawGeometry::LDrawSourceModel& model)
{
    while (child>=0 && child<model.references.size()) {
        if (child==parent) return true;
        child=model.references[child].parentId;
    }
    return false;
}
QString key(const Point& p)
{
    return QStringLiteral("%1|%2|%3").arg(std::llround(p.x*10000.0))
        .arg(std::llround(p.y*10000.0)).arg(std::llround(p.z*10000.0));
}
} // namespace

QVector<FunctionalFeature> StudReceivingAntiStudSemantic::recognize(const LDrawGeometry::LDrawLoadResult& source)
{
    QVector<FunctionalFeature> found;
    if (!source.ok() || !source.sourceModel) return found;
    const auto& model=*source.sourceModel;
    for (const auto& ref:model.references) {
        if (!antistud(model,ref) || ref.parentId<0) continue;
        const auto& t=ref.accumulatedTransform;
        const Point x=column(t,0), y=column(t,1), z=column(t,2);
        const double xLength=length(x), yLength=length(y), zLength=length(z);
        if (std::abs(xLength-MmPerLdu)>Tolerance || std::abs(zLength-MmPerLdu)>Tolerance ||
            yLength<MmPerLdu*.5 || yLength>MmPerLdu*4.0 ||
            std::abs(dot(x,y))>Tolerance || std::abs(dot(y,z))>Tolerance ||
            std::abs(dot(x,z))>Tolerance) continue;
        int certified=0;
        for (const auto& surface:model.surfaces) {
            if (!descendsFrom(surface.referenceId,ref.id,model)) continue;
            if (!surface.certified) { certified=0; break; }
            ++certified;
        }
        if (certified<32) continue;
        FunctionalFeature feature;
        feature.family=FunctionalInterfaceFamily::StudReceivingClutch;
        feature.role=FunctionalInterfaceRole::Female;
        feature.materialSide=FunctionalMaterialSide::EmptyInsideMaterialOutside;
        feature.eligibility=FunctionalEligibility::Eligible;
        feature.confidence=SemanticConfidence::HighConfidence;
        feature.operandAction=FunctionalOperandAction::Subtract;
        feature.frame.origin=position(t,-4.0);
        feature.frame.axis=scaled(y,1.0/yLength);
        feature.frame.profileU=scaled(x,1.0/xLength);
        feature.frame.profileV=cross(feature.frame.axis,feature.frame.profileU);
        feature.frame.mirrored=ref.mirrored;
        feature.nominalRadiusMillimetres=2.4;
        feature.nominalDiameterMillimetres=4.8;
        feature.nominalAxialExtentMillimetres=4.0*yLength;
        feature.nominalEngagementExtentMillimetres=feature.nominalAxialExtentMillimetres;
        feature.radialProfile={{0,2.4},{feature.nominalAxialExtentMillimetres,2.4}};
        feature.constructionRecipe=QStringLiteral("stud-receiving-antistud-bore-v1");
        feature.evidenceContract=QStringLiteral("official-ldraw-stud4o-antistud-bore-v1");
        feature.provenance={{model.files[ref.fileId].relativePath,ref.id,ref.sourceLine,ref.inverted}};
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(QStringLiteral("%1|%2|%3|%4|%5").arg(ref.id).arg(ref.sourceLine)
            .arg(feature.frame.origin.x,0,'g',17).arg(feature.frame.origin.y,0,'g',17)
            .arg(feature.frame.origin.z,0,'g',17).toUtf8());
        feature.stableIdentity=QStringLiteral("stud-receiving-antistud-bore:")+
            QString::fromLatin1(hash.result().toHex());
        feature.governingOperandIdentity=feature.stableIdentity+QStringLiteral(":body-bore");
        found.push_back(feature);
    }
    return found;
}

bool StudReceivingAntiStudSemantic::adjustPrepared(const LDrawGeometry::LDrawLoadResult& source,
    const PrintMesh& nominal, const FunctionalFeature& bore, double correction, PrintMesh* adjusted,
    QString* diagnostic)
{
    if (!adjusted || !source.ok() || !source.sourceModel ||
        bore.constructionRecipe!=QStringLiteral("stud-receiving-antistud-bore-v1") ||
        bore.evidenceContract!=QStringLiteral("official-ldraw-stud4o-antistud-bore-v1") ||
        bore.provenance.isEmpty() || !std::isfinite(correction) || std::abs(correction)>1.0 ||
        bore.nominalRadiusMillimetres+correction*.5<=0) {
        if (diagnostic) *diagnostic=QStringLiteral("The AntiStudBore correction or source contract is invalid.");
        return false;
    }
    if (correction==0.0) { *adjusted=nominal; if (diagnostic) diagnostic->clear(); return true; }
    const auto& model=*source.sourceModel;
    const int owner=bore.provenance.front().referenceId;
    QSet<QString> owned;
    for (const auto& surface:model.surfaces) {
        if (!surface.certified || !descendsFrom(surface.referenceId,owner,model) ||
            surface.triangleIndex<0 || surface.triangleIndex>=source.mesh.triangles.size()) continue;
        const auto& triangle=source.mesh.triangles[surface.triangleIndex];
        for (const auto& vertex:{triangle.a,triangle.b,triangle.c}) owned.insert(key(converted(vertex)));
    }
    PrintMesh result=nominal;
    int moved=0;
    for (auto& point:result.vertices) {
        if (!owned.contains(key(point))) continue;
        const auto relative=subtract(point,bore.frame.origin);
        const double axial=dot(relative,bore.frame.axis);
        const auto radial=subtract(relative,scaled(bore.frame.axis,axial));
        const double radius=length(radial);
        if (axial<-1e-4 || axial>bore.nominalAxialExtentMillimetres+1e-4 ||
            std::abs(radius-bore.nominalRadiusMillimetres)>1e-4) continue;
        point=add(point,scaled(radial,correction/(2.0*radius)));
        ++moved;
    }
    const auto analysis=analyzeSource(result);
    if (moved<16 || !validatePreparedMesh(analysis).ok() ||
        maximumBoundsDeviation(analyzeSource(nominal).bounds,analysis.bounds)>1e-6) {
        if (diagnostic) *diagnostic=QStringLiteral("The source-owned AntiStudBore wall correction failed topology or exterior validation.");
        return false;
    }
    *adjusted=std::move(result);
    if (diagnostic) *diagnostic=QStringLiteral("Certified centered AntiStudBore inner wall adjusted; exterior retained.");
    return true;
}

} // namespace PrintGeometry
