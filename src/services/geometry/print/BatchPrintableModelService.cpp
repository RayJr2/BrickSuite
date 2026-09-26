#include "BatchPrintableModelService.h"

#include "AutoFitProfileResolver.h"
#include "LDrawPrintPreparationService.h"
#include "ManufacturingMeshService.h"
#include "LocalPrintableOverrideService.h"
#include "../LDrawLibraryService.h"
#include "../ThreeMfWriter.h"

#include <lib3mf_implicit.hpp>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QMutexLocker>
#ifdef Q_OS_WIN
#include <io.h>
#else
#include <unistd.h>
#endif
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <random>

namespace PrintGeometry {
namespace {
bool durableFlush(QFileDevice& file)
{
    if(!file.flush())return false;
#ifdef Q_OS_WIN
    return ::_commit(file.handle())==0;
#else
    return ::fsync(file.handle())==0;
#endif
}

QString csv(QString value)
{
    value.replace('"',QStringLiteral("\"\""));
    return QStringLiteral("\"")+value+QStringLiteral("\"");
}

bool installed(const QString& root,QString candidate,const QHash<QString,QString>* installedNames = nullptr)
{
    candidate=candidate.trimmed();
    if(candidate.startsWith(QStringLiteral("parts/"),Qt::CaseInsensitive))candidate=candidate.mid(6);
    if(candidate.endsWith(QStringLiteral(".dat"),Qt::CaseInsensitive))candidate.chop(4);
    if(candidate.isEmpty()||candidate.contains('/')||candidate.contains('\\')||
       candidate==QStringLiteral(".")||candidate==QStringLiteral(".."))return false;
    const QDir parts(QDir(root).filePath(QStringLiteral("parts")));
    const auto usable=[](const QString& path){
        QFile file(path);return QFileInfo(path).isFile()&&file.open(QIODevice::ReadOnly)&&file.size()>0;
    };
    if(installedNames){
        const auto name=installedNames->value((candidate+QStringLiteral(".dat")).toCaseFolded());
        return !name.isEmpty()&&usable(parts.filePath(name));
    }
    if(usable(parts.filePath(candidate+QStringLiteral(".dat"))))return true;
    for(const auto&name:parts.entryList(QDir::Files))
        if(name.compare(candidate+QStringLiteral(".dat"),Qt::CaseInsensitive)==0&&usable(parts.filePath(name)))return true;
    return false;
}

QString modelFor(const BatchPrintablePart& part,const QString& root,
                 const QHash<QString,QString>* installedNames = nullptr)
{
    QStringList candidates=part.ldrawCandidates;
    if(!candidates.contains(part.partNumber,Qt::CaseInsensitive))candidates.prepend(part.partNumber);
    for(const auto&candidate:candidates)
        if(candidate.compare(part.partNumber,Qt::CaseInsensitive)==0&&installed(root,candidate,installedNames))
            return candidate;
    for(const auto&candidate:candidates)if(installed(root,candidate,installedNames))return candidate;
    return {};
}

BatchPrintCategory preparationCategory(const PrintPreparationResult& prepared)
{
    switch(prepared.error){
    case PrintPreparationError::ResourceLimitExceeded:return BatchPrintCategory::ResourceLimit;
    case PrintPreparationError::Cancelled:return BatchPrintCategory::Cancelled;
    case PrintPreparationError::FinalValidationFailed:
    case PrintPreparationError::BooleanResultInvalid:return BatchPrintCategory::ManifoldFailure;
    default:break;
    }
    if(!prepared.sourceCoverage.complete()&&prepared.sourceCoverage.groupedTriangleCount>0)
        return BatchPrintCategory::SourceCoverage;
    if(prepared.diagnostic.contains(QStringLiteral("self-intersect"),Qt::CaseInsensitive))
        return BatchPrintCategory::SelfIntersection;
    return BatchPrintCategory::PrepareNotReady;
}

bool reopen(const QString& path,std::size_t expectedFaces,QString* error)
{
    try{
        Lib3MF::CWrapper wrapper;auto model=wrapper.CreateModel();
        model->QueryReader("3mf")->ReadFromFile(path.toStdString());
        auto meshes=model->GetMeshObjects();
        if(!meshes->MoveNext()||meshes->GetCurrentMeshObject()->GetTriangleCount()!=expectedFaces){
            if(error)*error=QStringLiteral("Reopened 3MF has no mesh or an unexpected face count.");
            return false;
        }
        return true;
    }catch(const std::exception& exception){
        if(error)*error=QString::fromUtf8(exception.what());return false;
    }
}

QString safeFileName(QString value)
{
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")),QStringLiteral("_"));
    return value.left(80);
}

QString familyName(FunctionalInterfaceFamily family)
{
    switch(family){
    case FunctionalInterfaceFamily::RoundTechnicPassage:return QStringLiteral("RoundTechnicPassage");
    case FunctionalInterfaceFamily::StandardStud:return QStringLiteral("StandardStud");
    case FunctionalInterfaceFamily::StudReceivingClutch:return QStringLiteral("StudReceivingClutch");
    case FunctionalInterfaceFamily::FrictionlessTechnicPin:return QStringLiteral("FrictionlessTechnicPin");
    case FunctionalInterfaceFamily::FrictionTechnicPin:return QStringLiteral("FrictionTechnicPin");
    case FunctionalInterfaceFamily::TechnicAxle:return QStringLiteral("TechnicAxle");
    case FunctionalInterfaceFamily::TechnicAxleHole:return QStringLiteral("TechnicAxleHole");
    case FunctionalInterfaceFamily::StandardBar:return QStringLiteral("StandardBar");
    case FunctionalInterfaceFamily::CClipBarReceiver:return QStringLiteral("CClipBarReceiver");
    case FunctionalInterfaceFamily::BallJoint:return QStringLiteral("BallJoint");
    case FunctionalInterfaceFamily::BallSocket:return QStringLiteral("BallSocket");
    case FunctionalInterfaceFamily::PinBarrelHinge:return QStringLiteral("PinBarrelHinge");
    case FunctionalInterfaceFamily::InterleavedFingerHinge:return QStringLiteral("InterleavedFingerHinge");
    case FunctionalInterfaceFamily::ClickHinge:return QStringLiteral("ClickHinge");
    case FunctionalInterfaceFamily::RetainedRotatingWheel:return QStringLiteral("RetainedRotatingWheel");
    case FunctionalInterfaceFamily::PlainRoundBoreWheel:return QStringLiteral("PlainRoundBoreWheel");
    }
    return QStringLiteral("Unknown");
}
}

bool BatchPrintRunState::write(QString* error)
{
    if(m_path.isEmpty())return true;
    QSaveFile file(m_path);
    const auto bytes=QJsonDocument(m_state).toJson(QJsonDocument::Indented);
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||
       !durableFlush(file)||!file.commit()){
        if(error)*error=QStringLiteral("Could not persist audit run state: %1").arg(file.errorString());
        return false;
    }
    return true;
}

