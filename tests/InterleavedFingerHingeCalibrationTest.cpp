#include "../src/services/geometry/LDrawLibraryService.h"
#include "../src/services/geometry/ThreeMfWriter.h"
#include "../src/services/geometry/fit/FitCalibrationLibrary.h"
#include "../src/services/geometry/fit/InterleavedFingerHingeCalibrationArtifact.h"
#include "../src/services/geometry/print/InterleavedFingerHingeSemantic.h"
#include "../src/services/geometry/print/PinBarrelHingeSemantic.h"
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

using namespace PrintGeometry;
namespace {
bool check(bool value,const QString& message) {
    if(!value)QTextStream(stderr)<<"FAIL: "<<message<<Qt::endl;
    return value;
}
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    const auto args=app.arguments();
    const int at=args.indexOf(QStringLiteral("--ldraw"));
    if(at<0||at+1>=args.size())return 0;
    bool ok=true;
    const QString root=args[at+1];
    const auto source=LDrawLibraryService::loadPart(root,QStringLiteral("4275a"));
    const auto mate=LDrawLibraryService::loadPart(root,QStringLiteral("4276a"));
    ok&=check(source.ok()&&mate.ok(),"authoritative hinge halves load");
    if(!source.ok()||!mate.ok())return 1;
    const auto male=InterleavedFingerHingeSemantic::recognize(source);
    const auto female=InterleavedFingerHingeSemantic::recognize(mate);
    ok&=check(male.size()==1&&male.front().role==FunctionalInterfaceRole::Male&&
        male.front().provenance.back().sourceFile==QStringLiteral("p/h2.dat")&&
        female.size()==1&&female.front().role==FunctionalInterfaceRole::Female&&
        female.front().provenance.back().sourceFile==QStringLiteral("p/h1.dat"),
        "certified complementary h2/h1 source ownership");
    for(const QString& id:{QStringLiteral("4275b"),QStringLiteral("4276b")}) {
        const auto variant=LDrawLibraryService::loadPart(root,id);
        ok&=check(variant.ok()&&InterleavedFingerHingeSemantic::recognize(variant).size()==1,
            id+" shares the certified finger hinge despite different studs");
    }
    for(const QString& id:{QStringLiteral("3937"),QStringLiteral("3938"),QStringLiteral("30364"),
                           QStringLiteral("3700"),QStringLiteral("11476")}) {
        const auto negative=LDrawLibraryService::loadPart(root,id);
        ok&=check(negative.ok()&&InterleavedFingerHingeSemantic::recognize(negative).isEmpty(),
            id+" does not satisfy the finger-hinge contract");
    }
    ok&=check(PinBarrelHingeSemantic::recognize(source).isEmpty()&&
        PinBarrelHingeSemantic::recognize(mate).isEmpty(),"distinct from pin/barrel hinge");
    const auto triangles=source.mesh.triangles;
    const auto fixture=InterleavedFingerHingeCalibrationArtifact::generate(source);
    QTextStream(stdout)<<fixture.diagnostic<<Qt::endl;
    ok&=check(fixture.ok,"source-faithful seven-piece fixture");
    if(!fixture.ok)return 1;
    bool unchanged=triangles.size()==source.mesh.triangles.size();
    if(unchanged)for(int i=0;i<triangles.size();++i) {
        const auto& a=triangles[i];const auto& b=source.mesh.triangles[i];
        unchanged&=a.a==b.a&&a.b==b.b&&a.c==b.c;
    }
    ok&=check(unchanged,"authoritative source geometry remains unchanged");
    ok&=check(fixture.candidates.size()==7&&fixture.candidateMeshes.size()==7,"seven candidate pieces");
    for(int i=0;i<7;++i) {
        const auto& c=fixture.candidates[i];
        const auto analysis=analyzeSource(fixture.candidateMeshes[i]);
        ok&=check(c.index==i+1&&std::abs(c.heightCorrectionMillimetres-(i-3)*.05)<1e-8&&
            std::abs(c.functionalHeightMillimetres-(.15+i*.05))<1e-8&&
            validatePreparedMesh(analysis).ok()&&analysis.bounds.minimum.z>=-.01,
            QStringLiteral("candidate %1 dimension and manifold mesh").arg(i+1));
        for(int dot=0;dot<8;++dot) {
            const double centerX=-(-1.6+2.4-1.6*(dot/2));
            const double centerZ=8.0+(dot%2==0?-.82:.82);
            bool raised=false;
            for(const auto& p:fixture.candidateMeshes[i].vertices)if(p.y>4.82&&
                std::hypot(p.x-centerX,p.z-centerZ)<.65) {raised=true;break;}
            ok&=check(raised==(dot<=i),QStringLiteral("candidate %1 marker dot %2").arg(i+1).arg(dot+1));
        }
    }
    const auto experiment=InterleavedFingerHingeCalibrationArtifact::observationTemplate(fixture);
    ok&=check(experiment.featureFamily==QStringLiteral("InterleavedFingerHinge")&&
        experiment.correctionDimension==FitCorrectionDimension::Height&&
        experiment.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate&&
        experiment.preferredCandidateIndex==0&&experiment.state==FitEvidenceState::Draft,
        "parallel contact-bump calibration contract");
    const int outputAt=args.indexOf(QStringLiteral("--output"));
    if(outputAt>=0&&outputAt+1<args.size()) {
        QDir output(args[outputAt+1]);
        ok&=check(output.exists(),"preexisting artifact output directory");
        const QString modelPath=output.filePath(QStringLiteral("BrickSuite-")+fixture.artifactIdentity+".3mf");
        const QString sessionPath=output.filePath(QStringLiteral("BrickSuite-")+fixture.artifactIdentity+"-session.json");
        if(QFile::exists(modelPath)||QFile::exists(sessionPath))return check(false,"artifact already exists")?0:1;
        QVector<ThreeMfWriter::NamedMesh> parts;
        for(int i=0;i<7;++i)parts.push_back({QStringLiteral("Candidate %1 — %2 mm contact bump")
            .arg(i+1).arg(fixture.candidates[i].functionalHeightMillimetres,0,'f',2),
            fixture.candidateMeshes[i],{12.0+26.0*(i%4),10.0+20.0*(i/4),0.0}});
        ThreeMfWriter::Options options;
        options.objectName=fixture.artifactIdentity;
        options.partIdentity=fixture.artifactIdentity;
        options.modelColor=QColor("#A0A5A9");
        QString error;
        ok&=check(ThreeMfWriter::writeCollection(parts,modelPath,options,&error),"3MF export: "+error);
        try {
            Lib3MF::CWrapper wrapper;auto model=wrapper.CreateModel();
            model->QueryReader("3mf")->ReadFromFile(modelPath.toStdString());
            auto objects=model->GetMeshObjects();
            ok&=check(objects->Count()==7,"3MF reopens with seven dot-marked objects");
            for(int candidate=0;candidate<7&&objects->MoveNext();++candidate) {
                const auto object=objects->GetCurrentMeshObject();
                for(int dot=0;dot<8;++dot) {
                    const double x=-(-1.6+2.4-1.6*(dot/2))+12.0+26.0*(candidate%4);
                    const double z=8.0+(dot%2==0?-.82:.82);
                    bool raised=false;
                    for(Lib3MF_uint32 vertex=0;vertex<object->GetVertexCount();++vertex) {
                        const auto p=object->GetVertex(vertex);
                        if(p.m_Coordinates[1]>4.82+10.0+20.0*(candidate/4)&&
                           std::hypot(p.m_Coordinates[0]-x,p.m_Coordinates[2]-z)<.65) {
                            raised=true;break;
                        }
                    }
                    ok&=check(raised==(dot<=candidate),QStringLiteral("exported finger hinge candidate %1 dot %2")
                        .arg(candidate+1).arg(dot+1));
                }
            }
        } catch(const std::exception& exception) {
            ok&=check(false,QStringLiteral("3MF reload failed: %1").arg(QString::fromUtf8(exception.what())));
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
            file.commit(),"managed session export");
        QFile reload(sessionPath);
        FitCalibrationSession decoded,imported,resumed;
        bool fresh=reload.open(QIODevice::ReadOnly)&&
            FitCalibrationSessionJson::fromJson(QJsonDocument::fromJson(reload.readAll()).object(),&decoded,&error)&&
            decoded.sessionIdentity==session.sessionIdentity&&decoded.hasCoarseExperiment&&
            !decoded.hasFineExperiment&&decoded.coarseExperiment.featureFamily==QStringLiteral("InterleavedFingerHinge")&&
            decoded.coarseExperiment.regenerationPrototype.family==FunctionalInterfaceFamily::InterleavedFingerHinge&&
            decoded.process.actualPrintedOrientation==FitPrintedOrientation::FeatureAxisParallelToBuildPlate&&
            decoded.coarseExperiment.candidates.size()==7&&decoded.coarseExperiment.preferredCandidateIndex==0&&
            decoded.coarseExperiment.state==FitEvidenceState::Draft;
        if(fresh)for(int i=0;i<7;++i)fresh&=decoded.coarseExperiment.candidates[i].observations.isEmpty()&&
            std::abs(decoded.coarseExperiment.candidates[i].functionalHeightMillimetres-
                fixture.candidates[i].functionalHeightMillimetres)<1e-9;
        ok&=check(fresh,"managed session reloads with untouched candidates: "+error);
        QTemporaryDir temporary;
        ok&=check(temporary.isValid(),"isolated managed library");
        FitCalibrationLibrary library(QDir(temporary.path()).filePath(QStringLiteral("managed")));
        ok&=check(library.importSession(sessionPath,&imported,&error)&&
            library.loadSession(session.sessionIdentity,&resumed,&error)&&
            resumed.coarseExperiment.candidates.size()==7,"managed import/resume path: "+error);
        QTextStream(stdout)<<"model="<<modelPath<<Qt::endl<<"session="<<sessionPath<<Qt::endl;
    }
    return ok?0:1;
}
