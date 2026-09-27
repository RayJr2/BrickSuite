#include "BatchPrintableModelService.h"

#include "AutoFitProfileResolver.h"
#include "LDrawPrintPreparationService.h"
#include "ManufacturingMeshService.h"
#include "LocalPrintableOverrideService.h"
#include "../LDrawLibraryService.h"
#include "../ThreeMfWriter.h"

#include <lib3mf_implicit.hpp>
#include <QDateTime>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryFile>
#include <QThread>
#include <QDebug>
#include <QScopeGuard>
#include <QMutexLocker>
#include <cerrno>
#include <cstdio>
#ifdef Q_OS_WIN
#include <io.h>
#include <qt_windows.h>
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
#include <cmath>
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

bool saveJson(const QString& path,const QJsonObject& object,QString* error,
              const BatchPrintRunState::TestHooks& hooks = {})
{
    // Same-directory temporary file, fully flushed and closed before replacement.
    // Never remove the destination or fall back to a direct/partial overwrite.
    QElapsedTimer elapsed;elapsed.start();
    auto file=std::make_unique<QTemporaryFile>(path+QStringLiteral(".XXXXXX"));
    QString temporary=file->fileTemplate();
    const auto nativeFileError=[](){
#ifdef Q_OS_WIN
        return int(::GetLastError());
#else
        return errno;
#endif
    };
    const auto failure=[&](const QString& operation,int native,int attempt,const QString& detail){
        const QString message=QStringLiteral("Could not persist audit JSON: operation=%1; temp=%2; destination=%3; "
            "nativeError=%4; QtError=%5 (%6); attempt=%7/6; elapsedMs=%8; %9")
            .arg(operation,temporary,path)
            .arg(native).arg(file?int(file->error()):0).arg(file?file->errorString():QStringLiteral("native replacement"))
            .arg(attempt).arg(elapsed.elapsed()).arg(detail);
        if(error)*error=message;
        qWarning().noquote()<<message;
        return false;
    };
    if(!file->open())return failure(QStringLiteral("open temporary"),nativeFileError(),1,QStringLiteral("No replacement attempted."));
    temporary=file->fileName();
    const auto bytes=QJsonDocument(object).toJson(QJsonDocument::Indented);
    if(file->write(bytes)!=bytes.size())return failure(QStringLiteral("write temporary"),nativeFileError(),1,{});
    if(!file->flush())return failure(QStringLiteral("Qt flush temporary"),nativeFileError(),1,{});
#ifdef Q_OS_WIN
    if(::_commit(file->handle())!=0)return failure(QStringLiteral("sync temporary (_commit; errno)"),errno,1,{});
#else
    if(::fsync(file->handle())!=0)return failure(QStringLiteral("sync temporary (fsync; errno)"),errno,1,{});
#endif
    // QTemporaryFile::close() retains its native handle for reopening. Destroy
    // the object to release that handle before a Windows native replacement.
    file->setAutoRemove(false);
    file.reset();
    const auto cleanup=qScopeGuard([&]{QFile::remove(temporary);});
    for(int attempt=1;attempt<=6;++attempt){
        int native=hooks.replacementError?hooks.replacementError(path,object,attempt):0;
        if(!native){
#ifdef Q_OS_WIN
            if(!::MoveFileExW(reinterpret_cast<LPCWSTR>(temporary.utf16()),reinterpret_cast<LPCWSTR>(path.utf16()),
                             MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))native=int(::GetLastError());
#else
            if(::rename(QFile::encodeName(temporary).constData(),QFile::encodeName(path).constData())!=0)native=errno;
#endif
        }
        if(!native){
            if(attempt>1)qWarning().noquote()<<QStringLiteral("Audit JSON atomic replacement recovered: destination=%1; attempt=%2/6; elapsedMs=%3")
                .arg(path).arg(attempt).arg(elapsed.elapsed());
            return true;
        }
#ifdef Q_OS_WIN
        const bool retryable=native==ERROR_ACCESS_DENIED||native==ERROR_SHARING_VIOLATION||native==ERROR_LOCK_VIOLATION;
#else
        const bool retryable=native==EBUSY||native==EINTR;
#endif
        // Access denied can also mean a permanent ACL/read-only restriction. The
        // bounded policy does not identify a particular process or assume AV.
        if(!retryable||attempt==6||elapsed.elapsed()>=1500)
            return failure(QStringLiteral("atomic replace"),native,attempt,
                QStringLiteral("Replacement denied or retry limit reached; previous checkpoint preserved. Check destination permissions/sharing."));
        qWarning().noquote()<<QStringLiteral("Audit JSON replacement retry: operation=atomic replace; temp=%1; destination=%2; "
            "nativeError=%3; QtError=%4; attempt=%5/6; elapsedMs=%6")
            .arg(temporary,path).arg(native).arg(0).arg(attempt).arg(elapsed.elapsed());
        const auto delay=static_cast<unsigned long>(25u<<(attempt-1));
        if(hooks.wait)hooks.wait(delay);else QThread::msleep(delay);
    }
    return false;
}