bool BatchPrintRunState::save(const QString& path,const QJsonObject& state,QString* error)
{
    QMutexLocker lock(&m_mutex);m_path=path;m_state=state;
    m_state.insert(QStringLiteral("stopRequested"),m_stopRequested);
    if(m_stopRequested&&m_state.value(QStringLiteral("status"))==QStringLiteral("running"))
        m_state.insert(QStringLiteral("status"),QStringLiteral("stopping"));
    return write(error);
}

bool BatchPrintRunState::requestStop(QString* error)
{
    QMutexLocker lock(&m_mutex);m_stopRequested=true;
    m_state.insert(QStringLiteral("stopRequested"),true);
    if(m_state.value(QStringLiteral("status"))==QStringLiteral("running"))
        m_state.insert(QStringLiteral("status"),QStringLiteral("stopping"));
    return write(error);
}

double BatchPrintTotals::modelAvailabilityPercent() const
{return eligible?100.0*modelAvailable/eligible:0.0;}
double BatchPrintTotals::printServicePercent() const
{return modelAvailable?100.0*nativeSuccessful/modelAvailable:0.0;}
double BatchPrintTotals::practicalPrintablePercent() const
{return modelAvailable?100.0*successful/modelAvailable:0.0;}
double BatchPrintTotals::catalogToPrintablePercent() const
{return eligible?100.0*successful/eligible:0.0;}

QString BatchPrintableModelService::categoryCode(BatchPrintCategory category)
{
    switch(category){
    case BatchPrintCategory::Success:return QStringLiteral("native_success");
    case BatchPrintCategory::StrictOverrideSuccess:return QStringLiteral("strict_override_success");
    case BatchPrintCategory::UserOverrideSuccess:return QStringLiteral("user_override_success");
    case BatchPrintCategory::SkippedNoColor:return QStringLiteral("skipped_no_color");
    case BatchPrintCategory::SkippedStickerCategory:return QStringLiteral("skipped_sticker_category");
    case BatchPrintCategory::SkippedNonstandardId:return QStringLiteral("skipped_nonstandard_id");
    case BatchPrintCategory::NoCatalogPart:return QStringLiteral("no_catalog_part");
    case BatchPrintCategory::NoLDrawModel:return QStringLiteral("no_ldraw_model");
    case BatchPrintCategory::LoadFailed:return QStringLiteral("load_failed");
    case BatchPrintCategory::PrepareNotReady:return QStringLiteral("prepare_not_ready");
    case BatchPrintCategory::ResourceLimit:return QStringLiteral("resource_limit");
    case BatchPrintCategory::SourceCoverage:return QStringLiteral("source_coverage");
    case BatchPrintCategory::ManifoldFailure:return QStringLiteral("manifold_failure");
    case BatchPrintCategory::SelfIntersection:return QStringLiteral("self_intersection");
    case BatchPrintCategory::ManufacturingFailed:return QStringLiteral("manufacturing_failed");
    case BatchPrintCategory::ExportFailed:return QStringLiteral("export_failed");
    case BatchPrintCategory::ReopenFailed:return QStringLiteral("reopen_failed");
    case BatchPrintCategory::Cancelled:return QStringLiteral("cancelled");
    case BatchPrintCategory::NotStarted:return QStringLiteral("not_started");
    }
    return QStringLiteral("unknown");
}

