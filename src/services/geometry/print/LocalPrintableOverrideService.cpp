#include "LocalPrintableOverrideService.h"
#include "PrintMeshAnalysis.h"
#include "PrintMeshConversion.h"
#include "SourceSurfaceQueryIndex.h"
#include "McutMeshBooleanService.h"
#include "LocalPrintableOverrideReview.h"
#include "../ThreeMfWriter.h"
#include <lib3mf_implicit.hpp>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace PrintGeometry {
namespace {
constexpr int FormatVersion = 1;
constexpr std::size_t MaximumFaces = 100000;
constexpr std::size_t MaximumVertices = 300000;
constexpr qint64 MaximumFileBytes = 64 * 1024 * 1024;
constexpr double BoundsTolerance = 0.10; // mm, fixed policy; never fitted to an import
constexpr double SurfaceTolerance = 0.20;
constexpr double SamplePitch = 0.50;
constexpr std::size_t MaximumSamples = 1000000;
bool eligible(const LocalPrintableOverrideService::Context& c)
{
    return c.partId > 0 && !c.partNumber.isEmpty() && c.source.ok()
        && c.source.externalFilePath.isEmpty() && !c.source.mesh.ldrawId.isEmpty()
        && !c.source.mesh.triangles.isEmpty();
}
QByteArray meshBytes(const PrintMesh& mesh)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << quint64(mesh.vertices.size()) << quint64(mesh.faces.size());
    for (const auto& p : mesh.vertices) stream << p.x << p.y << p.z;
    for (const auto& f : mesh.faces) stream << quint32(f[0]) << quint32(f[1]) << quint32(f[2]);
    return bytes;
}
QByteArray hash(const QByteArray& bytes) { return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex(); }
Point center(const MeshBounds& b) { return {(b.minimum.x+b.maximum.x)/2, (b.minimum.y+b.maximum.y)/2, (b.minimum.z+b.maximum.z)/2}; }
double length(const Point& a, const Point& b) { return std::hypot(a.x-b.x, a.y-b.y, a.z-b.z); }

// Deterministic barycentric lattice on EVERY face, including face interiors.
// Refuse excess work rather than silently thinning samples on a complex import.
bool deviation(const PrintMesh& from, const PrintMesh& to, double* maximum)
{
    std::vector<SourceSurfaceQueryIndex::Triangle> triangles;
    for (const auto& f : to.faces) triangles.push_back({to.vertices[f[0]],to.vertices[f[1]],to.vertices[f[2]]});
    SourceSurfaceQueryIndex index(triangles,0,0,1,0,0);
    std::size_t samples = 0;
    std::uint64_t exactTests = 0;
    *maximum = 0;
    for (const auto& f : from.faces) {
        const auto& a=from.vertices[f[0]];const auto& b=from.vertices[f[1]];const auto& c=from.vertices[f[2]];
        const double span=std::max({length(a,b),length(a,c),length(b,c)});
        if (!std::isfinite(span) || span > 10000) return false;
        const int n=std::max(1,int(std::ceil(span/SamplePitch)));
        samples += std::size_t(n+1)*std::size_t(n+2)/2;
        if (samples > MaximumSamples) return false;
        for (int i=0;i<=n;++i) for (int j=0;j<=n-i;++j) {
            const double u=double(i)/n,v=double(j)/n;
            const Point p{a.x+(b.x-a.x)*u+(c.x-a.x)*v,a.y+(b.y-a.y)*u+(c.y-a.y)*v,a.z+(b.z-a.z)*u+(c.z-a.z)*v};
            *maximum=std::max(*maximum,std::sqrt(index.nearest(p,&exactTests).squaredDistance));
            if(exactTests>20000000)return false;
        }
    }
    return true;
}
LocalPrintableOverrideService::Result failure(const QString& message)
{
    LocalPrintableOverrideService::Result result;result.diagnostic=message;return result;
}
QString topologyDiagnostic(const MeshAnalysisResult& a,const MeshValidationResult& validation)
{
    return QStringLiteral("%1. Components: %2; boundary edges: %3; non-manifold edges: %4; non-manifold vertices: %5; degenerate faces: %6; duplicate faces: %7; self-intersections: %8; consistently oriented: %9; signed volume: %10 mm³. Bounds: %11 × %12 × %13 mm. Bounds compatibility and source-fidelity checks were not run because topology validation failed.")
        .arg(QString::fromStdString(validation.message)).arg(a.connectedComponents).arg(a.boundaryEdges)
        .arg(a.nonManifoldEdges).arg(a.nonManifoldVertices).arg(a.degenerateFaces).arg(a.duplicateFaces)
        .arg(a.selfIntersections).arg(a.consistentlyOriented?QStringLiteral("yes"):QStringLiteral("no"))
        .arg(a.signedVolume,0,'g',9).arg(a.bounds.maximum.x-a.bounds.minimum.x,0,'f',6)
        .arg(a.bounds.maximum.y-a.bounds.minimum.y,0,'f',6).arg(a.bounds.maximum.z-a.bounds.minimum.z,0,'f',6);
}
}

