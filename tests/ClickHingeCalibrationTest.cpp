#include "../src/services/geometry/LDrawLibraryService.h"
#include "../src/services/geometry/print/ClickHingeSemantic.h"
#include "../src/services/geometry/print/PinBarrelHingeSemantic.h"
#include "../src/services/geometry/print/InterleavedFingerHingeSemantic.h"
#include "../src/services/geometry/print/SourceSurfaceSolidifier.h"
#include "../src/services/geometry/print/PrintMeshAnalysis.h"
#include "../src/services/geometry/fit/ClickHingeCalibrationArtifact.h"
#include "../src/services/geometry/fit/FitCalibrationLibrary.h"
#include "../src/services/geometry/ThreeMfWriter.h"

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
    for(const auto& p:mesh.vertices)top=std::max(top,p.y);
    std::map<unsigned,unsigned> parents;
    const auto root=[&](unsigned index){while(parents.at(index)!=index)index=parents.at(index);return index;};
    for(const auto& face:mesh.faces){
        const auto& a=mesh.vertices[face[0]],&b=mesh.vertices[face[1]],&c=mesh.vertices[face[2]];
        if(std::abs(a.y-top)>1e-5||std::abs(b.y-top)>1e-5||std::abs(c.y-top)>1e-5)continue;
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
    const auto single=LDrawLibraryService::loadPart(args[at+1],QStringLiteral("30345"));
    const auto assembled=LDrawLibraryService::loadPart(args[at+1],QStringLiteral("76385"));
    const auto dual=LDrawLibraryService::loadPart(args[at+1],QStringLiteral("30394"));
    ok&=check(single.ok()&&assembled.ok()&&dual.ok(),"click insert, catalog assembly, and mate load");
    if(!single.ok()||!assembled.ok()||!dual.ok())return 1;
    const auto male=ClickHingeSemantic::recognize(single);
    const auto assembledMale=ClickHingeSemantic::recognize(assembled);
    const auto female=ClickHingeSemantic::recognize(dual);
    ok&=check(male.size()==1&&male.front().role==FunctionalInterfaceRole::Male,
        "certified clh1 single-finger arrestor recognized");
    ok&=check(female.size()==1&&female.front().role==FunctionalInterfaceRole::Female,
        "certified paired clh4 indexed mate recognized");
    ok&=check(male.size()==1&&assembledMale.size()==1&&assembledMale.front().role==FunctionalInterfaceRole::Male&&
        assembledMale.front().provenance.front().sourceFile==male.front().provenance.front().sourceFile,
        "catalog 76385 retains the same certified clh1 insert ancestry");
    for(const QString& id:{QStringLiteral("3937"),QStringLiteral("3938"),QStringLiteral("4275a"),
                           QStringLiteral("4276a"),QStringLiteral("30472"),QStringLiteral("30619"),QStringLiteral("14137"),
                           QStringLiteral("3700")}){
        const auto source=LDrawLibraryService::loadPart(args[at+1],id);
        ok&=check(source.ok()&&ClickHingeSemantic::recognize(source).isEmpty(),
            id+" is excluded from the first clh1/clh4 contract");
    }
    ok&=check(PinBarrelHingeSemantic::recognize(single).isEmpty()&&
        InterleavedFingerHingeSemantic::recognize(single).isEmpty(),
        "click lock is separate from pin/barrel and interleaved-finger hinges");
    const auto prepared=SourceSurfaceSolidifier::solidify(single);
    QTextStream(stdout)<<"clickSourceSolidification="<<prepared.diagnostic<<Qt::endl;
    ok&=check(prepared.successful&&validatePreparedMesh(prepared.analysis).ok(),
        "single-finger click hinge source can be solidified without changing source");
    const auto fixture=ClickHingeCalibrationArtifact::generate(single);
    QTextStream(stdout)<<"clickFixture="<<fixture.diagnostic<<Qt::endl;
    ok&=check(fixture.ok&&fixture.candidates.size()==7&&fixture.candidateMeshes.size()==7,
        "seven source-faithful indexed click-detent candidates");
    if(!fixture.ok)return 1;
    for(int i=0;i<7;++i){
        const auto& candidate=fixture.candidates[i];
        const auto analysis=analyzeSource(fixture.candidateMeshes[i]);
        ok&=check(candidate.index==i+1&&
            std::abs(candidate.heightCorrectionMillimetres-(i-3)*.05)<1e-9&&
            std::abs(candidate.functionalHeightMillimetres-(2.186+(i-3)*.05))<1e-9&&
            validatePreparedMesh(analysis).ok()&&analysis.bounds.minimum.z>=-.01&&
            topDotCount(fixture.candidateMeshes[i])==i+1,
            QStringLiteral("candidate %1 dimensions, manifold topology, and direct dot count").arg(i+1));
    }
    const auto experiment=ClickHingeCalibrationArtifact::observationTemplate(fixture);
    ok&=check(experiment.featureFamily==QStringLiteral("ClickHinge")&&
        experiment.correctionDimension==FitCorrectionDimension::Height&&
        experiment.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate&&
        experiment.preferredCandidateIndex==0&&experiment.state==FitEvidenceState::Draft,
        "parallel Click Hinge detent calibration contract remains unverified");
    const int outputAt=args.indexOf(QStringLiteral("--output"));
    if(outputAt>=0&&outputAt+1<args.size()){
        QDir directory(args[outputAt+1]);
        ok&=check(directory.exists(),"calibration output directory exists");
        const QString modelPath=directory.filePath(QStringLiteral("BrickSuite-")+fixture.artifactIdentity+".3mf");
        const QString sessionPath=directory.filePath(QStringLiteral("BrickSuite-")+fixture.artifactIdentity+"-session.json");
        if(QFile::exists(modelPath)||QFile::exists(sessionPath))return check(false,"canonical calibration artifacts already exist")?0:1;
        QVector<ThreeMfWriter::NamedMesh> pieces;
        for(int i=0;i<7;++i)pieces.push_back({QStringLiteral("Candidate %1 — %2 mm arrestor reach")
            .arg(i+1).arg(fixture.candidates[i].functionalHeightMillimetres,0,'f',3),
            fixture.candidateMeshes[i],{12.0+24.0*(i%4),10.0+20.0*(i/4),0.0}});
        ThreeMfWriter::Options options;
        options.objectName=fixture.artifactIdentity;
        options.partIdentity=fixture.artifactIdentity;
        options.modelColor=QColor("#A0A5A9");
        QString error;
        ok&=check(ThreeMfWriter::writeCollection(pieces,modelPath,options,&error),"Click Hinge 3MF export: "+error);
        if(QFile::exists(modelPath))try{
            Lib3MF::CWrapper wrapper;auto model=wrapper.CreateModel();
            model->QueryReader("3mf")->ReadFromFile(modelPath.toStdString());
            auto objects=model->GetMeshObjects();
            ok&=check(objects->Count()==7,"Click Hinge 3MF reopens with seven candidate objects");
            int index=0;
            while(objects->MoveNext()&&index<7){
                const auto object=objects->GetCurrentMeshObject();
                PrintMesh decoded;
                for(Lib3MF_uint32 i=0;i<object->GetVertexCount();++i){
                    const auto p=object->GetVertex(i);
                    decoded.vertices.push_back({p.m_Coordinates[0],p.m_Coordinates[1],p.m_Coordinates[2]});
                }
                for(Lib3MF_uint32 i=0;i<object->GetTriangleCount();++i){
                    const auto triangle=object->GetTriangle(i);
                    decoded.faces.push_back({triangle.m_Indices[0],triangle.m_Indices[1],triangle.m_Indices[2]});
                }
                ok&=check(topDotCount(decoded)==index+1,
                    QStringLiteral("exported 3MF candidate %1 retains its readable dot count").arg(index+1));
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
        session.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;
        session.process.orientationNotes=experiment.process.orientationNotes;
        session.hasCoarseExperiment=true;session.coarseExperiment=experiment;
        session.coarseExperiment.process=session.process;
        QSaveFile file(sessionPath);
        ok&=check(file.open(QIODevice::WriteOnly)&&
            file.write(QJsonDocument(FitCalibrationSessionJson::toJson(session)).toJson(QJsonDocument::Indented))>0&&
            file.commit(),"managed Click Hinge session export");
        QFile reload(sessionPath);
        FitCalibrationSession decoded,imported,resumed;
        bool fresh=reload.open(QIODevice::ReadOnly)&&
            FitCalibrationSessionJson::fromJson(QJsonDocument::fromJson(reload.readAll()).object(),&decoded,&error)&&
            decoded.sessionIdentity==session.sessionIdentity&&decoded.hasCoarseExperiment&&
            !decoded.hasFineExperiment&&decoded.coarseExperiment.featureFamily==QStringLiteral("ClickHinge")&&
            decoded.coarseExperiment.regenerationPrototype.family==FunctionalInterfaceFamily::ClickHinge&&
            decoded.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate&&
            decoded.coarseExperiment.candidates.size()==7&&decoded.coarseExperiment.preferredCandidateIndex==0&&
            decoded.coarseExperiment.state==FitEvidenceState::Draft;
        if(fresh)for(int i=0;i<7;++i)fresh&=decoded.coarseExperiment.candidates[i].observations.isEmpty()&&
            std::abs(decoded.coarseExperiment.candidates[i].functionalHeightMillimetres-
                fixture.candidates[i].functionalHeightMillimetres)<1e-9;
        ok&=check(fresh,"managed session reload matches unobserved fixture candidates: "+error);
        QTemporaryDir temporary;
        ok&=check(temporary.isValid(),"isolated managed calibration library");
        FitCalibrationLibrary library(QDir(temporary.path()).filePath(QStringLiteral("managed")));
        ok&=check(library.importSession(sessionPath,&imported,&error)&&
            library.loadSession(session.sessionIdentity,&resumed,&error)&&
            resumed.coarseExperiment.candidates.size()==7,"managed Click Hinge import/resume: "+error);
        QTextStream(stdout)<<"model="<<modelPath<<Qt::endl<<"session="<<sessionPath<<Qt::endl;
    }
    return ok?0:1;
}
