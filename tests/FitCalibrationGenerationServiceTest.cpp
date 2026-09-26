#include "../src/services/geometry/fit/FitCalibrationGenerationService.h"
#include "../src/services/geometry/fit/StandardStudCalibrationArtifact.h"
#include "../src/services/geometry/fit/FitCalibrationArtifactLocation.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <lib3mf_implicit.hpp>
#include <cstdio>

using namespace PrintGeometry;
using Service=FitCalibrationGenerationService;
namespace {
bool check(bool value,const char* message){if(!value)fprintf(stderr,"FAIL: %s\n",message);return value;}
QByteArray read(const QString& path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
bool write(const QString& path,const QByteArray& bytes){QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size();}
}
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);QTemporaryDir temp;bool ok=true;
    const QDir root(temp.path());const QString artifacts=root.filePath("artifacts"),managed=root.filePath("managed");
    Service service(artifacts,managed);FitCalibrationLibrary library(managed);
    Service::Request request;request.family="StandardStud";request.variant="Diameter";
    auto& process=request.workspace.process;process.printerIdentity="Synthetic printer";process.materialIdentity="PETG";
    process.profileName="Synthetic process";process.hasNozzleDiameter=true;process.nozzleDiameterMillimetres=.4;
    process.hasLayerHeight=true;process.layerHeightMillimetres=.2;process.dimensionalCompensationNotes="None";
    const auto result=service.generate(request);if(!result.ok())fprintf(stderr,"%s\n",qPrintable(result.diagnostic));
    ok&=check(result.ok()&&QFile::exists(result.fixturePath)&&QFile::exists(result.companionPath),"complete pair published and registered");
    if(!result.ok())return 1;
    ok&=check(result.companionPath==FitCalibrationArtifactLocation::companionPath(result.fixturePath),"pair shares filename base and directory");
    const auto original=read(result.companionPath);FitCalibrationSession portable;
    ok&=check(FitCalibrationSessionJson::fromJson(QJsonDocument::fromJson(original).object(),&portable)&&
        FitCalibrationSessionJson::toJson(portable)==FitCalibrationSessionJson::toJson(result.session),"standard single-session companion round trips unchanged");
    StandardStudCalibrationArtifactDefinition definition;definition.artifactIdentity=StandardStudCalibrationArtifact::diameterArtifactIdentity();
    const auto generated=StandardStudCalibrationArtifact::generate(StandardStudCalibrationArtifact::canonicalPrototype(),definition);
    const auto expected=StandardStudCalibrationArtifact::observationTemplate(generated,definition);
    const auto& experiment=result.session.coarseExperiment;
    ok&=check(experiment.candidates.size()==expected.candidates.size()&&
        experiment.regenerationPrototype.evidenceContract==expected.regenerationPrototype.evidenceContract,"generator and session share contract/candidates");
    for(int i=0;i<experiment.candidates.size();++i)ok&=check(experiment.candidates[i].index==expected.candidates[i].index&&
        experiment.candidates[i].diameterCorrectionMillimetres==expected.candidates[i].diameterCorrectionMillimetres,"candidate values are unchanged");
    Lib3MF::CWrapper wrapper;auto model=wrapper.CreateModel();model->QueryReader("3mf")->ReadFromFile(result.fixturePath.toStdString());
    auto meshes=model->GetMeshObjects();ok&=check(meshes->MoveNext()&&meshes->GetCurrentMeshObject()->GetTriangleCount()==generated.mesh.faces.size(),"published geometry matches unchanged generator");
    ok&=check(result.session.process.printerIdentity==process.printerIdentity&&result.session.process.layerHeightMillimetres==.2&&
        result.session.process.actualPrintedOrientation==expected.process.actualPrintedOrientation,"context inherited with family orientation");
    const auto second=service.generate(request);
    ok&=check(second.ok()&&second.directory!=result.directory&&second.session.sessionIdentity!=result.session.sessionIdentity&&
        second.session.coarseExperiment.artifactIdentity!=experiment.artifactIdentity&&read(result.companionPath)==original,"same fixture name gets fresh collision-safe print/session identities");
    ok&=check(service.recover(result.directory).ok()&&library.sessions().size()==2,"registration recovery is idempotent");
    auto observed=result.session;FitCalibrationObservation observation;observation.result=FitObservation::Acceptable;
    ok&=check(FitCalibrationEvidencePolicy::addObservation(&observed.coarseExperiment,4,observation)&&library.saveSession(&observed),"record later managed evidence");
    const auto recovered=service.recover(result.directory);
    ok&=check(recovered.ok()&&recovered.session.coarseExperiment.candidates[3].observations.size()==1,"retry preserves later observations");
    Service::Request child=request;child.hasParent=true;child.parent=observed;child.stage=Service::Stage::Verification;
    ok&=check(FitCalibrationEvidencePolicy::selectPreferredCandidate(&child.parent.coarseExperiment,4),"select preferred parent candidate");
    const auto childResult=service.generate(child);if(!childResult.ok())fprintf(stderr,"child: %s\n",qPrintable(childResult.diagnostic));
    ok&=check(childResult.ok()&&childResult.session.hasCoarseExperiment&&childResult.session.hasFineExperiment&&
        childResult.session.fineExperiment.parentArtifactIdentity==observed.coarseExperiment.artifactIdentity&&
        childResult.session.sessionIdentity!=observed.sessionIdentity&&childResult.session.coarseExperiment.candidates[3].observations.size()==1&&
        childResult.session.fineExperiment.candidates[1].observations.isEmpty(),"continuation retains lineage and parent evidence with fresh child identity/evidence");
    auto verified=observed;verified.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;
    verified.coarseExperiment.process=verified.process;
    for(int candidate:{3,4,5}){auto repeat=observation;repeat.repeatNumber=candidate==4?2:1;
        ok&=check(FitCalibrationEvidencePolicy::addObservation(&verified.coarseExperiment,candidate,repeat),"verification fixture observations");}
    ok&=check(FitCalibrationEvidencePolicy::selectPreferredCandidate(&verified.coarseExperiment,4)&&
        FitCalibrationEvidencePolicy::markVerified(&verified.coarseExperiment)&&library.saveSession(&verified),"persist explicitly Verified test evidence");
    const auto verifiedRecovery=service.recover(result.directory);
    ok&=check(verifiedRecovery.ok()&&verifiedRecovery.session.coarseExperiment.state==FitEvidenceState::Verified,"registration retry preserves Verified evidence");
    auto height=request;height.variant="Height";height.workspace.featureSessions={verified};
    const auto heightResult=service.generate(height);
    ok&=check(heightResult.ok()&&heightResult.session.coarseExperiment.correctionDimension==FitCorrectionDimension::Height&&
        heightResult.session.coarseExperiment.fixedDiameterCorrectionMillimetres==verified.coarseExperiment.candidates[3].diameterCorrectionMillimetres,"Stud Height inherits existing Verified OD prerequisite");
    const auto before=library.sessions().size();
    for(const auto point:{Service::Checkpoint::GeometryWritten,Service::Checkpoint::BeforeReopen,Service::Checkpoint::Published,Service::Checkpoint::Registered}){
        const QString output=root.filePath("fault-"+QString::number(int(point)));
        Service faulty(output,managed,[point](Service::Checkpoint at,const QString& path,QString* error){
            if(at!=point)return true;
            if(point==Service::Checkpoint::BeforeReopen){
                const auto files=QDir(path).entryList({"*.3mf"},QDir::Files);return !files.isEmpty()&&write(QDir(path).filePath(files.front()),"broken 3mf");
            }
            *error="Simulated interruption";return false;
        });
        const auto failed=faulty.generate(request);
        ok&=check(!failed.ok(),"injected failure is reported");
        if(point==Service::Checkpoint::Published||point==Service::Checkpoint::Registered){
            ok&=check(failed.published&&QFile::exists(failed.fixturePath)&&QFile::exists(failed.companionPath),"registration interruption preserves complete pair");
            const auto retry=service.recover(failed.directory);
            ok&=check(retry.ok()&&service.recover(failed.directory).ok(),"interrupted registration can be retried idempotently");
        }else ok&=check(!failed.published&&QDir(output).entryList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::Hidden).isEmpty(),"partial write/reopen failure leaves no complete or staging package");
    }
    ok&=check(library.sessions().size()==before+2,"only published packages register sessions");
    Service partial(root.filePath("write-failure"),managed,[](Service::Checkpoint at,const QString& path,QString*){
        if(at==Service::Checkpoint::GeometryWritten){
            const auto files=QDir(path).entryList({"*.3mf"},QDir::Files);
            if(files.isEmpty())return false;
            return QDir().mkpath(FitCalibrationArtifactLocation::companionPath(QDir(path).filePath(files.front())));
        }return true;
    });
    ok&=check(!partial.generate(request).published&&QDir(root.filePath("write-failure")).entryList(QDir::Dirs|QDir::Hidden|QDir::NoDotAndDotDot).isEmpty(),"companion write failure after geometry leaves no published package");
    Service mismatch(root.filePath("mismatch"),managed,[](Service::Checkpoint at,const QString& path,QString*){
        if(at==Service::Checkpoint::BeforeReopen){
            const auto files=QDir(path).entryList({"*-session.json"},QDir::Files);
            return !files.isEmpty()&&write(QDir(path).filePath(files.front()),"{}");
        }return true;
    });
    ok&=check(!mismatch.generate(request).published,"companion disagreement prevents publication");
    const QString blocked=root.filePath("not-a-directory");ok&=check(write(blocked,"sentinel"),"create unwritable destination control");
    const auto denied=Service(blocked,managed).generate(request);
    ok&=check(!denied.published&&!denied.registered&&read(blocked)=="sentinel","unwritable destination preserves existing file");
    const auto pendingRegistration=Service(root.filePath("register-failure"),blocked).generate(request);
    ok&=check(pendingRegistration.published&&!pendingRegistration.registered&&service.recover(pendingRegistration.directory).ok(),"actual managed-storage failure recovers without regeneration");
    FitCalibrationSession historical;historical.sessionIdentity="historical-session";historical.process=process;historical.hasCoarseExperiment=true;
    historical.coarseExperiment=expected;historical.coarseExperiment.process=process;
    const QString old=root.filePath("historical-session.json");const auto oldBytes=QJsonDocument(FitCalibrationSessionJson::toJson(historical)).toJson();
    ok&=check(write(old,oldBytes)&&library.importSession(old,&historical)&&read(old)==oldBytes,"historical single-session import is unchanged");
    ok&=check(write(second.companionPath,"{}")&&!service.recover(second.directory).ok(),"modified package companion rejected during recovery");
    for(const auto& family:QStringList{"RoundTechnicPassage","StudReceivingClutch","FrictionlessTechnicPin","FrictionTechnicPin","TechnicAxle","TechnicAxleHole","StandardBar","CClipBarReceiver","BallJoint"}){
        auto entry=request;entry.family=family;entry.variant=family=="StudReceivingClutch"?"TubeWallCell":"";
        if(family=="CClipBarReceiver"||family=="BallJoint")entry.orientation=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;
        const auto generatedPackage=service.generate(entry);
        if(!generatedPackage.ok())fprintf(stderr,"%s: %s\n",qPrintable(family),qPrintable(generatedPackage.diagnostic));
        ok&=check(generatedPackage.ok(),"currently supported coarse family publishes through shared service");
    }
    for(const auto& variant:QStringList{"PostWallCell","WallPocket","AntiStudBore"}){
        auto entry=request;entry.family="StudReceivingClutch";entry.variant=variant;
        const auto generatedPackage=service.generate(entry);
        if(!generatedPackage.ok())fprintf(stderr,"%s: %s\n",qPrintable(variant),qPrintable(generatedPackage.diagnostic));
        ok&=check(generatedPackage.ok(),"receiving variants retain existing generators");
    }
    for(const auto& family:QStringList{"CClipBarReceiver","BallJoint"}){
        auto entry=request;entry.family=family;entry.orientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;
        const auto generatedPackage=service.generate(entry);
        ok&=check(generatedPackage.ok()&&generatedPackage.session.process.actualPrintedOrientation==entry.orientation,"parallel orientation-specific generation preserved");
    }
    auto boundary=child;boundary.stage=Service::Stage::FineSearch;
    boundary.parent.coarseExperiment.candidates[0].observations.push_back(observation);
    boundary.parent.coarseExperiment.preferredCandidateIndex=1;
    const auto extension=service.generate(boundary);
    ok&=check(extension.ok()&&extension.session.fineExperiment.artifactIdentity.contains("extension")&&
        extension.session.fineExperiment.state!=FitEvidenceState::Verified,"boundary winner extends without premature verification");
    return ok?0:1;
}