LocalPrintableOverrideService::LocalPrintableOverrideService(QString root)
    : m_root(root.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+QStringLiteral("/printable-overrides") : std::move(root)) {}

PrintMesh LocalPrintableOverrideService::repairSource(const Context& context)
{
    PrintMesh mesh;
    // Retain every expanded source triangle. No preparation, seam weld, or viewer Scale.
    for (const auto& t : context.source.mesh.triangles) {
        Face face;
        int i=0;
        for (const auto& p : {t.a,t.b,t.c}) {
            face[i++]=std::uint32_t(mesh.vertices.size());
            mesh.vertices.push_back({0.4*double(p.x()),0.4*double(p.z()),-0.4*double(p.y())});
        }
        mesh.faces.push_back(face);
    }
    return mesh;
}

QByteArray LocalPrintableOverrideService::sourceFingerprint(const Context& c)
{
    QByteArray bytes=meshBytes(repairSource(c));
    QDataStream s(&bytes,QIODevice::Append);s.setVersion(QDataStream::Qt_6_0);
    s << c.partId << c.partNumber << c.source.mesh.ldrawId << c.source.mesh.sourceRelativePath;
    // Bind loaded geometry AND its reference ancestry; timestamps alone are insufficient.
    if (c.source.sourceModel) {
        const auto& model=*c.source.sourceModel;
        s << qint64(model.files.size());
        for (const auto& f:model.files) s << f.id << f.relativePath << int(f.classification) << f.description;
        s << qint64(model.references.size());
        for (const auto& r:model.references) {s << r.id << r.parentId << r.fileId << r.sourceLine << r.mirrored << r.inverted;for(double v:r.accumulatedTransform)s << v;}
        s << qint64(model.surfaces.size());
        for (const auto& f:model.surfaces) s << f.triangleIndex << f.referenceId << f.fileId << f.sourceLine << f.sourceType << f.certified << f.clipping << f.inverted;
    }
    return hash(bytes);
}

bool LocalPrintableOverrideService::exportRepairSource(const Context& c,const QString& path,QString* error)
{
    if (!eligible(c)) {if(error)*error=QStringLiteral("A loaded catalog Part with an internal Part identity is required.");return false;}
    ThreeMfWriter::Options options;
    options.objectName=c.partNumber+QStringLiteral(" — UNVALIDATED REPAIR SOURCE — nominal millimeters");
    options.partIdentity=c.partNumber;options.modelColor=QColor(160,160,160);
    return ThreeMfWriter::write(repairSource(c),path,options,error);
}

