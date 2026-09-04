#include "../src/database/DatabaseManager.h"
#include "../src/repositories/ExternalPartIdentifierRepository.h"
#include "../src/repositories/ExternalPartMappingRepository.h"
#include "../src/repositories/PartRepository.h"
#include "../src/services/images/PartImageService.h"
#include "../src/services/parts/PartExternalIdEnrichmentService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>

namespace {
bool require(bool condition, const QString& message) { if (!condition) qCritical().noquote() << message; return condition; }
void events() { QCoreApplication::sendPostedEvents(); QCoreApplication::processEvents(); }
QString status(QSqlDatabase db, int id) { QSqlQuery q(db); q.prepare("SELECT status FROM external_part_identifier_lookup WHERE part_id=:id AND source='Rebrickable'"); q.bindValue(":id",id); return q.exec()&&q.next()?q.value(0).toString():QString(); }
class Cleanup { QString path; public: explicit Cleanup(QString p):path(std::move(p)){} ~Cleanup(){DatabaseManager::instance().close();QDir(path).removeRecursively();} };
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("RFStateSideTests");
    QCoreApplication::setApplicationName("ExternalIds_"+QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString data=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation); Cleanup cleanup(data);
    if (!require(DatabaseManager::instance().initialize(),"Database initialization failed.")) return 1;
    QSqlDatabase db=DatabaseManager::instance().database(); QSqlQuery q(db); const QString t="2026-01-01T00:00:00.000Z";
    if (!require(q.exec("INSERT INTO part(part_number,name,is_active,created_utc,modified_utc,material) VALUES"
                        "('normal','Normal',1,'"+t+"','"+t+"','Plastic'),"
                        "('15625pr0015','Printed',1,'"+t+"','"+t+"','Plastic'),"
                        "('loaded','Loaded',1,'"+t+"','"+t+"','Plastic'),"
                        "('missing','Missing',1,'"+t+"','"+t+"','Plastic'),"
                        "('failure','Failure',1,'"+t+"','"+t+"','Plastic'),"
                        "('cached','Cached',1,'"+t+"','"+t+"','Plastic'),"
                        "('coloronly','Color only',1,'"+t+"','"+t+"','Plastic'),"
                        "('3001pr0001','Printed fallback',1,'"+t+"','"+t+"','Plastic'),"
                        "('3001pr0002','Missing printed',1,'"+t+"','"+t+"','Plastic'),"
                        "('3001pr0003','Transient printed',1,'"+t+"','"+t+"','Plastic')"),"Part seed failed.")) return 1;
    PartRepository parts; const int normal=parts.getByPartNumber("normal")->id(); const int printed=parts.getByPartNumber("15625pr0015")->id();
    const int loaded=parts.getByPartNumber("loaded")->id(); const int missing=parts.getByPartNumber("missing")->id(); const int failure=parts.getByPartNumber("failure")->id();
    const int printedFallback=parts.getByPartNumber("3001pr0001")->id();
    const int missingPrinted=parts.getByPartNumber("3001pr0002")->id();
    const int transientPrinted=parts.getByPartNumber("3001pr0003")->id();
    ExternalPartIdentifierRepository ids; ids.setLookupStatus(loaded,"Rebrickable","Loaded");

