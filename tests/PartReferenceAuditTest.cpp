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

#include "../src/services/geometry/print/BatchPrintableModelService.h"
#include "../src/services/geometry/fit/FitCalibrationLibrary.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QRegularExpression>
#include <cstdio>
#include <stdexcept>
#include <cerrno>
using namespace PrintGeometry;
namespace {
bool check(bool value,const char* text){if(!value)fprintf(stderr,"FAIL: %s\n",text);return value;}
QByteArray read(const QString& path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
bool write(const QString& path,const QByteArray& data){QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(data)==data.size();}
int sharingError(){
#ifdef Q_OS_WIN
    return 32; // ERROR_SHARING_VIOLATION, injected without a timing-dependent file lock.
#else
    return EBUSY;
#endif
}
bool checkpointTests(const QString& root)
{
    bool ok=true;QString error;
    for(int failures:{0,1,3,6}){
        const QString path=QDir(root).filePath(QStringLiteral("checkpoint-%1.json").arg(failures));
        BatchPrintRunState initial;
        ok&=check(initial.save(path,{{"completedCount",7}},&error),"normal durable checkpoint");
        const auto previous=read(path);int attempts=0;unsigned long waited=0;
        BatchPrintRunState::TestHooks hooks;
        hooks.wait=[&](unsigned long delay){waited+=delay;};
        hooks.replacementError=[&](const QString& destination,const QJsonObject&,int attempt){
            attempts=attempt;
            ok&=check(read(destination)==previous,"previous complete checkpoint survives every failed replacement");
            return attempt<=failures?sharingError():0;
        };
        BatchPrintRunState writer(hooks);
        const bool saved=writer.save(path,{{"completedCount",8}},&error);
        ok&=check(saved==(failures<6)&&attempts==std::min(failures+1,6)&&waited<=775,
            "normal, one/several transient, or exhausted replacement has bounded attempts/backoff");
        ok&=check(saved?QJsonDocument::fromJson(read(path)).object().value("completedCount").toInt()==8:read(path)==previous,
            "only committed replacement changes checkpoint");
        if(!saved)ok&=check(error.contains("operation=atomic replace")&&error.contains("temp=")&&
            error.contains(path)&&error.contains("nativeError=")&&error.contains("QtError=")&&
            error.contains("attempt=6/6")&&error.contains("elapsedMs="),"failure has actionable filesystem diagnostics");
        ok&=check(QDir(root).entryList({QFileInfo(path).fileName()+".??????"},QDir::Files).isEmpty(),"failed temporaries cleaned without removing destination");
    }
    BatchPrintRunState denied;
    ok&=check(!denied.save(QDir(root).filePath("missing-directory/state.json"),{},&error)&&
        error.contains("open temporary")&&error.contains("attempt=1/6"),"unwritable/missing directory stops without replacement retries");
#ifdef Q_OS_WIN
    for(int native:{5,33,87}){ // access denied, lock violation, non-retryable invalid parameter
        const auto path=QDir(root).filePath(QStringLiteral("native-%1.json").arg(native));
        ok&=check(denied.save(path,{{"completedCount",1}},&error),"native-error baseline checkpoint");
        const auto previous=read(path);int attempts=0;
        BatchPrintRunState::TestHooks hooks;hooks.wait=[](unsigned long){};
        hooks.replacementError=[&](const QString&,const QJsonObject&,int attempt){attempts=attempt;return native;};
        BatchPrintRunState blocked(hooks);
        ok&=check(!blocked.save(path,{{"completedCount",2}},&error)&&read(path)==previous&&attempts==(native==87?1:6),
            "persistent access/lock denial bounded, unrelated native error immediate, checkpoint intact");
    }
#endif
    return ok;
}
}
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);QTemporaryDir temp;bool ok=true;
    // Explicit opt-in, bounded installed-library acceptance harness. No catalog scan.
    if(argc==9&&(app.arguments()[1]==QStringLiteral("--fit-acceptance")||app.arguments()[1]==QStringLiteral("--fit-reporting"))){
        const auto args=app.arguments();
        const auto corpus=BatchPrintableModelService::readPartReferencePlan(args[2],args[3]);
        const auto ids=QString::fromUtf8(read(args[6])).split(QRegularExpression("[\\s,;]+"),Qt::SkipEmptyParts);
        if(!check(corpus.ok()&&!ids.isEmpty()&&ids.size()<=140,"bounded saved-corpus acceptance list"))return 1;
        QVector<BatchPrintablePart> parts;
        for(const auto& id:ids){
            const auto found=std::find_if(corpus.parts.cbegin(),corpus.parts.cend(),[&](const auto& part){return part.partNumber==id;});
            if(!check(found!=corpus.parts.cend(),"acceptance Part belongs to original corpus"))return 1;
            parts.append(*found);
        }
        BatchPrintOptions options;options.libraryRoot=args[3];options.fitLibraryRoot=args[4];
        options.selectedFitProfileIdentity=args[5]=="automatic"?QString():args[5];
        options.outputRoot=args[7];options.localOverrideRoot=temp.filePath("overrides");
        options.excludeNonstandardIds=false;
        if(args[8]=="x")options.printOrientation.rotate(PrintOrientation::Rotation::XPositive);
        else if(args[8]=="y")options.printOrientation.rotate(PrintOrientation::Rotation::YPositive);
        else if(args[8]!="nominal")return 1;
        const auto run=BatchPrintableModelService().run(parts,options,nullptr,[](const auto& row,const auto&){
            fprintf(stdout,"%s: %s; fit=%s; source=%d recognized=%d applicable=%d applied=%d nonzero=%d zero=%d partial=%d\n",
                qPrintable(row.partNumber),qPrintable(BatchPrintableModelService::categoryCode(row.category)),
                qPrintable(row.fitStatus),row.sourceRecognizedFeatureCount,row.recognizedFeatureCount,
                row.applicableVerifiedFeatureCount,row.correctedFeatureCount,row.nonzeroCorrectedFeatureCount,
                row.verifiedZeroFeatureCount,row.partialFitCoverage);fflush(stdout);
        });
        fprintf(stdout,"Run: %s\n%s\n",qPrintable(run.runDirectory),qPrintable(run.diagnostic));
        if(args[1]==QStringLiteral("--fit-reporting")){
            ok&=check(run.results.size()==4,"reporting proof contains exactly four controls");
            for(const auto& row:run.results){
                ok&=check(row.category==BatchPrintCategory::Success&&row.reopened&&row.nominalPreparedReady,
                    "reporting controls retain nominal readiness and successful export/reopen");
                if(row.partNumber=="4275b")ok&=check(!row.profileIdentity.isEmpty()&&row.correctedFeatureCount==0&&
                    row.fitStatus=="nominal_no_verified_application"&&row.diagnostic.contains("no compatible Verified corrections were applied")&&
                    !row.diagnostic.contains("Verified ManufacturingMesh"),"4275b resolved profile is not an application");
                else if(row.partNumber=="30374")ok&=check(row.verifiedZeroFeatureCount>0&&row.nonzeroCorrectedFeatureCount==0&&
                    row.fitStatus=="verified_zero_applied"&&row.diagnostic.contains("zero dimensional adjustment"),"genuine Standard Bar Verified-zero");
                else if(row.partNumber=="98138")ok&=check(row.nonzeroCorrectedFeatureCount>0&&
                    row.diagnostic.contains("Verified Fit corrections applied"),"actual AntiStudBore nonzero application");
                else if(row.partNumber=="3666")ok&=check(row.correctedFeatureCount==6&&row.recognizedFeatureCount==11&&
                    row.partialFitCoverage&&row.diagnostic.contains("Partial Verified Fit applied. 6 of 11"),"actual partial stud/post coverage");
                else ok&=check(false,"unexpected reporting control");
            }
        }
        return ok&&run.ok&&run.results.size()==parts.size()?0:1;
    }
    // Opt-in one-Part continuation proof; never runs the full installed corpus.
    if(argc==5&&app.arguments()[1]==QStringLiteral("--3021-saved-plan-check")){
        const auto plan=BatchPrintableModelService::readPartReferencePlan(app.arguments()[2],app.arguments()[3]);
        if(!check(plan.ok()&&plan.parts.size()>=145&&plan.parts[144].partNumber==QStringLiteral("3021"),"saved sequence 145 identifies 3021"))return 1;
        BatchPrintOptions proof;proof.libraryRoot=app.arguments()[3];proof.outputRoot=app.arguments()[4];
        proof.localOverrideRoot=temp.filePath("overrides");proof.autoFitEnabled=false;proof.partReference=true;proof.corpusPlan=plan.plan;
        QString statePath;int lastOperation=0;
        proof.beforePart=[&](const QString& path,int,const QString&){statePath=path;};
        proof.phaseProgress=[&](int,const QString&,const QString& phase){
            if(phase!=QStringLiteral("preparing/boolean_composition"))return;
            const auto state=QJsonDocument::fromJson(read(statePath)).object();
            const int current=state.value("currentBooleanOperation").toInt();
            ok&=check(state.value("currentReferenceSequence").toInt()==145&&state.value("currentPartNumber")=="3021"&&current>=lastOperation,
                "3021 operation checkpoint is durable before observer/work");lastOperation=current;
        };
        const auto run=BatchPrintableModelService().run(plan.parts.mid(144,1),proof);
        const auto state=QJsonDocument::fromJson(read(run.statePath)).object();
        ok&=check(run.ok&&run.results.size()==1&&lastOperation==5&&state.value("completedCount").toInt()==1&&
            state.value("status")=="completed"&&state.value("corpusFingerprint")==plan.plan.value("corpusFingerprint")&&
            run.results[0].nativeCategory!=BatchPrintCategory::Success,"single 3021 failure row completes with unchanged saved-plan fingerprint");
        fprintf(stdout,"3021 saved-plan result: %s, folder: %s\n",run.results.isEmpty()?"missing":qPrintable(BatchPrintableModelService::categoryCode(run.results[0].category)),qPrintable(run.runDirectory));
        return ok?0:1;
    }
    ok&=checkpointTests(temp.path());
    QDir(temp.path()).mkpath("parts");
    const QByteArray triangle="0 !LDRAW_ORG Part\n0 BFC CERTIFY CCW\n3 16 0 0 0 20 0 0 0 20 0\n";
    ok&=check(write(temp.filePath("parts/1100.dat"),triangle)&&write(temp.filePath("parts/1101pat.dat"),triangle),"model fixtures");
    BatchPrintablePart a;a.partId=1;a.partNumber="1100";a.catalogPresent=true;
    auto b=a;b.partId=2;b.partNumber="1101pat";b.noColor=true;b.category="Stickers";
    auto c=a;c.partId=3;c.partNumber="99999999";
    PartReferenceEntry first;first.partNumber=a.partNumber;first.catalog="Bricks";first.section="Ordinary";first.displayOrder=1;
    auto second=first;second.partNumber=b.partNumber;second.displayOrder=2;
    auto duplicate=first;duplicate.partId=1;duplicate.partNumber="alternate-identity";duplicate.catalog="Technic";
    auto missing=first;missing.partNumber=c.partNumber;missing.catalog="Technic";
    const QList<PartReferenceEntry> entries{first,second,duplicate,missing,PartReferenceEntry{}};
    const auto corpus=BatchPrintableModelService::partReferenceCorpus(entries,{a,b,c},temp.path());
    ok&=check(corpus.ok()&&corpus.parts.size()==3&&corpus.plan.value("manifestPositionCount").toInt()==5&&
        corpus.plan.value("duplicateMembershipCount").toInt()==1&&
        corpus.plan.value("structurallyInvalidEntries").toArray().size()==1,"positions, canonical deduplication, explicit invalid-entry accounting");
    ok&=check(corpus.parts[0].partId==1&&corpus.parts[0].referenceMemberships.size()==2&&
        corpus.parts[0].referenceMemberships[1].toObject().value("manifestPosition").toInt()==3&&
        corpus.parts[1].partNumber==b.partNumber,"first occurrence order and all memberships retained");
    const auto repeated=BatchPrintableModelService::partReferenceCorpus(entries,{c,b,a},temp.path());
    ok&=check(corpus.plan==repeated.plan,"catalog query order does not change corpus/fingerprint");
    auto changedEntries=entries;changedEntries[0].section="Changed";
    ok&=check(BatchPrintableModelService::partReferenceCorpus(changedEntries,{a,b,c},temp.path()).plan.value("corpusFingerprint")!=
        corpus.plan.value("corpusFingerprint"),"membership changes affect fingerprint");
    const auto preview=BatchPrintableModelService::summarizePartReference(corpus.parts,{});
    ok&=check(preview.value("modelBearingParts").toInt()==2&&preview.value("noModelParts").toInt()==1,"availability counts include excluded-looking and no-model Parts");
    BatchPrintOptions options;options.libraryRoot=temp.path();options.outputRoot=temp.filePath("runs");
    options.runId="reference";options.partReference=true;options.corpusPlan=corpus.plan;
    options.excludeNoModel=true;options.excludeNonstandardIds=true;options.autoFitEnabled=false;
    int started=0;
    options.beforePart=[&](const QString& path,int sequence,const QString&){
        ++started;const auto state=QJsonDocument::fromJson(read(path)).object();
        ok&=check(state.value("currentSampleSequence").toInt()==sequence&&state.value("orderedParts").toArray().size()==3,
            "complete order and active checkpoint precede work");
        const auto saved=BatchPrintableModelService::readPartReferencePlan(QFileInfo(path).dir().filePath("part-reference-plan.json"),temp.path());
        auto savedCorpus=saved.plan;savedCorpus.remove("printingContext");
        ok&=check(saved.ok()&&savedCorpus==corpus.plan&&saved.plan.contains("printingContext"),
            "full fingerprinted corpus and printing context persisted before first work");
    };
    auto wrongRange=corpus.parts;wrongRange[0].partNumber="not-the-saved-Part";
    ok&=check(!BatchPrintableModelService().run(wrongRange,options).ok,"mismatched selected identity refuses before output or geometry");
    const auto run=BatchPrintableModelService().run(corpus.parts,options);
    const auto context=QJsonDocument::fromJson(read(run.metadataPath)).object().value("printingContext").toObject();
    ok&=check(context.value("selection")=="automatic"&&!context.value("fingerprint").toString().isEmpty()&&
        QJsonDocument::fromJson(read(QDir(run.runDirectory).filePath("summary.json"))).object().value("printingContext")==context,
        "printing context persisted identically in metadata and summary");
    auto mismatch=options;mismatch.beforePart={};mismatch.runId="context-conflict";
    mismatch.corpusPlan.insert("printingContext",context);
    mismatch.printOrientation.rotate(PrintOrientation::Rotation::XPositive);
    const auto refused=BatchPrintableModelService().run(corpus.parts,mismatch);
    ok&=check(!refused.ok&&refused.diagnostic.contains("printing context differs")&&refused.runDirectory.isEmpty(),
        "saved-plan orientation conflict refuses before output or geometry");
    mismatch=options;mismatch.runId="missing-profile";mismatch.selectedFitProfileIdentity="missing";
    mismatch.fitLibraryRoot=temp.filePath("empty-profile-library");
    const auto invalid=BatchPrintableModelService().run(corpus.parts,mismatch);
    ok&=check(!invalid.ok&&invalid.runDirectory.isEmpty(),"invalid explicit profile cannot force fit or create output");
    FitProfile selected;selected.profileIdentity="phase-a-profile";selected.name="Synthetic printer / PETG";
    selected.sourceSessionIdentity="verified-test-session";selected.process.printerIdentity="Synthetic printer";
    selected.process.materialIdentity="PETG";selected.process.hasNozzleDiameter=true;
    selected.process.nozzleDiameterMillimetres=.4;selected.process.hasLayerHeight=true;
    selected.process.layerHeightMillimetres=.2;selected.process.profileName="Synthetic process";
    selected.process.actualPrintedOrientation=FitPrintedOrientation::FeatureAxisPerpendicularToBuildPlate;
    selected.processFingerprint=FitCalibrationLibrary::processFingerprint(selected.process);
    FitProfileCorrection correction;correction.featureFamily="RoundTechnicPassage";correction.featureRole="female";
    correction.printedOrientation="feature-axis-perpendicular-to-build-plate";correction.valueMillimetres=.2;
    correction.semanticContractVersion=FitCalibrationLibrary::currentSemanticContractVersion();
    correction.calibrationArtifactIdentity="verified-synthetic-fixture";selected.corrections.append(correction);
    FitCalibrationLibrary profiles(temp.filePath("managed-profiles"));QString profileError;
    ok&=check(profiles.saveProfile(&selected,&profileError),"synthetic managed Verified profile persists");
    auto explicitOptions=options;explicitOptions.beforePart={};explicitOptions.runId="explicit-profile";
    explicitOptions.fitLibraryRoot=profiles.storageRoot();explicitOptions.selectedFitProfileIdentity=selected.profileIdentity;
    const auto selectedRun=BatchPrintableModelService().run(corpus.parts,explicitOptions);
    const auto selectedContext=QJsonDocument::fromJson(read(selectedRun.metadataPath)).object().value("printingContext").toObject();
    ok&=check(selectedRun.ok&&selectedContext.value("selectedProfileIdentity")==selected.profileIdentity&&
        selectedContext.value("selectedProfileName")==selected.name&&selectedContext.value("profiles").toArray().size()==1,
        "explicit managed profile and complete process snapshot persist without requiring compatible geometry");
    const auto bound=BatchPrintableModelService::readPartReferencePlan(QDir(selectedRun.runDirectory).filePath("part-reference-plan.json"),temp.path());
    explicitOptions.corpusPlan=bound.plan;explicitOptions.runId="changed-profile";
    selected.corrections[0].valueMillimetres=.3;
    ok&=check(profiles.saveProfile(&selected,&profileError),"synthetic profile revision persists");
    const auto revised=BatchPrintableModelService().run(corpus.parts,explicitOptions);
    ok&=check(!revised.ok&&revised.diagnostic.contains("printing context differs")&&revised.runDirectory.isEmpty(),
        "same profile identity with changed evidence refuses continuation");
    ok&=check(QDir(QDir(run.runDirectory).filePath("exports/success")).exists()&&
        QDir(QDir(run.runDirectory).filePath("exports/diagnostic")).exists(),"success and diagnostic exports separated");
    ok&=check(run.ok&&started==3&&run.results.size()==3&&run.results[1].modelAvailable&&
        run.results[1].category!=BatchPrintCategory::SkippedNoColor&&run.results[1].category!=BatchPrintCategory::SkippedNonstandardId&&
        run.results[1].category!=BatchPrintCategory::SkippedStickerCategory&&run.results[2].category==BatchPrintCategory::NoLDrawModel,
        "entire corpus attempted regardless of Random exclusions");
    ok&=check(run.totals.modelAvailable==2&&run.totals.noModel==1&&run.totals.eligible==3&&
        read(run.csvPath).contains("part_reference_memberships,model_state,eligibility_notes")&&
        read(run.csvPath).contains("model_unavailable"),"CSV context and model-bearing denominator");
    ok&=check(QJsonDocument::fromJson(read(QDir(run.runDirectory).filePath("failure-review.json"))).object()
        .value("no_model").toArray().size()==1,"machine-readable no-model list");
    QVector<BatchPrintResult> rows(3);
    for(int i=0;i<3;++i){rows[i].sequence=i+1;rows[i].modelAvailabilityKnown=true;rows[i].modelAvailable=i<2;}
    rows[0].category=rows[0].nativeCategory=BatchPrintCategory::Success;
    rows[1].nativeCategory=BatchPrintCategory::SourceCoverage;rows[1].category=BatchPrintCategory::UserOverrideSuccess;
    rows[2].category=rows[2].nativeCategory=BatchPrintCategory::NoLDrawModel;
    auto fittedRows=rows;fittedRows[0].reopened=true;fittedRows[0].nominalPreparedReady=true;
    fittedRows[0].sourceRecognizedFeatureCount=2;fittedRows[0].sourceApplicableVerifiedFeatureCount=1;
    fittedRows[0].recognizedFeatureCount=2;fittedRows[0].applicableVerifiedFeatureCount=1;
    fittedRows[0].correctedFeatureCount=1;fittedRows[0].verifiedZeroFeatureCount=1;fittedRows[0].partialFitCoverage=true;
    fittedRows[1].category=BatchPrintCategory::ManufacturingFailed;fittedRows[1].nominalPreparedReady=true;
    fittedRows[1].sourceRecognizedFeatureCount=1;fittedRows[1].sourceApplicableVerifiedFeatureCount=1;
    fittedRows[1].applicableVerifiedFeatureCount=1;fittedRows[1].fitStatus="fitted_manufacturing_failed";
    const auto funnel=BatchPrintableModelService::summarizeFit(fittedRows);
    ok&=check(funnel.value("fittedPrintSuccess").toObject().value("numerator").toInt()==1&&
        funnel.value("fittedPrintSuccess").toObject().value("denominator").toInt()==2&&
        funnel.value("verifiedZeroSuccess").toInt()==1&&funnel.value("partialFittedSuccess").toInt()==1&&
        funnel.value("fittedManufacturingFailures").toInt()==1&&funnel.value("nominalPreparedReady").toInt()==2,
        "funnel separates Verified-zero partial success, nominal readiness and failed ManufacturingMesh");
    const auto summary=BatchPrintableModelService::summarizePartReference(corpus.parts,rows);
    ok&=check(summary.value("nativeSuccesses").toInt()==1&&summary.value("nativeSuccessPercent").toDouble()==50&&
        summary.value("overrideRecoveries").toInt()==1&&summary.value("practicalSuccessPercent").toDouble()==100&&
        summary.value("sourceCoverageFailures").toInt()==1,"native/practical metrics retain underlying native failure");
    const auto groups=summary.value("catalogs").toArray();
    ok&=check(groups.size()==2&&groups[0].toObject().value("uniqueParts").toInt()==2&&
        groups[1].toObject().value("uniqueParts").toInt()==2&&groups[1].toObject().value("modelBearingParts").toInt()==1,
        "descriptive per-catalog memberships overlap without global duplication");
    options.runId="interrupted";QString interruptedPath;
    options.beforePart=[&](const QString& path,int sequence,const QString&){interruptedPath=path;if(sequence==2)throw std::runtime_error("simulated interruption");};
    try{BatchPrintableModelService().run(corpus.parts,options);ok=false;}catch(const std::runtime_error&){}
    const auto state=QJsonDocument::fromJson(read(interruptedPath)).object();
    ok&=check(state.value("completedCount").toInt()==1&&state.value("currentSampleSequence").toInt()==2&&
        read(QFileInfo(interruptedPath).dir().filePath("results.csv")).contains("\"1100\""),"completed CSV survives interruption with active next Part");
    const auto frozen=BatchPrintableModelService::readPartReferencePlan(QFileInfo(interruptedPath).dir().filePath("part-reference-plan.json"),temp.path());
    const auto historicalState=read(interruptedPath);
    const auto historicalCsv=read(QFileInfo(interruptedPath).dir().filePath("results.csv"));
    options.beforePart={};options.runId="continuation-chunk";options.corpusPlan=frozen.plan;
    const auto chunk=BatchPrintableModelService().run(frozen.parts.mid(1),options);
    ok&=check(chunk.ok&&chunk.results.size()==2&&chunk.results[0].partNumber==b.partNumber&&
        chunk.referenceSummary.value("uniqueParts").toInt()==2,"saved-plan chunk preserves order and measures only selected range");
    const auto continuedState=QJsonDocument::fromJson(read(QDir(chunk.runDirectory).filePath("run-state.json"))).object();
    ok&=check(read(interruptedPath)==historicalState&&read(QFileInfo(interruptedPath).dir().filePath("results.csv"))==historicalCsv&&
        continuedState.value("corpusFingerprint")==state.value("corpusFingerprint")&&
        continuedState.value("firstReferenceSequence").toInt()==2,
        "continuation preserves original evidence and fingerprint, starts at interrupted global sequence");
    auto damaged=corpus.plan;damaged.insert("uniquePartCount",99);const auto damagedPath=temp.filePath("damaged.json");
    write(damagedPath,QJsonDocument(damaged).toJson());
    ok&=check(!BatchPrintableModelService::readPartReferencePlan(damagedPath,temp.path()).ok(),"damaged plan refuses continuation");
    const auto random=BatchPrintableModelService::randomSample({a,b,c},3,123,true,nullptr,true,temp.path());
    ok&=check(random.size()==1&&random.front().partId==1,"Random Sample retains existing prefilters");
    auto unavailable=first;unavailable.partNumber="unavailable-canonical";
    const auto unavailableCorpus=BatchPrintableModelService::partReferenceCorpus({unavailable},{},temp.path());
    options.runId="unresolved-catalog";options.corpusPlan=unavailableCorpus.plan;
    const auto unavailableRun=BatchPrintableModelService().run(unavailableCorpus.parts,options);
    ok&=check(unavailableRun.ok&&unavailableRun.results.size()==1&&
        unavailableRun.results[0].category==BatchPrintCategory::NoCatalogPart&&
        unavailableRun.results[0].diagnostic.contains("not found in the active canonical catalog")&&
        unavailableRun.totals.noModel==1,"unresolved catalog identity retained with independent no-model accounting");
    options.partReference=false;options.corpusPlan={};options.runId="explicit";
    const auto explicitRun=BatchPrintableModelService().run(BatchPrintableModelService::explicitParts({a,b,c},{c.partNumber}),options);
    ok&=check(explicitRun.ok&&explicitRun.results.size()==1&&explicitRun.results[0].category==BatchPrintCategory::NoLDrawModel,
        "Part List still reports explicitly requested missing model");
    // Checkpoint failure after a durable CSV row: that row is ambiguous until
    // completedCount commits. Rerun it in a separate saved-plan continuation.
    for(bool persistent:{false,true}){
        BatchPrintOptions retry;retry.libraryRoot=temp.path();retry.outputRoot=temp.filePath("retry-runs");
        retry.runId=persistent?"persistent":"transient";retry.partReference=true;retry.corpusPlan=corpus.plan;
        retry.autoFitEnabled=false;retry.localOverrideRoot=temp.filePath("overrides");
        int begun=0,attempts=0;QByteArray beforeReplacement;
        BatchPrintRunState::TestHooks hooks;hooks.wait=[](unsigned long){};
        hooks.replacementError=[&](const QString& path,const QJsonObject& proposed,int attempt){
            if(proposed.value("currentSampleSequence").toInt()!=2||proposed.value("currentPhase")!="run_state"||
               proposed.value("completedCount").toInt()!=1)return 0;
            if(attempt==1)beforeReplacement=read(path);
            attempts=attempt;
            ok&=check(begun==2&&read(path)==beforeReplacement,"next Part cannot begin during replacement retries");
            return persistent||attempt<3?sharingError():0;
        };
        retry.runState=std::make_shared<BatchPrintRunState>(hooks);
        retry.beforePart=[&](const QString& path,int sequence,const QString&){
            ++begun;const auto s=QJsonDocument::fromJson(read(path)).object();
            ok&=check(s.value("completedCount").toInt()==sequence-1,"every next Part follows durable completion");
        };
        const auto result=BatchPrintableModelService().run(corpus.parts,retry);
        const auto savedState=QJsonDocument::fromJson(read(result.statePath)).object();
        if(!persistent){
            ok&=check(result.ok&&begun==3&&attempts==3&&savedState.value("status")=="completed",
                "transient error recovers and completes without skipped Parts");
            continue;
        }
        ok&=check(!result.ok&&!result.stopped&&begun==2&&attempts==6&&savedState.value("status")=="running"&&
            savedState.value("completedCount").toInt()==1&&savedState.value("currentReferenceSequence").toInt()==2&&
            result.diagnostic.contains("atomic replace"),"persistent failure stops with last valid active checkpoint, never claims clean completion");
        const auto oldState=read(result.statePath),oldCsv=read(result.csvPath);
        ok&=check(oldCsv.split('\n').size()==4,"header and two flushed rows survive checkpoint interruption");
        const auto savedPlan=BatchPrintableModelService::readPartReferencePlan(QDir(result.runDirectory).filePath("part-reference-plan.json"),temp.path());
        const int next=savedState.value("firstReferenceSequence").toInt()+savedState.value("completedCount").toInt();
        retry.runId="ambiguous-continuation";retry.runState={};retry.beforePart={};retry.corpusPlan=savedPlan.plan;
        const auto resumed=BatchPrintableModelService().run(savedPlan.parts.mid(next-1),retry);
        ok&=check(next==2&&resumed.ok&&resumed.results.size()==2&&resumed.results[0].partNumber==b.partNumber&&
            resumed.results[1].partNumber==c.partNumber,"ambiguous row rerun, durable first row not duplicated, final Part not skipped");
        ok&=check(oldState==read(result.statePath)&&oldCsv==read(result.csvPath),"continuation leaves failed-run evidence unchanged");
    }
    return ok?0:1;
}
