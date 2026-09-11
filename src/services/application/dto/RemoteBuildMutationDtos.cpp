#include "RemoteBuildMutationDtos.h"

#include <QDateTime>
#include <QJsonArray>
#include <QSet>

namespace {
constexpr qint64 MaxJsonInteger = 9007199254740991LL;
void invalid(RemoteMutationDto::Error* error, const QString& message)
{ if (error) *error = {QStringLiteral("INVALID_ARGUMENT"), message, false}; }
bool text(const QJsonValue& value, QString* out, bool allowEmpty = false, int maximum = 4000)
{ if (!value.isString()) return false; *out=value.toString(); return out->size()<=maximum&&(allowEmpty||!out->trimmed().isEmpty()); }
bool id(const QJsonValue& value, qint64* out, bool zero = false)
{ if(!value.isDouble())return false;const double d=value.toDouble();if(d!=qint64(d)||d<(zero?0:1)||d>MaxJsonInteger)return false;*out=qint64(d);return true; }
QJsonObject expectedJson(const RemoteBuildMutationDto::ExpectedState& e)
{ return {{"modifiedUtc",e.modifiedUtc},{"buildType",e.buildType},{"reference",e.reference},{"inventoryMode",e.inventoryMode},{"manufacturer",e.manufacturer},{"name",e.name},{"status",e.status},{"notes",e.notes},{"active",e.active}}; }
bool parseExpected(const QJsonObject& o, RemoteBuildMutationDto::ExpectedState* e)
{ return o.size()==9&&text(o["modifiedUtc"],&e->modifiedUtc)&&QDateTime::fromString(e->modifiedUtc,Qt::ISODateWithMs).isValid()&&text(o["buildType"],&e->buildType)&&text(o["reference"],&e->reference,true,200)&&text(o["inventoryMode"],&e->inventoryMode)&&text(o["manufacturer"],&e->manufacturer,true,200)&&text(o["name"],&e->name,false,500)&&text(o["status"],&e->status)&&text(o["notes"],&e->notes,true)&&o["active"].isBool()&&(e->active=o["active"].toBool(),true); }
}

namespace RemoteBuildMutationDto {

RemoteMutationDto::Metadata toMetadata(const QString& operation, const Request& r)
{
    QJsonObject mutation, expected;
    if (operation == QStringLiteral("builds.add")) {
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
    const bool add=operation=="builds.add",edit=operation=="builds.edit",active=operation=="builds.setActive",complete=operation=="builds.complete",cancel=operation=="builds.cancel";
    if(!add&&!edit&&!active&&!complete&&!cancel){invalid(error,"Unsupported Build mutation.");return false;}
    if(add){
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
    if(!out||!source.authoritative.value("build").isObject()){invalid(error,"The Host returned an invalid Build result.");return false;}
    *out={source.mutationId,source.operation,source.replayed,source.authoritative.value("build").toObject(),source.authoritative.value("effects").toObject()};return true;
}
}
