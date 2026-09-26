#include "FitCalibrationGenerationService.h"
#include "FitCalibrationArtifactLocation.h"
#include "FitCalibrationCapabilities.h"
#include "BallSocketCalibrationArtifact.h"
#include "PinBarrelHingeCalibrationArtifact.h"
#include "InterleavedFingerHingeCalibrationArtifact.h"
#include "ClickHingeCalibrationArtifact.h"
#include "RetainedRotatingWheelCalibrationArtifact.h"

#include "FitCalibrationFixtureLabel.h"
#include "../ThreeMfWriter.h"
#include "../LDrawLibraryService.h"
#include "RoundTechnicCalibrationArtifact.h"
#include "StandardStudCalibrationArtifact.h"
#include "StudReceivingCalibrationArtifact.h"
#include "FrictionlessTechnicPinCalibrationArtifact.h"
#include "StandardBarCalibrationArtifact.h"
#include "CClipBarReceiverCalibrationArtifact.h"
#include "BallJointCalibrationArtifact.h"
#include "FrictionTechnicPinCalibrationArtifact.h"
#include "TechnicAxleCalibrationArtifact.h"
#include "TechnicAxleHoleCalibrationArtifact.h"
#include "PlainRoundBoreWheelCalibrationArtifact.h"
#include <lib3mf_implicit.hpp>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace PrintGeometry {
namespace {
using Service=FitCalibrationGenerationService;
bool fail(QString* error,const QString& message){if(error)*error=message;return false;}
const FitCalibrationExperiment* active(const FitCalibrationSession& s){return s.hasFineExperiment?&s.fineExperiment:s.hasCoarseExperiment?&s.coarseExperiment:nullptr;}
struct Generated {
    PrintMesh mesh;
    QVector<ThreeMfWriter::NamedMesh> collection;
    FitCalibrationExperiment experiment;
    FitCalibrationSession parent;
    bool hasParent=false,alreadyLabeled=false;
    QString stage="coarse";
};
template<class Artifact>
bool sourceCandidates(const Artifact& artifact,const FitCalibrationExperiment& experiment,Generated* out,QString* error){
    if(!artifact.ok)return fail(error,artifact.diagnostic);
    out->experiment=experiment;
    // Keep detachable candidates in a compact plate layout. A single long row
    // needlessly magnifies float-coordinate serialization error far from origin.
    double width=0,depth=0;QVector<MeshBounds> candidateBounds;
    for(const auto& mesh:artifact.candidateMeshes){
        if(mesh.vertices.empty())return fail(error,"The source candidate has no vertices.");
        MeshBounds b;b.minimum=b.maximum=mesh.vertices.front();
        for(const auto& v:mesh.vertices){b.minimum.x=std::min(b.minimum.x,v.x);b.maximum.x=std::max(b.maximum.x,v.x);
            b.minimum.y=std::min(b.minimum.y,v.y);b.maximum.y=std::max(b.maximum.y,v.y);
            b.minimum.z=std::min(b.minimum.z,v.z);b.maximum.z=std::max(b.maximum.z,v.z);}
        candidateBounds.push_back(b);
        width=std::max(width,b.maximum.x-b.minimum.x);depth=std::max(depth,b.maximum.y-b.minimum.y);
    }
    for(int i=0;i<artifact.candidateMeshes.size();++i){
        const auto bounds=candidateBounds[i];
        out->collection.push_back({QString("Candidate %1").arg(i+1),artifact.candidateMeshes[i],
            {(i%3)*(width+6.0)-bounds.minimum.x,(i/3)*(depth+6.0)-bounds.minimum.y,-bounds.minimum.z}});
    }
    return true;
}
bool sourceFixture(const Service::Request& request,const QString& family,const QString& identity,
                   const QString& parent,double center,double spacing,int count,Generated* out,QString* error){
    const auto capability=FitCalibrationCapabilities::find(family);
    if(capability.sourcePart.isEmpty())return fail(error,"No installed-source generator is available.");
    const auto source=LDrawLibraryService::loadPart(request.libraryRoot,capability.sourcePart);
    if(!source.ok())return fail(error,"The required installed LDraw source could not be loaded: "+capability.sourcePart);
    if(family=="BallSocket"){
        BallSocketCalibrationDefinition d;d.artifactIdentity=identity;d.parentArtifactIdentity=parent;
        d.centerCorrectionMillimetres=center;d.spacingMillimetres=spacing;d.candidateCount=count;
        const auto a=BallSocketCalibrationArtifact::generate(source,d);
        return sourceCandidates(a,BallSocketCalibrationArtifact::observationTemplate(a,d),out,error);
    }
    if(family=="PinBarrelHinge"){
        PinBarrelHingeCalibrationDefinition d;d.artifactIdentity=identity;d.parentArtifactIdentity=parent;
        d.centerCorrectionMillimetres=center;d.spacingMillimetres=spacing;d.candidateCount=count;
        const auto a=PinBarrelHingeCalibrationArtifact::generate(source,d);
        return sourceCandidates(a,PinBarrelHingeCalibrationArtifact::observationTemplate(a,d),out,error);
    }
    if(family=="InterleavedFingerHinge"){
        InterleavedFingerHingeCalibrationDefinition d;d.artifactIdentity=identity;d.parentArtifactIdentity=parent;
        d.centerCorrectionMillimetres=center;d.spacingMillimetres=spacing;d.candidateCount=count;
        const auto a=InterleavedFingerHingeCalibrationArtifact::generate(source,d);
        return sourceCandidates(a,InterleavedFingerHingeCalibrationArtifact::observationTemplate(a,d),out,error);
    }
    if(family=="ClickHinge"){
        ClickHingeCalibrationDefinition d;d.artifactIdentity=identity;d.parentArtifactIdentity=parent;
        d.centerCorrectionMillimetres=center;d.spacingMillimetres=spacing;d.candidateCount=count;
        const auto a=ClickHingeCalibrationArtifact::generate(source,d);
        return sourceCandidates(a,ClickHingeCalibrationArtifact::observationTemplate(a,d),out,error);
    }
    if(family=="RetainedRotatingWheel"){
        RetainedRotatingWheelCalibrationDefinition d;d.artifactIdentity=identity;d.parentArtifactIdentity=parent;
        d.centerCorrectionMillimetres=center;d.spacingMillimetres=spacing;d.candidateCount=count;
        const auto a=RetainedRotatingWheelCalibrationArtifact::generate(source,d);
        return sourceCandidates(a,RetainedRotatingWheelCalibrationArtifact::observationTemplate(a,d),out,error);
    }
    if(parent.isEmpty()){const auto initial=PlainRoundBoreWheelCalibrationArtifact::generate(source);
        return sourceCandidates(initial,PlainRoundBoreWheelCalibrationArtifact::observationTemplate(initial),out,error);}
    PlainRoundBoreWheelCalibrationDefinition d;d.artifactIdentity=identity;d.parentArtifactIdentity=parent;
    d.centerCorrectionMillimetres=center;d.candidateSpacingMillimetres=spacing;d.candidateCount=count;
    const auto a=PlainRoundBoreWheelCalibrationArtifact::generate(source,active(request.parent)->regenerationPrototype,d);
    return sourceCandidates(a,PlainRoundBoreWheelCalibrationArtifact::observationTemplate(a),out,error);
}
bool continuation(const Service::Request& request,Generated* out,QString* error){
    const auto* current=active(request.parent);
    if(!current||!current->hasRegenerationPrototype||!FitCalibrationEvidencePolicy::continuationAvailable(*current))
        return fail(error,"No supported continuation is available for this evidence.");
    const auto source=*current;
    auto plan=FitCalibrationEvidencePolicy::nextSearchPlan(source);
    const bool extension=plan.boundary!=FitPreferredBoundary::None;
    const bool directVerification=!extension&&request.stage==Service::Stage::Verification;
    if(directVerification)plan=FitCalibrationEvidencePolicy::directVerificationPlan(source);
    const bool axleHoleModelCorrection=source.featureFamily=="TechnicAxleHole"&&
        source.regenerationPrototype.constructionRecipe=="technic-axle-hole-cross-profile-v1"&&
        source.artifactIdentity.contains("extension")&&plan.uniformResult==FitUniformResult::AllTooTight;
    const double spacing=axleHoleModelCorrection?.10:request.candidateSpacing>0&&!directVerification?request.candidateSpacing:plan.candidateSpacingMillimetres;
    const int count=axleHoleModelCorrection?7:request.candidateCount>0&&!directVerification?request.candidateCount:plan.candidateCount;
    const QString identity=axleHoleModelCorrection?TechnicAxleHoleArmWidthCalibrationArtifact::artifactIdentity():
        source.artifactIdentity+(extension?"-boundary-extension-v2":directVerification?"-direct-verification-v2":"-fine-search-v2");
    if(!FitCalibrationCapabilities::find(source.featureFamily).sourcePart.isEmpty()){
        if(!sourceFixture(request,source.featureFamily,identity,source.artifactIdentity,plan.centerCorrectionMillimetres,spacing,count,out,error))return false;
        if(out->experiment.regenerationPrototype.stableIdentity!=source.regenerationPrototype.stableIdentity||
           out->experiment.regenerationPrototype.evidenceContract!=source.regenerationPrototype.evidenceContract||
           out->experiment.regenerationPrototype.constructionRecipe!=source.regenerationPrototype.constructionRecipe)
            return fail(error,"The installed source does not match this historical continuation contract.");
        out->parent=request.parent;out->hasParent=true;
        out->stage=extension?"extension":directVerification?"verification":"fine-search";return true;
    }
    PrintMesh mesh;QVector<ThreeMfWriter::NamedMesh> collection;FitCalibrationExperiment verification;QString diagnostic;
    if(source.featureFamily=="FrictionlessTechnicPin"){
        FrictionlessTechnicPinCalibrationDefinition d;d.artifactIdentity=identity;d.parentArtifactIdentity=source.artifactIdentity;
        d.centerCorrectionMillimetres=plan.centerCorrectionMillimetres;d.spacingMillimetres=spacing;d.candidateCount=count;
        const auto a=FrictionlessTechnicPinCalibrationArtifact::generate(source.regenerationPrototype,d);
        if(a.ok){mesh=a.mesh;verification=FrictionlessTechnicPinCalibrationArtifact::observationTemplate(a);}diagnostic=a.diagnostic;
    }
    else if(source.featureFamily==QStringLiteral("StandardStud")){StandardStudCalibrationArtifactDefinition d;d.artifactIdentity=identity;d.parentArtifactIdentity=source.artifactIdentity;d.dimension=source.correctionDimension==FitCorrectionDimension::Height?StandardStudCalibrationDimension::Height:StandardStudCalibrationDimension::Diameter;d.centerCorrectionMillimetres=plan.centerCorrectionMillimetres;d.fixedDiameterCorrectionMillimetres=source.fixedDiameterCorrectionMillimetres;d.candidateSpacingMillimetres=spacing;d.candidateCount=count;const auto artifact=StandardStudCalibrationArtifact::generate(source.regenerationPrototype,d);if(artifact.ok){mesh=artifact.mesh;verification=StandardStudCalibrationArtifact::observationTemplate(artifact,d);}diagnostic=artifact.diagnostic;}
    else if(source.featureFamily==QStringLiteral("StandardBar")){StandardBarCalibrationDefinition d;d.artifactIdentity=identity;d.parentArtifactIdentity=source.artifactIdentity;d.centerCorrectionMillimetres=plan.centerCorrectionMillimetres;d.spacingMillimetres=spacing;d.candidateCount=count;const auto artifact=StandardBarCalibrationArtifact::generate(d);if(artifact.ok){mesh=artifact.mesh;verification=StandardBarCalibrationArtifact::observationTemplate(artifact,d);}diagnostic=artifact.diagnostic;}
    else if(source.featureFamily==QStringLiteral("CClipBarReceiver")){CClipBarReceiverCalibrationDefinition d;d.artifactIdentity=identity;d.parentArtifactIdentity=source.artifactIdentity;d.centerCorrectionMillimetres=plan.centerCorrectionMillimetres;d.spacingMillimetres=spacing;d.candidateCount=count;d.orientation=source.process.actualPrintedOrientation;const auto artifact=CClipBarReceiverCalibrationArtifact::generate(d);if(artifact.ok){mesh=artifact.mesh;verification=CClipBarReceiverCalibrationArtifact::observationTemplate(artifact,d);}diagnostic=artifact.diagnostic;}
    else if(source.featureFamily==QStringLiteral("BallJoint")){BallJointCalibrationDefinition d;d.artifactIdentity=identity;d.parentArtifactIdentity=source.artifactIdentity;d.centerCorrectionMillimetres=plan.centerCorrectionMillimetres;d.spacingMillimetres=spacing;d.candidateCount=count;d.orientation=source.process.actualPrintedOrientation;const auto artifact=BallJointCalibrationArtifact::generate(d);if(artifact.ok){mesh=artifact.mesh;verification=BallJointCalibrationArtifact::observationTemplate(artifact,d);}diagnostic=artifact.diagnostic;}
    else if(source.featureFamily==QStringLiteral("StudReceivingClutch")){StudReceivingCalibrationArtifactDefinition d;d.artifactIdentity=identity;d.parentArtifactIdentity=source.artifactIdentity;d.centerDiameterCorrectionMillimetres=plan.centerCorrectionMillimetres;d.candidateSpacingMillimetres=spacing;d.candidateCount=count;const bool wallPocket=source.regenerationPrototype.constructionRecipe==QStringLiteral("stud-receiving-wall-pocket-square-v1"),antiStud=source.regenerationPrototype.constructionRecipe==QStringLiteral("stud-receiving-antistud-bore-v1");const auto artifact=antiStud?StudReceivingCalibrationArtifact::generateAntiStudBore(source.regenerationPrototype,d):wallPocket?StudReceivingCalibrationArtifact::generateWallPocket(source.regenerationPrototype,d):StudReceivingCalibrationArtifact::generate(source.regenerationPrototype,d);if(artifact.ok){mesh=artifact.mesh;verification=StudReceivingCalibrationArtifact::observationTemplate(artifact,d);}diagnostic=artifact.diagnostic;}
    else if(source.featureFamily==QStringLiteral("FrictionTechnicPin")){FrictionTechnicPinCalibrationArtifactDefinition d;d.artifactIdentity=identity;d.parentArtifactIdentity=source.artifactIdentity;d.centerRidgeEnvelopeCorrectionMillimetres=plan.centerCorrectionMillimetres;d.candidateSpacingMillimetres=spacing;d.candidateCount=count;const auto artifact=FrictionTechnicPinCalibrationArtifact::generate(source.regenerationPrototype,d);if(artifact.ok){mesh=artifact.mesh;verification=FrictionTechnicPinCalibrationArtifact::observationTemplate(artifact);}diagnostic=artifact.diagnostic;}
    else if(source.featureFamily==QStringLiteral("TechnicAxle")){TechnicAxleCalibrationArtifactDefinition d;d.artifactIdentity=identity;d.parentArtifactIdentity=source.artifactIdentity;d.centerTipToTipCorrectionMillimetres=plan.centerCorrectionMillimetres;d.candidateSpacingMillimetres=spacing;d.candidateCount=count;const auto artifact=TechnicAxleCalibrationArtifact::generate(source.regenerationPrototype,d);if(artifact.ok){mesh=artifact.mesh;verification=TechnicAxleCalibrationArtifact::observationTemplate(artifact,d);}diagnostic=artifact.diagnostic;}
    else if(source.featureFamily==QStringLiteral("TechnicAxleHole")&&(axleHoleModelCorrection||source.regenerationPrototype.constructionRecipe==QStringLiteral("technic-axle-hole-arm-width-clearance-v2"))){auto prototype=source.regenerationPrototype;if(axleHoleModelCorrection){prototype.constructionRecipe=QStringLiteral("technic-axle-hole-arm-width-clearance-v2");prototype.evidenceContract=QStringLiteral("official-ldraw-axlehole-arm-width-clearance-v2");}TechnicAxleHoleArmWidthArtifactDefinition d;d.artifactIdentity=identity;d.parentArtifactIdentity=source.artifactIdentity;if(!axleHoleModelCorrection){d.centerArmWidthCorrectionMillimetres=plan.centerCorrectionMillimetres;d.candidateSpacingMillimetres=spacing;d.candidateCount=count;}const auto artifact=TechnicAxleHoleArmWidthCalibrationArtifact::generate(prototype,d);if(artifact.ok){mesh=artifact.mesh;verification=TechnicAxleHoleArmWidthCalibrationArtifact::observationTemplate(artifact,d);}diagnostic=artifact.diagnostic;}
    else if(source.featureFamily==QStringLiteral("TechnicAxleHole")){TechnicAxleHoleCalibrationArtifactDefinition d;d.artifactIdentity=identity;d.parentArtifactIdentity=source.artifactIdentity;d.centerTipToTipCorrectionMillimetres=plan.centerCorrectionMillimetres;d.candidateSpacingMillimetres=spacing;d.candidateCount=count;const auto artifact=TechnicAxleHoleCalibrationArtifact::generate(source.regenerationPrototype,d);if(artifact.ok){mesh=artifact.mesh;verification=TechnicAxleHoleCalibrationArtifact::observationTemplate(artifact,d);}diagnostic=artifact.diagnostic;}
    else if(source.featureFamily==QStringLiteral("RoundTechnicPassage")){RoundTechnicCalibrationArtifactDefinition d;d.artifactIdentity=identity;d.parentArtifactIdentity=source.artifactIdentity;d.centerDiameterCorrectionMillimetres=plan.centerCorrectionMillimetres;d.candidateSpacingMillimetres=spacing;d.candidateCount=count;const auto artifact=RoundTechnicCalibrationArtifact::generate(source.regenerationPrototype,d);if(artifact.ok){mesh=artifact.mesh;verification=RoundTechnicCalibrationArtifact::observationTemplate(artifact,d);}diagnostic=artifact.diagnostic;}
    else diagnostic=QStringLiteral("No source-faithful continuation fixture is available for this calibration contract.");

    if(mesh.faces.empty()&&collection.isEmpty())return fail(error,diagnostic);
    out->mesh=std::move(mesh);out->collection=std::move(collection);out->experiment=verification;
    out->parent=request.parent;out->hasParent=true;
    out->stage=extension?"extension":directVerification?"verification":"fine-search";
    return true;
}

template<class Artifact>
bool capture(const Artifact& artifact,const FitCalibrationExperiment& experiment,Generated* out,QString* error){
    if(!artifact.ok)return fail(error,artifact.diagnostic);
    out->mesh=artifact.mesh;out->experiment=experiment;return true;
}
bool coarse(const Service::Request& r,Generated* out,QString* error){
    if(!FitCalibrationCapabilities::find(r.family).sourcePart.isEmpty())
        return sourceFixture(r,r.family,{},{},0,r.family=="BallSocket"||r.family=="PlainRoundBoreWheel"?.1:.05,7,out,error);
    if(r.family=="RoundTechnicPassage"){
        const auto d=RoundTechnicCalibrationArtifact::parallelCoarseDefinition();
        const auto a=RoundTechnicCalibrationArtifact::generate(RoundTechnicCalibrationArtifact::canonicalPrototype(),d);
        return capture(a,RoundTechnicCalibrationArtifact::observationTemplate(a,d),out,error);
    }
    if(r.family=="StandardStud"){
        StandardStudCalibrationArtifactDefinition d;
        d.dimension=r.variant=="Height"?StandardStudCalibrationDimension::Height:StandardStudCalibrationDimension::Diameter;
        d.artifactIdentity=d.dimension==StandardStudCalibrationDimension::Height?StandardStudCalibrationArtifact::heightArtifactIdentity():StandardStudCalibrationArtifact::diameterArtifactIdentity();
        if(d.dimension==StandardStudCalibrationDimension::Height){
            bool found=false;double od=0;
            for(const auto& session:r.workspace.featureSessions){const auto* e=active(session);
                if(!e||e->featureFamily!="StandardStud"||e->correctionDimension!=FitCorrectionDimension::Diameter||e->state!=FitEvidenceState::Verified||
                   e->process.actualPrintedOrientation!=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate||
                   FitCalibrationLibrary::manufacturingContextFingerprint(session.process)!=FitCalibrationLibrary::manufacturingContextFingerprint(r.workspace.process))continue;
                for(const auto& c:e->candidates)if(c.index==e->preferredCandidateIndex){od=c.diameterCorrectionMillimetres;found=true;break;}
                if(found)break;
            }
            if(!found)return fail(error,"Verify Stud OD in this manufacturing workspace before generating Stud Height.");
            d.fixedDiameterCorrectionMillimetres=od;
            for(const auto& session:r.workspace.featureSessions){const auto* e=active(session);
                if(e&&e->featureFamily=="StandardStud"&&e->correctionDimension==FitCorrectionDimension::Height&&e->preferredCandidateIndex>0){
                    if(!StandardStudCalibrationArtifact::heightVerificationDefinition(*e,od,&d,error))return false;
                    out->parent=session;out->hasParent=true;out->stage="verification";break;
                }
            }
        }
        const auto a=StandardStudCalibrationArtifact::generate(StandardStudCalibrationArtifact::canonicalPrototype(),d);
        out->alreadyLabeled=d.artifactIdentity==StandardStudCalibrationArtifact::diameterArtifactIdentity()||d.artifactIdentity==StandardStudCalibrationArtifact::heightArtifactIdentity();
        return capture(a,StandardStudCalibrationArtifact::observationTemplate(a,d),out,error);
    }
    if(r.family=="StudReceivingClutch"){
        if(r.variant!="TubeWallCell"&&r.variant!="PostWallCell"&&r.variant!="WallPocket"&&r.variant!="AntiStudBore")return fail(error,"Unsupported receiving-clutch variant.");
        const auto a=r.variant=="PostWallCell"?StudReceivingCalibrationArtifact::generatePostWallCell():
            r.variant=="WallPocket"?StudReceivingCalibrationArtifact::generateWallPocket(false):
            r.variant=="AntiStudBore"?StudReceivingCalibrationArtifact::generateAntiStudBore():StudReceivingCalibrationArtifact::generate();
        return capture(a,StudReceivingCalibrationArtifact::observationTemplate(a),out,error);
    }
    if(r.family=="FrictionlessTechnicPin"){
        const auto a=FrictionlessTechnicPinCalibrationArtifact::generate();
        return capture(a,FrictionlessTechnicPinCalibrationArtifact::observationTemplate(a),out,error);
    }
    if(r.family=="FrictionTechnicPin"){
        const auto a=FrictionTechnicPinCalibrationArtifact::generate();
        return capture(a,FrictionTechnicPinCalibrationArtifact::observationTemplate(a),out,error);
    }
    if(r.family=="TechnicAxle"){
        const auto a=TechnicAxleCalibrationArtifact::generate();
        return capture(a,TechnicAxleCalibrationArtifact::observationTemplate(a),out,error);
    }
    if(r.family=="TechnicAxleHole"){
        const auto a=TechnicAxleHoleArmWidthCalibrationArtifact::generate();
        return capture(a,TechnicAxleHoleArmWidthCalibrationArtifact::observationTemplate(a),out,error);
    }
    if(r.family=="StandardBar"){
        const auto a=StandardBarCalibrationArtifact::generate();
        return capture(a,StandardBarCalibrationArtifact::observationTemplate(a),out,error);
    }
    if(r.family=="CClipBarReceiver"){
        CClipBarReceiverCalibrationDefinition d;d.orientation=r.orientation;
        d.artifactIdentity=r.orientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate?CClipBarReceiverCalibrationArtifact::parallelArtifactIdentity():CClipBarReceiverCalibrationArtifact::artifactIdentity();
        const auto a=CClipBarReceiverCalibrationArtifact::generate(d);
        return capture(a,CClipBarReceiverCalibrationArtifact::observationTemplate(a,d),out,error);
    }
    if(r.family=="BallJoint"){
        BallJointCalibrationDefinition d;d.orientation=r.orientation;
        if(r.orientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate){d.centerCorrectionMillimetres=-.15;d.spacingMillimetres=.15;}
        d.artifactIdentity=r.orientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate?BallJointCalibrationArtifact::parallelArtifactIdentity():BallJointCalibrationArtifact::artifactIdentity();
        const auto a=BallJointCalibrationArtifact::generate(d);
        return capture(a,BallJointCalibrationArtifact::observationTemplate(a,d),out,error);
    }
    return fail(error,"No supported generator is available for this calibration family.");
}
QByteArray read(const QString& path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
QString digest(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};QCryptographicHash h(QCryptographicHash::Sha256);if(!h.addData(&f))return {};return QString::fromLatin1(h.result().toHex());}
bool writeJson(const QString& path,const QJsonObject& object,QString* error){
    QSaveFile file(path);const auto bytes=QJsonDocument(object).toJson(QJsonDocument::Indented);
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())return fail(error,"Could not write package file: "+file.errorString());return true;
}
bool reopen(const QString& path,const QVector<ThreeMfWriter::NamedMesh>* expected,QString* error){
    try{
        Lib3MF::CWrapper wrapper;auto model=wrapper.CreateModel();model->QueryReader("3mf")->ReadFromFile(path.toStdString());
        if(model->GetUnit()!=Lib3MF::eModelUnit::MilliMeter)return fail(error,"Calibration 3MF units changed.");
        auto meshes=model->GetMeshObjects();int index=0;
        while(meshes->MoveNext()){
            const auto mesh=meshes->GetCurrentMeshObject();
            if(expected){
                if(index>=expected->size())return fail(error,"Unexpected calibration mesh object.");
                const auto& e=(*expected)[index].mesh;const auto shift=(*expected)[index].translation;
                if(mesh->GetVertexCount()!=e.vertices.size()||mesh->GetTriangleCount()!=e.faces.size())return fail(error,"Calibration geometry count changed on reopen.");
                for(std::size_t i=0;i<e.vertices.size();++i){const auto p=mesh->GetVertex(i);const auto& v=e.vertices[i];
                    if(std::abs(p.m_Coordinates[0]-float(v.x+shift.x))>1e-5||std::abs(p.m_Coordinates[1]-float(v.y+shift.y))>1e-5||std::abs(p.m_Coordinates[2]-float(v.z+shift.z))>1e-5)return fail(error,"Calibration vertices changed on reopen.");}
                for(std::size_t i=0;i<e.faces.size();++i){const auto t=mesh->GetTriangle(i);for(int j=0;j<3;++j)if(t.m_Indices[j]!=e.faces[i][j])return fail(error,"Calibration faces changed on reopen.");}
            }
            ++index;
        }
        if(index==0||(expected&&index!=expected->size())||model->GetBuildItems()->Count()!=unsigned(index))return fail(error,"Incomplete calibration 3MF.");
        return true;
    }catch(const std::exception& e){return fail(error,QString::fromUtf8(e.what()));}
}
bool leafName(const QString& name){return !name.isEmpty()&&QFileInfo(name).fileName()==name&&!name.contains('\\')&&name!="."&&name!="..";}
QJsonObject definitionOnly(FitCalibrationSession session){
    session.process={};
    const auto clear=[](FitCalibrationExperiment& e){e.process={};e.state=FitEvidenceState::Draft;
        e.preferredCandidateIndex=0;e.performedUtc={};for(auto& c:e.candidates)c.observations.clear();};
    clear(session.coarseExperiment);clear(session.fineExperiment);for(auto& e:session.history)clear(e);
    return FitCalibrationSessionJson::toJson(session);
}
}

