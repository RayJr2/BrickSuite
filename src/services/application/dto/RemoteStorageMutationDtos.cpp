#include "RemoteStorageMutationDtos.h"
#include "RemoteReadJson.h"
#include "../HostStorageMutationService.h"

#include <QSet>
#include <cmath>

namespace {
bool exactKeys(const QJsonObject&o,const QSet<QString>&keys){QSet<QString>actual;for(auto it=o.constBegin();it!=o.constEnd();++it)actual.insert(it.key());return actual==keys;}
bool integer(const QJsonValue&v,qint64 minimum,qint64 maximum,qint64*out){if(!v.isDouble()||!std::isfinite(v.toDouble()))return false;const auto n=qint64(v.toDouble());if(v.toDouble()!=double(n)||n<minimum||n>maximum)return false;*out=n;return true;}
void invalid(RemoteMutationDto::Error*e,const QString&m){if(e)*e={QStringLiteral("INVALID_ARGUMENT"),m,false};}
QJsonObject expectedJson(const RemoteStorageMutationDto::ExpectedState&v){return{{"modifiedUtc",v.modifiedUtc},{"parentStorageId",double(v.parentStorageId)},{"storageTypeId",double(v.storageTypeId)},{"name",v.name},{"description",v.description},{"sortOrder",v.sortOrder},{"active",v.active},{"allowsInventory",v.allowsInventory},{"allowsCollection",v.allowsCollection}};}
}
namespace RemoteStorageMutationDto {
RemoteMutationDto::Metadata toMetadata(const QString&operation,const Request&r)
{
    QJsonObject mutation;
    if(operation==QStringLiteral("storage.setActive"))mutation={{"storageId",double(r.storageId)},{"active",r.active}};
    else {mutation={{"name",r.name},{"description",r.description},{"parentStorageId",double(r.parentStorageId)},{"storageTypeId",double(r.storageTypeId)},{"allowsInventory",r.allowsInventory},{"allowsCollection",r.allowsCollection}};if(operation==QStringLiteral("storage.edit"))mutation["storageId"]=double(r.storageId);}
    return {r.workspaceId,r.mutationId,operation==QStringLiteral("storage.add")?QJsonObject{}:expectedJson(r.expected),mutation};
}
bool fromMetadata(const QString&operation,const RemoteMutationDto::Metadata&m,Request*out,RemoteMutationDto::Error*error)
{
    if(!out)return false; Request r;r.workspaceId=m.workspaceId;r.mutationId=m.mutationId;qint64 value=0;
    const bool add=operation==QStringLiteral("storage.add"),edit=operation==QStringLiteral("storage.edit"),active=operation==QStringLiteral("storage.setActive");
    const QSet<QString> addKeys={"name","description","parentStorageId","storageTypeId","allowsInventory","allowsCollection"};
    const QSet<QString> editKeys=addKeys+QSet<QString>{"storageId"}; const QSet<QString> activeKeys={"storageId","active"};
    const QSet<QString> expectedKeys={"modifiedUtc","parentStorageId","storageTypeId","name","description","sortOrder","active","allowsInventory","allowsCollection"};
    if((!add&&!edit&&!active)||!exactKeys(m.mutation,add?addKeys:(edit?editKeys:activeKeys))||(add&&!m.expected.isEmpty())||(!add&&!exactKeys(m.expected,expectedKeys))){invalid(error,"The Storage mutation JSON shape is invalid.");return false;}
    if(!add){if(!integer(m.mutation.value("storageId"),1,INT_MAX,&r.storageId)){invalid(error,"A positive storageId is required.");return false;}if(!integer(m.expected.value("parentStorageId"),0,INT_MAX,&r.expected.parentStorageId)||!integer(m.expected.value("storageTypeId"),1,INT_MAX,&r.expected.storageTypeId)||!integer(m.expected.value("sortOrder"),INT_MIN,INT_MAX,&value)||!m.expected.value("modifiedUtc").isString()||!m.expected.value("name").isString()||!m.expected.value("description").isString()||!m.expected.value("active").isBool()||!m.expected.value("allowsInventory").isBool()||!m.expected.value("allowsCollection").isBool()){invalid(error,"The expected Storage state is invalid.");return false;}r.expected.sortOrder=int(value);r.expected.modifiedUtc=m.expected.value("modifiedUtc").toString();r.expected.name=m.expected.value("name").toString();r.expected.description=m.expected.value("description").toString();const auto expectedTime=QDateTime::fromString(r.expected.modifiedUtc,Qt::ISODateWithMs);if(!expectedTime.isValid()||expectedTime.offsetFromUtc()!=0||r.expected.name.size()>HostStorageMutationService::MaximumNameLength||r.expected.description.size()>HostStorageMutationService::MaximumDescriptionLength){invalid(error,"The expected Storage state is invalid.");return false;}r.expected.active=m.expected.value("active").toBool();r.expected.allowsInventory=m.expected.value("allowsInventory").toBool();r.expected.allowsCollection=m.expected.value("allowsCollection").toBool();}
    if(active){if(!m.mutation.value("active").isBool()){invalid(error,"The desired active state is invalid.");return false;}r.active=m.mutation.value("active").toBool();}
    else {r.name=m.mutation.value("name").toString();r.description=m.mutation.value("description").toString();if(!integer(m.mutation.value("parentStorageId"),0,INT_MAX,&r.parentStorageId)||!integer(m.mutation.value("storageTypeId"),1,INT_MAX,&r.storageTypeId)||!m.mutation.value("allowsInventory").isBool()||!m.mutation.value("allowsCollection").isBool()||r.name.trimmed().isEmpty()||r.name.size()>HostStorageMutationService::MaximumNameLength||r.description.size()>HostStorageMutationService::MaximumDescriptionLength){invalid(error,"The Storage fields are invalid.");return false;}r.allowsInventory=m.mutation.value("allowsInventory").toBool();r.allowsCollection=m.mutation.value("allowsCollection").toBool();}
    *out=r;return true;
}
bool resultFromMutation(const RemoteMutationDto::Result&s,Result*out,RemoteMutationDto::Error*error)
{if(!out)return false;RemoteReadJson::DecodeError decode;const auto item=s.authoritative.value("storage");if(!item.isObject()||!RemoteReadJson::fromJson(item.toObject(),&out->storage,&decode)||!s.authoritative.value("created").isBool()){invalid(error,decode.message.isEmpty()?QStringLiteral("The Host returned an invalid Storage result."):decode.message);return false;}out->mutationId=s.mutationId;out->operation=s.operation;out->replayed=s.replayed;out->created=s.authoritative.value("created").toBool();return true;}
}
