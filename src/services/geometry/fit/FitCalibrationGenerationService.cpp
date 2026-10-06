/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#include "FitCalibrationGenerationService.h"
#include "FitCalibrationArtifactLocation.h"
#include "FitCalibrationCapabilities.h"
#include "FitCalibrationPackage.h"
#include <QSet>
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
#include <QRegularExpression>
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

namespace {
FitPrintedOrientation packageOrientation(const FitCalibrationExperiment& e){
    return e.modeledOrientationIdentity.contains("perpendicular")?FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate:
        e.modeledOrientationIdentity.contains("parallel")?FitPrintedOrientation::FeatureAxisParallelToBuildPlate:e.process.actualPrintedOrientation;
}
QString packageVariant(const FitCalibrationExperiment& e){
    switch(FitCalibrationNamingCatalog::keyFor(e,packageOrientation(e))){
    case FitCalibrationNameKey::StudOd:return "Diameter";
    case FitCalibrationNameKey::StudHeight:return "Height";
    case FitCalibrationNameKey::ClutchTubeWall:return "TubeWallCell";
    case FitCalibrationNameKey::ClutchPostWall:return "PostWallCell";
    case FitCalibrationNameKey::ClutchWallPocketBrick:
    case FitCalibrationNameKey::ClutchWallPocketPlate:return "WallPocket";
    case FitCalibrationNameKey::ClutchAntiStudBore:return "AntiStudBore";
    default:return {};
    }
}
}

FitCalibrationGenerationService::PackageRequest FitCalibrationGenerationService::recommended(
    const FitCalibrationWorkspace& workspace,const QString& libraryRoot)
{
    PackageRequest result;
    for(const auto& pair:QVector<QPair<QString,QString>>{{"StandardStud","Diameter"},{"StudReceivingClutch","TubeWallCell"},
        {"StudReceivingClutch","PostWallCell"},{"TechnicAxleHole",{}}}){
        Request r;r.family=pair.first;r.variant=pair.second;r.orientation=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;
        r.workspace=workspace;r.libraryRoot=libraryRoot;result.selections.push_back(r);
    }
    return result;
}

