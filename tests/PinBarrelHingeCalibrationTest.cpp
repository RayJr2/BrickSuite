#include "../src/services/geometry/fit/PinBarrelHingeCalibrationArtifact.h"
#include "../src/services/geometry/fit/FitCalibrationLibrary.h"
#include "../src/services/geometry/print/PinBarrelHingeSemantic.h"
#include "../src/services/geometry/print/PrintMeshAnalysis.h"
#include "../src/services/geometry/ThreeMfWriter.h"
#include "../src/services/geometry/LDrawLibraryService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <lib3mf_implicit.hpp>
#include <cmath>

using namespace PrintGeometry;
namespace {
bool check(bool value,const QString& message) {
    if(!value)QTextStream(stderr)<<"FAIL: "<<message<<Qt::endl;
    return value;
}
double topHit(const PrintMesh& mesh,double x,double y) {
    double hit=-1e100;
    for(const auto& face:mesh.faces) {
        const auto& a=mesh.vertices[face[0]];
        const auto& b=mesh.vertices[face[1]];
        const auto& c=mesh.vertices[face[2]];
        const double den=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
        if(std::abs(den)<1e-12)continue;
        const double u=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/den;
        const double v=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/den;
        if(u>=-1e-8&&v>=-1e-8&&u+v<=1+1e-8)
            hit=std::max(hit,u*a.z+v*b.z+(1-u-v)*c.z);
    }
    return hit;
}
double bottomHit(const PrintMesh& mesh,double x,double y) {
    double hit=1e100;
    for(const auto& face:mesh.faces) {
        const auto& a=mesh.vertices[face[0]];
        const auto& b=mesh.vertices[face[1]];
        const auto& c=mesh.vertices[face[2]];
        const double den=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
        if(std::abs(den)<1e-12)continue;
        const double u=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/den;
        const double v=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/den;
        if(u>=-1e-8&&v>=-1e-8&&u+v<=1+1e-8)
            hit=std::min(hit,u*a.z+v*b.z+(1-u-v)*c.z);
    }
    return hit;
}
double nextHitAbove(const PrintMesh& mesh,double x,double y,double z) {
    double hit=1e100;
    for(const auto& face:mesh.faces) {
        const auto& a=mesh.vertices[face[0]];
        const auto& b=mesh.vertices[face[1]];
        const auto& c=mesh.vertices[face[2]];
        const double den=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);
        if(std::abs(den)<1e-12)continue;
        const double u=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/den;
        const double v=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/den;
        if(u<-1e-8||v<-1e-8||u+v>1+1e-8)continue;
        const double candidate=u*a.z+v*b.z+(1-u-v)*c.z;
        if(candidate>z+1e-5)hit=std::min(hit,candidate);
    }
    return hit;
}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    const auto args=app.arguments();
    const int reopenAt=args.indexOf(QStringLiteral("--reopen"));
    if(reopenAt>=0&&reopenAt+1<args.size()) {
        try {
            Lib3MF::CWrapper wrapper;
            auto model=wrapper.CreateModel();
            model->QueryReader("3mf")->ReadFromFile(args[reopenAt+1].toStdString());
            return check(model->GetMeshObjects()->Count()==7,
                "existing 3MF reopens with seven candidate objects")?0:1;
        } catch(const std::exception& exception) {
            check(false,QStringLiteral("existing 3MF reopening failed: %1")
                .arg(QString::fromUtf8(exception.what())));
            return 1;
        }
    }
    const int libraryAt=args.indexOf(QStringLiteral("--ldraw"));
    if(libraryAt<0||libraryAt+1>=args.size())return 0;
    bool ok=true;
    const QString root=args[libraryAt+1];
    const auto source=LDrawLibraryService::loadPart(root,QStringLiteral("3938"));
    ok&=check(source.ok(),"3938 source loads");
    if(!source.ok())return 1;
    const auto male=PinBarrelHingeSemantic::recognize(source);
    ok&=check(male.size()==2,"both certified hollow hinge pins recognized");
    const auto base=LDrawLibraryService::loadPart(root,QStringLiteral("3937"));
    ok&=check(base.ok()&&PinBarrelHingeSemantic::recognize(base).size()==2,
              "paired open retaining barrel contacts recognized");
    for(const QString& id:{QStringLiteral("4275a"),QStringLiteral("4276a"),
                           QStringLiteral("30364"),QStringLiteral("3700"),
                           QStringLiteral("11476")}) {
        const auto negative=LDrawLibraryService::loadPart(root,id);
        ok&=check(negative.ok()&&PinBarrelHingeSemantic::recognize(negative).isEmpty(),
                  id+" is not a pin/barrel hinge");
    }
    const auto sourceTriangles=source.mesh.triangles;
    const auto artifact=PinBarrelHingeCalibrationArtifact::generate(source);
    QTextStream(stdout)<<artifact.diagnostic<<Qt::endl;
    ok&=check(artifact.ok,"source-faithful seven-candidate hinge fixture");
    if(!artifact.ok)return 1;
    bool sourceUnchanged=source.mesh.triangles.size()==sourceTriangles.size();
    if(sourceUnchanged)for(int i=0;i<sourceTriangles.size();++i) {
        const auto& before=sourceTriangles[i];const auto& after=source.mesh.triangles[i];
        sourceUnchanged&=before.a==after.a&&before.b==after.b&&before.c==after.c;
    }
    ok&=check(sourceUnchanged,"authoritative 3938 source triangles remain unchanged");
    ok&=check(artifact.candidates.size()==7&&artifact.candidateMeshes.size()==7,
              "seven independently printable pieces");
    for(int i=0;i<7;++i) {
        const auto& c=artifact.candidates[i];
        const auto analysis=analyzeSource(artifact.candidateMeshes[i]);
        ok&=check(c.index==i+1&&std::abs(c.diameterCorrectionMillimetres-(i-3)*.05)<1e-9&&
            std::abs(c.functionalDiameterMillimetres-(3.05+i*.05))<1e-9&&
            validatePreparedMesh(analysis).ok()&&analysis.bounds.minimum.z>=-.05,
            QStringLiteral("candidate %1 exact OD and valid side-down solid").arg(i+1));
    }
    const auto& frame=artifact.regenerationPrototype.frame;
    const double pinX=frame.origin.x+frame.axis.x*.8;
    const double pinY=frame.origin.y+frame.axis.y*.8;
    const double lowerFirst=bottomHit(artifact.candidateMeshes.front(),pinX,pinY);
    const double lowerNominal=bottomHit(artifact.candidateMeshes[3],pinX,pinY);
    const double lowerLast=bottomHit(artifact.candidateMeshes.back(),pinX,pinY);
    const auto& other=male[1];
    const double secondX=other.frame.origin.x+other.frame.axis.x*.8;
    const double secondY=-other.frame.origin.z-other.frame.axis.z*.8;
    const double secondFirst=bottomHit(artifact.candidateMeshes.front(),secondX,secondY);
    const double secondNominal=bottomHit(artifact.candidateMeshes[3],secondX,secondY);
    const double secondLast=bottomHit(artifact.candidateMeshes.back(),secondX,secondY);
    ok&=check(lowerFirst>lowerNominal&&lowerNominal>lowerLast&&
        secondFirst>secondNominal&&secondNominal>secondLast&&
        std::abs((lowerFirst-lowerLast)-.15)<.04&&
        std::abs((secondFirst-secondLast)-.15)<.04&&
        std::abs(lowerNominal-2.4)<.04&&std::abs(secondNominal-2.4)<.04,
        "both prepared source-owned pin envelopes follow the candidate correction");
    QTextStream(stdout)<<"hingePinOuterLower="<<lowerFirst<<','<<lowerNominal<<','<<lowerLast
        <<" second="<<secondFirst<<','<<secondNominal<<','<<secondLast<<Qt::endl;
    const double boreFirst=nextHitAbove(artifact.candidateMeshes.front(),pinX,pinY,frame.origin.z);
    const double boreNominal=nextHitAbove(artifact.candidateMeshes[3],pinX,pinY,frame.origin.z);
    const double boreLast=nextHitAbove(artifact.candidateMeshes.back(),pinX,pinY,frame.origin.z);
    ok&=check(std::abs((boreFirst-frame.origin.z)-.8)<.05&&
        std::abs((boreNominal-frame.origin.z)-.8)<.05&&
        std::abs((boreLast-frame.origin.z)-.8)<.05&&
        std::abs(boreFirst-boreLast)<.03,
        "protected 1.60 mm hollow-pin bore remains nominal across the OD range");
    QTextStream(stdout)<<"hingeProtectedBore="<<boreFirst<<','<<boreNominal<<','<<boreLast<<Qt::endl;
    for(int i=0;i<7;++i)for(int dot=0;dot<8;++dot) {
        const double x=dot%2==0?-.82:.82;
        const double y=-(2.4-1.6*(dot/2));
        const double top=topHit(artifact.candidateMeshes[i],x,y);
        const bool raised=top>8.70;
        ok&=check(raised==(dot<=i),
            QStringLiteral("candidate %1 has exactly its first %2 robust exterior dots (slot %3 top %4)")
                .arg(i+1).arg(i+1).arg(dot+1).arg(top));
    }
    const auto experiment=PinBarrelHingeCalibrationArtifact::observationTemplate(artifact);
    ok&=check(experiment.featureFamily==QStringLiteral("PinBarrelHinge")&&
        experiment.featureRole==QStringLiteral("male")&&
        experiment.regenerationPrototype.family==FunctionalInterfaceFamily::PinBarrelHinge&&
        experiment.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate&&
        experiment.preferredCandidateIndex==0&&experiment.state==FitEvidenceState::Draft,
        "parallel unverified managed calibration definition");
    const int outputAt=args.indexOf(QStringLiteral("--output"));
    QTemporaryDir temporary;
    ok&=check(temporary.isValid(),"isolated session import directory");
    if(outputAt>=0&&outputAt+1<args.size()) {
        QDir output(args[outputAt+1]);
        ok&=check(output.mkpath(QStringLiteral(".")),"artifact output directory");
        QVector<ThreeMfWriter::NamedMesh> parts;
        for(int i=0;i<7;++i)parts.push_back({QStringLiteral("Candidate %1 — %2 mm pin OD")
            .arg(i+1).arg(artifact.candidates[i].functionalDiameterMillimetres,0,'f',2),
            artifact.candidateMeshes[i],{10.0+20.0*(i%4),9.0+20.0*(i/4),0.0}});
        ThreeMfWriter::Options options;
        options.objectName=artifact.artifactIdentity;
        options.partIdentity=artifact.artifactIdentity;
        options.modelColor=QColor("#A0A5A9");
        QString error;
        const QString modelPath=output.filePath(QStringLiteral("BrickSuite-")+artifact.artifactIdentity+
            QStringLiteral(".3mf"));
        ok&=check(ThreeMfWriter::writeCollection(parts,modelPath,options,&error),
                  "3MF export: "+error);
        try {
            Lib3MF::CWrapper wrapper;
            auto model=wrapper.CreateModel();
            model->QueryReader("3mf")->ReadFromFile(modelPath.toStdString());
            auto objects=model->GetMeshObjects();
            ok&=check(objects->Count()==7,"3MF reopens with seven dot-marked objects");
            for(int candidate=0;candidate<7&&objects->MoveNext();++candidate) {
                const auto object=objects->GetCurrentMeshObject();
                for(int dot=0;dot<8;++dot) {
                    const double x=(dot%2==0?-.82:.82)+10.0+20.0*(candidate%4);
                    const double y=-(2.4-1.6*(dot/2))+9.0+20.0*(candidate/4);
                    bool raised=false;
                    for(Lib3MF_uint32 vertex=0;vertex<object->GetVertexCount();++vertex) {
                        const auto p=object->GetVertex(vertex);
                        if(p.m_Coordinates[2]>8.70&&
                           std::hypot(p.m_Coordinates[0]-x,p.m_Coordinates[1]-y)<.65) {
                            raised=true;break;
                        }
                    }
                    ok&=check(raised==(dot<=candidate),QStringLiteral("exported pin hinge candidate %1 dot %2")
                        .arg(candidate+1).arg(dot+1));
                }
            }
        } catch(const std::exception& exception) {
            ok&=check(false,QStringLiteral("3MF reopening failed: %1").arg(QString::fromUtf8(exception.what())));
        }
        FitCalibrationSession session;
        session.sessionIdentity=FitCalibrationLibrary::newStableIdentity();
        session.process.printerIdentity=QStringLiteral("Bambu H2D");
        session.process.materialIdentity=QStringLiteral("PETG");
        session.process.profileName=QStringLiteral("0.20mm Standard @BBL H2D");
        session.process.hasNozzleDiameter=true;
        session.process.nozzleDiameterMillimetres=.4;
        session.process.hasLayerHeight=true;
        session.process.layerHeightMillimetres=.2;
        session.process.dimensionalCompensationNotes=QStringLiteral("None / Bambu Studio defaults");
        session.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;
        session.process.orientationNotes=experiment.process.orientationNotes;
        session.hasCoarseExperiment=true;
        session.coarseExperiment=experiment;
        session.coarseExperiment.process=session.process;
        const QString sessionPath=output.filePath(QStringLiteral("BrickSuite-")+artifact.artifactIdentity+
            QStringLiteral("-session.json"));
        QSaveFile file(sessionPath);
        ok&=check(file.open(QIODevice::WriteOnly)&&
            file.write(QJsonDocument(FitCalibrationSessionJson::toJson(session)).toJson(QJsonDocument::Indented))>0&&
            file.commit(),"managed session export");
        QFile reload(sessionPath);
        FitCalibrationSession decoded,imported,resumed;
        bool fresh=reload.open(QIODevice::ReadOnly)&&
            FitCalibrationSessionJson::fromJson(QJsonDocument::fromJson(reload.readAll()).object(),&decoded,&error)&&
            decoded.sessionIdentity==session.sessionIdentity&&decoded.hasCoarseExperiment&&
            !decoded.hasFineExperiment&&decoded.coarseExperiment.featureFamily==QStringLiteral("PinBarrelHinge")&&
            decoded.coarseExperiment.regenerationPrototype.family==FunctionalInterfaceFamily::PinBarrelHinge&&
            decoded.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate&&
            decoded.coarseExperiment.candidates.size()==7&&decoded.coarseExperiment.preferredCandidateIndex==0&&
            decoded.coarseExperiment.state==FitEvidenceState::Draft;
        if(fresh)for(int i=0;i<7;++i)fresh&=decoded.coarseExperiment.candidates[i].observations.isEmpty()&&
            std::abs(decoded.coarseExperiment.candidates[i].functionalDiameterMillimetres-
                artifact.candidates[i].functionalDiameterMillimetres)<1e-9;
        ok&=check(fresh,"managed session reloads with exact fresh candidates: "+error);
        FitCalibrationLibrary library(QDir(temporary.path()).filePath(QStringLiteral("managed")));
        ok&=check(library.importSession(sessionPath,&imported,&error)&&
            library.loadSession(session.sessionIdentity,&resumed,&error)&&
            resumed.coarseExperiment.candidates.size()==7,
            "managed import/resume path: "+error);
        QTextStream(stdout)<<"model="<<modelPath<<Qt::endl<<"session="<<sessionPath<<Qt::endl;
    }
    return ok?0:1;
}