QVector<BatchPrintablePart> BatchPrintableModelService::loadCatalog(const QString& databasePath,QString* error)
{
    QVector<BatchPrintablePart> result;
    const QString connection=QStringLiteral("batch-print-catalog-")+QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        auto database=QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),connection);
        database.setDatabaseName(databasePath);
        database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if(!database.open()){
            if(error)*error=database.lastError().text();
        }else{
            QSqlQuery query(database);
            if(!query.exec(QStringLiteral("SELECT p.id,p.part_number,COALESCE(p.material,''),"
                "p.part_category_id,c.rebrickable_id,COALESCE(c.name,''),e.external_id FROM part p "
                "LEFT JOIN part_category c ON c.id=p.part_category_id "
                "LEFT JOIN external_part_identifier e ON e.part_id=p.id AND e.is_active=1 "
                "AND e.provider='LDraw' COLLATE NOCASE WHERE p.is_active=1 "
                "ORDER BY p.part_number COLLATE NOCASE,e.external_id COLLATE NOCASE"))){
                if(error)*error=query.lastError().text();
            }else{
                int lastId=-1;
                QHash<int,int> partPositions;
                while(query.next()){
                    const int id=query.value(0).toInt();
                    if(id!=lastId){
                        BatchPrintablePart part;part.partId=id;part.partNumber=query.value(1).toString();
                        part.material=query.value(2).toString();
                        part.categoryId=query.value(3).toInt();
                        part.rebrickableCategoryId=query.value(4).toInt();
                        part.category=query.value(5).toString();
                        part.catalogPresent=true;
                        partPositions.insert(id,result.size());result.push_back(std::move(part));lastId=id;
                    }
                    const auto candidate=query.value(6).toString().trimmed();
                    if(!candidate.isEmpty()&&!result.back().ldrawCandidates.contains(candidate,Qt::CaseInsensitive))
                        result.back().ldrawCandidates.push_back(candidate);
                }
                // Color is not intrinsic to the `part` table. Use the exact
                // Rebrickable 9999 identity in observed catalog Part/Color
                // associations, and exclude only Parts with no real color.
                QHash<int,int> colorFlags;
                QSqlQuery colors(database);
                if(!colors.exec(QStringLiteral(
                    "SELECT part_id,rebrickable_id FROM ("
                    "SELECT s.part_id,c.rebrickable_id FROM set_catalog_part s JOIN color c ON c.id=s.color_id "
                    "UNION ALL SELECT m.part_id,c.rebrickable_id FROM minifig_catalog_part m JOIN color c ON c.id=m.color_id "
                    "UNION ALL SELECT e.part_id,c.rebrickable_id FROM part_element_identifier e JOIN color c ON c.id=e.color_id WHERE e.is_active=1"
                    ") WHERE rebrickable_id IS NOT NULL"))){
                    if(error)*error=colors.lastError().text();result.clear();
                }else{
                    while(colors.next()){
                        const int id=colors.value(0).toInt();
                        if(!partPositions.contains(id))continue;
                        colorFlags[id]|=colors.value(1).toInt()==9999?1:2;
                    }
                    for(auto it=partPositions.cbegin();it!=partPositions.cend();++it)
                        result[it.value()].noColor=colorFlags.value(it.key())==1;
                    if(error)error->clear();
                }
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connection);
    return result;
}

QVector<BatchPrintablePart> BatchPrintableModelService::explicitParts(
    const QVector<BatchPrintablePart>& catalog,const QStringList& partNumbers)
{
    QVector<BatchPrintablePart> selected;selected.reserve(partNumbers.size());
    for(const auto&number:partNumbers){
        const auto found=std::find_if(catalog.cbegin(),catalog.cend(),[&](const auto&entry){
            return entry.partNumber.compare(number.trimmed(),Qt::CaseInsensitive)==0;});
        if(found!=catalog.cend())selected.push_back(*found);
        else {BatchPrintablePart unknown;unknown.partNumber=number.trimmed();selected.push_back(unknown);}
    }
    return selected;
}

QVector<BatchPrintablePart> BatchPrintableModelService::randomSample(
    const QVector<BatchPrintablePart>& catalog,int count,quint32 seed,bool excludeNonstandardIds,
    BatchPrintPopulation* population,bool excludeNoModel,const QString& libraryRoot)
{
    BatchPrintPopulation counts;counts.catalogTotal=catalog.size();
    QVector<BatchPrintablePart> shuffled;
    QHash<QString,QString> installedNames;
    if(excludeNoModel){
        const QDir directory(QDir(libraryRoot).filePath(QStringLiteral("parts")));
        for(const auto& name:directory.entryList(QDir::Files))
            installedNames.insert(name.toCaseFolded(),name);
    }
    for(const auto& part:catalog){
        if(isStickerCategory(part)){++counts.excludedStickerCategory;continue;}
        if(part.noColor){++counts.excludedNoColor;continue;}
        if(excludeNonstandardIds&&!isStandardAuditPartNumber(part.partNumber)){
            ++counts.excludedNonstandardId;continue;
        }
        if(excludeNoModel&&modelFor(part,libraryRoot,&installedNames).isEmpty()){++counts.excludedNoModel;continue;}
        shuffled.push_back(part);
    }
    counts.eligibleTotal=shuffled.size();
    std::mt19937 generator(seed);std::shuffle(shuffled.begin(),shuffled.end(),generator);
    if(count>0&&shuffled.size()>count)shuffled.resize(count);
    counts.actualSampled=shuffled.size();
    if(population)*population=counts;
    return shuffled;
}

bool BatchPrintableModelService::isStandardAuditPartNumber(const QString& partNumber)
{
    static const QRegularExpression standard(QStringLiteral("^[0-9]+[A-Za-z]?$"));
    return standard.match(partNumber).hasMatch();
}

bool BatchPrintableModelService::isStickerCategory(const BatchPrintablePart& part)
{
    return part.category.trimmed().compare(QStringLiteral("Stickers"),Qt::CaseInsensitive)==0;
}

