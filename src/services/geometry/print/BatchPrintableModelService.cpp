#include "BatchPrintableModelService.h"

#include "AutoFitProfileResolver.h"
#include "LDrawPrintPreparationService.h"
#include "ManufacturingMeshService.h"
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
QString csv(QString value)
{
    value.replace('"',QStringLiteral("\"\""));
    return QStringLiteral("\"")+value+QStringLiteral("\"");
}

bool installed(const QString& root,QString candidate)
{
    candidate=candidate.trimmed();
    if(candidate.startsWith(QStringLiteral("parts/"),Qt::CaseInsensitive))candidate=candidate.mid(6);
    if(candidate.endsWith(QStringLiteral(".dat"),Qt::CaseInsensitive))candidate.chop(4);
    if(candidate.isEmpty()||candidate.contains('/')||candidate.contains('\\')||
       candidate==QStringLiteral(".")||candidate==QStringLiteral(".."))return false;
    const QDir parts(QDir(root).filePath(QStringLiteral("parts")));
    if(QFileInfo::exists(parts.filePath(candidate+QStringLiteral(".dat"))))return true;
    for(const auto&name:parts.entryList(QDir::Files))
        if(name.compare(candidate+QStringLiteral(".dat"),Qt::CaseInsensitive)==0)return true;
    return false;
}

