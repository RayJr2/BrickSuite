#include "RemoteBuildMutationDtos.h"

#include <QDateTime>
#include <QJsonArray>
#include <QSet>
#include <climits>

namespace {
constexpr qint64 MaxJsonInteger = 9007199254740991LL;
void invalid(RemoteMutationDto::Error* error, const QString& message)
{ if (error) *error = {QStringLiteral("INVALID_ARGUMENT"), message, false}; }
bool text(const QJsonValue& value, QString* out, bool allowEmpty = false, int maximum = 4000)
{ if (!value.isString()) return false; *out=value.toString(); return out->size()<=maximum&&(allowEmpty||!out->trimmed().isEmpty()); }
bool id(const QJsonValue& value, qint64* out, bool zero = false)
{ if(!value.isDouble())return false;const double d=value.toDouble();if(d!=qint64(d)||d<(zero?0:1)||d>MaxJsonInteger)return false;*out=qint64(d);return true; }
bool integer(const QJsonValue&value,qint64 minimum,qint64 maximum,qint64*out)
{if(!value.isDouble())return false;const double d=value.toDouble();if(d!=qint64(d)||d<minimum||d>maximum)return false;*out=qint64(d);return true;}
QJsonObject expectedJson(const RemoteBuildMutationDto::ExpectedState& e)
{ return {{"modifiedUtc",e.modifiedUtc},{"buildType",e.buildType},{"reference",e.reference},{"inventoryMode",e.inventoryMode},{"manufacturer",e.manufacturer},{"name",e.name},{"status",e.status},{"notes",e.notes},{"active",e.active}}; }
bool parseExpected(const QJsonObject& o, RemoteBuildMutationDto::ExpectedState* e)
{ return o.size()==9&&text(o["modifiedUtc"],&e->modifiedUtc)&&QDateTime::fromString(e->modifiedUtc,Qt::ISODateWithMs).isValid()&&text(o["buildType"],&e->buildType)&&text(o["reference"],&e->reference,true,200)&&text(o["inventoryMode"],&e->inventoryMode)&&text(o["manufacturer"],&e->manufacturer,true,200)&&text(o["name"],&e->name,false,500)&&text(o["status"],&e->status)&&text(o["notes"],&e->notes,true)&&o["active"].isBool()&&(e->active=o["active"].toBool(),true); }
QJsonObject requirementExpectedJson(const RemoteBuildMutationDto::RequirementExpectedState&e)
{return{{"requirementId",double(e.requirementId)},{"buildId",double(e.buildId)},{"modifiedUtc",e.modifiedUtc},{"partNumber",e.partNumber},{"rebrickableColorId",e.rebrickableColorId},{"substitutePartNumber",e.substitutePartNumber},{"substituteRebrickableColorId",e.substituteRebrickableColorId},{"quantityRequired",e.quantityRequired},{"quantityPulled",e.quantityPulled},{"quantityReleased",e.quantityReleased},{"spare",e.spare}};}
bool parseRequirementExpected(const QJsonObject&o,RemoteBuildMutationDto::RequirementExpectedState*e)
{qint64 color=0,subColor=0,required=0,pulled=0,released=0;return o.size()==11&&id(o["requirementId"],&e->requirementId)&&id(o["buildId"],&e->buildId)&&text(o["modifiedUtc"],&e->modifiedUtc)&&QDateTime::fromString(e->modifiedUtc,Qt::ISODateWithMs).isValid()&&text(o["partNumber"],&e->partNumber,false,100)&&integer(o["rebrickableColorId"],0,INT_MAX,&color)&&text(o["substitutePartNumber"],&e->substitutePartNumber,true,100)&&integer(o["substituteRebrickableColorId"],-1,INT_MAX,&subColor)&&id(o["quantityRequired"],&required)&&id(o["quantityPulled"],&pulled,true)&&id(o["quantityReleased"],&released,true)&&required<=1000000&&pulled<=1000000&&released<=1000000&&o["spare"].isBool()&&(e->rebrickableColorId=int(color),e->substituteRebrickableColorId=int(subColor),e->quantityRequired=int(required),e->quantityPulled=int(pulled),e->quantityReleased=int(released),e->spare=o["spare"].toBool(),true);}
QJsonObject allocationJson(const RemoteBuildMutationDto::AllocationRow&a)
{return{{"allocationId",double(a.allocationId)},{"inventoryRecordId",double(a.inventoryRecordId)},{"quantity",a.quantity},{"expectedQuantity",a.expectedQuantity},{"modifiedUtc",a.modifiedUtc},{"inventoryModifiedUtc",a.inventoryModifiedUtc},{"inventoryQuantity",a.inventoryQuantity}};}
bool parseAllocation(const QJsonObject&o,RemoteBuildMutationDto::AllocationRow*a)
{qint64 q=0,eq=0,iq=0;return o.size()==7&&id(o["allocationId"],&a->allocationId,true)&&id(o["inventoryRecordId"],&a->inventoryRecordId)&&id(o["quantity"],&q,true)&&q<=1000000&&id(o["expectedQuantity"],&eq,true)&&eq<=1000000&&text(o["modifiedUtc"],&a->modifiedUtc,true)&&text(o["inventoryModifiedUtc"],&a->inventoryModifiedUtc,true)&&id(o["inventoryQuantity"],&iq,true)&&iq<=100000000&&(a->quantity=int(q),a->expectedQuantity=int(eq),a->inventoryQuantity=int(iq),true);}
}