bool BatchPrintableModelService::writeDiagnosticThreeMf(const PrintMesh& candidate,
    const QString& path,const QString& partNumber,const QColor& color,QString* error,
    const CancellationState* cancellation,const std::function<void(const QString&)>& phase)
{
    if(!diagnosticWithinBounds(candidate)){
        if(error)*error=QStringLiteral("Diagnostic export bounded out (limit: 50000 faces / 150000 vertices).");
        return false;
    }
    if(cancellation&&cancellation->isCancelled()){
        if(error)*error=QStringLiteral("Diagnostic export skipped after Stop.");return false;
    }
    ThreeMfWriter::Options options;options.phase=phase;
    options.objectName=partNumber+QStringLiteral(" - DIAGNOSTIC, NOT ACCEPTED FOR PRINTING");
    options.partIdentity=partNumber;options.modelColor=color;
    if(!ThreeMfWriter::write(candidate,path,options,error))return false;
    if(cancellation&&cancellation->isCancelled()){
        if(error)*error=QStringLiteral("Diagnostic reopen skipped after Stop; package not validated.");return false;
    }
    if(phase)phase(QStringLiteral("reopen"));
    return reopen(path,candidate.faces.size(),error);
}

bool BatchPrintableModelService::diagnosticWithinBounds(const PrintMesh& mesh)
{
    return mesh.faces.size()<=50000&&mesh.vertices.size()<=150000;
}

BatchPrintTotals BatchPrintableModelService::summarize(const QVector<BatchPrintResult>& results)
{
    BatchPrintTotals totals;totals.input=results.size();
    for(const auto&result:results){
        if(result.category==BatchPrintCategory::NotStarted)continue;
        ++totals.attempted;
        if(result.category==BatchPrintCategory::SkippedNoColor){++totals.skippedNoColor;continue;}
        if(result.category==BatchPrintCategory::SkippedStickerCategory){++totals.skippedStickerCategory;continue;}
        if(result.category==BatchPrintCategory::SkippedNonstandardId){++totals.skippedNonstandardIds;continue;}
        if(result.category==BatchPrintCategory::NoCatalogPart)continue;
        ++totals.eligible;
        if(!result.ldrawModel.isEmpty())++totals.modelAvailable;
        if(result.category==BatchPrintCategory::Success){++totals.nativeSuccessful;++totals.successful;}
        if(result.category==BatchPrintCategory::StrictOverrideSuccess||result.category==BatchPrintCategory::UserOverrideSuccess){
            ++totals.overrideRecoveries;++totals.successful;
        }
        if(result.category==BatchPrintCategory::NoLDrawModel)++totals.noModel;
        if(result.category==BatchPrintCategory::ManufacturingFailed)++totals.manufacturingFailures;
        if(result.category==BatchPrintCategory::ExportFailed||result.category==BatchPrintCategory::ReopenFailed)
            ++totals.exportFailures;
        const auto native=result.nativeCategory==BatchPrintCategory::NotStarted?result.category:result.nativeCategory;
        if(native==BatchPrintCategory::PrepareNotReady||native==BatchPrintCategory::ResourceLimit||
           native==BatchPrintCategory::SourceCoverage||native==BatchPrintCategory::ManifoldFailure||
           native==BatchPrintCategory::SelfIntersection)++totals.prepareFailures;
    }
    return totals;
}

