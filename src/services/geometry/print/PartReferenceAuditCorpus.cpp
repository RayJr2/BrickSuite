#include "BatchPrintableModelService.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QSet>
#include <QMap>

namespace PrintGeometry {
namespace {
QString fingerprint(const QJsonObject& plan)
{
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(plan).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex());
}
QJsonObject partJson(const BatchPrintablePart& part)
{
    return {{"partId",part.partId},{"partNumber",part.partNumber},{"category",part.category},
        {"categoryId",part.categoryId},{"rebrickableCategoryId",part.rebrickableCategoryId},
        {"material",part.material},{"catalogPresent",part.catalogPresent},{"noColor",part.noColor},
        {"ldrawCandidates",QJsonArray::fromStringList(part.ldrawCandidates)},
        {"referenceSequence",part.referenceSequence},{"memberships",part.referenceMemberships}};
}
QString key(const BatchPrintablePart& part)
{
    return part.partId>0?QStringLiteral("id:%1").arg(part.partId):
        QStringLiteral("unresolved:")+part.partNumber.trimmed().toCaseFolded();
}
}

BatchPrintCorpus BatchPrintableModelService::partReferenceCorpus(const QList<PartReferenceEntry>& entries,
    const QVector<BatchPrintablePart>& catalog,const QString& libraryRoot)
{
    BatchPrintCorpus result;
    QHash<QString,BatchPrintablePart> byNumber;
    QHash<int,BatchPrintablePart> byId;
    for(const auto& part:catalog){byNumber.insert(part.partNumber.trimmed().toCaseFolded(),part);
        if(part.partId>0)byId.insert(part.partId,part);}
    QHash<QString,int> unique;
    QJsonArray invalid;
    int position=0,duplicates=0;
    for(const auto& entry:entries){
        ++position;
        if(entry.partNumber.trimmed().isEmpty()){
            invalid.append(QJsonObject{{"position",position},{"reason","empty_part_identity"}});continue;
        }
        auto part=entry.partId>0&&byId.contains(entry.partId)?byId.value(entry.partId):
            byNumber.value(entry.partNumber.trimmed().toCaseFolded());
        if(part.partNumber.isEmpty()){
            part.partNumber=entry.partNumber.trimmed();part.category=entry.sourceCategory;
            part.rebrickableCategoryId=entry.sourceCategoryId;part.material=entry.material;
        }
        const QString identity=key(part);
        if(!unique.contains(identity)){
            part.referenceSequence=result.parts.size()+1;
            unique.insert(identity,result.parts.size());result.parts.push_back(part);
        }else ++duplicates;
        result.parts[unique.value(identity)].referenceMemberships.append(QJsonObject{
            {"catalog",entry.catalog},{"section",entry.section},{"manifestPosition",position},
            {"displayOrder",entry.displayOrder},{"entryPartNumber",entry.partNumber},
            {"origin",entry.origin==PartReferenceEntry::Origin::User?"user":"built_in"}});
    }
    resolveModels(result.parts,libraryRoot);
    QJsonArray ordered;
    for(const auto& part:result.parts)ordered.append(partJson(part));
    result.plan={{"planSchemaVersion",1},{"auditSchemaVersion",6},
        {"preparationPolicy",QString::fromLatin1(LDrawPrintPreparationProfile::Version)},
        {"manifestPositionCount",position},{"uniquePartCount",result.parts.size()},
        {"duplicateMembershipCount",duplicates},{"structurallyInvalidEntries",invalid},
        {"orderedCorpus",ordered}};
    result.plan.insert("corpusFingerprint",fingerprint(result.plan));
    if(result.parts.isEmpty())result.diagnostic=QStringLiteral("Part Reference contains no usable Part entries.");
    return result;
}

BatchPrintCorpus BatchPrintableModelService::readPartReferencePlan(const QString& path,const QString& libraryRoot)
{
    BatchPrintCorpus result;
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)||file.size()>32*1024*1024){
        result.diagnostic=QStringLiteral("Cannot read a bounded Part Reference plan.");return result;}
    QJsonParseError error;
    result.plan=QJsonDocument::fromJson(file.readAll(),&error).object();
    auto signedPlan=result.plan;const auto savedHash=signedPlan.take("corpusFingerprint").toString();
    const auto ordered=result.plan.value("orderedCorpus").toArray();
    if(error.error!=QJsonParseError::NoError||savedHash.isEmpty()||fingerprint(signedPlan)!=savedHash||
       result.plan.value("planSchemaVersion").toInt()!=1||result.plan.value("auditSchemaVersion").toInt()!=6||
       result.plan.value("preparationPolicy").toString()!=QString::fromLatin1(LDrawPrintPreparationProfile::Version)||
       ordered.isEmpty()||ordered.size()>100000||result.plan.value("uniquePartCount").toInt()!=ordered.size()){
        result.diagnostic=QStringLiteral("Incompatible or damaged Part Reference plan; no run was started.");return result;}
    QSet<QString> identities;
    for(const auto& item:ordered){
        const auto object=item.toObject();BatchPrintablePart part;
        part.partId=object.value("partId").toInt();part.partNumber=object.value("partNumber").toString();
        part.category=object.value("category").toString();part.categoryId=object.value("categoryId").toInt();
        part.rebrickableCategoryId=object.value("rebrickableCategoryId").toInt();
        part.material=object.value("material").toString();part.catalogPresent=object.value("catalogPresent").toBool();
        part.noColor=object.value("noColor").toBool();part.referenceSequence=object.value("referenceSequence").toInt();
        part.referenceMemberships=object.value("memberships").toArray();
        for(const auto& candidate:object.value("ldrawCandidates").toArray())part.ldrawCandidates.append(candidate.toString());
        if(part.partNumber.isEmpty()||part.referenceSequence!=result.parts.size()+1||
           part.referenceMemberships.isEmpty()||identities.contains(key(part))){
            result.parts.clear();result.diagnostic=QStringLiteral("Invalid canonical Part/order in saved corpus.");return result;}
        identities.insert(key(part));result.parts.append(part);
    }
    resolveModels(result.parts,libraryRoot);
    return result;
}

