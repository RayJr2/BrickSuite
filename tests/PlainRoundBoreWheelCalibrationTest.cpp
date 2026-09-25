#include "../src/services/geometry/LDrawLibraryService.h"
#include "../src/services/geometry/ThreeMfWriter.h"
#include "../src/services/geometry/fit/PlainRoundBoreWheelCalibrationArtifact.h"
#include "../src/services/geometry/fit/FitCalibrationLibrary.h"
#include "../src/services/geometry/print/PlainRoundBoreWheelSemantic.h"
#include "../src/services/geometry/print/RetainedRotatingWheelSemantic.h"
#include "../src/services/geometry/print/StandardBarSemantic.h"
#include "../src/services/geometry/print/PrintMeshAnalysis.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <lib3mf_implicit.hpp>
#include <cmath>
#include <map>
#include <set>

using namespace PrintGeometry;
namespace {
bool check(bool value,const QString& message){
    if(!value)QTextStream(stderr)<<"FAIL: "<<message<<Qt::endl;
    return value;
}
int topDotCount(const PrintMesh& mesh){
    double top=-1e100;
    for(const auto& p:mesh.vertices)top=std::max(top,p.z);
    std::map<unsigned,unsigned> parents;
    const auto root=[&](unsigned index){while(parents.at(index)!=index)index=parents.at(index);return index;};
    for(const auto& face:mesh.faces){
        const auto& a=mesh.vertices[face[0]],&b=mesh.vertices[face[1]],&c=mesh.vertices[face[2]];
        if(std::abs(a.z-top)>1e-5||std::abs(b.z-top)>1e-5||std::abs(c.z-top)>1e-5)continue;
        for(const unsigned vertex:{face[0],face[1],face[2]})parents.emplace(vertex,vertex);
        const unsigned common=root(face[0]);parents[root(face[1])]=common;parents[root(face[2])]=common;
    }
    std::set<unsigned> components;
    for(const auto& pair:parents)components.insert(root(pair.first));
    return int(components.size());
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    const auto args=app.arguments();const int at=args.indexOf(QStringLiteral("--ldraw"));
    if(at<0||at+1>=args.size())return 0;
    bool ok=true;
    const auto holder=LDrawLibraryService::loadPart(args[at+1],QStringLiteral("4488"));
    const auto wheel=LDrawLibraryService::loadPart(args[at+1],QStringLiteral("30027a"));
    ok&=check(holder.ok()&&wheel.ok(),"catalog 4488 wheel holder and 30027a plain wheel load");
    if(!holder.ok()||!wheel.ok())return 1;
    const auto pin=RetainedRotatingWheelSemantic::recognize(holder);
    const auto bore=PlainRoundBoreWheelSemantic::recognize(wheel);
    ok&=check(pin.size()==1&&pin.front().role==FunctionalInterfaceRole::Male&&
        bore.size()==1&&bore.front().role==FunctionalInterfaceRole::Female&&
        bore.front().provenance.size()==2&&
        bore.front().provenance.front().sourceFile==QStringLiteral("p/4-4cyli.dat")&&
        std::abs(bore.front().nominalDiameterMillimetres-3.2)<1e-9&&
        std::abs(bore.front().nominalEngagementExtentMillimetres-4.8)<1e-9,
        "certified plain blind-bore wheel and wheel holder have distinct source ownership");
    for(const QString& id:{QStringLiteral("30027b"),QStringLiteral("3938"),
                           QStringLiteral("22253"),QStringLiteral("3700"),
                           QStringLiteral("30374"),QStringLiteral("3673")}){
        const auto source=LDrawLibraryService::loadPart(args[at+1],id);
        ok&=check(source.ok()&&PlainRoundBoreWheelSemantic::recognize(source).isEmpty(),
            id+" is not the plain blind-bore wheel contract");
    }
    ok&=check(RetainedRotatingWheelSemantic::recognize(wheel).isEmpty()&&
        StandardBarSemantic::recognize(wheel).isEmpty()&&
        PlainRoundBoreWheelSemantic::recognize(holder).isEmpty(),
        "plain wheel stays separate from notched wheel and Standard Bar");
    const auto fixture=PlainRoundBoreWheelCalibrationArtifact::generate(wheel);
    QTextStream(stdout)<<"plainWheelFixture="<<fixture.diagnostic<<Qt::endl;
    ok&=check(fixture.ok&&fixture.candidates.size()==7&&fixture.candidateMeshes.size()==7,
        "seven source-faithful plain blind-bore candidates");
    if(!fixture.ok)return 1;
    for(int i=0;i<7;++i){
        const auto& candidate=fixture.candidates[i];
        const auto analysis=analyzeSource(fixture.candidateMeshes[i]);
        ok&=check(candidate.index==i+1&&
            std::abs(candidate.diameterCorrectionMillimetres-(i-3)*.1)<1e-9&&
            std::abs(candidate.functionalDiameterMillimetres-(3.2+(i-3)*.1))<1e-9&&
            validatePreparedMesh(analysis).ok()&&analysis.bounds.minimum.z>=-.01&&
            topDotCount(fixture.candidateMeshes[i])==i+1,
            QStringLiteral("plain-wheel candidate %1 dimensions, manifold and dot count").arg(i+1));
    }
    const auto experiment=PlainRoundBoreWheelCalibrationArtifact::observationTemplate(fixture);
    ok&=check(experiment.featureFamily==QStringLiteral("PlainRoundBoreWheel")&&
        experiment.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate&&
        experiment.preferredCandidateIndex==0&&experiment.state==FitEvidenceState::Draft,
        "perpendicular plain-wheel calibration remains unverified");
    auto observed=experiment;
    observed.candidates[6].observations.push_back({FitObservation::Preferred,1,0,false,
        QStringLiteral("Best physical fit at coarse upper boundary")});
    observed.preferredCandidateIndex=7;
    observed.state=FitEvidenceState::CandidateSelected;
    const auto plan=FitCalibrationEvidencePolicy::nextSearchPlan(observed);
    ok&=check(plan.boundary==FitPreferredBoundary::Upper&&plan.candidateCount==7&&
        std::abs(plan.candidateSpacingMillimetres-.05)<1e-9&&
        std::abs(plan.centerCorrectionMillimetres-.45)<1e-9,
        "upper-boundary preferred wheel uses the managed half-spacing extension policy");
    PlainRoundBoreWheelCalibrationDefinition extensionDefinition;
    extensionDefinition.artifactIdentity=experiment.artifactIdentity+QStringLiteral("-boundary-extension-v2");
    extensionDefinition.parentArtifactIdentity=experiment.artifactIdentity;
    extensionDefinition.centerCorrectionMillimetres=plan.centerCorrectionMillimetres;
    extensionDefinition.candidateSpacingMillimetres=plan.candidateSpacingMillimetres;
    extensionDefinition.candidateCount=plan.candidateCount;
    const auto extension=PlainRoundBoreWheelCalibrationArtifact::generate(wheel,
        observed.regenerationPrototype,extensionDefinition);
    ok&=check(extension.ok&&extension.candidates.size()==7&&extension.candidateMeshes.size()==7,
        "source-faithful plain-wheel extension generates seven blind bores: "+extension.diagnostic);
    if(extension.ok){
        for(int i=0;i<7;++i)ok&=check(
            std::abs(extension.candidates[i].functionalDiameterMillimetres-(3.50+.05*i))<1e-9&&
            validatePreparedMesh(analyzeSource(extension.candidateMeshes[i])).ok()&&
            topDotCount(extension.candidateMeshes[i])==i+1,
            QStringLiteral("extension candidate %1 remains a marked manifold blind-bore wheel").arg(i+1));
        FitCalibrationSession parent;
        parent.sessionIdentity=QStringLiteral("plain-wheel-parent");
        parent.process.printerIdentity=QStringLiteral("Bambu H2D");
        parent.process.materialIdentity=QStringLiteral("PETG");
        parent.process.profileName=QStringLiteral("0.20mm Standard @BBL H2D");
        parent.process.hasNozzleDiameter=true;parent.process.nozzleDiameterMillimetres=.4;
        parent.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;
        parent.hasCoarseExperiment=true;parent.coarseExperiment=observed;
        const auto child=FitCalibrationLibrary::continuationSession(parent,observed,
            PlainRoundBoreWheelCalibrationArtifact::observationTemplate(extension));
        FitCalibrationSession reloaded;QString error;
        const bool roundTrip=FitCalibrationSessionJson::fromJson(
            FitCalibrationSessionJson::toJson(child),&reloaded,&error);
        ok&=check(roundTrip&&reloaded.sessionIdentity!=parent.sessionIdentity&&
            reloaded.coarseExperiment.preferredCandidateIndex==7&&
            reloaded.coarseExperiment.candidates[6].observations.size()==1&&
            reloaded.fineExperiment.parentArtifactIdentity==observed.artifactIdentity&&
            reloaded.fineExperiment.candidates[0].observations.isEmpty()&&
            reloaded.process.actualPrintedOrientation==parent.process.actualPrintedOrientation&&
            reloaded.process.profileName==parent.process.profileName,
            "managed extension reload preserves observed coarse evidence, lineage and process context: "+error);
        const int extensionOutputAt=args.indexOf(QStringLiteral("--extension-output"));
        if(extensionOutputAt>=0&&extensionOutputAt+1<args.size()){
            const QDir directory(args[extensionOutputAt+1]);
            const QString path=directory.filePath(QStringLiteral("plain-round-bore-wheel-perpendicular-extension-v2.3mf"));
            ok&=check(directory.exists(),"extension output directory exists");
            if(directory.exists()&&!QFile::exists(path)){
                QVector<ThreeMfWriter::NamedMesh> pieces;
                for(int i=0;i<7;++i)pieces.push_back({QStringLiteral("Candidate %1 — %2 mm blind bore")
                    .arg(i+1).arg(extension.candidates[i].functionalDiameterMillimetres,0,'f',3),
                    extension.candidateMeshes[i],{14.0+26.0*(i%4),12.0+23.0*(i/4),0.0}});
                ThreeMfWriter::Options options;options.objectName=extension.artifactIdentity;
                options.partIdentity=extension.artifactIdentity;options.modelColor=QColor("#A0A5A9");
                ok&=check(ThreeMfWriter::writeCollection(pieces,path,options,&error),
                    "plain-wheel extension 3MF export: "+error);
            }
            if(QFile::exists(path))try{
                Lib3MF::CWrapper wrapper;auto model=wrapper.CreateModel();
                model->QueryReader("3mf")->ReadFromFile(path.toStdString());
                ok&=check(model->GetMeshObjects()->Count()==7,
                    "plain-wheel extension 3MF reopens with seven wheel objects");
            }catch(const std::exception& exception){
                ok&=check(false,QStringLiteral("extension 3MF reopen failed: %1")
                    .arg(QString::fromUtf8(exception.what())));
            }
            QTextStream(stdout)<<"extensionModel="<<path<<Qt::endl;
            const int parentAt=args.indexOf(QStringLiteral("--parent-session"));
            if(parentAt>=0&&parentAt+1<args.size()){
                QFile parentFile(args[parentAt+1]);FitCalibrationSession actualParent;
                QString parseError;
                ok&=check(parentFile.open(QIODevice::ReadOnly)&&
                    FitCalibrationSessionJson::fromJson(
                        QJsonDocument::fromJson(parentFile.readAll()).object(),&actualParent,&parseError),
                    "observed managed parent session reloads: "+parseError);
                if(actualParent.hasCoarseExperiment&&
                   actualParent.coarseExperiment.featureFamily==QStringLiteral("PlainRoundBoreWheel")){
                    const auto& actual=actualParent.coarseExperiment;
                    const auto actualPlan=FitCalibrationEvidencePolicy::nextSearchPlan(actual);
                    const bool matches=actual.preferredCandidateIndex==7&&actual.candidates.size()==7&&
                        !actual.candidates[6].observations.isEmpty()&&
                        actualPlan.boundary==FitPreferredBoundary::Upper&&
                        std::abs(actualPlan.centerCorrectionMillimetres-plan.centerCorrectionMillimetres)<1e-9&&
                        std::abs(actualPlan.candidateSpacingMillimetres-plan.candidateSpacingMillimetres)<1e-9&&
                        actual.regenerationPrototype.stableIdentity==extension.regenerationPrototype.stableIdentity;
                    ok&=check(matches,"observed parent contract and continuation plan match the printed extension");
                    if(matches){
                        auto next=FitCalibrationLibrary::continuationSession(actualParent,actual,
                            PlainRoundBoreWheelCalibrationArtifact::observationTemplate(extension));
                        const QString sessionPath=directory.filePath(QStringLiteral(
                            "plain-round-bore-wheel-perpendicular-extension-v2-session.json"));
                        ok&=check(!QFile::exists(sessionPath),
                            "canonical managed continuation companion does not already exist");
                        if(!QFile::exists(sessionPath)){
                            QTemporaryDir managedRoot,importRoot;
                            FitCalibrationLibrary managed(QDir(managedRoot.path()).filePath(QStringLiteral("managed")));
                            FitCalibrationLibrary importedLibrary(QDir(importRoot.path()).filePath(QStringLiteral("managed")));
                            FitCalibrationSession imported;
                            ok&=check(managedRoot.isValid()&&importRoot.isValid()&&
                                managed.saveSession(&next,&parseError)&&
                                managed.exportSession(next.sessionIdentity,sessionPath,&parseError)&&
                                importedLibrary.importSession(sessionPath,&imported,&parseError)&&
                                imported.coarseExperiment.candidates[6].observations.size()==
                                    actual.candidates[6].observations.size()&&
                                imported.coarseExperiment.preferredCandidateIndex==7&&
                                imported.fineExperiment.candidates[0].observations.isEmpty()&&
                                imported.process.profileName==actualParent.process.profileName,
                                "real managed continuation exports, imports and preserves parent evidence: "+parseError);
                            QTextStream(stdout)<<"extensionSession="<<sessionPath<<Qt::endl;
                        }
                    }
                }else ok&=check(false,"observed parent is not the coarse Plain Round-Bore Wheel session");
            }
        }
    }
    const int outputAt=args.indexOf(QStringLiteral("--output"));
    if(outputAt>=0&&outputAt+1<args.size()){
        QDir directory(args[outputAt+1]);
        ok&=check(directory.exists(),"calibration output directory exists");
        const QString modelPath=directory.filePath(QStringLiteral("BrickSuite-")+fixture.artifactIdentity+".3mf");
        const QString sessionPath=directory.filePath(QStringLiteral("BrickSuite-")+fixture.artifactIdentity+"-session.json");
        if(QFile::exists(modelPath)||QFile::exists(sessionPath))return check(false,"canonical artifacts already exist")?0:1;
        QVector<ThreeMfWriter::NamedMesh> pieces;
        for(int i=0;i<7;++i)pieces.push_back({QStringLiteral("Candidate %1 — %2 mm blind bore")
            .arg(i+1).arg(fixture.candidates[i].functionalDiameterMillimetres,0,'f',3),
            fixture.candidateMeshes[i],{14.0+26.0*(i%4),12.0+23.0*(i/4),0.0}});
        ThreeMfWriter::Options options;
        options.objectName=fixture.artifactIdentity;
        options.partIdentity=fixture.artifactIdentity;
        options.modelColor=QColor("#A0A5A9");
        QString error;
        ok&=check(ThreeMfWriter::writeCollection(pieces,modelPath,options,&error),"plain-wheel 3MF export: "+error);
        if(QFile::exists(modelPath))try{
            Lib3MF::CWrapper wrapper;auto model=wrapper.CreateModel();
            model->QueryReader("3mf")->ReadFromFile(modelPath.toStdString());
            auto objects=model->GetMeshObjects();
            ok&=check(objects->Count()==7,"plain-wheel 3MF reopens with seven objects");
            int index=0;
            while(objects->MoveNext()&&index<7){
                const auto object=objects->GetCurrentMeshObject();PrintMesh decoded;
                for(Lib3MF_uint32 i=0;i<object->GetVertexCount();++i){
                    const auto p=object->GetVertex(i);
                    decoded.vertices.push_back({p.m_Coordinates[0],p.m_Coordinates[1],p.m_Coordinates[2]});
                }
                for(Lib3MF_uint32 i=0;i<object->GetTriangleCount();++i){
                    const auto t=object->GetTriangle(i);
                    decoded.faces.push_back({t.m_Indices[0],t.m_Indices[1],t.m_Indices[2]});
                }
                ok&=check(topDotCount(decoded)==index+1,
                    QStringLiteral("exported plain-wheel candidate %1 retains dot count").arg(index+1));
                ++index;
            }
        }catch(const std::exception& exception){
            ok&=check(false,QStringLiteral("3MF reopen failed: %1").arg(QString::fromUtf8(exception.what())));
        }
        FitCalibrationSession session;
        session.sessionIdentity=FitCalibrationLibrary::newStableIdentity();
        session.process.printerIdentity=QStringLiteral("Bambu H2D");
        session.process.materialIdentity=QStringLiteral("PETG");
        session.process.profileName=QStringLiteral("0.20mm Standard @BBL H2D");
        session.process.hasNozzleDiameter=true;session.process.nozzleDiameterMillimetres=.4;
        session.process.hasLayerHeight=true;session.process.layerHeightMillimetres=.2;
        session.process.dimensionalCompensationNotes=QStringLiteral("None / Bambu Studio defaults");
        session.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;
        session.process.orientationNotes=experiment.process.orientationNotes;
        session.hasCoarseExperiment=true;session.coarseExperiment=experiment;
        session.coarseExperiment.process=session.process;
        QSaveFile file(sessionPath);
        ok&=check(file.open(QIODevice::WriteOnly)&&
            file.write(QJsonDocument(FitCalibrationSessionJson::toJson(session)).toJson(QJsonDocument::Indented))>0&&
            file.commit(),"managed plain-wheel session export");
        QFile reload(sessionPath);FitCalibrationSession decoded,imported,resumed;
        bool fresh=reload.open(QIODevice::ReadOnly)&&
            FitCalibrationSessionJson::fromJson(QJsonDocument::fromJson(reload.readAll()).object(),&decoded,&error)&&
            decoded.sessionIdentity==session.sessionIdentity&&decoded.hasCoarseExperiment&&
            !decoded.hasFineExperiment&&decoded.coarseExperiment.featureFamily==QStringLiteral("PlainRoundBoreWheel")&&
            decoded.coarseExperiment.regenerationPrototype.family==FunctionalInterfaceFamily::PlainRoundBoreWheel&&
            decoded.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate&&
            decoded.coarseExperiment.candidates.size()==7&&decoded.coarseExperiment.preferredCandidateIndex==0&&
            decoded.coarseExperiment.state==FitEvidenceState::Draft;
        if(fresh)for(int i=0;i<7;++i)fresh&=decoded.coarseExperiment.candidates[i].observations.isEmpty()&&
            std::abs(decoded.coarseExperiment.candidates[i].functionalDiameterMillimetres-
                fixture.candidates[i].functionalDiameterMillimetres)<1e-9;
        ok&=check(fresh,"managed plain-wheel session reload: "+error);
        QTemporaryDir temporary;
        ok&=check(temporary.isValid(),"isolated managed library");
        FitCalibrationLibrary library(QDir(temporary.path()).filePath(QStringLiteral("managed")));
        ok&=check(library.importSession(sessionPath,&imported,&error)&&
            library.loadSession(session.sessionIdentity,&resumed,&error)&&
            resumed.coarseExperiment.candidates.size()==7,"managed plain-wheel import/resume: "+error);
        QTextStream(stdout)<<"model="<<modelPath<<Qt::endl<<"session="<<sessionPath<<Qt::endl;
    }
    return ok?0:1;
}