namespace RemoteBuildMutationDto {

RemoteMutationDto::Metadata toMetadata(const QString& operation, const Request& r)
{
    QJsonObject mutation, expected;
    if(operation==QStringLiteral("builds.requirements.add")){
        mutation={{"buildId",double(r.buildId)},{"partNumber",r.partNumber},{"rebrickableColorId",r.rebrickableColorId},{"substitutePartNumber",r.substitutePartNumber},{"substituteRebrickableColorId",r.substituteRebrickableColorId},{"quantityRequired",r.quantityRequired},{"spare",r.spare}};
    }else if(operation==QStringLiteral("builds.requirements.edit")){
        mutation={{"requirementId",double(r.requirementId)},{"substitutePartNumber",r.substitutePartNumber},{"substituteRebrickableColorId",r.substituteRebrickableColorId},{"quantityRequired",r.quantityRequired},{"spare",r.spare}};expected=requirementExpectedJson(r.expectedRequirement);
    }else if(operation==QStringLiteral("builds.requirements.remove")){
        mutation={{"requirementId",double(r.requirementId)}};expected=requirementExpectedJson(r.expectedRequirement);
    }else if(operation==QStringLiteral("builds.allocations.set")){
        mutation={{"requirementId",double(r.requirementId)}};QJsonArray rows;for(const auto&a:r.allocations)rows.append(allocationJson(a));mutation["allocations"]=rows;expected=requirementExpectedJson(r.expectedRequirement);
    }else if(operation==QStringLiteral("builds.allocateAvailable")){
        mutation={{"buildId",double(r.buildId)},{"preferredStorageId",double(r.preferredStorageId)}};expected=expectedJson(r.expected);
    } else if (operation == QStringLiteral("builds.add")) {
        mutation={{"buildType",r.buildType},{"reference",r.reference},{"inventoryMode",r.inventoryMode},{"manufacturer",r.manufacturer},{"initialStatus",r.initialStatus},{"name",r.name},{"notes",r.notes}};
    } else {
        mutation={{"buildId",double(r.buildId)}};
        expected=expectedJson(r.expected);
        if(operation==QStringLiteral("builds.edit")){mutation["name"]=r.name;mutation["manufacturer"]=r.manufacturer;mutation["notes"]=r.notes;}
        else if(operation==QStringLiteral("builds.setActive"))mutation["desiredActive"]=r.desiredActive;
        else if(operation==QStringLiteral("builds.cancel")){
            mutation["linkedCollectionState"]=r.linkedCollectionState;
            QJsonArray rows;for(const auto& row:r.returns)rows.append(QJsonObject{{"requirementId",double(row.requirementId)},{"manufacturer",row.manufacturer},{"storageId",double(row.storageId)},{"quantity",row.quantity},{"spare",row.spare}});mutation["returns"]=rows;
        }
    }
    return {r.workspaceId,r.mutationId,expected,mutation};
}

bool fromMetadata(const QString& operation, const RemoteMutationDto::Metadata& md,
                  Request* out, RemoteMutationDto::Error* error)
{
    if(!out)return false;Request r;r.workspaceId=md.workspaceId;r.mutationId=md.mutationId;
    const bool reqAdd=operation=="builds.requirements.add",reqEdit=operation=="builds.requirements.edit",reqRemove=operation=="builds.requirements.remove",allocSet=operation=="builds.allocations.set",allocate=operation=="builds.allocateAvailable";
    const bool add=operation=="builds.add",edit=operation=="builds.edit",active=operation=="builds.setActive",complete=operation=="builds.complete",cancel=operation=="builds.cancel";
    if(!reqAdd&&!reqEdit&&!reqRemove&&!allocSet&&!allocate&&!add&&!edit&&!active&&!complete&&!cancel){invalid(error,"Unsupported Build mutation.");return false;}
    if(reqAdd){qint64 color=0,subColor=0,quantity=0;if(!md.expected.isEmpty()||md.mutation.size()!=7||!id(md.mutation["buildId"],&r.buildId)||!text(md.mutation["partNumber"],&r.partNumber,false,100)||!integer(md.mutation["rebrickableColorId"],0,INT_MAX,&color)||!text(md.mutation["substitutePartNumber"],&r.substitutePartNumber,true,100)||!integer(md.mutation["substituteRebrickableColorId"],-1,INT_MAX,&subColor)||!id(md.mutation["quantityRequired"],&quantity)||quantity>1000000||!md.mutation["spare"].isBool()){invalid(error,"The requirement Add request is invalid.");return false;}r.rebrickableColorId=int(color);r.substituteRebrickableColorId=int(subColor);r.quantityRequired=int(quantity);r.spare=md.mutation["spare"].toBool();
    }else if(reqEdit||reqRemove||allocSet){if(!parseRequirementExpected(md.expected,&r.expectedRequirement)||!id(md.mutation["requirementId"],&r.requirementId)||r.requirementId!=r.expectedRequirement.requirementId){invalid(error,"The requirement expected state is invalid.");return false;}if(reqEdit){qint64 subColor=0,quantity=0;if(md.mutation.size()!=5||!text(md.mutation["substitutePartNumber"],&r.substitutePartNumber,true,100)||!integer(md.mutation["substituteRebrickableColorId"],-1,INT_MAX,&subColor)||!id(md.mutation["quantityRequired"],&quantity)||quantity>1000000||!md.mutation["spare"].isBool()){invalid(error,"The requirement Edit request is invalid.");return false;}r.substituteRebrickableColorId=int(subColor);r.quantityRequired=int(quantity);r.spare=md.mutation["spare"].toBool();}else if(reqRemove){if(md.mutation.size()!=1){invalid(error,"The requirement Remove request is invalid.");return false;}}else{if(md.mutation.size()!=2||!md.mutation["allocations"].isArray()||md.mutation["allocations"].toArray().size()>500){invalid(error,"The allocation-set request is invalid.");return false;}QSet<qint64> seen;for(const auto&v:md.mutation["allocations"].toArray()){if(!v.isObject()){invalid(error,"An allocation row is invalid.");return false;}AllocationRow a;if(!parseAllocation(v.toObject(),&a)||seen.contains(a.inventoryRecordId)){invalid(error,"An allocation row is invalid or duplicated.");return false;}seen.insert(a.inventoryRecordId);r.allocations.append(a);}}
    }else if(allocate){if(!parseExpected(md.expected,&r.expected)||md.mutation.size()!=2||!id(md.mutation["buildId"],&r.buildId)||!id(md.mutation["preferredStorageId"],&r.preferredStorageId,true)){invalid(error,"The Allocate Available request is invalid.");return false;}
    }else if(add){
        if(!md.expected.isEmpty()||md.mutation.size()!=7||!text(md.mutation["buildType"],&r.buildType)||!text(md.mutation["reference"],&r.reference,true,200)||!text(md.mutation["inventoryMode"],&r.inventoryMode)||!text(md.mutation["manufacturer"],&r.manufacturer,true,200)||!text(md.mutation["initialStatus"],&r.initialStatus)||!text(md.mutation["name"],&r.name,false,500)||!text(md.mutation["notes"],&r.notes,true)){invalid(error,"The Build Add request is invalid.");return false;}
    }else{
        if(!parseExpected(md.expected,&r.expected)||!id(md.mutation["buildId"],&r.buildId)){invalid(error,"The Build expected state is invalid.");return false;}
        if(edit){if(md.mutation.size()!=4||!text(md.mutation["name"],&r.name,false,500)||!text(md.mutation["manufacturer"],&r.manufacturer,true,200)||!text(md.mutation["notes"],&r.notes,true)){invalid(error,"The Build Edit request is invalid.");return false;}}
        else if(active){if(md.mutation.size()!=2||!md.mutation["desiredActive"].isBool()){invalid(error,"The Build active-state request is invalid.");return false;}r.desiredActive=md.mutation["desiredActive"].toBool();}
        else if(complete){if(md.mutation.size()!=1){invalid(error,"The Build Complete request is invalid.");return false;}}
        else {
            if(md.mutation.size()!=3||!text(md.mutation["linkedCollectionState"],&r.linkedCollectionState)||!md.mutation["returns"].isArray()||md.mutation["returns"].toArray().size()>10000){invalid(error,"The Build Cancel request is invalid.");return false;}
            QSet<QString> duplicates;for(const auto& value:md.mutation["returns"].toArray()){if(!value.isObject()){invalid(error,"A Build return row is invalid.");return false;}const auto o=value.toObject();ReturnRow row;qint64 quantity=0;if(o.size()!=5||!id(o["requirementId"],&row.requirementId)||!text(o["manufacturer"],&row.manufacturer,false,200)||!id(o["storageId"],&row.storageId)||!id(o["quantity"],&quantity)||quantity>1000000||!o["spare"].isBool()){invalid(error,"A Build return row is invalid.");return false;}row.quantity=int(quantity);row.spare=o["spare"].toBool();const QString key=QString("%1|%2|%3|%4").arg(row.requirementId).arg(row.manufacturer.toCaseFolded()).arg(row.storageId).arg(row.spare);if(duplicates.contains(key)){invalid(error,"Duplicate Build return rows are not allowed.");return false;}duplicates.insert(key);r.returns.append(row);}
        }
    }
    *out=r;return true;
}

bool resultFromMutation(const RemoteMutationDto::Result& source, Result* out,
                        RemoteMutationDto::Error* error)
{
    const bool requirement=source.operation.startsWith(QStringLiteral("builds.requirements."));
    const bool allocation=source.operation==QStringLiteral("builds.allocations.set")||source.operation==QStringLiteral("builds.allocateAvailable");
    if(!out||(requirement&&!source.authoritative.value("requirement").isObject())||(allocation&&(!source.authoritative.value("build").isObject()||!source.authoritative.value("allocations").isArray()))||(!requirement&&!allocation&&!source.authoritative.value("build").isObject())){invalid(error,"The Host returned an invalid Build result.");return false;}
    out->mutationId=source.mutationId;out->operation=source.operation;out->replayed=source.replayed;out->build=source.authoritative.value("build").toObject();out->effects=source.authoritative.value("effects").toObject();out->requirement=source.authoritative.value("requirement").toObject();out->allocations=source.authoritative.value("allocations").toArray();return true;
}
}