QString modelFor(const BatchPrintablePart& part,const QString& root)
{
    QStringList candidates=part.ldrawCandidates;
    if(!candidates.contains(part.partNumber,Qt::CaseInsensitive))candidates.prepend(part.partNumber);
    for(const auto&candidate:candidates)
        if(candidate.compare(part.partNumber,Qt::CaseInsensitive)==0&&installed(root,candidate))
            return candidate;
    for(const auto&candidate:candidates)if(installed(root,candidate))return candidate;
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

double BatchPrintTotals::modelAvailabilityPercent() const
{return eligible?100.0*modelAvailable/eligible:0.0;}
double BatchPrintTotals::printServicePercent() const
{return modelAvailable?100.0*successful/modelAvailable:0.0;}
double BatchPrintTotals::catalogToPrintablePercent() const
{return eligible?100.0*successful/eligible:0.0;}

QString BatchPrintableModelService::categoryCode(BatchPrintCategory category)
{
    switch(category){
    case BatchPrintCategory::Success:return QStringLiteral("success");
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
                        BatchPrintablePart part;part.partNumber=query.value(1).toString();
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
    BatchPrintPopulation* population)
{
    BatchPrintPopulation counts;counts.catalogTotal=catalog.size();
    QVector<BatchPrintablePart> shuffled;
    for(const auto& part:catalog){
        if(isStickerCategory(part)){++counts.excludedStickerCategory;continue;}
        if(part.noColor){++counts.excludedNoColor;continue;}
        if(excludeNonstandardIds&&!isStandardAuditPartNumber(part.partNumber)){
            ++counts.excludedNonstandardId;continue;
        }
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
    const QString& path,const QString& partNumber,const QColor& color,QString* error)
{
    ThreeMfWriter::Options options;
    options.objectName=partNumber+QStringLiteral(" - DIAGNOSTIC, NOT ACCEPTED FOR PRINTING");
    options.partIdentity=partNumber;options.modelColor=color;
    if(!ThreeMfWriter::write(candidate,path,options,error))return false;
    return reopen(path,candidate.faces.size(),error);
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
        if(result.category==BatchPrintCategory::Success)++totals.successful;
        if(result.category==BatchPrintCategory::NoLDrawModel)++totals.noModel;
        if(result.category==BatchPrintCategory::ManufacturingFailed)++totals.manufacturingFailures;
        if(result.category==BatchPrintCategory::ExportFailed||result.category==BatchPrintCategory::ReopenFailed)
            ++totals.exportFailures;
        if(result.category==BatchPrintCategory::PrepareNotReady||result.category==BatchPrintCategory::ResourceLimit||
           result.category==BatchPrintCategory::SourceCoverage||result.category==BatchPrintCategory::ManifoldFailure||
           result.category==BatchPrintCategory::SelfIntersection)++totals.prepareFailures;
    }
    return totals;
}

BatchPrintRun BatchPrintableModelService::run(const QVector<BatchPrintablePart>& parts,
    const BatchPrintOptions& options,CancellationState* cancellation,const Progress& progress) const
{
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
        {QStringLiteral("sampleMode"),options.randomSample?QStringLiteral("random"):QStringLiteral("part_list")},
        {QStringLiteral("seed"),static_cast<qint64>(options.seed)},
        {QStringLiteral("requestedEligibleCount"),options.requestedEligibleCount},
        {QStringLiteral("actualSampledCount"),options.randomSample?run.population.actualSampled:parts.size()},
        {QStringLiteral("catalogPopulation"),run.population.catalogTotal},
        {QStringLiteral("excludedNoColor"),run.population.excludedNoColor},
        {QStringLiteral("excludedStickerCategory"),run.population.excludedStickerCategory},
        {QStringLiteral("excludedNonstandardId"),run.population.excludedNonstandardId},
        {QStringLiteral("eligiblePopulation"),run.population.eligibleTotal},
        {QStringLiteral("excludeNoColor"),true},
        {QStringLiteral("excludeNonstandardIds"),options.excludeNonstandardIds},
        {QStringLiteral("partIdEligibilityRule"),QStringLiteral("^[0-9]+[A-Za-z]?$")}
    };
    const auto metadataBytes=QJsonDocument(metadata).toJson(QJsonDocument::Indented);
    if(metadataFile.write(metadataBytes)!=metadataBytes.size()||!metadataFile.flush()){
        run.diagnostic=metadataFile.errorString();return run;
    }
    metadataFile.close();
    run.csvPath=QDir(run.runDirectory).filePath(QStringLiteral("results.csv"));
    QFile csvFile(run.csvPath);
    if(!csvFile.open(QIODevice::WriteOnly|QIODevice::Text)){
        run.diagnostic=csvFile.errorString();return run;
    }
    csvFile.write("csv_schema_version,run_id,seed,sample_mode,requested_eligible_count,exclude_no_color,exclude_nonstandard_ids,part_id_eligibility_rule,sample_sequence,part_number,category_id,rebrickable_category_id,category_name,ldraw_model,result_category,preparation_route,source_triangles,source_groups,coverage_complete,prepare_ms,prepared_vertices,prepared_faces,recognized_features,fit_resolution,profile_identity,corrections,export_path,reopened,diagnostic_export_available,diagnostic_export_path,total_ms,diagnostic\n");
    const auto save=[&](const BatchPrintResult& row){
        const auto& part=parts[row.sequence-1];
        const QString line=QStringList{QStringLiteral("4"),csv(runId),QString::number(options.seed),
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
            QString::number(row.totalMilliseconds),csv(row.diagnostic)}.join(',')+QLatin1Char('\n');
        const auto bytes=line.toUtf8();
        return csvFile.write(bytes)==bytes.size()&&csvFile.flush();
    };
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
        const auto&part=parts[index];BatchPrintResult row;row.sequence=index+1;row.partNumber=part.partNumber;
        const auto diagnosticExport=[&](const PrintMesh* mesh){
            if(!mesh||mesh->vertices.empty()||mesh->faces.empty()){
                row.diagnostic+=QStringLiteral(" No bounded diagnostic candidate mesh was available.");return;
            }
            const QString path=exportPathFor(row);
            QString error;
            if(writeDiagnosticThreeMf(options.printOrientation.apply(*mesh),path,part.partNumber,
                                      options.modelColor,&error)){
                row.diagnosticExportAvailable=true;row.diagnosticExportPath=path;
            }else row.diagnostic+=QStringLiteral(" Diagnostic 3MF could not be saved/reopened: ")+error;
        };
        if(cancellation&&cancellation->isCancelled()){
            run.stopped=true;row.category=BatchPrintCategory::NotStarted;
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
                const auto source=LDrawLibraryService::loadPart(options.libraryRoot,row.ldrawModel);
                if(!source.ok()){
                    row.category=BatchPrintCategory::LoadFailed;row.diagnostic=source.error.message;break;
                }
                row.sourceTriangles=source.mesh.triangles.size();
                PrintPreparationRequest request;request.partReference=part.partNumber;
                request.ldrawIdentity=row.ldrawModel;request.libraryAuthority=options.libraryRoot;
                request.loadResult=source;
                const auto prepared=LDrawPrintPreparationService().prepare(request,cancellation);
                row.prepareMilliseconds=prepared.timings.totalMilliseconds;
                row.sourceGroups=prepared.sourceCoverage.groups.size();
                row.coverageComplete=prepared.sourceCoverage.complete();
                if(!prepared.ready()){
                    row.category=preparationCategory(prepared);row.diagnostic=prepared.diagnostic;
                    diagnosticExport(prepared.diagnosticCandidateMesh.get());break;
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
                exportOptions.partIdentity=part.partNumber;exportOptions.modelColor=options.modelColor;
                QString error;
                if(!ThreeMfWriter::write(options.printOrientation.apply(output),row.exportPath,
                                          exportOptions,&error)){
                    row.category=BatchPrintCategory::ExportFailed;row.diagnostic=error;
                    row.exportPath.clear();break;
                }
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
        run.results.push_back(row);
        if(!save(row)){
            run.diagnostic=QStringLiteral("The audit CSV could not be flushed: %1").arg(csvFile.errorString());
            csvFile.close();return run;
        }
        run.totals=summarize(run.results);
        if(progress)progress(row,run.totals);
    }
    csvFile.close();run.ok=true;
    return run;
}

} // namespace PrintGeometry