bool LocalPrintableOverrideService::readThreeMf(const QString& path,PrintMesh* mesh,QString* error)
{
    try {
        if (QFileInfo(path).size()<=0 || QFileInfo(path).size()>MaximumFileBytes) throw std::runtime_error("3MF file is empty or exceeds the 64 MiB import limit.");
        Lib3MF::CWrapper wrapper;auto model=wrapper.CreateModel();auto reader=model->QueryReader("3mf");
        // Repair tools add optional vendor metadata (e.g. Bambu Studio). Permit
        // only optional metadata/attribute warnings from pinned lib3mf; never
        // ignore missing geometry, invalid coordinates, or required extensions.
        reader->SetStrictModeActive(false);reader->ReadFromFile(path.toStdString());
        for(Lib3MF_uint32 i=0;i<reader->GetWarningCount();++i){
            Lib3MF_uint32 code=0;const auto warning=reader->GetWarning(i,code);
            constexpr Lib3MF_uint32 UnknownOptionalMetadata=0x80B2;
            constexpr Lib3MF_uint32 UnknownOptionalAttribute=0x80A7;
            // ReadMetaDataNode falls back to the prefix as namespace when a
            // vendor metadata name has no xmlns binding. Mesh parsing is intact.
            constexpr Lib3MF_uint32 UnresolvedMetadataNamespace=0x80AE;
            if(code!=UnknownOptionalMetadata&&code!=UnknownOptionalAttribute&&code!=UnresolvedMetadataNamespace)throw std::runtime_error("3MF reader warning: "+warning);
        }
        if (model->GetUnit()!=Lib3MF::eModelUnit::MilliMeter) throw std::runtime_error("Export the repaired mesh in millimeters. Import does not infer or change units.");
        auto items=model->GetBuildItems();
        if (items->Count()!=1) throw std::runtime_error("Import requires exactly one build item containing one repaired solid.");
        PrintMesh result;std::size_t visited=0;
        std::function<void(Lib3MF::PObject,std::vector<Lib3MF::sTransform>,int)> append;
        append=[&](Lib3MF::PObject object,std::vector<Lib3MF::sTransform> transforms,int depth) {
            if (depth>16 || ++visited>256) throw std::runtime_error("3MF component hierarchy exceeds the import limit.");
            if (object->IsComponentsObject()) {
                auto group=model->GetComponentsObjectByID(object->GetUniqueResourceID());
                if(group->GetComponentCount()>256)throw std::runtime_error("Too many 3MF components.");
                for(Lib3MF_uint32 i=0;i<group->GetComponentCount();++i){auto child=group->GetComponent(i);auto next=transforms;next.push_back(child->GetTransform());append(child->GetObjectResource(),next,depth+1);}
                return;
            }
            if(!object->IsMeshObject())throw std::runtime_error("Only triangle mesh 3MF objects are supported.");
            auto part=model->GetMeshObjectByID(object->GetUniqueResourceID());
            if(part->BeamLattice()->GetBeamCount()||part->BeamLattice()->GetBallCount())throw std::runtime_error("Export beam-lattice geometry as an explicit triangle mesh before importing.");
            if(result.vertices.size()+part->GetVertexCount()>MaximumVertices || result.faces.size()+part->GetTriangleCount()>MaximumFaces)throw std::runtime_error("Repaired mesh exceeds the 300,000 vertex / 100,000 face limit.");
            const auto offset=std::uint32_t(result.vertices.size());
            std::vector<Lib3MF::sPosition> vertices;part->GetVertices(vertices);
            for(const auto& p:vertices){Point q{p.m_Coordinates[0],p.m_Coordinates[1],p.m_Coordinates[2]};
                for(auto it=transforms.rbegin();it!=transforms.rend();++it){const auto& t=it->m_Fields;q={q.x*t[0][0]+q.y*t[1][0]+q.z*t[2][0]+t[3][0],q.x*t[0][1]+q.y*t[1][1]+q.z*t[2][1]+t[3][1],q.x*t[0][2]+q.y*t[1][2]+q.z*t[2][2]+t[3][2]};}
                if(!std::isfinite(q.x)||!std::isfinite(q.y)||!std::isfinite(q.z)||std::max({std::abs(q.x),std::abs(q.y),std::abs(q.z)})>100000)throw std::runtime_error("Invalid or excessive 3MF coordinates.");
                result.vertices.push_back(q);}
            std::vector<Lib3MF::sTriangle> faces;part->GetTriangleIndices(faces);
            for(const auto& f:faces){for(auto v:f.m_Indices)if(v>=vertices.size())throw std::runtime_error("Invalid 3MF triangle index.");result.faces.push_back({offset+f.m_Indices[0],offset+f.m_Indices[1],offset+f.m_Indices[2]});}
        };
        items->MoveNext();auto item=items->GetCurrent();append(item->GetObjectResource(),{item->GetObjectTransform()},0);
        *mesh=std::move(result);return true;
    } catch(const std::exception& e) {if(error)*error=QString::fromUtf8(e.what());return false;}
}