    QStringList batches; QStringList details;
    QHash<int, PartExternalIdEnrichmentService::LookupOutcome> outcomes;
    {
        PartExternalIdEnrichmentService service(nullptr,false);
        QObject::connect(&service,&PartExternalIdEnrichmentService::batchRequested,[&](const QStringList& value){batches.append(value);});
        QObject::connect(&service,&PartExternalIdEnrichmentService::detailsRequested,[&](const QString& value){details.append(value);});
        QObject::connect(&service,&PartExternalIdEnrichmentService::externalIdsLookupFinished,
                         [&](int partId, PartExternalIdEnrichmentService::LookupOutcome outcome){outcomes.insert(partId,outcome);});

        service.ensureExternalIds(normal); service.ensureExternalIds(normal); service.ensureExternalIds(loaded); events();
        if (!require(batches.count("normal")==1 && !batches.contains("loaded"),"Deduplication or Loaded skip failed.")) return 1;
        RebrickableService::PartImageUrlsResult response; response.success=true; response.requestedPartNumbers={"normal"};
        RebrickableService::PartImageUrl item; item.partNumber="normal"; item.partImageUrl="https://example/normal.png";
        item.externalIds={{"BrickLink",{"bl-normal"}},{"BrickOwl",{"bo-1"}},{"LDraw",{"ld-1","ld-2"}},{"LEGO",{"lego-1"}},{"FutureProvider",{"future-1"}}}; response.parts={item};
        service.handleBatchResult(response);
        if (!require(status(db,normal)=="Loaded","Successful persistence was not marked Loaded.")) return 1;
        if (!require(outcomes.value(normal)==PartExternalIdEnrichmentService::LookupOutcome::Loaded,
                     "Loaded completion outcome was not emitted.")) return 1;
        for (const QString provider : {"BrickLink","BrickOwl","LDraw","LEGO","FutureProvider"})
            if (!require(!ids.findByProviderAndExternalId(provider, provider=="BrickLink"?"bl-normal":provider=="BrickOwl"?"bo-1":provider=="LDraw"?"ld-1":provider=="LEGO"?"lego-1":"future-1",true).isEmpty(),"Provider was not retained: "+provider)) return 1;
        if (!require(ids.findByProviderAndExternalId("LDraw","ld-2",true).size()==1,"Multiple provider IDs were not retained.")) return 1;
        const auto projected=ExternalPartMappingRepository().getByPartAndProvider(normal,"BrickLink");
        if (!require(projected && projected->externalId=="bl-normal","Unique BrickLink projection failed.")) return 1;

        service.ensureExternalIds(printed); events();
        RebrickableService::PartImageUrlsResult printedResponse; printedResponse.success=true; printedResponse.requestedPartNumbers={"15625pr0015"};
        RebrickableService::PartImageUrl printedItem; printedItem.partNumber="15625pr0015"; printedItem.externalIds={{"BrickLink",{"15625pb025","other"}}}; printedResponse.parts={printedItem};
        service.handleBatchResult(printedResponse);
        if (!require(!ExternalPartMappingRepository().getByPartAndProvider(printed,"BrickLink"),"Multiple BrickLink IDs were guessed.")) return 1;
        if (!require(ids.findByProviderAndExternalId("BrickLink","15625pb025",true).size()==1,"Known decorated mapping was not authoritative local data.")) return 1;

        service.ensureExternalIds(printedFallback); events();
        RebrickableService::PartImageUrlsResult printedAbsent; printedAbsent.success=true;
        printedAbsent.requestedPartNumbers={"3001pr0001"}; service.handleBatchResult(printedAbsent);
        if (!require(details.count("3001pr0001")==1,"Printed batch omission did not use the bounded direct fallback.")) return 1;
        const int directBefore=batches.size(); service.ensureExternalIds(printedFallback); events();
        if (!require(batches.size()==directBefore,"A pending printed fallback was queued again.")) return 1;
        RebrickableService::PartDetailsResult direct; direct.success=true; direct.requestedPartNumber="3001pr0001";
        direct.part.partNumber="3001pr0001"; direct.part.externalIds={{"BrickLink",{"3001pb0001"}}};
        service.handleDetailsResult(direct);
        if (!require(status(db,printedFallback)=="Loaded", "Successful direct fallback was not persisted.")) return 1;

        service.ensureExternalIds(missingPrinted); service.ensureExternalIds(transientPrinted); events();
        RebrickableService::PartImageUrlsResult absentPrinted; absentPrinted.success=true;
        absentPrinted.requestedPartNumbers={"3001pr0002","3001pr0003"}; service.handleBatchResult(absentPrinted);
        RebrickableService::PartDetailsResult notFound; notFound.requestedPartNumber="3001pr0002"; notFound.httpStatusCode=404;
        service.handleDetailsResult(notFound);
        if (!require(status(db,missingPrinted)=="Unavailable", "A definitive direct 404 was not persisted.")) return 1;
        if (!require(outcomes.value(missingPrinted)==PartExternalIdEnrichmentService::LookupOutcome::Unavailable,
                     "Unavailable completion outcome was not emitted.")) return 1;
        RebrickableService::PartDetailsResult directTransient; directTransient.requestedPartNumber="3001pr0003"; directTransient.httpStatusCode=500;
        service.handleDetailsResult(directTransient);
        if (!require(outcomes.value(transientPrinted)==PartExternalIdEnrichmentService::LookupOutcome::RetryableFailure,
                     "Retryable completion outcome was not emitted.")) return 1;
        const int directRetryBefore=batches.size(); service.ensureExternalIds(transientPrinted); events();
        if (!require(status(db,transientPrinted).isEmpty() && batches.size()==directRetryBefore+1,
                     "A transient direct failure was not left retryable.")) return 1;

        service.ensureExternalIds(missing); events();
        RebrickableService::PartImageUrlsResult absent; absent.success=true; absent.requestedPartNumbers={"missing"}; service.handleBatchResult(absent);
        if (!require(status(db,missing)=="Unavailable","Definitive batch omission was not marked Unavailable.")) return 1;
        const int before=batches.size(); service.ensureExternalIds(missing); events();
        if (!require(batches.size()==before,"Unavailable Part was requested again.")) return 1;

        service.ensureExternalIds(failure); events(); RebrickableService::PartImageUrlsResult transient; transient.requestedPartNumbers={"failure"}; service.handleBatchResult(transient);
        const int retryBefore=batches.size(); service.ensureExternalIds(failure); events();
        if (!require(status(db,failure).isEmpty() && batches.size()==retryBefore+1,"Transient failure was not retryable.")) return 1;

        QDir().mkpath(QDir(data).filePath("cache/parts")); QFile cached(QDir(data).filePath("cache/parts/cached.jpg"));
        if (!require(cached.open(QIODevice::WriteOnly) && cached.write("image") == 5,
                     "Unable to create the general image cache fixture.")) return 1;
        cached.close();
        PartImageService images; const int cachedBefore=batches.size(); images.requestPartImage("cached",QString()); events();
        if (!require(batches.size()==cachedBefore+1 && batches.contains("cached"),"A general image cache hit did not trigger enrichment.")) return 1;
        QDir().mkpath(QDir(data).filePath("cache/parts/colors")); QFile color(QDir(data).filePath("cache/parts/colors/coloronly_1.jpg"));
        if (!require(color.open(QIODevice::WriteOnly) && color.write("image") == 5,
                     "Unable to create the color image cache fixture.")) return 1;
        color.close();
        const int colorBefore=batches.size(); images.requestPartColorImage("coloronly",1,QString()); events();
        if (!require(batches.size()==colorBefore,"Color-specific image triggered external-ID enrichment.")) return 1;

        if (!require(q.exec("CREATE TRIGGER fail_external_id BEFORE INSERT ON external_part_identifier WHEN NEW.provider='Bad' BEGIN SELECT RAISE(ABORT,'forced'); END"),"Failure trigger failed.")) return 1;
        const int failedId=parts.getByPartNumber("failure")->id();
        if (!require(!service.persistExternalIds(failedId,{{"Bad",{"x"}}}) && status(db,failedId).isEmpty(),"Persistence failure marked enrichment complete.")) return 1;
    }
    batches.clear(); { PartExternalIdEnrichmentService restarted(nullptr,false); QObject::connect(&restarted,&PartExternalIdEnrichmentService::batchRequested,[&](const QStringList& v){batches.append(v);}); restarted.ensureExternalIds(normal); events(); }
    if (!require(batches.isEmpty(),"Loaded status did not survive service restart.")) return 1;
    qInfo() << "Background Part external-ID enrichment validation passed.";
    return 0;
}
