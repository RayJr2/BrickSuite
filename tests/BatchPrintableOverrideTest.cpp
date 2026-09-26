#include "../src/services/geometry/print/BatchPrintableModelService.h"
#include "../src/services/geometry/print/LocalPrintableOverrideService.h"
#include "../src/services/geometry/LDrawLibraryService.h"
#include "../src/services/geometry/ThreeMfWriter.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cstdio>

using namespace PrintGeometry;
namespace {
bool check(bool value,const char* message){if(!value)fprintf(stderr,"FAIL: %s\n",message);return value;}
QByteArray read(const QString& path){QFile file(path);return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray();}
bool write(const QString& path,const QByteArray& bytes){QFile file(path);return file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size();}
PrintMesh cube(){return {{{0,0,0},{10,0,0},{10,10,0},{0,10,0},{0,0,10},{10,0,10},{10,10,10},{0,10,10}},
    {{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},{1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}}};}
QByteArray ldraw(const PrintMesh& mesh){
    QByteArray bytes="0 Synthetic audit fixture\n0 BFC CERTIFY CCW\n";
    for(const auto& face:mesh.faces){bytes+="3 16";
        for(auto i:face){const auto& p=mesh.vertices[i];bytes+=" "+QByteArray::number(p.x/.4,'g',17)+" "+QByteArray::number(-p.z/.4,'g',17)+" "+QByteArray::number(p.y/.4,'g',17);}
        bytes+='\n';}
    return bytes;
}
}