LocalPrintableOverrideService::Result LocalPrintableOverrideService::validate(const Context& c,PrintMesh mesh,bool allowReview)
{
    if(!eligible(c))return failure(QStringLiteral("Local overrides require a loaded catalog Part identity."));
    if(mesh.faces.empty()||mesh.faces.size()>MaximumFaces||mesh.vertices.size()>MaximumVertices)return failure(QStringLiteral("Override mesh is empty or exceeds validation limits."));
    auto analysis=analyzeSource(mesh);auto valid=validatePreparedMesh(analysis);
    if(!valid.ok())return failure(topologyDiagnostic(analysis,valid));
    if(c.source.mesh.triangles.size()>qint64(MaximumFaces))return failure(QStringLiteral("Authoritative Source exceeds the fidelity validation face limit."));
    const auto source=PrintMeshConversion::fromPartMesh(c.source.mesh);const auto sourceAnalysis=analyzeSource(source);
    if(source.faces.empty()||!sourceAnalysis.finite||!sourceAnalysis.indicesValid||sourceAnalysis.degenerateFaces)return failure(QStringLiteral("Authoritative source cannot be used for bounded fidelity validation."));
    // Remove slicer bed placement only. No inferred scale or rotation is applied.
    const auto a=center(sourceAnalysis.bounds),b=center(analysis.bounds);
    for(auto& p:mesh.vertices){p.x+=a.x-b.x;p.y+=a.y-b.y;p.z+=a.z-b.z;}
    analysis=analyzeSource(mesh);valid=validatePreparedMesh(analysis);
    if(!valid.ok())return failure(topologyDiagnostic(analysis,valid));
    const double boundsError=maximumBoundsDeviation(sourceAnalysis.bounds,analysis.bounds);
    if(boundsError>BoundsTolerance)return failure(QStringLiteral("Bounds differ from authoritative Source by %1 mm (limit %2 mm). Keep the original scale and orientation; no automatic rescaling is performed.").arg(boundsError,0,'f',4).arg(BoundsTolerance));
    double toSource=0,toRepair=0;
    if(!deviation(mesh,source,&toSource)||!deviation(source,mesh,&toRepair))return failure(QStringLiteral("Source-fidelity sampling exceeds the safe workload limit."));
    bool reviewable=false;QString reviewDiagnostic;
    if(toSource>SurfaceTolerance||toRepair>SurfaceTolerance){
        const auto strict=QStringLiteral("Source fidelity rejected: repair → Source %1 mm; Source → repair %2 mm; limit %3 mm.").arg(toSource,0,'f',4).arg(toRepair,0,'f',4).arg(SurfaceTolerance);
        if(!allowReview||toSource>SurfaceTolerance)return failure(strict);
        if(!OverrideReview::eligible(c.source,source,mesh,&reviewDiagnostic))return failure(strict+QLatin1Char(' ')+reviewDiagnostic);
        reviewable=true;
    }
    auto prepared=std::make_shared<PreparedMesh>();prepared->mesh=std::move(mesh);prepared->sourceAnalysis=sourceAnalysis;prepared->finalAnalysis=analysis;prepared->millimetreBounds=analysis.bounds;prepared->componentCount=analysis.connectedComponents;
    prepared->partReference=c.partNumber;prepared->ldrawIdentity=c.source.mesh.ldrawId;prepared->dependencyFingerprint=c.source.dependencyFingerprint;
    prepared->localRepairedOverride=true;prepared->preparationMethod=QStringLiteral("local-repaired-override-nominal-v1");
    prepared->sourceTriangleCount=c.source.mesh.triangles.size();prepared->preparedTriangleCount=prepared->mesh.faces.size();
    prepared->warnings << QStringLiteral("Externally repaired nominal geometry; source/fit ownership is unproven. Auto Fit and ManufacturingMesh compensation are unavailable.");
    prepared->dimensionalFidelity.maximumBoundsDeviationMillimetres=boundsError;prepared->dimensionalFidelity.allowedBoundsDeviationMillimetres=BoundsTolerance;
    Result result;result.prepared=prepared;result.diagnostic=QStringLiteral("Strictly Validated Local Override (Nominal): one closed, outward-oriented manifold. Sampled maximum repair → Source %1 mm; Source → repair %2 mm. Slicer placement translation removed; scale/orientation preserved. Auto Fit unavailable: repaired surfaces have no proven ownership.").arg(toSource,0,'f',4).arg(toRepair,0,'f',4);
    if(reviewable){result.prepared.reset();result.reviewCandidate=prepared;result.reviewable=true;result.reviewedSourceFingerprint=sourceFingerprint(c);result.reviewedMeshFingerprint=hash(meshBytes(prepared->mesh));result.diagnostic=reviewDiagnostic+QStringLiteral(" Sampled maximum repair → Source %1 mm; Source → repair %2 mm; maximum bounds deviation %3 mm.").arg(toSource,0,'f',6).arg(toRepair,0,'f',6).arg(boundsError,0,'f',6);}
    return result;
}

QString LocalPrintableOverrideService::storagePath(const Context& c) const
{
    return QDir(m_root).filePath(QString::number(c.partId)+QLatin1Char('-')+QString::fromLatin1(hash(c.source.mesh.ldrawId.toUtf8()))+QStringLiteral(".json"));
}