BatchPrintRun BatchPrintableModelService::run(const QVector<BatchPrintablePart>& parts,
    const BatchPrintOptions& options,CancellationState* cancellation,const Progress& progress) const
{
    CancellationState localCancellation;if(!cancellation)cancellation=&localCancellation;
    BatchPrintRun run;run.population=options.population;
    const QString root=options.outputRoot.isEmpty()?QDir(QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation)).filePath(QStringLiteral("BrickSuite/Print Capability Audits")):
        options.outputRoot;
    const QString runId=options.runId.isEmpty()?QDateTime::currentDateTimeUtc().toString(
        QStringLiteral("yyyyMMddTHHmmssZ-"))+QUuid::createUuid().toString(QUuid::WithoutBraces).left(8):
        safeFileName(options.runId);
    run.runDirectory=QDir(root).filePath(runId);
    if(QFileInfo::exists(run.runDirectory)){
        run.diagnostic=QStringLiteral("The audit run identity already exists; no output was overwritten.");return run;
    }
    if(!QDir().mkpath(QDir(run.runDirectory).filePath(QStringLiteral("exports")))){
        run.diagnostic=QStringLiteral("Could not create the audit output directory.");return run;
    }
    run.metadataPath=QDir(run.runDirectory).filePath(QStringLiteral("run-metadata.json"));
    QFile metadataFile(run.metadataPath);
    if(!metadataFile.open(QIODevice::WriteOnly|QIODevice::Text)){
        run.diagnostic=metadataFile.errorString();return run;
    }
    const QJsonObject metadata{
        {QStringLiteral("runId"),runId},
        {QStringLiteral("localOverridePolicy"),QStringLiteral("existing-validated-nominal-after-native-preparation-failure-v1")},
        {QStringLiteral("auditPreparationProfile"),QStringLiteral("audit-bounded-v1")},
        {QStringLiteral("maximumSequentialBooleanOperations"),8},
        {QStringLiteral("diagnosticMaximumFaces"),50000},
        {QStringLiteral("diagnosticMaximumVertices"),150000},
        {QStringLiteral("sampleMode"),options.randomSample?QStringLiteral("random"):QStringLiteral("part_list")},
        {QStringLiteral("seed"),static_cast<qint64>(options.seed)},
        {QStringLiteral("requestedEligibleCount"),options.requestedEligibleCount},
        {QStringLiteral("actualSampledCount"),parts.size()},
        {QStringLiteral("catalogPopulation"),run.population.catalogTotal},
        {QStringLiteral("excludedNoColor"),run.population.excludedNoColor},
        {QStringLiteral("excludedStickerCategory"),run.population.excludedStickerCategory},
        {QStringLiteral("excludedNonstandardId"),run.population.excludedNonstandardId},
        {QStringLiteral("excludedNoModel"),run.population.excludedNoModel},
        {QStringLiteral("excludeNoModel"),options.randomSample&&options.excludeNoModel},
        {QStringLiteral("excludeStickerCategory"),true},
        {QStringLiteral("eligiblePopulation"),run.population.eligibleTotal},
        {QStringLiteral("excludeNoColor"),true},
        {QStringLiteral("excludeNonstandardIds"),options.excludeNonstandardIds},
        {QStringLiteral("partIdEligibilityRule"),QStringLiteral("^[0-9]+[A-Za-z]?$")}
    };
    const auto metadataBytes=QJsonDocument(metadata).toJson(QJsonDocument::Indented);
    if(metadataFile.write(metadataBytes)!=metadataBytes.size()||!durableFlush(metadataFile)){
        run.diagnostic=metadataFile.errorString();return run;
    }
    metadataFile.close();
    run.statePath=QDir(run.runDirectory).filePath(QStringLiteral("run-state.json"));
    QJsonArray sample;QJsonObject candidates;
    for(const auto& part:parts){sample.append(part.partNumber);
        candidates.insert(part.partNumber,QJsonArray::fromStringList(part.ldrawCandidates));}
    QJsonObject state=metadata;
    state.insert(QStringLiteral("stateSchemaVersion"),1);
    state.insert(QStringLiteral("orderedParts"),sample);
    state.insert(QStringLiteral("ldrawCandidates"),candidates);
    state.insert(QStringLiteral("requestedSampleCount"),options.randomSample?options.requestedEligibleCount:parts.size());
    state.insert(QStringLiteral("libraryRoot"),options.libraryRoot);
    state.insert(QStringLiteral("currentSampleSequence"),0);
    state.insert(QStringLiteral("currentPartNumber"),QString());
    state.insert(QStringLiteral("completedCount"),0);
    state.insert(QStringLiteral("currentPartStatus"),QStringLiteral("pending"));
    state.insert(QStringLiteral("status"),QStringLiteral("running"));
    state.insert(QStringLiteral("currentPhase"),QStringLiteral("pending"));
    const auto runState=options.runState?options.runState:std::make_shared<BatchPrintRunState>();
    const auto checkpoint=[&](){return runState->save(run.statePath,state,&run.diagnostic);};
    if(!checkpoint())return run;
    run.csvPath=QDir(run.runDirectory).filePath(QStringLiteral("results.csv"));
    QFile csvFile(run.csvPath);
    if(!csvFile.open(QIODevice::WriteOnly|QIODevice::Text)){
        run.diagnostic=csvFile.errorString();return run;
    }
    csvFile.write("csv_schema_version,run_id,seed,sample_mode,requested_eligible_count,exclude_no_color,exclude_nonstandard_ids,part_id_eligibility_rule,sample_sequence,part_number,category_id,rebrickable_category_id,category_name,ldraw_model,result_category,preparation_route,source_triangles,source_groups,coverage_complete,prepare_ms,prepared_vertices,prepared_faces,recognized_features,fit_resolution,profile_identity,corrections,export_path,reopened,diagnostic_export_available,diagnostic_export_path,total_ms,diagnostic,native_result_category,native_diagnostic,local_override_state,local_override_used,local_override_stale,local_override_diagnostic,local_override_route,local_override_identity,export_result,reopen_result\n");
    const auto save=[&](const BatchPrintResult& row){
        const auto& part=parts[row.sequence-1];
        const QString line=QStringList{QStringLiteral("5"),csv(runId),QString::number(options.seed),
            csv(options.randomSample?QStringLiteral("random"):QStringLiteral("part_list")),
            QString::number(options.requestedEligibleCount),QStringLiteral("1"),
            options.excludeNonstandardIds?QStringLiteral("1"):QStringLiteral("0"),
            csv(QStringLiteral("^[0-9]+[A-Za-z]?$")),QString::number(row.sequence),
            csv(row.partNumber),part.categoryId?QString::number(part.categoryId):QString(),
            part.rebrickableCategoryId?QString::number(part.rebrickableCategoryId):QString(),
            csv(part.category),csv(row.ldrawModel),csv(categoryCode(row.category)),
            csv(row.preparationRoute),QString::number(row.sourceTriangles),QString::number(row.sourceGroups),
            row.coverageComplete?QStringLiteral("1"):QStringLiteral("0"),QString::number(row.prepareMilliseconds),
            QString::number(row.preparedVertices),QString::number(row.preparedFaces),
            csv(row.recognizedFeatures),csv(row.fitResolution),csv(row.profileIdentity),
            csv(row.correctionSummary),csv(row.exportPath),row.reopened?QStringLiteral("1"):QStringLiteral("0"),
            row.diagnosticExportAvailable?QStringLiteral("1"):QStringLiteral("0"),csv(row.diagnosticExportPath),
            QString::number(row.totalMilliseconds),csv(row.diagnostic),
            csv(categoryCode(row.nativeCategory)),csv(row.nativeDiagnostic),csv(row.localOverrideState),
            row.localOverrideUsed?QStringLiteral("1"):QStringLiteral("0"),
            row.localOverrideStale?QStringLiteral("1"):QStringLiteral("0"),
            csv(row.localOverrideDiagnostic),csv(row.localOverrideRoute),csv(row.localOverrideIdentity),
            csv(row.category==BatchPrintCategory::ExportFailed?QStringLiteral("failed"):
                row.exportPath.isEmpty()?QStringLiteral("not_attempted"):QStringLiteral("success")),
            csv(row.reopened?QStringLiteral("success"):row.category==BatchPrintCategory::ReopenFailed?
                QStringLiteral("failed"):QStringLiteral("not_attempted"))}.join(',')+QLatin1Char('\n');
        const auto bytes=line.toUtf8();
        return csvFile.write(bytes)==bytes.size()&&durableFlush(csvFile);
    };
    if(!durableFlush(csvFile)){run.diagnostic=QStringLiteral("Could not flush CSV header.");return run;}
    QFile timingsFile(QDir(run.runDirectory).filePath(QStringLiteral("stage-timings.jsonl")));
    if(!timingsFile.open(QIODevice::WriteOnly)){run.diagnostic=timingsFile.errorString();return run;}
    run.results.reserve(parts.size());
    QSet<QString> usedExportNames;
    const auto exportPathFor=[&](const BatchPrintResult& row){
        const QString base=safeFileName(row.partNumber)+QLatin1Char('-')+categoryCode(row.category);
        QString name=base+QStringLiteral(".3mf");
        if(usedExportNames.contains(name)||QFileInfo::exists(QDir(run.runDirectory).filePath(QStringLiteral("exports/")+name))){
            const QString model=safeFileName(row.ldrawModel);
            name=base+QLatin1Char('-')+(model.isEmpty()?QStringLiteral("model-unknown"):model)+QStringLiteral(".3mf");
            int duplicate=2;
            while(usedExportNames.contains(name)||QFileInfo::exists(QDir(run.runDirectory).filePath(QStringLiteral("exports/")+name)))
                name=base+QLatin1Char('-')+(model.isEmpty()?QStringLiteral("model-unknown"):model)+
                    QStringLiteral("-repeat%1.3mf").arg(duplicate++);
        }
        usedExportNames.insert(name);
        return QDir(run.runDirectory).filePath(QStringLiteral("exports/")+name);
    };
    for(int index=0;index<parts.size();++index){
        if(cancellation&&cancellation->isCancelled()){run.stopped=true;break;}
        state.insert(QStringLiteral("currentSampleSequence"),index+1);
        state.insert(QStringLiteral("currentPartNumber"),parts[index].partNumber);
        state.insert(QStringLiteral("currentPartStatus"),QStringLiteral("active"));
        state.insert(QStringLiteral("currentPhase"),QStringLiteral("starting"));
        state.insert(QStringLiteral("phaseTimingsMs"),QJsonObject());
        state.remove(QStringLiteral("nativeResultCategory"));
        if(!checkpoint())return run;
        if(options.phaseProgress)options.phaseProgress(index+1,parts[index].partNumber,QStringLiteral("starting"));
        if(options.beforePart)options.beforePart(run.statePath,index+1,parts[index].partNumber);
        const auto&part=parts[index];BatchPrintResult row;row.sequence=index+1;row.partNumber=part.partNumber;
        QElapsedTimer phaseTimer;phaseTimer.start();QString currentPhase=QStringLiteral("starting");
        QJsonObject stageTimes;bool persistenceOk=true;
        const auto phase=[&](const QString& name){
            stageTimes.insert(currentPhase,stageTimes.value(currentPhase).toInteger()+phaseTimer.elapsed());
            currentPhase=name;
            state.insert(QStringLiteral("currentPhase"),name);
            state.insert(QStringLiteral("phaseTimingsMs"),stageTimes);
            persistenceOk=checkpoint()&&persistenceOk;
            if(!persistenceOk&&cancellation)cancellation->cancel();
            if(options.phaseProgress)options.phaseProgress(row.sequence,row.partNumber,name);
            phaseTimer.restart();
        };
        const auto diagnosticExport=[&](const PrintMesh* mesh){
            phase(QStringLiteral("diagnostic_candidate"));
            if(!mesh||mesh->vertices.empty()||mesh->faces.empty()){
                row.diagnostic+=QStringLiteral(" No bounded diagnostic candidate mesh was available.");return;
            }
            if(!diagnosticWithinBounds(*mesh)||(cancellation&&cancellation->isCancelled())){
                row.diagnostic+=QStringLiteral(" Diagnostic export unavailable: bounded out or Stop requested.");return;
            }
            if(!persistenceOk)return;
            phase(QStringLiteral("diagnostic_export"));
            const QString path=exportPathFor(row);
            QString error;
            if(writeDiagnosticThreeMf(options.printOrientation.apply(*mesh),path,part.partNumber,
                                      options.modelColor,&error,cancellation,phase)){
                row.diagnosticExportAvailable=true;row.diagnosticExportPath=path;
            }else row.diagnostic+=QStringLiteral(" Diagnostic 3MF could not be saved/reopened: ")+error;
        };
        if(cancellation&&cancellation->isCancelled()){
            run.stopped=true;
            state.insert(QStringLiteral("currentPartStatus"),QStringLiteral("not_started"));break;
        }else{
            QElapsedTimer timer;timer.start();
            try{
            do{
                if(!part.catalogPresent||part.partNumber.isEmpty()){
                    row.category=BatchPrintCategory::NoCatalogPart;row.diagnostic=QStringLiteral("Empty Part identity.");break;}
                if(part.noColor){row.category=BatchPrintCategory::SkippedNoColor;
                    row.diagnostic=QStringLiteral("No Color / Sticker catalog Part.");break;}
                if(isStickerCategory(part)){row.category=BatchPrintCategory::SkippedStickerCategory;
                    row.diagnostic=QStringLiteral("Sticker catalog category is outside the print audit population.");break;}
                if(options.excludeNonstandardIds&&!isStandardAuditPartNumber(part.partNumber)){
                    row.category=BatchPrintCategory::SkippedNonstandardId;
                    row.diagnostic=QStringLiteral("Part ID is outside the standard audit population.");break;}
                row.ldrawModel=modelFor(part,options.libraryRoot);
                if(row.ldrawModel.isEmpty()){
                    row.category=BatchPrintCategory::NoLDrawModel;
                    row.diagnostic=QStringLiteral("No usable installed LDraw model was found.");break;
                }
                phase(QStringLiteral("loading"));
                if(!persistenceOk)break;
                const auto source=LDrawLibraryService::loadPart(options.libraryRoot,row.ldrawModel);
                if(!source.ok()){
                    row.category=BatchPrintCategory::LoadFailed;row.diagnostic=source.error.message;break;
                }
                row.sourceTriangles=source.mesh.triangles.size();
                PrintPreparationRequest request;request.partReference=part.partNumber;
                request.ldrawIdentity=row.ldrawModel;request.libraryAuthority=options.libraryRoot;
                request.loadResult=source;
                request.profile.identity+=QStringLiteral("-audit-bounded-v1");
                request.profile.maximumBooleanOperations=8;
                phase(QStringLiteral("preparing"));
                if(!persistenceOk)break;
                QElapsedTimer preparationTimer;preparationTimer.start();
                const auto prepared=LDrawPrintPreparationService().prepare(request,cancellation,[&](const PrintPreparationProgress& p){
                    static const char* names[]={"source_analysis","semantic_construction","operand_validation","boolean_composition","final_validation","diagnostic_candidate"};
                    if(p.phase==PrintPreparationPhase::BooleanComposition)
                        state.insert(QStringLiteral("booleanOperations"),p.totalOperations);
                    phase(QStringLiteral("preparing/")+QString::fromLatin1(names[int(p.phase)]));
                });
                row.prepareMilliseconds=preparationTimer.elapsed();
                phase(QStringLiteral("source_coverage"));
                row.sourceGroups=prepared.sourceCoverage.groups.size();
                row.coverageComplete=prepared.sourceCoverage.complete();
                if(!prepared.ready()){
                    row.category=preparationCategory(prepared);row.diagnostic=prepared.diagnostic;
                    row.nativeCategory=row.category;row.nativeDiagnostic=row.diagnostic;
                    state.insert(QStringLiteral("nativeResultCategory"),categoryCode(row.nativeCategory));
                    phase(QStringLiteral("failure_classified"));
                    diagnosticExport(prepared.diagnosticCandidateMesh.get());
                    if(!persistenceOk||cancellation->isCancelled()||row.category==BatchPrintCategory::Cancelled)break;
                    phase(QStringLiteral("local_override_load"));
                    if(!persistenceOk||cancellation->isCancelled())break;
                    const LocalPrintableOverrideService overrides(options.localOverrideRoot);
                    const LocalPrintableOverrideService::Context context{part.partId,part.partNumber,source};
                    const auto local=overrides.load(context);
                    row.localOverrideStale=local.stale;
                    row.localOverrideDiagnostic=local.diagnostic;
                    row.localOverrideState=local.stale?QStringLiteral("stale"):
                        local.ok()?(local.prepared->userAcceptedOverride?QStringLiteral("user_accepted"):
                            QStringLiteral("strictly_validated")):
                        (part.partId<=0||!local.diagnostic.isEmpty()||local.reviewable?
                            QStringLiteral("invalid_unavailable"):QStringLiteral("none"));
                    if(!local.ok()||local.stale||local.reviewable)break;
                    if(cancellation->isCancelled())break;
                    row.localOverrideUsed=true;
                    row.localOverrideRoute=local.prepared->preparationMethod;
                    row.localOverrideIdentity=local.prepared->overrideIdentity;
                    row.preparedVertices=local.prepared->mesh.vertices.size();
                    row.preparedFaces=local.prepared->mesh.faces.size();
                    row.fitResolution=QStringLiteral("Nominal local override; Auto Fit not invoked.");
                    const auto success=local.prepared->userAcceptedOverride?
                        BatchPrintCategory::UserOverrideSuccess:BatchPrintCategory::StrictOverrideSuccess;
                    row.category=success;row.exportPath=exportPathFor(row);
                    phase(QStringLiteral("local_override_export"));
                    if(!persistenceOk||cancellation->isCancelled()){
                        row.category=BatchPrintCategory::Cancelled;row.exportPath.clear();break;
                    }
                    ThreeMfWriter::Options exportOptions;exportOptions.phase=phase;
                    exportOptions.objectName=part.partNumber+QStringLiteral(" - Local Override (Nominal)");
                    exportOptions.partIdentity=part.partNumber;exportOptions.modelColor=options.modelColor;
                    QString error;
                    if(!ThreeMfWriter::write(options.printOrientation.apply(local.prepared->mesh),
                        row.exportPath,exportOptions,&error)){
                        row.category=BatchPrintCategory::ExportFailed;row.diagnostic=error;
                        row.exportPath.clear();break;
                    }
                    phase(QStringLiteral("local_override_reopen"));
                    const auto validator=options.reopenValidator?options.reopenValidator:
                        std::function<bool(const QString&,std::size_t,QString*)>(reopen);
                    if(!persistenceOk||cancellation->isCancelled()){
                        row.category=BatchPrintCategory::Cancelled;row.diagnostic=QStringLiteral("Stopped before override reopen.");
                    }else if(!validator(row.exportPath,local.prepared->mesh.faces.size(),&error)){
                        row.category=BatchPrintCategory::ReopenFailed;row.diagnostic=error;
                    }else{
                        row.reopened=true;
                        row.diagnostic=QStringLiteral("Existing local override nominal PreparedMesh exported and reopened. ")+local.diagnostic;
                    }
                    if(row.category!=success){
                        const QString failedPath=exportPathFor(row);
                        if(QFile::rename(row.exportPath,failedPath))row.exportPath=failedPath;
                        else row.diagnostic+=QStringLiteral(" Export file could not be renamed to its failure status.");
                    }
                    break;
                }
                row.preparationRoute=prepared.preparedMesh->preparationMethod;
                row.preparedVertices=prepared.preparedMesh->mesh.vertices.size();
                row.preparedFaces=prepared.preparedMesh->mesh.faces.size();
                QStringList featureContracts;
                for(const auto&feature:prepared.preparedMesh->functionalFeatures)
                    if(feature.eligibility==FunctionalEligibility::Eligible)
                        featureContracts<<familyName(feature.family)+QStringLiteral(":")+
                            feature.evidenceContract;
                featureContracts.removeDuplicates();row.recognizedFeatures=featureContracts.join(';');
                if(cancellation&&cancellation->isCancelled()){row.category=BatchPrintCategory::Cancelled;break;}
                phase(QStringLiteral("manufacturing"));
                PrintMesh output=prepared.preparedMesh->mesh;
                const FitCalibrationLibrary library;
                const auto fit=AutoFitProfileResolver::resolveManaged(options.autoFitEnabled,
                    part.partNumber,library,source,options.printOrientation);
                row.fitResolution=fit.diagnostic;
                if(fit.resolved()){
                    row.profileIdentity=fit.profile.profileIdentity;
                    const auto manufacturing=ManufacturingMeshService().generate(source,
                        *prepared.preparedMesh,fit.profile,options.printOrientation);
                    if(!manufacturing.ok()||!manufacturing.manufacturingMesh){
                        row.category=BatchPrintCategory::ManufacturingFailed;
                        row.diagnostic=manufacturing.diagnostic;
                        diagnosticExport(&prepared.preparedMesh->mesh);break;
                    }
                    output=manufacturing.manufacturingMesh->mesh;
                    row.correctionSummary=manufacturing.manufacturingMesh->provenance.join(';');
                }
                row.category=BatchPrintCategory::Success;
                row.exportPath=exportPathFor(row);
                ThreeMfWriter::Options exportOptions;exportOptions.objectName=part.partNumber;
                exportOptions.phase=phase;
                exportOptions.partIdentity=part.partNumber;exportOptions.modelColor=options.modelColor;
                QString error;
                if(!ThreeMfWriter::write(options.printOrientation.apply(output),row.exportPath,
                                          exportOptions,&error)){
                    row.category=BatchPrintCategory::ExportFailed;row.diagnostic=error;
                    row.exportPath.clear();break;
                }
                phase(QStringLiteral("reopen"));
                const auto validator=options.reopenValidator?options.reopenValidator:
                    std::function<bool(const QString&,std::size_t,QString*)>(reopen);
                if(!validator(row.exportPath,output.faces.size(),&error)){
                    row.category=BatchPrintCategory::ReopenFailed;row.diagnostic=error;
                    const QString failedPath=exportPathFor(row);
                    if(QFile::rename(row.exportPath,failedPath))row.exportPath=failedPath;
                    else row.diagnostic+=QStringLiteral(" Export file could not be renamed to its failure status.");
                    break;
                }
                row.reopened=true;row.category=BatchPrintCategory::Success;
                row.diagnostic=fit.resolved()?QStringLiteral("Verified ManufacturingMesh exported and reopened."):
                    QStringLiteral("Nominal PreparedMesh exported and reopened.");
            }while(false);
            }catch(const std::exception&exception){
                row.category=BatchPrintCategory::PrepareNotReady;
                row.diagnostic=QStringLiteral("Part workflow threw an exception: %1")
                    .arg(QString::fromUtf8(exception.what()));
            }
            row.totalMilliseconds=timer.elapsed();
        }
        if(!persistenceOk)return run;
        if(row.nativeCategory==BatchPrintCategory::NotStarted){
            row.nativeCategory=row.category;row.nativeDiagnostic=row.diagnostic;
        }
        phase(QStringLiteral("persisting"));
        run.results.push_back(row);
        if(!save(row)){
            run.diagnostic=QStringLiteral("The audit CSV could not be flushed: %1").arg(csvFile.errorString());
            csvFile.close();return run;
        }
        phase(QStringLiteral("run_state"));
        state.insert(QStringLiteral("completedCount"),run.results.size());
        state.insert(QStringLiteral("currentPartStatus"),QStringLiteral("finished"));
        if(!checkpoint())return run;
        phase(QStringLiteral("completed"));
        if(!persistenceOk)return run;
        const auto timingBytes=QJsonDocument(QJsonObject{{"sampleSequence",row.sequence},{"partNumber",row.partNumber},
            {"timingsMs",stageTimes}}).toJson(QJsonDocument::Compact)+'\n';
        if(timingsFile.write(timingBytes)!=timingBytes.size()||!durableFlush(timingsFile)){
            run.diagnostic=QStringLiteral("Could not flush stage timings.");return run;
        }
        run.totals=summarize(run.results);
        if(progress)progress(row,run.totals);
    }
    run.stopped=run.stopped||(cancellation&&cancellation->isCancelled());
    state.insert(QStringLiteral("status"),run.stopped?QStringLiteral("stopped"):QStringLiteral("completed"));
    if(!checkpoint())return run;
    csvFile.close();run.ok=true;
    return run;
}

} // namespace PrintGeometry
