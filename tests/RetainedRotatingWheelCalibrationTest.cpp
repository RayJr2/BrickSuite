#include "../src/services/geometry/LDrawLibraryService.h"
#include "../src/services/geometry/ThreeMfWriter.h"
#include "../src/services/geometry/fit/RetainedRotatingWheelCalibrationArtifact.h"
#include "../src/services/geometry/fit/FitCalibrationLibrary.h"
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
    const auto male=LDrawLibraryService::loadPart(args[at+1],QStringLiteral("4488"));
    const auto female=LDrawLibraryService::loadPart(args[at+1],QStringLiteral("30027b"));
    ok&=check(male.ok()&&female.ok(),"catalog wheel holder and notched wheel load");
    if(!male.ok()||!female.ok())return 1;
    const auto pin=RetainedRotatingWheelSemantic::recognize(male);
    const auto bearing=RetainedRotatingWheelSemantic::recognize(female);
    QTextStream(stdout)<<"wheelRecognition male="<<pin.size()<<" female="<<bearing.size()
        <<" root="<<(female.sourceModel&& !female.sourceModel->files.isEmpty()?female.sourceModel->files.front().relativePath:QString())<<Qt::endl;
    ok&=check(pin.size()==1&&pin.front().role==FunctionalInterfaceRole::Male&&
        pin.front().provenance.front().sourceFile==QStringLiteral("p/wpin2a.dat")&&
        bearing.size()==1&&bearing.front().role==FunctionalInterfaceRole::Female&&
        bearing.front().provenance.front().sourceFile==QStringLiteral("p/wpinhol2.dat")&&
        std::abs(pin.front().nominalDiameterMillimetres-3.2)<1e-9&&
        std::abs(bearing.front().nominalDiameterMillimetres-3.2)<1e-9,
        "certified wheel pin and notched bearing have separate source ownership");
    for(const QString& id:{QStringLiteral("30027a"),QStringLiteral("3938"),
                           QStringLiteral("22253"),QStringLiteral("3700"),
                           QStringLiteral("30374"),QStringLiteral("3673")}){
        const auto source=LDrawLibraryService::loadPart(args[at+1],id);
        ok&=check(source.ok()&&RetainedRotatingWheelSemantic::recognize(source).isEmpty(),
            id+" is not the retained notched wheel contract");
    }
    ok&=check(StandardBarSemantic::recognize(male).isEmpty()&&
        StandardBarSemantic::recognize(female).isEmpty(),"wheel interface is not a Standard Bar");
    const auto fixture=RetainedRotatingWheelCalibrationArtifact::generate(female);
    QTextStream(stdout)<<"wheelFixture="<<fixture.diagnostic<<Qt::endl;
    ok&=check(fixture.ok&&fixture.candidates.size()==7&&fixture.candidateMeshes.size()==7,
        "seven source-faithful notched wheel candidates");
    if(!fixture.ok)return 1;
    for(int i=0;i<7;++i){
        const auto& candidate=fixture.candidates[i];
        const auto analysis=analyzeSource(fixture.candidateMeshes[i]);
        ok&=check(candidate.index==i+1&&
            std::abs(candidate.diameterCorrectionMillimetres-(i-3)*.05)<1e-9&&
            std::abs(candidate.functionalDiameterMillimetres-(3.2+(i-3)*.05))<1e-9&&
            validatePreparedMesh(analysis).ok()&&analysis.bounds.minimum.z>=-.01&&
            topDotCount(fixture.candidateMeshes[i])==i+1,
            QStringLiteral("candidate %1 dimensions, manifold and dot count").arg(i+1));
    }
    const auto experiment=RetainedRotatingWheelCalibrationArtifact::observationTemplate(fixture);
    ok&=check(experiment.featureFamily==QStringLiteral("RetainedRotatingWheel")&&
        experiment.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate&&
        experiment.preferredCandidateIndex==0&&experiment.state==FitEvidenceState::Draft,
        "perpendicular wheel calibration is unverified");
    const int outputAt=args.indexOf(QStringLiteral("--output"));
    if(outputAt>=0&&outputAt+1<args.size()){
        QDir directory(args[outputAt+1]);
        ok&=check(directory.exists(),"calibration output directory exists");
        const QString modelPath=directory.filePath(QStringLiteral("BrickSuite-")+fixture.artifactIdentity+".3mf");
        const QString sessionPath=directory.filePath(QStringLiteral("BrickSuite-")+fixture.artifactIdentity+"-session.json");
        if(QFile::exists(modelPath)||QFile::exists(sessionPath))return check(false,"canonical artifacts already exist")?0:1;
        QVector<ThreeMfWriter::NamedMesh> pieces;
        for(int i=0;i<7;++i)pieces.push_back({QStringLiteral("Candidate %1 — %2 mm bearing")
            .arg(i+1).arg(fixture.candidates[i].functionalDiameterMillimetres,0,'f',3),
            fixture.candidateMeshes[i],{14.0+26.0*(i%4),12.0+23.0*(i/4),0.0}});
        ThreeMfWriter::Options options;
        options.objectName=fixture.artifactIdentity;
        options.partIdentity=fixture.artifactIdentity;
        options.modelColor=QColor("#A0A5A9");
        QString error;
        ok&=check(ThreeMfWriter::writeCollection(pieces,modelPath,options,&error),"wheel 3MF export: "+error);
        if(QFile::exists(modelPath))try{
            Lib3MF::CWrapper wrapper;auto model=wrapper.CreateModel();
            model->QueryReader("3mf")->ReadFromFile(modelPath.toStdString());
            auto objects=model->GetMeshObjects();
            ok&=check(objects->Count()==7,"wheel 3MF reopens with seven objects");
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
                    QStringLiteral("exported candidate %1 retains dot count").arg(index+1));
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
            file.commit(),"managed wheel session export");
        QFile reload(sessionPath);FitCalibrationSession decoded,imported,resumed;
        bool fresh=reload.open(QIODevice::ReadOnly)&&
            FitCalibrationSessionJson::fromJson(QJsonDocument::fromJson(reload.readAll()).object(),&decoded,&error)&&
            decoded.sessionIdentity==session.sessionIdentity&&decoded.hasCoarseExperiment&&
            !decoded.hasFineExperiment&&decoded.coarseExperiment.featureFamily==QStringLiteral("RetainedRotatingWheel")&&
            decoded.coarseExperiment.regenerationPrototype.family==FunctionalInterfaceFamily::RetainedRotatingWheel&&
            decoded.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate&&
            decoded.coarseExperiment.candidates.size()==7&&decoded.coarseExperiment.preferredCandidateIndex==0&&
            decoded.coarseExperiment.state==FitEvidenceState::Draft;
        if(fresh)for(int i=0;i<7;++i)fresh&=decoded.coarseExperiment.candidates[i].observations.isEmpty()&&
            std::abs(decoded.coarseExperiment.candidates[i].functionalDiameterMillimetres-
                fixture.candidates[i].functionalDiameterMillimetres)<1e-9;
        ok&=check(fresh,"managed wheel session reload: "+error);
        QTemporaryDir temporary;
        ok&=check(temporary.isValid(),"isolated managed library");
        FitCalibrationLibrary library(QDir(temporary.path()).filePath(QStringLiteral("managed")));
        ok&=check(library.importSession(sessionPath,&imported,&error)&&
            library.loadSession(session.sessionIdentity,&resumed,&error)&&
            resumed.coarseExperiment.candidates.size()==7,"managed wheel import/resume: "+error);
        QTextStream(stdout)<<"model="<<modelPath<<Qt::endl<<"session="<<sessionPath<<Qt::endl;
    }
    return ok?0:1;
}
