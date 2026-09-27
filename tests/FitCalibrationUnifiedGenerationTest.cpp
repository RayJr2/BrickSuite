#include "../src/services/geometry/fit/FitCalibrationGenerationService.h"
#include "../src/services/geometry/fit/FitCalibrationCapabilities.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QCryptographicHash>
#include <cstdio>
using namespace PrintGeometry;
using Service=FitCalibrationGenerationService;
namespace {
bool check(bool ok,const char* message){if(!ok)fprintf(stderr,"FAIL: %s\n",message);return ok;}
QByteArray read(const QString& path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
bool write(const QString& path,const QByteArray& data){QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(data)==data.size();}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);QTemporaryDir temp;const QDir root(temp.path());bool ok=true;
    FitCalibrationWorkspace workspace;auto& p=workspace.process;p.printerIdentity="Synthetic printer";p.materialIdentity="PETG";p.profileName="Synthetic process";
    p.hasNozzleDiameter=true;p.nozzleDiameterMillimetres=.4;p.hasLayerHeight=true;p.layerHeightMillimetres=.2;p.dimensionalCompensationNotes="None";
    Service service(root.filePath("artifacts"),root.filePath("managed"));FitCalibrationLibrary library(root.filePath("managed"));
    auto recommended=Service::recommended(workspace);auto planned=Service::planPackage(recommended);
    ok&=check(planned.ok()&&planned.fixtures.size()==1&&planned.fixtures.front().combinedCore&&planned.fixtures.front().selections.size()==4,"recommended reuses four independent zones");
    auto invalid=recommended;invalid.selections.push_back(invalid.selections.front());ok&=check(!Service::planPackage(invalid).ok(),"duplicate selection rejected");
    invalid=recommended;invalid.selections[1].variant={};ok&=check(!Service::planPackage(invalid).ok(),"missing variant rejected before generation");
    invalid=recommended;invalid.selections.back().workspace.process.materialIdentity="Different";ok&=check(!Service::planPackage(invalid).ok(),"mixed manufacturing context rejected");
    invalid=recommended;invalid.selections.front().variant="Height";ok&=check(!Service::planPackage(invalid).ok(),"missing prerequisite rejected before generation");
    invalid=recommended;invalid.selections.front().family="BallSocket";invalid.selections.front().variant={};invalid.selections.front().orientation=FitPrintedOrientation::FeatureAxisParallelToBuildPlate;
    ok&=check(!Service::planPackage(invalid).ok(),"missing installed source rejected");
    auto extra=recommended.selections.front();extra.family="StandardBar";extra.variant={};recommended.selections.push_back(extra);
    planned=Service::planPackage(recommended);ok&=check(planned.ok()&&planned.fixtures.size()==2,"core plus separate fixture planned");
    QStringList stages;const auto result=service.generatePackage(recommended,[&](const QString& stage){stages.push_back(stage);});
    if(!result.ok())fprintf(stderr,"package: %s\n",qPrintable(result.diagnostic));
    ok&=check(result.ok()&&result.sessions.size()==5&&result.fixturePaths.size()==2&&result.registeredCount==5,"complete package generated/reopened/registered");if(!result.ok())return 1;
    ok&=check(Service::isPackageCompanion(result.companionPath)&&QFileInfo(result.companionPath).fileName()=="package-session.json"&&
        QFileInfo(result.fixturePaths.front()).fileName()=="perpendicular-core.3mf"&&QFileInfo(result.fixturePaths.back()).fileName()=="02-standard-bar-perpendicular.3mf","portable envelope and readable filenames");
    const auto envelope=QJsonDocument::fromJson(read(result.companionPath)).object();
    ok&=check(envelope["version"].toInt()==1&&envelope["manufacturingContext"].toObject()["printerIdentity"].toString()==p.printerIdentity,"versioned context serialized");
    QSet<QString> ids{envelope["packageIdentity"].toString()};
    for(const auto& f:envelope["fixtures"].toArray())ids.insert(f.toObject()["identity"].toString());
    for(const auto& value:envelope["sessions"].toArray()){const auto e=value.toObject();ids.insert(e["identity"].toString());ids.insert(e["zoneIdentity"].toString());
        FitCalibrationSession portable;ok&=check(FitCalibrationSessionJson::fromJson(e["session"].toObject(),&portable)&&portable.sessionIdentity==e["identity"].toString(),"each embedded session retains legacy single-session compatibility");}
    ok&=check(ids.size()==13&&stages.contains("Publishing calibration package..."),"package fixture session zone identities distinct and real progress reported");
    const auto core=envelope["fixtures"].toArray().first().toObject()["zones"].toObject()["zones"].toArray();
    ok&=check(core.size()==4,"pilot zone manifest retained");
    for(int i=0;i<core.size();++i){const auto z=core[i].toObject();ok&=check(z["sessionIdentity"].toString()==result.sessions[i].sessionIdentity&&
        z["meshSha256"].toString().size()==64&&!z["physicalLabel"].toString().isEmpty(),"zone/session/hash/physical-label binding");}
    auto verified=result.sessions.front();verified.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;
    auto& e=verified.coarseExperiment;e.process=verified.process;FitCalibrationObservation o;o.result=FitObservation::Acceptable;
    for(int c:{3,4,5})FitCalibrationEvidencePolicy::addObservation(&e,c,o);o.repeatNumber=2;FitCalibrationEvidencePolicy::addObservation(&e,4,o);
    FitCalibrationEvidencePolicy::selectPreferredCandidate(&e,4);
    ok&=check(FitCalibrationEvidencePolicy::markVerified(&e)&&library.saveSession(&verified),"one sibling explicitly verified");
    FitProfile profile;QString error;ok&=check(FitCalibrationLibrary::promoteVerifiedSession(verified,"Synthetic fit",&profile,&error),"package Verified evidence uses normal profile eligibility");
    const auto recovered=service.recoverPackage(result.directory);
    ok&=check(recovered.ok()&&library.sessions().size()==5&&recovered.sessions.front().coarseExperiment.state==FitEvidenceState::Verified&&
        recovered.sessions[1].coarseExperiment.candidates[3].observations.isEmpty(),"recovery preserves Verified sibling and independent draft observations");
    Service::Request child;child.hasParent=true;child.parent=recovered.sessions[1];child.stage=Service::Stage::Verification;
    o.repeatNumber=1;FitCalibrationEvidencePolicy::addObservation(&child.parent.coarseExperiment,4,o);FitCalibrationEvidencePolicy::selectPreferredCandidate(&child.parent.coarseExperiment,4);
    const auto continuation=service.generate(child);if(!continuation.ok())fprintf(stderr,"child: %s\n",qPrintable(continuation.diagnostic));
    ok&=check(continuation.ok()&&continuation.session.sessionIdentity!=child.parent.sessionIdentity&&
        continuation.session.fineExperiment.parentArtifactIdentity==child.parent.coarseExperiment.artifactIdentity&&continuation.session.coarseExperiment.candidates[3].observations.size()==1,"one sibling continues through unchanged single-family path");
    ok&=check(service.recoverPackage(result.directory).sessions.front().coarseExperiment.state==FitEvidenceState::Verified,"continuation leaves Verified sibling untouched");
    Service::PackageRequest childPackage;childPackage.selections={child};
    const auto packagedChild=service.generatePackage(childPackage);
    if(!packagedChild.ok())fprintf(stderr,"child package: %s\n",qPrintable(packagedChild.diagnostic));
    ok&=check(packagedChild.ok()&&packagedChild.sessions.size()==1&&packagedChild.sessions.front().fineExperiment.parentArtifactIdentity==child.parent.coarseExperiment.artifactIdentity,
        "package API also supports one authoritative continuation with lineage");
    Service::PackageRequest small;small.selections={extra};auto second=extra;second.family="BallJoint";small.selections.push_back(second);
    const auto before=library.sessions().size();
    for(const auto checkpoint:{Service::Checkpoint::GeometryWritten,Service::Checkpoint::BeforeReopen,Service::Checkpoint::Published,Service::Checkpoint::Registered}){
        const auto output=root.filePath("fault-"+QString::number(int(checkpoint)));int registered=0;
        Service fault(output,root.filePath("managed"),[&](Service::Checkpoint at,const QString& directory,QString* error){
            if(at!=checkpoint)return true;
            if(at==Service::Checkpoint::BeforeReopen){
                // Keep the hashes consistent so rejection exercises the actual
                // lib3mf reopen, not just an earlier checksum failure.
                const auto companion=QDir(directory).filePath("package-session.json");auto json=QJsonDocument::fromJson(read(companion)).object();
                auto fixtures=json["fixtures"].toArray();auto fixture=fixtures.first().toObject();
                if(!write(QDir(directory).filePath(fixture["file"].toString()),"invalid"))return false;
                fixture["sha256"]=QString::fromLatin1(QCryptographicHash::hash("invalid",QCryptographicHash::Sha256).toHex());fixtures[0]=fixture;json["fixtures"]=fixtures;
                const auto bytes=QJsonDocument(json).toJson();const auto recordPath=QDir(directory).filePath("publication.json");auto record=QJsonDocument::fromJson(read(recordPath)).object();
                record["companionSha256"]=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
                return write(companion,bytes)&&write(recordPath,QJsonDocument(record).toJson());
            }
            if(at==Service::Checkpoint::Registered&&++registered<1)return true;
            *error="Simulated interruption";return false;
        });
        const auto interrupted=fault.generatePackage(small);ok&=check(!interrupted.ok(),"failure never reports complete package");
        if(checkpoint==Service::Checkpoint::Published||checkpoint==Service::Checkpoint::Registered){
            ok&=check(interrupted.published&&interrupted.registeredCount==(checkpoint==Service::Checkpoint::Registered?1:0),"partial registration explicitly reported");
            ok&=check(service.recoverPackage(interrupted.directory).ok()&&service.recoverPackage(interrupted.directory).ok(),"interrupted registration recovers idempotently");
        }else ok&=check(!interrupted.published&&QDir(output).entryList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::Hidden).isEmpty(),"failed fixture/reopen cannot expose half package");
    }
    ok&=check(library.sessions().size()==before+4,"only published packages register user sessions");
    const auto original=read(result.companionPath);const auto recordPath=QDir(result.directory).filePath("publication.json");const auto originalRecord=read(recordPath);
    auto mismatched=envelope;auto entries=mismatched["sessions"].toArray();auto changed=entries.first().toObject();changed["intendedOrientation"]=int(FitPrintedOrientation::FeatureAxisParallelToBuildPlate);entries[0]=changed;mismatched["sessions"]=entries;
    const auto mismatchBytes=QJsonDocument(mismatched).toJson();auto record=QJsonDocument::fromJson(originalRecord).object();record["companionSha256"]=QString::fromLatin1(QCryptographicHash::hash(mismatchBytes,QCryptographicHash::Sha256).toHex());
    ok&=check(write(result.companionPath,mismatchBytes)&&write(recordPath,QJsonDocument(record).toJson())&&!service.recoverPackage(result.directory).ok(),"semantic orientation mismatch rejected even with matching file hash");
    write(recordPath,originalRecord);write(result.companionPath,original);
    ok&=check(write(result.companionPath,"{}")&&!service.recoverPackage(result.directory).ok(),"companion hash mismatch rejected");write(result.companionPath,original);
    ok&=check(write(result.fixturePaths.front(),"invalid")&&!service.recoverPackage(result.directory).ok(),"changed fixture rejected without touching existing sessions");
    return ok?0:1;
}