FitCalibrationGenerationService::PackagePlan FitCalibrationGenerationService::planPackage(const PackageRequest& request)
{
    PackagePlan result;
    if(request.selections.isEmpty()||request.selections.size()>32){result.diagnostic="Select from 1 to 32 independent calibrations.";return result;}
    QString context;QSet<QString> unique;QVector<int> core(4,-1);
    const auto coreRequests=recommended({}).selections;
    for(int i=0;i<request.selections.size();++i){const auto& r=request.selections[i];
        const auto reason=unavailableReason(r);if(!reason.isEmpty()){result.diagnostic=reason;return result;}
        const auto process=r.hasParent?r.parent.process:r.workspace.process;
        if(process.printerIdentity.trimmed().isEmpty()||process.materialIdentity.trimmed().isEmpty()||process.profileName.trimmed().isEmpty()||
           !process.hasNozzleDiameter||!process.hasLayerHeight||!std::isfinite(process.nozzleDiameterMillimetres)||process.nozzleDiameterMillimetres<=0||
           !std::isfinite(process.layerHeightMillimetres)||process.layerHeightMillimetres<=0){result.diagnostic="Complete the manufacturing workspace before planning a package.";return result;}
        const auto fingerprint=FitCalibrationLibrary::manufacturingContextFingerprint(process);
        if(context.isEmpty())context=fingerprint;
        if(context!=fingerprint){result.diagnostic="All package selections must share one manufacturing context.";return result;}
        if(!r.hasParent&&(r.stage!=Stage::Coarse||r.candidateCount||r.candidateSpacing!=0)){result.diagnostic="New coarse fixtures use their authoritative family candidate definition.";return result;}
        const auto* e=r.hasParent?active(r.parent):nullptr;
        const auto capability=FitCalibrationCapabilities::find(e?e->featureFamily:r.family,e?packageVariant(*e):r.variant);
        if(!e&&!capability.variant.isEmpty()&&r.variant.isEmpty()){result.diagnostic="Select an explicit calibration variant.";return result;}
        const auto orientation=e?packageOrientation(*e):r.orientation==FitPrintedOrientation::Unknown?capability.orientations.front():r.orientation;
        if(e&&r.orientation!=FitPrintedOrientation::Unknown&&r.orientation!=orientation){result.diagnostic="Continuation must retain its modeled fixture orientation.";return result;}
        const QString identity=e?e->artifactIdentity:r.family+"/"+capability.variant+"/"+QString::number(int(orientation));
        if(unique.contains(identity)){result.diagnostic="Duplicate calibration selections are not allowed.";return result;}unique.insert(identity);
        for(int c=0;c<4;++c)if(!r.hasParent&&r.stage==Stage::Coarse&&r.family==coreRequests[c].family&&r.variant==coreRequests[c].variant&&
            (r.orientation==FitPrintedOrientation::Unknown||r.orientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate))core[c]=i;
    }
    QSet<int> grouped;
    if(!core.contains(-1)){
        result.fixtures.push_back({"Perpendicular Core Calibration","perpendicular-core.3mf",core,true});
        for(int i:core)grouped.insert(i);
    }
    for(int i=0;i<request.selections.size();++i)if(!grouped.contains(i)){
        const auto& r=request.selections[i];const auto* e=r.hasParent?active(r.parent):nullptr;
        const auto c=FitCalibrationCapabilities::find(e?e->featureFamily:r.family,e?packageVariant(*e):r.variant);
        const auto orientation=e?packageOrientation(*e):r.orientation==FitPrintedOrientation::Unknown?c.orientations.front():r.orientation;
        const QString label=c.name+(c.variantName.isEmpty()?QString():" — "+c.variantName)+
            (orientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate?" — Parallel":" — Perpendicular");
        const QString slug=label.toLower().replace(QRegularExpression("[^a-z0-9]+"),"-");
        result.fixtures.push_back({label,QString("%1-%2.3mf").arg(result.fixtures.size()+1,2,10,QChar('0')).arg(slug),{i},false});
    }
    return result;
}

bool FitCalibrationGenerationService::isPackageCompanion(const QString& path)
{
    return QJsonDocument::fromJson(read(path)).object()["format"].toString()=="BrickSuiteUnifiedCalibrationPackage";
}