bool BatchPrintableModelService::validPartReferenceSelection(const QVector<BatchPrintablePart>& parts,const QJsonObject& plan)
{
    auto content=plan;const auto hash=content.take("corpusFingerprint").toString();
    const auto ordered=plan.value("orderedCorpus").toArray();
    if(parts.isEmpty()||hash.isEmpty()||fingerprint(content)!=hash||
       plan.value("planSchemaVersion").toInt()!=1||plan.value("auditSchemaVersion").toInt()!=6||
       plan.value("preparationPolicy").toString()!=QString::fromLatin1(LDrawPrintPreparationProfile::Version))return false;
    int expected=parts.front().referenceSequence;
    for(const auto& part:parts){
        if(part.referenceSequence!=expected++||part.referenceSequence<1||part.referenceSequence>ordered.size()||
           partJson(part)!=ordered[part.referenceSequence-1].toObject())return false;
    }
    return true;
}

QJsonObject BatchPrintableModelService::summarizePartReference(const QVector<BatchPrintablePart>& parts,
    const QVector<BatchPrintResult>& results)
{
    const auto summarizeGroup=[&](const QVector<int>& indices){
        int models=0,native=0,practical=0,overrides=0,coverage=0,resources=0,other=0,completed=0;
        for(int index:indices){
            const BatchPrintResult* row=index<results.size()?&results[index]:nullptr;
            const bool available=row&&row->modelAvailabilityKnown?row->modelAvailable:!parts[index].resolvedModel.isEmpty();
            models+=available;
            if(!row)continue;
            ++completed;
            native+=row->nativeCategory==BatchPrintCategory::Success;
            const bool recovered=row->category==BatchPrintCategory::StrictOverrideSuccess||
                row->category==BatchPrintCategory::UserOverrideSuccess;
            overrides+=recovered;practical+=recovered||row->category==BatchPrintCategory::Success;
            coverage+=row->nativeCategory==BatchPrintCategory::SourceCoverage;
            resources+=row->nativeCategory==BatchPrintCategory::ResourceLimit;
            other+=row->category!=BatchPrintCategory::Success&&!recovered&&
                row->nativeCategory!=BatchPrintCategory::SourceCoverage&&
                row->nativeCategory!=BatchPrintCategory::ResourceLimit&&
                row->category!=BatchPrintCategory::NoLDrawModel;
        }
        return QJsonObject{{"uniqueParts",indices.size()},{"completedParts",completed},
            {"modelBearingParts",models},{"noModelParts",indices.size()-models},
            {"nativeSuccesses",native},{"nativeSuccessPercent",models?100.0*native/models:0.0},
            {"overrideRecoveries",overrides},{"practicalSuccesses",practical},
            {"practicalSuccessPercent",models?100.0*practical/models:0.0},
            {"sourceCoverageFailures",coverage},{"resourceLimitFailures",resources},{"otherFailures",other}};
    };
    QVector<int> all;QMap<QString,QVector<int>> catalogs;
    for(int index=0;index<parts.size();++index){
        all.append(index);QSet<QString> memberships;
        for(const auto& member:parts[index].referenceMemberships)
            memberships.insert(member.toObject().value("catalog").toString());
        for(const auto& catalog:memberships)catalogs[catalog].append(index);
    }
    QJsonArray groups;
    for(auto it=catalogs.cbegin();it!=catalogs.cend();++it){auto group=summarizeGroup(it.value());
        group.insert("catalog",it.key());groups.append(group);}
    auto summary=summarizeGroup(all);summary.insert("catalogs",groups);
    summary.insert("catalogDenominatorNote","Membership views overlap; do not sum catalogs to reconstruct the global denominator.");
    summary.insert("rateDenominator","All model-bearing Parts in the selected range; incomplete runs are partial measurements.");
    return summary;
}
} // namespace PrintGeometry