LocalPrintableOverrideService::Result LocalPrintableOverrideService::importRepaired(const Context& c,const QString& path,bool explicitlyConfirmed,const QByteArray& reviewedSource,const QByteArray& reviewedMesh) const
{
    const auto suffix=QFileInfo(path).suffix().toLower();
    const bool stl=suffix==QStringLiteral("stl");
    if(!stl&&suffix!=QStringLiteral("3mf"))return failure(QStringLiteral("File/container import failed: select a repaired .3mf or .stl file."));
    const QString units=stl?QStringLiteral("STL interpreted as millimeters. "):QString();
    PrintMesh mesh;QString error;if(!(stl?readStl(path,&mesh,&error):readThreeMf(path,&mesh,&error)))return failure(QStringLiteral("File/container import failed: %1%2").arg(units,error));
    auto result=validate(c,mesh,true);
    NormalizationResult normalization;
    if(!result.ok()&&eligible(c)&&analyzeSource(mesh).connectedComponents>1){
        normalization=normalizeClosedComponents(mesh);
        if(normalization.accepted&&normalization.normalized){
            result=validate(c,std::move(normalization.mesh),true);
            result.diagnostic=QStringLiteral("Closed-component union normalization: %1 Strict override validation: %2").arg(normalization.diagnostic,result.diagnostic);
        }else result.diagnostic+=QLatin1Char(' ')+normalization.diagnostic;
    }
    result.diagnostic=units+result.diagnostic;
    if(explicitlyConfirmed){
        if(!result.reviewable||reviewedSource.isEmpty()||reviewedMesh.isEmpty()||reviewedSource!=result.reviewedSourceFingerprint||reviewedMesh!=result.reviewedMeshFingerprint){auto rejected=failure(QStringLiteral("Review is stale or candidate changed. Validate and explicitly review again."));rejected.stale=true;return rejected;}
        auto accepted=std::make_shared<PreparedMesh>(*result.reviewCandidate);accepted->userAcceptedOverride=true;accepted->overrideIdentity=QString::fromLatin1(hash(result.reviewedSourceFingerprint+result.reviewedMeshFingerprint+"user-reviewed-nominal-v1"));accepted->preparationMethod=QStringLiteral("user-accepted-local-override-v1");
        accepted->warnings<<QStringLiteral("User accepted geometry after review; source exposure is not certified. Experimental Auto Fit requires a separate warning.");
        result.prepared=accepted;result.reviewable=false;result.reviewCandidate.reset();result.diagnostic=QStringLiteral("Ready — User-Accepted Local Override (Nominal). ")+result.diagnostic;
    }
    if(result.reviewable)return result;
    if(!result.ok()){result.diagnostic=QStringLiteral("Mesh loaded but override validation failed: %1").arg(result.diagnostic);return result;}
    QJsonArray vertices,faces;
    for(const auto& p:result.prepared->mesh.vertices)vertices.append(QJsonArray{p.x,p.y,p.z});
    for(const auto& f:result.prepared->mesh.faces)faces.append(QJsonArray{int(f[0]),int(f[1]),int(f[2])});
    QJsonObject document{{"version",FormatVersion},{"partId",c.partId},{"partNumber",c.partNumber},{"ldrawIdentity",c.source.mesh.ldrawId},{"sourceFingerprint",QString::fromLatin1(sourceFingerprint(c))},{"meshFingerprint",QString::fromLatin1(hash(meshBytes(result.prepared->mesh)))},{"vertices",vertices},{"faces",faces}};
    if(stl){document["importFormat"]=QStringLiteral("stl");document["interpretedUnits"]=QStringLiteral("millimeter");}
    if(result.prepared->userAcceptedOverride){document["acceptanceKind"]=QStringLiteral("user-reviewed-nominal");document["acceptanceVersion"]=1;document["validationVersion"]=1;document["explicitlyConfirmed"]=true;}
    if(normalization.normalized)document["normalization"]=QJsonObject{
        {"method",QStringLiteral("closed-component-union-v1")},{"backend",McutMeshBooleanService().versionIdentity()},
        {"inputComponents",int(normalization.components.size())},{"booleanOperations",1},
        {"inputTriangles",int(mesh.faces.size())},{"outputTriangles",int(result.prepared->mesh.faces.size())},
        {"elapsedMilliseconds",normalization.elapsedMilliseconds},{"workerDeadlineMilliseconds",10000},{"workerMemoryLimitMiB",512}};
    if(!QDir().mkpath(m_root))return failure(QStringLiteral("Cannot create local override storage."));
    QSaveFile file(storagePath(c));const auto bytes=QJsonDocument(document).toJson(QJsonDocument::Compact);
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())return failure(QStringLiteral("Cannot atomically store override. The previous override is unchanged."));
    return result;
}