namespace {
QJsonArray packageCandidates(const FitCalibrationExperiment& e){
    QJsonArray result;for(const auto& c:e.candidates)result.push_back(QJsonObject{{"index",c.index},
        {"correction",FitCalibrationEvidencePolicy::candidateCorrection(e,c)},
        {"functionalValue",FitCalibrationEvidencePolicy::candidateFunctionalDimension(e,c)}});return result;
}
// Validate every member before any managed write. Hashes bind the published files;
// the existing pilot validator additionally binds generated zone meshes/candidates.
bool readPackage(const QString& directory,QJsonObject* envelope,QVector<FitCalibrationSession>* sessions,QString* error){
    const QDir dir(directory);const auto record=QJsonDocument::fromJson(read(dir.filePath("publication.json"))).object();
    if(record["version"].toInt()!=2||record["companion"].toString()!="package-session.json"||
       digest(dir.filePath("package-session.json")).isEmpty()||digest(dir.filePath("package-session.json"))!=record["companionSha256"].toString())
        return fail(error,"The package publication record or companion hash is invalid.");
    const auto json=QJsonDocument::fromJson(read(dir.filePath("package-session.json"))).object();
    if(json["format"].toString()!="BrickSuiteUnifiedCalibrationPackage"||json["version"].toInt()!=1||
       json["packageIdentity"]!=record["packageIdentity"]||QUuid(json["packageIdentity"].toString()).isNull())return fail(error,"Unsupported or mismatched package identity/version.");
    QSet<QString> identities{json["packageIdentity"].toString()},files,fixtures,usedFixtures;
    const auto fixtureEntries=json["fixtures"].toArray(),sessionEntries=json["sessions"].toArray();
    if(fixtureEntries.isEmpty()||sessionEntries.isEmpty()||sessionEntries.size()>32)return fail(error,"The package has no complete fixture/session plan.");
    for(const auto& value:fixtureEntries){const auto f=value.toObject();const auto id=f["identity"].toString(),file=f["file"].toString();
        if(QUuid(id).isNull()||identities.contains(id)||!leafName(file)||files.contains(file)||digest(dir.filePath(file)).isEmpty()||
           digest(dir.filePath(file))!=f["sha256"].toString()||!reopen(dir.filePath(file),nullptr,error))return fail(error,"A package fixture is missing, changed, unreadable, or duplicated.");
        identities.insert(id);fixtures.insert(id);files.insert(file);
    }
    QVector<FitCalibrationSession> decoded;
    for(const auto& value:sessionEntries){const auto entry=value.toObject();const auto id=entry["identity"].toString(),zone=entry["zoneIdentity"].toString();
        const auto file=entry["file"].toString(),fixture=entry["fixtureIdentity"].toString();
        if(QUuid(id).isNull()||identities.contains(id)||zone.isEmpty()||zone==id||identities.contains(zone)||!fixtures.contains(fixture)||
           !leafName(file)||files.contains(file)||digest(dir.filePath(file)).isEmpty()||digest(dir.filePath(file))!=entry["sha256"].toString())return fail(error,"A package session or zone mapping is missing, conflicting, or changed.");
        FitCalibrationSession session;
        const auto sessionJson=QJsonDocument::fromJson(read(dir.filePath(file))).object();
        if(!FitCalibrationSessionJson::fromJson(sessionJson,&session,error)||FitCalibrationSessionJson::toJson(session)!=entry["session"].toObject()||
           session.sessionIdentity!=id||FitCalibrationLibrary::manufacturingContextFingerprint(session.process)!=json["contextFingerprint"].toString())return fail(error,"Package and session definitions disagree.");
        const auto* experiment=active(session);
        if(!experiment||experiment->featureFamily!=entry["family"].toString()||packageVariant(*experiment)!=entry["variant"].toString()||
           int(packageOrientation(*experiment))!=entry["intendedOrientation"].toInt(-1)||packageCandidates(*experiment)!=entry["candidates"].toArray())return fail(error,"Package variant, orientation, or candidate mapping disagrees with its session.");
        identities.insert(id);identities.insert(zone);files.insert(file);usedFixtures.insert(fixture);decoded.push_back(session);
    }
    if(usedFixtures!=fixtures)return fail(error,"Every fixture must have independent session mappings.");
    auto contextCarrier=sessionEntries.first().toObject()["session"].toObject();contextCarrier["process"]=json["manufacturingContext"];
    FitCalibrationSession contextSession;
    if(!FitCalibrationSessionJson::fromJson(contextCarrier,&contextSession,error)||
       FitCalibrationLibrary::manufacturingContextFingerprint(contextSession.process)!=json["contextFingerprint"].toString())return fail(error,"Package manufacturing context is inconsistent.");
    for(const auto& value:fixtureEntries){const auto fixture=value.toObject();QSet<QString> expected,actual;
        for(const auto& id:fixture["sessionIdentities"].toArray()){if(expected.contains(id.toString()))return fail(error,"Duplicate fixture membership.");expected.insert(id.toString());}
        for(const auto& entry:sessionEntries)if(entry.toObject()["fixtureIdentity"]==fixture["identity"])actual.insert(entry.toObject()["identity"].toString());
        if(actual!=expected)return fail(error,"Fixture membership is incomplete.");
        const auto pilotJson=fixture["zones"].toObject();
        if(!pilotJson.isEmpty()){
            FitCalibrationPackageManifest pilot;if(!FitCalibrationPackage::fromJson(pilotJson,&pilot,error)||pilot.identity!=fixture["identity"].toString()||
                pilot.manufacturingContextFingerprint!=json["contextFingerprint"].toString()||pilot.zones.size()!=actual.size())return fail(error,"Combined zone manifest is inconsistent.");
            QSet<QString> mapped;
            for(const auto& zone:pilot.zones){int index=-1;for(int i=0;i<sessionEntries.size();++i)if(sessionEntries[i].toObject()["identity"].toString()==zone.sessionIdentity)index=i;
                if(index<0||mapped.contains(zone.sessionIdentity))return fail(error,"A combined zone has no unique session.");
                mapped.insert(zone.sessionIdentity);const auto entry=sessionEntries[index].toObject();const auto* e=active(decoded[index]);
                if(entry["zoneIdentity"].toString()!=zone.identity||entry["fixtureIdentity"]!=fixture["identity"]||zone.memberFile!=fixture["file"].toString()||
                   zone.sessionFile!=entry["file"].toString()||zone.artifactIdentity!=e->artifactIdentity||zone.featureFamily!=e->featureFamily||
                   zone.intendedPrintOrientation!=FitPrintedOrientation(entry["intendedOrientation"].toInt())||zone.semanticContract!=e->regenerationPrototype.evidenceContract||
                   zone.candidates.size()!=e->candidates.size())return fail(error,"Combined zone/session mapping disagrees.");
                for(int i=0;i<zone.candidates.size();++i)if(zone.candidates[i].index!=e->candidates[i].index||
                    std::abs(zone.candidates[i].correctionMillimetres-FitCalibrationEvidencePolicy::candidateCorrection(*e,e->candidates[i]))>1e-8||
                    std::abs(zone.candidates[i].functionalValueMillimetres-FitCalibrationEvidencePolicy::candidateFunctionalDimension(*e,e->candidates[i]))>1e-8)
                    return fail(error,"Combined zone candidate values disagree.");
            }
        }
    }

    *envelope=json;*sessions=decoded;return true;
}
}