FitCalibrationGenerationService::FitCalibrationGenerationService(QString artifactRoot,QString managedRoot,Observer observer)
    :m_artifactRoot(artifactRoot.isEmpty()?FitCalibrationArtifactLocation::directory():std::move(artifactRoot)),
     m_managedRoot(std::move(managedRoot)),m_observer(std::move(observer)){}

QString FitCalibrationGenerationService::unavailableReason(const Request& request)
{
    const auto* parent=request.hasParent?active(request.parent):nullptr;
    if(request.hasParent&&!parent)return "No parent calibration is available.";
    const auto c=FitCalibrationCapabilities::find(parent?parent->featureFamily:request.family,parent?QString():request.variant);
    if(!c.initial)return "This family/variant is not supported.";
    if(request.orientation!=FitPrintedOrientation::Unknown&&!c.orientations.contains(request.orientation))
        return "This fixture cannot be generated in the requested orientation.";
    if(!c.sourcePart.isEmpty()&&!QFileInfo::exists(QDir(request.libraryRoot).filePath("parts/"+c.sourcePart+".dat")))
        return "Requires installed LDraw source "+c.sourcePart+" and its dependencies.";
    if(!parent&&!c.prerequisite.isEmpty()){
        bool found=false;
        for(const auto& session:request.workspace.featureSessions){const auto* e=active(session);
            if(e&&e->featureFamily=="StandardStud"&&e->correctionDimension==FitCorrectionDimension::Diameter&&e->state==FitEvidenceState::Verified&&
               e->process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate&&
               FitCalibrationLibrary::manufacturingContextFingerprint(session.process)==FitCalibrationLibrary::manufacturingContextFingerprint(request.workspace.process))found=true;
        }
        if(!found)return "Requires "+c.prerequisite+".";
    }
    if(parent){
        if(!c.continuation||!parent->hasRegenerationPrototype||!FitCalibrationEvidencePolicy::continuationAvailable(*parent))return "Record eligible parent evidence before continuing.";
        if(!c.sourcePart.isEmpty()&&!c.orientations.contains(parent->process.actualPrintedOrientation))return "Continuation requires the supported physical fixture orientation.";
        auto plan=FitCalibrationEvidencePolicy::nextSearchPlan(*parent);
        if(request.stage==Stage::Verification&&plan.boundary==FitPreferredBoundary::None)plan=FitCalibrationEvidencePolicy::directVerificationPlan(*parent);
        const int count=request.candidateCount?request.candidateCount:plan.candidateCount;
        const double spacing=request.candidateSpacing?request.candidateSpacing:plan.candidateSpacingMillimetres;
        if(count<c.minimumCandidates||count>c.maximumCandidates||count%2==0)return "Use an odd candidate count from 3 to 7; historical candidate mappings are unchanged.";
        if(!std::isfinite(spacing)||spacing<=0||!std::isfinite(plan.centerCorrectionMillimetres))return "Candidate spacing must be finite and positive.";
        if(plan.centerCorrectionMillimetres-(count/2)*spacing<c.minimumCorrection-1e-9||
           plan.centerCorrectionMillimetres+(count/2)*spacing>c.maximumCorrection+1e-9)
            return "The requested search exceeds this source fixture's established correction range. A wider source contract requires separate geometry validation.";
    }
    return {};
}