QStringList eligibilityNotes(const BatchPrintablePart& part)
{
    QStringList notes;
    if(part.noColor)notes.append(QStringLiteral("no_color"));
    if(BatchPrintableModelService::isStickerCategory(part))notes.append(QStringLiteral("sticker_category"));
    if(!BatchPrintableModelService::isStandardAuditPartNumber(part.partNumber))notes.append(QStringLiteral("nonstandard_id"));
    if(!part.catalogPresent)notes.append(QStringLiteral("no_catalog_part"));
    return notes;
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

void appendFitFeatures(QVector<FunctionalFeature>& features,const QVector<FunctionalFeature>& additions)
{
    for(const auto& candidate:additions){
        const auto equivalent=[&](const FunctionalFeature& feature){
            if(feature.stableIdentity==candidate.stableIdentity)return true;
            const auto& a=feature.frame;const auto& b=candidate.frame;
            // The existing source and operand recognizers can name the same interface differently.
            return feature.family==candidate.family&&feature.role==candidate.role&&
                feature.evidenceContract==candidate.evidenceContract&&
                std::abs(a.origin.x-b.origin.x)<=1e-6&&std::abs(a.origin.y-b.origin.y)<=1e-6&&
                std::abs(a.origin.z-b.origin.z)<=1e-6&&
                std::abs(a.axis.x*b.axis.x+a.axis.y*b.axis.y+a.axis.z*b.axis.z)>=1.0-1e-6;
        };
        if(std::none_of(features.cbegin(),features.cend(),equivalent))features.append(candidate);
    }
}

QJsonArray sourceFeatureJson(const QVector<FunctionalFeature>& features,const PrintOrientation& orientation)
{
    QJsonArray result;
    const auto point=[](const Point& p){return QJsonArray{p.x,p.y,p.z};};
    for(const auto& feature:features){
        QJsonArray provenance;
        for(const auto& owner:feature.provenance)provenance.append(QJsonObject{
            {"file",owner.sourceFile},{"reference",owner.referenceId},{"line",owner.sourceLine}});
        result.append(QJsonObject{{"identity",feature.stableIdentity},{"family",familyName(feature.family)},
            {"role",feature.role==FunctionalInterfaceRole::Male?"male":"female"},
            {"contract",feature.evidenceContract},{"recipe",feature.constructionRecipe},
            {"modelAxis",point(feature.frame.axis)},{"buildAxis",point(orientation.map(feature.frame.axis))},
            {"printedOrientation",int(ManufacturingMeshService::transformedOrientation(feature,orientation))},
            {"eligibility",int(feature.eligibility)},{"sourceAncestry",provenance}});
    }
    return result;
}

void measureFit(BatchPrintResult& row,const QVector<FunctionalFeature>& features,
                const QVector<FitProfile>& profiles,bool explicitProfile,bool enabled,
                const PrintOrientation& orientation)
{
    row.recognizedFeatureCount=features.size();
    QStringList contracts;
    for(const auto& feature:features)contracts.append(familyName(feature.family)+":"+feature.evidenceContract);
    contracts.removeDuplicates();row.recognizedFeatures=contracts.join(';');
    int compatibleProfiles=0,best=0;
    for(const auto& profile:profiles){
        int applicable=0;
        for(const auto& feature:features)
            applicable+=!ManufacturingMeshService::featureCorrections(profile,feature,orientation).isEmpty();
        compatibleProfiles+=applicable>0;best=std::max(best,applicable);
    }
    row.applicableVerifiedFeatureCount=best;
    row.fitStatus=features.isEmpty()?QStringLiteral("no_supported_features_recognized"):
        !enabled?QStringLiteral("auto_fit_disabled"):
        compatibleProfiles>1&&!explicitProfile?QStringLiteral("recognized_multiple_profiles"):
        best>0?QStringLiteral("applicable_evidence_not_applied"):
        explicitProfile?QStringLiteral("selected_profile_no_applicable_evidence"):
        QStringLiteral("recognized_no_applicable_verified_evidence");
}
}