FitCalibrationGenerationService::PackageResult FitCalibrationGenerationService::generatePackage(const PackageRequest& request,Progress progress) const
{
    PackageResult result;const auto plan=planPackage(request);if(!plan.ok()){result.diagnostic=plan.diagnostic;return result;}
    try{
        if(!QDir().mkpath(m_artifactRoot)){result.diagnostic="Cannot create the calibration artifact folder.";return result;}
        QTemporaryDir staging(QDir(m_artifactRoot).filePath(".pending-XXXXXX"));if(!staging.isValid()){result.diagnostic="Cannot stage the package.";return result;}
        const QDir dir(staging.path());const QString packageId=FitCalibrationLibrary::newStableIdentity();
        QJsonArray fixtureEntries,sessionEntries;int sessionNumber=0;
        const auto addSession=[&](const FitCalibrationSession& session,const QString& fixtureId,const QString& zoneId){
            const auto* e=active(session);const auto file=QString("calibration-%1-session.json").arg(++sessionNumber,2,10,QChar('0'));
            if(!writeJson(dir.filePath(file),FitCalibrationSessionJson::toJson(session),&result.diagnostic))return false;
            sessionEntries.push_back(QJsonObject{{"identity",session.sessionIdentity},{"zoneIdentity",zoneId},{"fixtureIdentity",fixtureId},
                {"family",e->featureFamily},{"variant",packageVariant(*e)},{"intendedOrientation",int(packageOrientation(*e))},{"candidates",packageCandidates(*e)},
                {"file",file},{"sha256",digest(dir.filePath(file))},{"session",FitCalibrationSessionJson::toJson(session)}});
            return true;
        };
        for(int index=0;index<plan.fixtures.size();++index){const auto& fixture=plan.fixtures[index];
            if(progress)progress(QString("Generating fixture %1 of %2 — %3...").arg(index+1).arg(plan.fixtures.size()).arg(fixture.name));
            const QString fixtureId=FitCalibrationLibrary::newStableIdentity(),path=dir.filePath(fixture.fileName);QJsonObject pilotJson;
            if(fixture.combinedCore){
                auto process=request.selections[fixture.selections.front()].workspace.process;process.actualPrintedOrientation=FitPrintedOrientation::Unknown;
                FitCalibrationPackageManifest pilot;QVector<FitCalibrationSession> sessions;PrintMesh assembled;
                if(!FitCalibrationPackage::generateFourZonePilot(process,&pilot,&sessions,&assembled,&result.diagnostic))return result;
                pilot.identity=fixtureId;QVector<ThreeMfWriter::NamedMesh> meshes;
                for(int i=0;i<pilot.zones.size();++i){auto& zone=pilot.zones[i];auto& session=sessions[i];
                    session.sessionIdentity=FitCalibrationLibrary::newStableIdentity();
                    session.coarseExperiment.artifactIdentity=zone.artifactIdentity+"-print-"+FitCalibrationLibrary::newStableIdentity();
                    zone.artifactIdentity=session.coarseExperiment.artifactIdentity;zone.identity=zone.artifactIdentity+":zone-v1";
                    zone.sessionIdentity=session.sessionIdentity;zone.memberFile=fixture.fileName;
                    zone.sessionFile=QString("calibration-%1-session.json").arg(sessionNumber+1,2,10,QChar('0'));
                    for(auto& candidate:zone.candidates)candidate.identity=zone.artifactIdentity+QString(":candidate-%1").arg(candidate.index);
                    meshes.push_back({zone.featureDisplayName,zone.mesh,zone.translation});
                    if(!addSession(session,fixtureId,zone.identity))return result;
                }
                if(!FitCalibrationPackage::validate(pilot,sessions,&result.diagnostic))return result;
                ThreeMfWriter::Options options;options.collectionDecimalPrecision=9;options.objectName=fixture.name;options.modelColor=QColor("#0055BF");
                if(!ThreeMfWriter::writeCollection(meshes,path,options,&result.diagnostic)||!reopen(path,&meshes,&result.diagnostic))return result;
                pilotJson=FitCalibrationPackage::toJson(pilot);
            }else{
                // Reuse the entire single-family pipeline in private disposable storage.
                // It cannot publish or register anything in the user's managed library.
                // Keep the temporary single-package path short for lib3mf on Windows.
                // Only validated copies enter the outer publication directory.
                QTemporaryDir work(QDir::temp().filePath("BrickSuite-cal-XXXXXX"));if(!work.isValid()){result.diagnostic="Cannot stage fixture generation.";return result;}
                const auto& selection=request.selections[fixture.selections.front()];
                const auto single=FitCalibrationGenerationService(QDir(work.path()).filePath("a"),QDir(work.path()).filePath("m")).generate(selection);
                if(!single.ok()){result.diagnostic=single.diagnostic;return result;}
                if(!QFile::copy(single.fixturePath,path)){result.diagnostic="Cannot assemble package fixture.";return result;}
                if(!addSession(single.session,fixtureId,FitCalibrationLibrary::newStableIdentity()))return result;
            }
            QJsonArray membership;for(const auto& entry:sessionEntries)if(entry.toObject()["fixtureIdentity"].toString()==fixtureId)membership.push_back(entry.toObject()["identity"]);
            fixtureEntries.push_back(QJsonObject{{"identity",fixtureId},{"name",fixture.name},{"file",fixture.fileName},{"sha256",digest(path)},{"zones",pilotJson},{"sessionIdentities",membership}});
            if(m_observer&&!m_observer(Checkpoint::GeometryWritten,staging.path(),&result.diagnostic))return result;
        }
        if(progress)progress("Writing calibration companions...");
        const auto& first=request.selections.front();const auto context=first.hasParent?first.parent.process:first.workspace.process;
        FitCalibrationSession contextSession;contextSession.process=context;
        const QJsonObject envelope{{"format","BrickSuiteUnifiedCalibrationPackage"},{"version",1},{"packageIdentity",packageId},
            {"contextFingerprint",FitCalibrationLibrary::manufacturingContextFingerprint(context)},
            {"manufacturingContext",FitCalibrationSessionJson::toJson(contextSession).value("process")},
            {"fixtures",fixtureEntries},{"sessions",sessionEntries}};
        if(!writeJson(dir.filePath("package-session.json"),envelope,&result.diagnostic)||
           !writeJson(dir.filePath("publication.json"),{{"version",2},{"packageIdentity",packageId},{"companion","package-session.json"},
                {"companionSha256",digest(dir.filePath("package-session.json"))}},&result.diagnostic))return result;
        if(progress)progress("Verifying calibration package...");
        if(m_observer&&!m_observer(Checkpoint::BeforeReopen,staging.path(),&result.diagnostic))return result;
        QJsonObject verified;QVector<FitCalibrationSession> decoded;
        if(!readPackage(staging.path(),&verified,&decoded,&result.diagnostic)||verified!=envelope)return result;
        if(progress)progress("Publishing calibration package...");
        result.directory=QDir(m_artifactRoot).filePath("calibration-package-"+packageId);
        if(QFileInfo::exists(result.directory)||!QDir().rename(staging.path(),result.directory)){result.diagnostic="Cannot publish complete package without overwriting a destination.";return result;}
        staging.setAutoRemove(false);result.published=true;result.companionPath=QDir(result.directory).filePath("package-session.json");
        if(m_observer&&!m_observer(Checkpoint::Published,result.directory,&result.diagnostic))return result;
        return recoverPackage(result.directory,progress);
    }catch(const std::exception& e){result.diagnostic=QString::fromUtf8(e.what());return result;}
}

