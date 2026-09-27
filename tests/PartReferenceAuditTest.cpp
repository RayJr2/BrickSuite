#include "../src/services/geometry/print/BatchPrintableModelService.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <cstdio>
#include <stdexcept>
using namespace PrintGeometry;
namespace {
bool check(bool value,const char* text){if(!value)fprintf(stderr,"FAIL: %s\n",text);return value;}
QByteArray read(const QString& path){QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
bool write(const QString& path,const QByteArray& data){QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(data)==data.size();}
}
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);QTemporaryDir temp;bool ok=true;
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
        ok&=check(saved.ok()&&saved.plan==corpus.plan,"full fingerprinted corpus persisted before first work");
    };
    auto wrongRange=corpus.parts;wrongRange[0].partNumber="not-the-saved-Part";
    ok&=check(!BatchPrintableModelService().run(wrongRange,options).ok,"mismatched selected identity refuses before output or geometry");
    const auto run=BatchPrintableModelService().run(corpus.parts,options);
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
    options.beforePart={};options.runId="continuation-chunk";options.corpusPlan=frozen.plan;
    const auto chunk=BatchPrintableModelService().run(frozen.parts.mid(1),options);
    ok&=check(chunk.ok&&chunk.results.size()==2&&chunk.results[0].partNumber==b.partNumber&&
        chunk.referenceSummary.value("uniqueParts").toInt()==2,"saved-plan chunk preserves order and measures only selected range");
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
    return ok?0:1;
}