bool BatchPrintRunState::write(QString* error)
{
    if(m_path.isEmpty())return true;
    return saveJson(m_path,m_state,error,m_testHooks);
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

QString BatchPrintableModelService::resolvedModel(const BatchPrintablePart& part,const QString& root)
{ return modelFor(part,root); }

void BatchPrintableModelService::resolveModels(QVector<BatchPrintablePart>& parts,const QString& root)
{
    QHash<QString,QString> names;
    for(const auto& name:QDir(QDir(root).filePath(QStringLiteral("parts"))).entryList(QDir::Files))
        names.insert(name.toCaseFolded(),name);
    for(auto& part:parts)part.resolvedModel=modelFor(part,root,&names);
}
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
        if(result.category==BatchPrintCategory::NoCatalogPart&&!result.modelAvailabilityKnown)continue;
        ++totals.eligible;
        if(result.modelAvailabilityKnown?result.modelAvailable:!result.ldrawModel.isEmpty())++totals.modelAvailable;
        if(result.category==BatchPrintCategory::Success){++totals.nativeSuccessful;++totals.successful;}
        if(result.category==BatchPrintCategory::StrictOverrideSuccess||result.category==BatchPrintCategory::UserOverrideSuccess){
            ++totals.overrideRecoveries;++totals.successful;
        }
        if(result.modelAvailabilityKnown?!result.modelAvailable:result.category==BatchPrintCategory::NoLDrawModel)++totals.noModel;
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

QJsonObject BatchPrintableModelService::summarizeFit(const QVector<BatchPrintResult>& results)
{
    int models=0,ready=0,native=0,recognized=0,sourceApplicable=0,applicable=0,fitted=0,nonzero=0,zero=0,partial=0,nominal=0,failed=0;
    for(const auto& row:results){
        models+=row.modelAvailable||!row.ldrawModel.isEmpty();
        ready+=row.nominalPreparedReady;
        native+=row.category==BatchPrintCategory::Success&&row.reopened;
        recognized+=row.sourceRecognizedFeatureCount>0;
        sourceApplicable+=row.sourceApplicableVerifiedFeatureCount>0;
        applicable+=row.applicableVerifiedFeatureCount>0;
        const bool success=row.category==BatchPrintCategory::Success&&row.reopened&&row.correctedFeatureCount>0;
        fitted+=success;
        nonzero+=success&&row.nonzeroCorrectedFeatureCount>0;
        zero+=success&&row.nonzeroCorrectedFeatureCount==0&&row.verifiedZeroFeatureCount>0;
        partial+=success&&row.partialFitCoverage;
        nominal+=row.reopened&&row.correctedFeatureCount==0;
        failed+=row.fitStatus==QStringLiteral("fitted_manufacturing_failed");
    }
    const auto ratio=[](int numerator,int denominator){return QJsonObject{
        {"numerator",numerator},{"denominator",denominator},
        {"percent",denominator?QJsonValue(100.0*numerator/denominator):QJsonValue(QJsonValue::Null)}};};
    return {{"completedParts",results.size()},{"modelCoverage",ratio(models,results.size())},
        {"nominalPreparedReady",ready},{"nativePrintableCoverage",ratio(native,models)},
        {"sourceFitOpportunityCoverage",ratio(recognized,models)},
        {"applicableVerifiedEvidenceCoverage",ratio(sourceApplicable,recognized)},
        {"allApplicableOpportunityParts",applicable},
        {"fittedPrintSuccess",ratio(fitted,applicable)},
        {"nonzeroFittedSuccess",nonzero},{"verifiedZeroSuccess",zero},{"partialFittedSuccess",partial},
        {"nominalFallback",nominal},{"fittedManufacturingFailures",failed},
        {"note","Completed rows only. Partial overlaps nonzero/zero. Source opportunities use source-only recognizers; preparation-dependent tube/body contracts are additional opportunities, not retrospectively source-certified."}};
}

BatchPrintRun BatchPrintableModelService::run(const QVector<BatchPrintablePart>& parts,
    const BatchPrintOptions& options,CancellationState* cancellation,const Progress& progress) const
{
    CancellationState localCancellation;if(!cancellation)cancellation=&localCancellation;
    BatchPrintRun run;run.population=options.population;
    QElapsedTimer elapsed;elapsed.start();
    const QString mode=options.partReference?QStringLiteral("part_reference"):
        options.randomSample?QStringLiteral("random"):QStringLiteral("part_list");
    const FitCalibrationLibrary fitLibrary(options.fitLibraryRoot);
    QVector<FitProfile> fitProfiles;
    FitProfile selectedProfile;
    const bool explicitProfile=!options.selectedFitProfileIdentity.isEmpty();
    if(explicitProfile){
        if(!fitLibrary.loadProfile(options.selectedFitProfileIdentity,&selectedProfile,&run.diagnostic)||
           !FitCalibrationLibrary::profileCompatibility(selectedProfile,&run.diagnostic)){
            run.diagnostic=QStringLiteral("The selected managed Verified Fit Profile is unavailable or incompatible: %1").arg(run.diagnostic);
            return run;
        }
        fitProfiles.append(selectedProfile);
        run.diagnostic.clear();
    }else if(options.autoFitEnabled){
        for(const auto& summary:fitLibrary.profiles()){
            FitProfile profile;
            if(summary.compatible&&fitLibrary.loadProfile(summary.identity,&profile,nullptr))fitProfiles.append(profile);
        }
        std::sort(fitProfiles.begin(),fitProfiles.end(),[](const auto& a,const auto& b){return a.profileIdentity<b.profileIdentity;});
    }
    QJsonArray profiles;
    for(const auto& profile:fitProfiles)profiles.append(FitProfileJson::toJson(profile));
    QJsonObject printingContext{{"version",1},{"selection",explicitProfile?"explicit":"automatic"},
        {"autoFitEnabled",explicitProfile||options.autoFitEnabled},{"selectedProfileIdentity",options.selectedFitProfileIdentity},
        {"selectedProfileName",selectedProfile.name},{"profiles",profiles},
        {"printOrientation",options.printOrientation.summary()},{"profileFormatVersion",FitProfileJson::CurrentFormatVersion}};
    printingContext.insert("fingerprint",QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(printingContext).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex()));
    const auto savedContext=options.corpusPlan.value("printingContext").toObject();
    if(options.partReference&&((!savedContext.isEmpty()&&savedContext!=printingContext)||
       (savedContext.isEmpty()&&options.continuationPlan&&(explicitProfile||!options.autoFitEnabled||!options.printOrientation.isIdentity())))){
        run.diagnostic=QStringLiteral("Saved plan printing context differs from this selection. Refresh current Part Reference to create a new run with the intended profile/orientation; continuation was not started.");
        return run;
    }
    if(options.partReference&&!validPartReferenceSelection(parts,options.corpusPlan)){
        run.diagnostic=QStringLiteral("The selected range does not match a compatible fingerprinted Part Reference corpus.");return run;
    }
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
    if(!QDir().mkpath(QDir(run.runDirectory).filePath(QStringLiteral("exports/success")))||
       !QDir().mkpath(QDir(run.runDirectory).filePath(QStringLiteral("exports/diagnostic")))){
        run.diagnostic=QStringLiteral("Could not create the audit output directory.");return run;
    }
    run.metadataPath=QDir(run.runDirectory).filePath(QStringLiteral("run-metadata.json"));
    QJsonObject metadata{
        {QStringLiteral("runId"),runId},
        {QStringLiteral("localOverridePolicy"),QStringLiteral("existing-validated-nominal-after-native-preparation-failure-v1")},
        {QStringLiteral("auditPreparationProfile"),QString::fromLatin1(LDrawPrintPreparationProfile::Version)},
        {QStringLiteral("booleanExecutionPolicy"),QStringLiteral("isolated-v1: 30s / 512MiB / 50000 input faces per operation")},
        {QStringLiteral("maximumSequentialBooleanOperations"),int(LDrawPrintPreparationProfile::MaximumSequentialBooleanOperations)},
        {QStringLiteral("diagnosticMaximumFaces"),50000},
        {QStringLiteral("diagnosticMaximumVertices"),150000},
        {QStringLiteral("sampleMode"),mode},
        {QStringLiteral("seed"),static_cast<qint64>(options.seed)},
        {QStringLiteral("requestedEligibleCount"),options.requestedEligibleCount},
        {QStringLiteral("actualSampledCount"),parts.size()},
        {QStringLiteral("catalogPopulation"),run.population.catalogTotal},
        {QStringLiteral("excludedNoColor"),run.population.excludedNoColor},
        {QStringLiteral("excludedStickerCategory"),run.population.excludedStickerCategory},
        {QStringLiteral("excludedNonstandardId"),run.population.excludedNonstandardId},
        {QStringLiteral("excludedNoModel"),run.population.excludedNoModel},
        {QStringLiteral("excludeNoModel"),options.randomSample&&options.excludeNoModel},
        {QStringLiteral("excludeStickerCategory"),!options.partReference},
        {QStringLiteral("eligiblePopulation"),run.population.eligibleTotal},
        {QStringLiteral("excludeNoColor"),!options.partReference},
        {QStringLiteral("excludeNonstandardIds"),!options.partReference&&options.excludeNonstandardIds},
        {QStringLiteral("partIdEligibilityRule"),QStringLiteral("^[0-9]+[A-Za-z]?$")}
    };
    metadata.insert("auditSchemaVersion",7);
    metadata.insert("printingContext",printingContext);
    metadata.insert("sourceInspectionPolicy","existing-source-only-recognizers-v1; tube/body-closure contracts are inspected only if preparation establishes them");
    metadata.insert("startedUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    metadata.insert("applicationVersion",QCoreApplication::applicationVersion());
    metadata.insert("qtVersion",QString::fromLatin1(qVersion()));
    metadata.insert("autoFitEnabled",options.autoFitEnabled);
    metadata.insert("libraryRoot",options.libraryRoot);
    if(options.partReference){
        metadata.insert("corpusFingerprint",options.corpusPlan.value("corpusFingerprint"));
        metadata.insert("manifestPositionCount",options.corpusPlan.value("manifestPositionCount"));
        metadata.insert("uniquePartCount",options.corpusPlan.value("uniquePartCount"));
        metadata.insert("duplicateMembershipCount",options.corpusPlan.value("duplicateMembershipCount"));
        metadata.insert("structurallyInvalidEntries",options.corpusPlan.value("structurallyInvalidEntries"));
        metadata.insert("firstReferenceSequence",parts.front().referenceSequence);
        metadata.insert("lastReferenceSequence",parts.back().referenceSequence);
        metadata.insert("selectedRange",summarizePartReference(parts,{}));
        auto savedPlan=options.corpusPlan;savedPlan.insert("printingContext",printingContext);
        if(!saveJson(QDir(run.runDirectory).filePath("part-reference-plan.json"),savedPlan,&run.diagnostic))return run;
    }
    if(!saveJson(run.metadataPath,metadata,&run.diagnostic))return run;
    run.statePath=QDir(run.runDirectory).filePath(QStringLiteral("run-state.json"));
    QJsonArray sample;QJsonObject candidates;
    for(const auto& part:parts){sample.append(part.partNumber);
        candidates.insert(part.partNumber,QJsonArray::fromStringList(part.ldrawCandidates));}
    QJsonObject state=metadata;
    state.insert(QStringLiteral("stateSchemaVersion"),2);
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
    csvFile.write("csv_schema_version,run_id,seed,sample_mode,requested_eligible_count,exclude_no_color,exclude_nonstandard_ids,part_id_eligibility_rule,sample_sequence,part_number,category_id,rebrickable_category_id,category_name,ldraw_model,result_category,preparation_route,source_triangles,source_groups,coverage_complete,prepare_ms,prepared_vertices,prepared_faces,recognized_features,fit_resolution,profile_identity,corrections,export_path,reopened,diagnostic_export_available,diagnostic_export_path,total_ms,diagnostic,native_result_category,native_diagnostic,local_override_state,local_override_used,local_override_stale,local_override_diagnostic,local_override_route,local_override_identity,export_result,reopen_result,canonical_part_id,part_reference_sequence,part_reference_memberships,model_state,eligibility_notes,geometry_result,fit_status,source_recognized_feature_count,recognized_feature_count,applicable_verified_feature_count,corrected_feature_count,nonzero_corrected_feature_count,verified_zero_feature_count,partial_fit_coverage,selected_fit_profile,print_orientation,nominal_prepared_ready,source_fit_features,printing_context_fingerprint,source_applicable_verified_feature_count,fit_diagnostic,fitted_export_succeeded\n");
    const auto save=[&](const BatchPrintResult& row){
        const auto& part=parts[row.sequence-1];
        const QString line=QStringList{QStringLiteral("7"),csv(runId),QString::number(options.seed),
            csv(mode),
            QString::number(options.requestedEligibleCount),options.partReference?QStringLiteral("0"):QStringLiteral("1"),
            !options.partReference&&options.excludeNonstandardIds?QStringLiteral("1"):QStringLiteral("0"),
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
                QStringLiteral("failed"):QStringLiteral("not_attempted")),
            QString::number(part.partId),part.referenceSequence?QString::number(part.referenceSequence):QString(),
            csv(QString::fromUtf8(QJsonDocument(part.referenceMemberships).toJson(QJsonDocument::Compact))),
            csv(row.modelAvailabilityKnown?(row.modelAvailable?QStringLiteral("model_available"):QStringLiteral("model_unavailable")):
                (!row.ldrawModel.isEmpty()?QStringLiteral("model_available"):QStringLiteral("not_resolved"))),
            csv(eligibilityNotes(part).join(';')),csv(row.geometryResult),csv(row.fitStatus),
            QString::number(row.sourceRecognizedFeatureCount),QString::number(row.recognizedFeatureCount),
            QString::number(row.applicableVerifiedFeatureCount),QString::number(row.correctedFeatureCount),
            QString::number(row.nonzeroCorrectedFeatureCount),QString::number(row.verifiedZeroFeatureCount),
            row.partialFitCoverage?QStringLiteral("1"):QStringLiteral("0"),csv(row.selectedFitProfile),
            csv(row.printOrientation),row.nominalPreparedReady?QStringLiteral("1"):QStringLiteral("0"),
            csv(QString::fromUtf8(QJsonDocument(row.sourceFitFeatures).toJson(QJsonDocument::Compact))),
            csv(printingContext.value("fingerprint").toString()),QString::number(row.sourceApplicableVerifiedFeatureCount),
            csv(row.fitStatus==QStringLiteral("fitted_manufacturing_failed")?row.diagnostic:row.fitResolution),
            row.reopened&&row.category==BatchPrintCategory::Success&&row.correctedFeatureCount>0?QStringLiteral("1"):QStringLiteral("0")}.join(',')+QLatin1Char('\n');
        const auto bytes=line.toUtf8();
        return csvFile.write(bytes)==bytes.size()&&durableFlush(csvFile);
    };
    if(!durableFlush(csvFile)){run.diagnostic=QStringLiteral("Could not flush CSV header.");return run;}
    QFile timingsFile(QDir(run.runDirectory).filePath(QStringLiteral("stage-timings.jsonl")));
    if(!timingsFile.open(QIODevice::WriteOnly)){run.diagnostic=timingsFile.errorString();return run;}
    run.results.reserve(parts.size());
    QSet<QString> usedExportNames;
    const auto exportPathFor=[&](const BatchPrintResult& row){
        const bool success=row.reopened&&(row.category==BatchPrintCategory::Success||
            row.category==BatchPrintCategory::StrictOverrideSuccess||row.category==BatchPrintCategory::UserOverrideSuccess);
        const QString folder=success?QStringLiteral("exports/success/"):QStringLiteral("exports/diagnostic/");
        const QString base=safeFileName(row.partNumber)+QLatin1Char('-')+categoryCode(row.category);
        QString name=base+QStringLiteral(".3mf");
        if(usedExportNames.contains(folder+name)||QFileInfo::exists(QDir(run.runDirectory).filePath(folder+name))){
            const QString model=safeFileName(row.ldrawModel);
            name=base+QLatin1Char('-')+(model.isEmpty()?QStringLiteral("model-unknown"):model)+QStringLiteral(".3mf");
            int duplicate=2;
            while(usedExportNames.contains(folder+name)||QFileInfo::exists(QDir(run.runDirectory).filePath(folder+name)))
                name=base+QLatin1Char('-')+(model.isEmpty()?QStringLiteral("model-unknown"):model)+
                    QStringLiteral("-repeat%1.3mf").arg(duplicate++);
        }
        usedExportNames.insert(folder+name);
        return QDir(run.runDirectory).filePath(folder+name);
    };
    for(int index=0;index<parts.size();++index){
        if(cancellation&&cancellation->isCancelled()){run.stopped=true;break;}
        state.insert(QStringLiteral("currentSampleSequence"),index+1);
        if(options.partReference)state.insert(QStringLiteral("currentReferenceSequence"),parts[index].referenceSequence);
        state.insert(QStringLiteral("currentPartNumber"),parts[index].partNumber);
        state.insert(QStringLiteral("currentPartStatus"),QStringLiteral("active"));
        state.insert(QStringLiteral("currentBooleanOperation"),0);
        state.insert(QStringLiteral("booleanOperations"),0);
        state.insert(QStringLiteral("currentPhase"),QStringLiteral("starting"));
        state.insert(QStringLiteral("phaseTimingsMs"),QJsonObject());
        state.remove(QStringLiteral("nativeResultCategory"));
        if(!checkpoint())return run;
        if(options.phaseProgress)options.phaseProgress(index+1,parts[index].partNumber,QStringLiteral("starting"));
        if(options.beforePart)options.beforePart(run.statePath,index+1,parts[index].partNumber);
        const auto&part=parts[index];BatchPrintResult row;row.sequence=index+1;row.partNumber=part.partNumber;
        row.selectedFitProfile=options.selectedFitProfileIdentity;row.printOrientation=options.printOrientation.summary();
        if(options.partReference){
            row.modelAvailabilityKnown=true;
            row.ldrawModel=modelFor(part,options.libraryRoot);
            row.modelAvailable=!row.ldrawModel.isEmpty();
        }
        QElapsedTimer phaseTimer;phaseTimer.start();QString currentPhase=QStringLiteral("starting");
        QJsonObject stageTimes;bool persistenceOk=true;
        const auto phase=[&](const QString& name){
            if(!persistenceOk)return; // Preserve the first failure and durable recovery point.
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
                    row.category=BatchPrintCategory::NoCatalogPart;
                    row.diagnostic=part.partNumber.isEmpty()?QStringLiteral("Empty Part identity."):
                        QStringLiteral("Part was not found in the active canonical catalog.");
                    break;}
                if(!options.partReference&&part.noColor){row.category=BatchPrintCategory::SkippedNoColor;
                    row.diagnostic=QStringLiteral("No Color / Sticker catalog Part.");break;}
                if(!options.partReference&&isStickerCategory(part)){row.category=BatchPrintCategory::SkippedStickerCategory;
                    row.diagnostic=QStringLiteral("Sticker catalog category is outside the print audit population.");break;}
                if(!options.partReference&&options.excludeNonstandardIds&&!isStandardAuditPartNumber(part.partNumber)){
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
                phase(QStringLiteral("source_fit_inspection"));
                if(!persistenceOk||(cancellation&&cancellation->isCancelled()))break;
                auto fitFeatures=ManufacturingMeshService::inspectSourceFitFeatures(source);
                row.sourceRecognizedFeatureCount=fitFeatures.size();
                row.sourceFitFeatures=sourceFeatureJson(fitFeatures,options.printOrientation);
                measureFit(row,fitFeatures,fitProfiles,explicitProfile,explicitProfile||options.autoFitEnabled,options.printOrientation);
                row.sourceApplicableVerifiedFeatureCount=row.applicableVerifiedFeatureCount;
                row.fitResolution=QStringLiteral("Source inspection: %1 recognized interfaces; %2 have compatible Verified evidence. Application requires successful native preparation and ManufacturingMesh validation.")
                    .arg(row.sourceRecognizedFeatureCount).arg(row.sourceApplicableVerifiedFeatureCount);
                PrintPreparationRequest request;request.partReference=part.partNumber;
                request.ldrawIdentity=row.ldrawModel;request.libraryAuthority=options.libraryRoot;
                request.loadResult=source;
                phase(QStringLiteral("preparing"));
                if(!persistenceOk)break;
                QElapsedTimer preparationTimer;preparationTimer.start();
                const auto prepared=LDrawPrintPreparationService().prepare(request,cancellation,[&](const PrintPreparationProgress& p){
                    static const char* names[]={"source_analysis","semantic_construction","operand_validation","boolean_composition","final_validation","diagnostic_candidate"};
                    if(p.phase==PrintPreparationPhase::BooleanComposition){
                        state.insert(QStringLiteral("booleanOperations"),p.totalOperations);
                        state.insert(QStringLiteral("currentBooleanOperation"),p.currentOperation);
                    }
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
                row.nominalPreparedReady=true;
                appendFitFeatures(fitFeatures,prepared.preparedMesh->functionalFeatures);
                measureFit(row,fitFeatures,fitProfiles,explicitProfile,explicitProfile||options.autoFitEnabled,options.printOrientation);
                if(cancellation&&cancellation->isCancelled()){row.category=BatchPrintCategory::Cancelled;break;}
                phase(QStringLiteral("manufacturing"));
                PrintMesh output=prepared.preparedMesh->mesh;
                const auto fit=explicitProfile
                    ?AutoFitProfileResolver::resolveExplicit(selectedProfile,part.partNumber,source,options.printOrientation)
                    :AutoFitProfileResolver::resolve(options.autoFitEnabled,part.partNumber,fitProfiles,source,options.printOrientation);
                row.fitResolution=fit.diagnostic;
                if(fit.resolved()){
                    row.profileIdentity=fit.profile.profileIdentity;
                    const auto manufacturing=ManufacturingMeshService().generate(source,
                        *prepared.preparedMesh,fit.profile,options.printOrientation);
                    if(!manufacturing.ok()||!manufacturing.manufacturingMesh){
                        row.category=BatchPrintCategory::ManufacturingFailed;
                        row.fitStatus=QStringLiteral("fitted_manufacturing_failed");
                        row.diagnostic=manufacturing.diagnostic;
                        diagnosticExport(&prepared.preparedMesh->mesh);break;
                    }
                    for(const auto& applied:manufacturing.manufacturingMesh->appliedFitFeatures){
                        ++row.correctedFeatureCount;
                        if(applied.nonzero)++row.nonzeroCorrectedFeatureCount;else ++row.verifiedZeroFeatureCount;
                    }
                    row.partialFitCoverage=row.correctedFeatureCount>0&&row.correctedFeatureCount<row.recognizedFeatureCount;
                    row.fitStatus=row.partialFitCoverage?QStringLiteral("partial_verified_fit"):
                        row.nonzeroCorrectedFeatureCount>0?QStringLiteral("verified_nonzero_applied"):
                        row.verifiedZeroFeatureCount>0?QStringLiteral("verified_zero_applied"):
                        QStringLiteral("nominal_no_verified_application");
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
        // A crash, write failure or failed reopen must never leave an apparent
        // accepted file in success/. Publish there only after successful reopen.
        if(row.reopened&&!row.exportPath.isEmpty()){
            const auto acceptedPath=exportPathFor(row);
            if(QFile::rename(row.exportPath,acceptedPath))row.exportPath=acceptedPath;
            else{
                row.category=BatchPrintCategory::ExportFailed;
                row.diagnostic=QStringLiteral("Validated 3MF could not be published to exports/success; file remains diagnostic: %1").arg(row.exportPath);
            }
        }
        if(row.nativeCategory==BatchPrintCategory::NotStarted){
            row.nativeCategory=row.category;row.nativeDiagnostic=row.diagnostic;
        }
        row.geometryResult=row.nominalPreparedReady?QStringLiteral("native_prepared_ready"):
            row.localOverrideUsed?QStringLiteral("local_override_ready"):categoryCode(row.nativeCategory);
        if(row.localOverrideUsed)row.fitStatus=QStringLiteral("local_override_nominal");
        phase(QStringLiteral("persisting"));
        if(!persistenceOk)return run;
        run.results.push_back(row);
        if(!save(row)){
            run.diagnostic=QStringLiteral("The audit CSV could not be flushed: %1").arg(csvFile.errorString());
            csvFile.close();return run;
        }
        phase(QStringLiteral("run_state"));
        if(!persistenceOk)return run;
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
    metadata.insert("elapsedMilliseconds",elapsed.elapsed());
    metadata.insert("status",run.stopped?"stopped":"completed");
    metadata.insert("completedCount",run.results.size());
    if(options.partReference){
        run.referenceSummary=summarizePartReference(parts,run.results);
        run.referenceSummary.insert("elapsedMilliseconds",elapsed.elapsed());
        run.referenceSummary.insert("corpusFingerprint",options.corpusPlan.value("corpusFingerprint"));
        run.referenceSummary.insert("manifestPositionCount",options.corpusPlan.value("manifestPositionCount"));
        metadata.insert("summary",run.referenceSummary);
        QJsonObject failures{{"source_coverage",QJsonArray()},{"resource_limit",QJsonArray()},
            {"no_model",QJsonArray()},{"unexpected_failures",QJsonArray()}};
        for(const auto& row:run.results){
            QStringList groups;
            if(!row.modelAvailable)groups.append("no_model");
            if(row.nativeCategory==BatchPrintCategory::SourceCoverage)groups.append("source_coverage");
            else if(row.nativeCategory==BatchPrintCategory::ResourceLimit)groups.append("resource_limit");
            else if(row.category!=BatchPrintCategory::Success&&row.category!=BatchPrintCategory::StrictOverrideSuccess&&
                row.category!=BatchPrintCategory::UserOverrideSuccess&&row.category!=BatchPrintCategory::NoLDrawModel)
                groups.append("unexpected_failures");
            for(const auto& group:groups){auto list=failures.value(group).toArray();
                list.append(QJsonObject{{"partNumber",row.partNumber},{"sampleSequence",row.sequence},
                    {"referenceSequence",parts[row.sequence-1].referenceSequence},
                    {"nativeResultCategory",categoryCode(row.nativeCategory)},
                    {"resultCategory",categoryCode(row.category)},{"diagnostic",row.diagnostic}});
                failures.insert(group,list);}
        }
        failures.insert("note","Review lists are observations, not an automatic recommendation to change geometry.");
        if(!saveJson(QDir(run.runDirectory).filePath("summary.json"),run.referenceSummary,&run.diagnostic)||
           !saveJson(QDir(run.runDirectory).filePath("failure-review.json"),failures,&run.diagnostic))return run;
    }
    const auto fitSummary=summarizeFit(run.results);
    metadata.insert("fitSummary",fitSummary);
    QJsonObject summary=run.referenceSummary;
    summary.insert("printingContext",printingContext);summary.insert("fitSummary",fitSummary);
    summary.insert("status",run.stopped?"stopped":"completed");
    if(!saveJson(QDir(run.runDirectory).filePath("summary.json"),summary,&run.diagnostic))return run;
    if(!saveJson(run.metadataPath,metadata,&run.diagnostic))return run;
    state.insert(QStringLiteral("status"),run.stopped?QStringLiteral("stopped"):QStringLiteral("completed"));
    if(!checkpoint())return run;
    csvFile.close();run.ok=true;
    return run;
}

} // namespace PrintGeometry