FitCalibrationGenerationService::PackageResult FitCalibrationGenerationService::recoverPackage(const QString& directory,Progress progress) const
{
    PackageResult result;result.directory=directory;result.companionPath=QDir(directory).filePath("package-session.json");
    if(QFileInfo(directory).fileName().startsWith(".pending-")){result.diagnostic="This package was never published.";return result;}
    try{
        QJsonObject envelope;QVector<FitCalibrationSession> sessions;
        if(progress)progress("Verifying calibration package...");
        if(!readPackage(directory,&envelope,&sessions,&result.diagnostic))return result;
        result.published=true;for(const auto& f:envelope["fixtures"].toArray())result.fixturePaths.push_back(QDir(directory).filePath(f.toObject()["file"].toString()));
        FitCalibrationLibrary library(m_managedRoot);
        if(!QDir().mkpath(library.storageRoot())){result.diagnostic="Cannot create managed calibration storage. Recover this package to retry.";return result;}
        QLockFile lock(QDir(library.storageRoot()).filePath("package-registration.lock"));if(!lock.tryLock(0)){result.diagnostic="Another registration is active. Retry recovery.";return result;}
        // Detect definition conflicts for all siblings before beginning registration.
        for(const auto& session:sessions){FitCalibrationSession existing;
            if(library.loadSession(session.sessionIdentity,&existing,nullptr)){
                if(definitionOnly(existing)!=definitionOnly(session)){result.diagnostic="Existing managed evidence has a conflicting definition; it was preserved.";return result;}
            }else if(QFileInfo::exists(QDir(library.sessionsDirectory()).filePath(session.sessionIdentity+".json"))){result.diagnostic="An existing session cannot be read; recovery will not overwrite it.";return result;}
        }
        const auto entries=envelope["sessions"].toArray();
        for(int i=0;i<sessions.size();++i){
            if(progress)progress(QString("Registering calibration session %1 of %2...").arg(i+1).arg(sessions.size()));
            FitCalibrationSession registered;
            if(!library.loadSession(sessions[i].sessionIdentity,&registered,nullptr)&&
               !library.importSession(QDir(directory).filePath(entries[i].toObject()["file"].toString()),&registered,&result.diagnostic))return result;
            result.sessions.push_back(registered);++result.registeredCount;
            if(m_observer&&!m_observer(Checkpoint::Registered,directory,&result.diagnostic))return result;
        }
        result.registered=true;result.diagnostic="Calibration package generated and all sessions registered.";return result;
    }catch(const std::exception& e){result.diagnostic=QString::fromUtf8(e.what());return result;}
}
} // namespace PrintGeometry