FitCalibrationGenerationService::Result FitCalibrationGenerationService::generate(const Request& request) const
{
    Result result;
    result.diagnostic=unavailableReason(request);if(!result.diagnostic.isEmpty())return result;
    try{
        const auto process=request.hasParent?request.parent.process:request.workspace.process;
        if(!request.hasParent&&(request.stage!=Stage::Coarse||request.candidateCount!=0||request.candidateSpacing!=0)){
            result.diagnostic="A coarse fixture uses its existing family definition; later stages require parent evidence.";return result;
        }
        if(process.printerIdentity.trimmed().isEmpty()||process.materialIdentity.trimmed().isEmpty()||
           process.profileName.trimmed().isEmpty()||!process.hasNozzleDiameter||!process.hasLayerHeight||
           !std::isfinite(process.nozzleDiameterMillimetres)||process.nozzleDiameterMillimetres<=0||
           !std::isfinite(process.layerHeightMillimetres)||process.layerHeightMillimetres<=0){
            result.diagnostic="Choose a complete manufacturing workspace before generating calibration.";return result;
        }
        Generated g;
        if(!(request.hasParent?continuation(request,&g,&result.diagnostic):coarse(request,&g,&result.diagnostic)))return result;
        auto& e=g.experiment;
        auto orientation=g.hasParent?g.parent.process.actualPrintedOrientation:e.process.actualPrintedOrientation;
        const auto instructions=e.process.orientationNotes;
        const auto intended=e.modeledOrientationIdentity.contains("perpendicular")?FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate:
            e.modeledOrientationIdentity.contains("parallel")?FitPrintedOrientation::FeatureAxisParallelToBuildPlate:orientation;
        if(request.orientation!=FitPrintedOrientation::Unknown){
            if(request.orientation!=intended){result.diagnostic="The requested orientation is not supported by this fixture.";return result;}
            orientation=request.orientation;
        }
        const QString definition=e.artifactIdentity;
        const QString printId=FitCalibrationLibrary::newStableIdentity();
        e.artifactIdentity=definition+"-print-"+printId;
        e.process=process;e.process.actualPrintedOrientation=orientation;
        // The generator's print instructions are preserved independently of shared context.
        e.process.orientationNotes=instructions;
        if(g.hasParent){
            const auto* parent=active(g.parent);
            result.session=FitCalibrationLibrary::continuationSession(g.parent,*parent,e);
        }else{
            result.session.sessionIdentity=FitCalibrationLibrary::newStableIdentity();
            result.session.process=e.process;result.session.hasCoarseExperiment=true;result.session.coarseExperiment=e;
        }
        const auto key=FitCalibrationNamingCatalog::keyFor(e,orientation);
        if(g.collection.isEmpty()){
            if(!g.alreadyLabeled){PrintMesh labeled;if(!FitCalibrationFixtureLabel::recessStandalone(g.mesh,key,&labeled,nullptr,&result.diagnostic))return result;g.mesh=std::move(labeled);}
            g.collection.push_back({e.artifactIdentity,std::move(g.mesh),{0,0,0}});
        }
        if(!QDir().mkpath(m_artifactRoot)){result.diagnostic="Cannot create the calibration artifact folder.";return result;}
        QTemporaryDir staging(QDir(m_artifactRoot).filePath(".pending-XXXXXX"));
        if(!staging.isValid()){result.diagnostic="Cannot create calibration staging directory.";return result;}
        const QString stem=QString::fromLatin1(FitCalibrationNamingCatalog::forKey(key).slug)+"-"+g.stage;
        const QString modelName=stem+".3mf",sessionName=stem+"-session.json";
        const QString modelPath=QDir(staging.path()).filePath(modelName),sessionPath=QDir(staging.path()).filePath(sessionName);
        ThreeMfWriter::Options options;options.collectionDecimalPrecision=9;options.objectName=e.artifactIdentity;options.partIdentity=e.artifactIdentity;
        options.modelColor=QColor(g.hasParent||request.family=="RoundTechnicPassage"||request.family=="StandardStud"||request.family=="StudReceivingClutch"?"#0055BF":"#A0A5A9");
        if(!ThreeMfWriter::writeCollection(g.collection,modelPath,options,&result.diagnostic))return result;
        if(m_observer&&!m_observer(Checkpoint::GeometryWritten,staging.path(),&result.diagnostic))return result;
        const auto sessionJson=FitCalibrationSessionJson::toJson(result.session);
        if(!writeJson(sessionPath,sessionJson,&result.diagnostic))return result;
        if(m_observer&&!m_observer(Checkpoint::BeforeReopen,staging.path(),&result.diagnostic))return result;
        FitCalibrationSession decoded;
        if(!FitCalibrationSessionJson::fromJson(QJsonDocument::fromJson(read(sessionPath)).object(),&decoded,&result.diagnostic)||
           FitCalibrationSessionJson::toJson(decoded)!=sessionJson){result.diagnostic="Companion session does not agree with its generation definition. "+result.diagnostic;return result;}
        if(!reopen(modelPath,&g.collection,&result.diagnostic))return result;
        const QJsonObject record{{"version",1},{"packageIdentity",printId},{"definitionIdentity",definition},
            {"fixture",modelName},{"companion",sessionName},{"fixtureSha256",digest(modelPath)},
            {"companionSha256",digest(sessionPath)},{"sessionIdentity",result.session.sessionIdentity}};
        if(!writeJson(QDir(staging.path()).filePath("publication.json"),record,&result.diagnostic))return result;
        result.directory=QDir(m_artifactRoot).filePath(stem+"-"+printId);
        if(QFileInfo::exists(result.directory)||!QDir().rename(staging.path(),result.directory)){result.diagnostic="Could not publish calibration package without overwriting an existing destination.";return result;}
        staging.setAutoRemove(false);result.published=true;
        result.fixturePath=QDir(result.directory).filePath(modelName);result.companionPath=QDir(result.directory).filePath(sessionName);
        if(m_observer&&!m_observer(Checkpoint::Published,result.directory,&result.diagnostic))return result;
        return recover(result.directory);
    }catch(const std::exception& e){result.diagnostic=QString::fromUtf8(e.what());return result;}
}