int main(int argc,char** argv)
{
    if(const auto worker=LocalPrintableOverrideService::runUnionWorker(argc,argv))return *worker;
    QCoreApplication app(argc,argv);
    // Optional read-only real catalog/store acceptance run. Never import or approve.
    const auto args=app.arguments();
    if(args.size()==6){
        app.setOrganizationName(QStringLiteral("RFStateSide"));app.setApplicationName(QStringLiteral("BrickSuite"));
        QString error;const auto catalog=BatchPrintableModelService::loadCatalog(args[1],&error);
        if(!error.isEmpty()){fprintf(stderr,"%s\n",qPrintable(error));return 1;}
        BatchPrintOptions options;options.libraryRoot=args[2];options.localOverrideRoot=args[3];options.outputRoot=args[4];
        const auto run=BatchPrintableModelService().run(BatchPrintableModelService::explicitParts(catalog,args[5].split(',')),options);
        for(const auto& row:run.results)fprintf(stdout,"%s native=%s final=%s override=%s reopened=%d: %s\n",
            qPrintable(row.partNumber),qPrintable(BatchPrintableModelService::categoryCode(row.nativeCategory)),
            qPrintable(BatchPrintableModelService::categoryCode(row.category)),qPrintable(row.localOverrideState),row.reopened,qPrintable(row.diagnostic));
        fprintf(stdout,"run=%s ok=%d %s\n",qPrintable(run.runDirectory),run.ok,qPrintable(run.diagnostic));return run.ok?0:1;
    }
    QTemporaryDir temp;const QDir root(temp.path());bool ok=true;
    QDir().mkpath(root.filePath("library/parts"));
    QDir().mkpath(root.filePath("library/p"));
    BatchPrintOptions options;options.libraryRoot=root.filePath("library");options.outputRoot=root.filePath("runs");
    options.localOverrideRoot=root.filePath("overrides");options.autoFitEnabled=true;
    LocalPrintableOverrideService store(options.localOverrideRoot);
    const auto mesh=cube();QString error;ThreeMfWriter::Options writer;writer.modelColor=QColor(160,160,160);
    const QString repair=root.filePath("repair.3mf");
    ok&=check(ThreeMfWriter::write(mesh,repair,writer,&error),"write valid repair fixture");
    const auto fixture=[&](int id,double sheetZ){
        auto source=mesh;
        if(sheetZ>=0){source.vertices.insert(source.vertices.end(),{{4,4,sheetZ},{6,4,sheetZ},{5,6,sheetZ}});source.faces.push_back({8,9,10});}
        BatchPrintablePart part;part.partId=id;part.partNumber=QString::number(id);part.catalogPresent=true;
        ok&=check(write(root.filePath("library/parts/"+part.partNumber+".dat"),ldraw(source)),"write authoritative synthetic source");return part;
    };
    const auto context=[&](const BatchPrintablePart& part){return LocalPrintableOverrideService::Context{
        part.partId,part.partNumber,LDrawLibraryService::loadPart(options.libraryRoot,part.partNumber)};};
    const auto native=fixture(7000,-1),strict=fixture(7001,.1),user=fixture(7002,5),absent=fixture(7003,5);
    const auto stale=fixture(7004,5),invalid=fixture(7005,5),unaccepted=fixture(7006,5);
    const auto strictImport=store.importRepaired(context(strict),repair);
    ok&=check(strictImport.ok()&&!strictImport.prepared->userAcceptedOverride,"strict repair accepted by unchanged validator");
    for(const auto& part:{user,stale}){
        const auto c=context(part);const auto review=store.importRepaired(c,repair);
        ok&=check(review.reviewable&&!review.ok(),"internal sheet requires explicit fixture review");
        ok&=check(store.importRepaired(c,repair,true,review.reviewedSourceFingerprint,review.reviewedMeshFingerprint).ok(),"create accepted synthetic fixture before audit");
    }
    const auto review=store.importRepaired(context(unaccepted),repair);
    ok&=check(review.reviewable&&!QFile::exists(store.storagePath(context(unaccepted))),"unaccepted candidate is not stored");
    const QString stalePath=store.storagePath(context(stale));
    auto staleJson=QJsonDocument::fromJson(read(stalePath)).object();staleJson["sourceFingerprint"]="stale";
    ok&=check(write(stalePath,QJsonDocument(staleJson).toJson()),"create stale fixture");
    QDir().mkpath(options.localOverrideRoot);
    ok&=check(write(store.storagePath(context(invalid)),"{}"),"create invalid fixture");
    const auto before=read(store.storagePath(context(user)));
    const auto run=BatchPrintableModelService().run({native,strict,user,absent,stale,invalid,unaccepted},options);
    ok&=check(run.ok&&run.results.size()==7,"audit completes all synthetic Parts");
    if(run.results.size()!=7)return 1;
    for(const auto& r:run.results)fprintf(stderr,"fixture %s native=%s final=%s state=%s %s\n",qPrintable(r.partNumber),
        qPrintable(BatchPrintableModelService::categoryCode(r.nativeCategory)),qPrintable(BatchPrintableModelService::categoryCode(r.category)),qPrintable(r.localOverrideState),qPrintable(r.localOverrideDiagnostic));
    ok&=check(run.results[0].category==run.results[0].nativeCategory&&!run.results[0].localOverrideUsed,"uncertified synthetic source retains native result");
    ok&=check(run.results[1].category==BatchPrintCategory::StrictOverrideSuccess&&run.results[1].reopened,"strict override rescues native failure");
    const auto& rescued=run.results[2];
    ok&=check(rescued.category==BatchPrintCategory::UserOverrideSuccess&&rescued.nativeCategory!=BatchPrintCategory::Success&&
        rescued.reopened&&rescued.localOverrideUsed&&!rescued.nativeDiagnostic.isEmpty()&&rescued.profileIdentity.isEmpty()&&
        rescued.correctionSummary.isEmpty()&&rescued.exportPath.endsWith("7002-user_override_success.3mf"),"accepted override exports nominal without fit and retains native failure");
    ok&=check(run.results[3].localOverrideState=="none"&&run.results[3].category==run.results[3].nativeCategory,"missing override preserves failure");
    ok&=check(run.results[4].localOverrideStale&&run.results[4].localOverrideState=="stale"&&!run.results[4].localOverrideUsed,"stale source fingerprint refused");
    ok&=check(run.results[5].localOverrideState=="invalid_unavailable"&&!run.results[5].localOverrideUsed,"invalid store refused");
    ok&=check(run.results[6].localOverrideState=="none"&&!run.results[6].localOverrideUsed,"reviewable unaccepted repair is not a recovery");
    ok&=check(read(store.storagePath(context(user)))==before,"audit does not modify accepted override");
    ok&=check(run.totals.modelAvailable==7&&run.totals.nativeSuccessful==0&&run.totals.overrideRecoveries==2&&
        run.totals.successful==2&&run.totals.prepareFailures==7&&
        run.totals.printServicePercent()==0&&qAbs(run.totals.practicalPrintablePercent()-200.0/7)<1e-8,"native and practical metrics share model-bearing denominator");
    auto combined=run.results;BatchPrintResult nativeSuccess;nativeSuccess.category=BatchPrintCategory::Success;nativeSuccess.ldrawModel="3001";
    combined.append(nativeSuccess);const auto totals=BatchPrintableModelService::summarize(combined);
    ok&=check(totals.nativeSuccessful==1&&totals.overrideRecoveries==2&&totals.printServicePercent()==12.5&&totals.practicalPrintablePercent()==37.5,"native successes remain visible alongside override recoveries");
    const auto csv=read(run.csvPath);
    ok&=check(csv.contains("native_result_category,native_diagnostic,local_override_state,local_override_used,local_override_stale")&&
        csv.contains("export_result,reopen_result")&&csv.contains("\"user_override_success\"")&&csv.contains("\"strict_override_success\""),"CSV records native and final results with override and export outcomes");
    options.reopenValidator=[](const QString&,std::size_t,QString* error){*error="simulated reopen failure";return false;};
    const auto failed=BatchPrintableModelService().run({user},options).results.front();
    ok&=check(failed.category==BatchPrintCategory::ReopenFailed&&failed.localOverrideUsed&&!failed.reopened&&
        failed.nativeCategory==rescued.nativeCategory&&failed.exportPath.endsWith("7002-reopen_failed.3mf"),"override reopen failure is not success and preserves native category");
    options.reopenValidator={};
    auto meshStale=QJsonDocument::fromJson(before).object();meshStale["meshFingerprint"]="changed";
    ok&=check(write(store.storagePath(context(user)),QJsonDocument(meshStale).toJson()),"tamper synthetic repaired fingerprint");
    const auto meshStaleRun=BatchPrintableModelService().run({user},options);
    ok&=check(meshStaleRun.results.front().localOverrideStale&&meshStaleRun.totals.overrideRecoveries==0,"stale repaired fingerprint refused");
    CancellationState stop;
    options.phaseProgress=[&](int,const QString&,const QString& phase){if(phase=="local_override_load")stop.cancel();};
    const auto stopped=BatchPrintableModelService().run({strict,native},options,&stop);
    ok&=check(stopped.ok&&stopped.stopped&&stopped.results.size()==1&&!stopped.results[0].localOverrideUsed,"Stop before lookup prevents override and next Part");
    return ok?0:1;
}