LocalPrintableOverrideService::Result LocalPrintableOverrideService::load(const Context& c) const
{
    if(!eligible(c))return {};
    QFile file(storagePath(c));if(!file.exists())return {};
    if(file.size()>MaximumFileBytes||!file.open(QIODevice::ReadOnly))return failure(QStringLiteral("Local override cannot be read or exceeds the storage limit."));
    const auto document=QJsonDocument::fromJson(file.readAll()).object();
    if(document["version"].toInt()!=FormatVersion||document["partId"].toInt()!=c.partId||document["partNumber"].toString()!=c.partNumber||document["ldrawIdentity"].toString()!=c.source.mesh.ldrawId)return failure(QStringLiteral("Local override has incompatible identity or format."));
    if(document["sourceFingerprint"].toString().toLatin1()!=sourceFingerprint(c)){auto result=failure(QStringLiteral("Local override is stale: authoritative Source geometry or reference ancestry changed. Re-export and repair the current Source."));result.stale=true;return result;}
    const auto vertices=document["vertices"].toArray(),faces=document["faces"].toArray();
    if(vertices.size()>qint64(MaximumVertices)||faces.size()>qint64(MaximumFaces))return failure(QStringLiteral("Stored override exceeds mesh limits."));
    PrintMesh mesh;
    for(const auto& value:vertices){const auto p=value.toArray();if(p.size()!=3||!p[0].isDouble()||!p[1].isDouble()||!p[2].isDouble())return failure(QStringLiteral("Invalid stored vertex."));mesh.vertices.push_back({p[0].toDouble(),p[1].toDouble(),p[2].toDouble()});}
    for(const auto& value:faces){const auto f=value.toArray();if(f.size()!=3)return failure(QStringLiteral("Invalid stored face."));Face face;for(int i=0;i<3;++i){const int v=f[i].toInt(-1);if(v<0||v>=vertices.size()||f[i].toDouble(-1)!=v)return failure(QStringLiteral("Invalid stored index."));face[i]=std::uint32_t(v);}mesh.faces.push_back(face);}
    if(document["meshFingerprint"].toString().toLatin1()!=hash(meshBytes(mesh))){auto r=failure(QStringLiteral("Local override mesh fingerprint is stale; validate and review again."));r.stale=true;return r;}
    const bool userAccepted=document["acceptanceKind"].toString()==QStringLiteral("user-reviewed-nominal");
    if(userAccepted&&(document["acceptanceVersion"].toInt()!=1||document["validationVersion"].toInt()!=1||!document["explicitlyConfirmed"].toBool())){auto r=failure(QStringLiteral("User acceptance is stale or incomplete; explicit review is required again."));r.stale=true;return r;}
    auto result=validate(c,std::move(mesh),userAccepted);
    if(userAccepted&&(result.reviewable||result.ok())){auto accepted=std::make_shared<PreparedMesh>(*(result.reviewable?result.reviewCandidate:result.prepared));accepted->userAcceptedOverride=true;accepted->overrideIdentity=QString::fromLatin1(hash(sourceFingerprint(c)+document["meshFingerprint"].toString().toLatin1()+"user-reviewed-nominal-v1"));accepted->preparationMethod=QStringLiteral("user-accepted-local-override-v1");result.prepared=accepted;result.reviewCandidate.reset();result.reviewable=false;result.diagnostic=QStringLiteral("Ready — User-Accepted Local Override (Nominal); explicit review retained, not source-certified. ")+result.diagnostic;}
    if(result.ok()&&document["importFormat"].toString()==QStringLiteral("stl"))result.diagnostic=QStringLiteral("STL interpreted as millimeters. ")+result.diagnostic;
    if(result.ok()&&document["normalization"].toObject()["method"].toString()==QStringLiteral("closed-component-union-v1"))
        result.diagnostic=QStringLiteral("Stored closed-component union normalization (nominal, no fit ownership). ")+result.diagnostic;
    return result;
}

bool LocalPrintableOverrideService::remove(const Context& c,QString* error) const
{
    if(!eligible(c)){if(error)*error=QStringLiteral("A catalog Part identity is required.");return false;}
    QFile file(storagePath(c));if(!file.exists()||file.remove())return true;
    if(error)*error=QStringLiteral("Cannot remove the local override.");return false;
}
} // namespace PrintGeometry