FitCalibrationGenerationService::Result FitCalibrationGenerationService::recover(const QString& directory) const
{
    Result result;result.directory=directory;
    try{
        if(QFileInfo(directory).fileName().startsWith(".pending-")){result.diagnostic="This package was not published.";return result;}
        const QDir dir(directory);const auto record=QJsonDocument::fromJson(read(dir.filePath("publication.json"))).object();
        const auto model=record["fixture"].toString(),companion=record["companion"].toString();
        if(record["version"].toInt()!=1||!leafName(model)||!leafName(companion)){result.diagnostic="Invalid calibration publication record.";return result;}
        result.fixturePath=dir.filePath(model);result.companionPath=dir.filePath(companion);
        if(digest(result.fixturePath).isEmpty()||digest(result.fixturePath)!=record["fixtureSha256"].toString()||
           digest(result.companionPath).isEmpty()||digest(result.companionPath)!=record["companionSha256"].toString()){
            result.diagnostic="The published calibration package is missing files or has changed.";return result;
        }
        if(!reopen(result.fixturePath,nullptr,&result.diagnostic)||
           !FitCalibrationSessionJson::fromJson(QJsonDocument::fromJson(read(result.companionPath)).object(),&result.session,&result.diagnostic))return result;
        if(QUuid(result.session.sessionIdentity).isNull()||result.session.sessionIdentity!=record["sessionIdentity"].toString()){result.diagnostic="Publication/session identity mismatch.";return result;}
        result.published=true;
        FitCalibrationLibrary library(m_managedRoot);
        if(!QDir().mkpath(library.storageRoot())){result.diagnostic="Cannot create managed calibration storage. Retry registration from this package.";return result;}
        QLockFile lock(QDir(library.storageRoot()).filePath("package-registration.lock"));
        if(!lock.tryLock(0)){result.diagnostic="Another package registration is in progress. Retry registration.";return result;}
        FitCalibrationSession registered;
        if(library.loadSession(result.session.sessionIdentity,&registered,nullptr)){
            // Recovery must not roll a user's later observations/verification back to the published draft.
            if(definitionOnly(registered)!=definitionOnly(result.session)){
                result.diagnostic="A different calibration definition already uses this session identity. Existing evidence was preserved.";return result;
            }
        }else{
            if(QFileInfo::exists(QDir(library.sessionsDirectory()).filePath(result.session.sessionIdentity+".json"))){
                result.diagnostic="Existing managed session cannot be read; recovery will not overwrite it.";return result;
            }
            if(!library.importSession(result.companionPath,&registered,&result.diagnostic))return result;
        }
        result.session=registered;result.registered=true;
        if(m_observer&&!m_observer(Checkpoint::Registered,directory,&result.diagnostic)){result.registered=false;return result;}
        result.diagnostic="Calibration package generated and registered.";
        return result;
    }catch(const std::exception& e){result.diagnostic=QString::fromUtf8(e.what());return result;}
}
} // namespace PrintGeometry
