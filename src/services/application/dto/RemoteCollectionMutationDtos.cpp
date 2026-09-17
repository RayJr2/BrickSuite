#include "RemoteCollectionMutationDtos.h"

#include <QDateTime>
#include <QJsonArray>
#include <QSet>

namespace {
void bad(RemoteMutationDto::Error* error, const QString& message)
{ if (error) *error={QStringLiteral("INVALID_ARGUMENT"),message,false}; }
bool text(const QJsonValue& value, QString* out, bool empty=false, int maximum=4000)
{ if(!value.isString())return false;*out=value.toString();return out->size()<=maximum&&(empty||!out->trimmed().isEmpty()); }
bool id(const QJsonValue& value, qint64* out, bool zero=false)
{ if(!value.isDouble())return false;const double number=value.toDouble();if(number!=qint64(number)||number<(zero?0:1)||number>9007199254740991.)return false;*out=qint64(number);return true; }
QJsonObject expected(const RemoteCollectionMutationDto::ExpectedState& value)
{ return{{"modifiedUtc",value.modifiedUtc},{"active",value.active},{"type",value.type},{"setNumber",value.setNumber},{"minifigNumber",value.minifigNumber},{"sourceBuildId",double(value.sourceBuildId)},{"state",value.state},{"condition",value.condition},{"completeness",value.completeness},{"storageId",double(value.storageId)},{"nickname",value.nickname},{"notes",value.notes}}; }
bool parseExpected(const QJsonObject& value, RemoteCollectionMutationDto::ExpectedState* out)
{ return value.size()==12&&text(value["modifiedUtc"],&out->modifiedUtc)&&value["active"].isBool()&&text(value["type"],&out->type)&&text(value["setNumber"],&out->setNumber,true,200)&&text(value["minifigNumber"],&out->minifigNumber,true,200)&&id(value["sourceBuildId"],&out->sourceBuildId,true)&&text(value["state"],&out->state)&&text(value["condition"],&out->condition)&&text(value["completeness"],&out->completeness)&&id(value["storageId"],&out->storageId,true)&&text(value["nickname"],&out->nickname,true,500)&&text(value["notes"],&out->notes,true)&&QDateTime::fromString(out->modifiedUtc,Qt::ISODateWithMs).isValid()&&(out->active=value["active"].toBool(),true); }
}

namespace RemoteCollectionMutationDto {
RemoteMutationDto::Metadata toMetadata(const QString& operation,const Request& request)
{
    QJsonObject mutation, expectedValue;
    if(operation=="collection.add")mutation={{"sourceType",request.sourceType},{"setNumber",request.setNumber},{"minifigNumber",request.minifigNumber},{"buildId",double(request.buildId)},{"state",request.state},{"condition",request.condition},{"completeness",request.completeness},{"storageId",double(request.storageId)},{"nickname",request.nickname},{"notes",request.notes}};
    else if(operation=="collection.edit"){mutation={{"collectionItemId",double(request.collectionItemId)},{"state",request.state},{"condition",request.condition},{"completeness",request.completeness},{"storageId",double(request.storageId)},{"nickname",request.nickname},{"notes",request.notes}};expectedValue=expected(request.expected);}
    else if(operation=="collection.partsSource.set"){mutation={{"collectionItemId",double(request.collectionItemId)},{"allowPartsSource",request.allowPartsSource}};expectedValue={{"modifiedUtc",request.expected.modifiedUtc},{"allowPartsSource",request.expected.allowPartsSource}};}
    else if(operation=="collection.disassemble"){QJsonArray rows;for(const auto&row:request.returns)rows.append(QJsonObject{{"rowIndex",double(row.rowIndex)},{"storageId",double(row.storageId)},{"quantity",row.quantity}});mutation={{"collectionItemId",double(request.collectionItemId)},{"planId",request.planId},{"returns",rows}};expectedValue=expected(request.expected);}
    else {mutation={{"collectionItemId",double(request.collectionItemId)},{"desiredActive",request.desiredActive}};expectedValue=expected(request.expected);}
    return{request.workspaceId,request.mutationId,expectedValue,mutation};
}

bool fromMetadata(const QString& operation,const RemoteMutationDto::Metadata& metadata,
                  Request* out,RemoteMutationDto::Error* error)
{
    if(!out)return false;Request request;request.workspaceId=metadata.workspaceId;request.mutationId=metadata.mutationId;
    const bool add=operation=="collection.add",edit=operation=="collection.edit",active=operation=="collection.setActive",parts=operation=="collection.partsSource.set",disassemble=operation=="collection.disassemble";
    if(!add&&!edit&&!active&&!parts&&!disassemble){bad(error,"Unsupported Collection mutation.");return false;}
    if(add){if(!metadata.expected.isEmpty()||metadata.mutation.size()!=10||!text(metadata.mutation["sourceType"],&request.sourceType)||!text(metadata.mutation["setNumber"],&request.setNumber,true,200)||!text(metadata.mutation["minifigNumber"],&request.minifigNumber,true,200)||!id(metadata.mutation["buildId"],&request.buildId,true)||!text(metadata.mutation["state"],&request.state)||!text(metadata.mutation["condition"],&request.condition)||!text(metadata.mutation["completeness"],&request.completeness)||!id(metadata.mutation["storageId"],&request.storageId,true)||!text(metadata.mutation["nickname"],&request.nickname,true,500)||!text(metadata.mutation["notes"],&request.notes,true)){bad(error,"The Collection Add request is invalid.");return false;}}
    else if(parts){if(metadata.mutation.size()!=2||metadata.expected.size()!=2||!id(metadata.mutation["collectionItemId"],&request.collectionItemId)||!metadata.mutation["allowPartsSource"].isBool()||!text(metadata.expected["modifiedUtc"],&request.expected.modifiedUtc)||!metadata.expected["allowPartsSource"].isBool()||!QDateTime::fromString(request.expected.modifiedUtc,Qt::ISODateWithMs).isValid()){bad(error,"The Collection parts-source request is invalid.");return false;}request.allowPartsSource=metadata.mutation["allowPartsSource"].toBool();request.expected.allowPartsSource=metadata.expected["allowPartsSource"].toBool();}
    else {
        if(!parseExpected(metadata.expected,&request.expected)||!id(metadata.mutation["collectionItemId"],&request.collectionItemId)){bad(error,"The Collection expected state is invalid.");return false;}
        if(edit){if(metadata.mutation.size()!=7||!text(metadata.mutation["state"],&request.state)||!text(metadata.mutation["condition"],&request.condition)||!text(metadata.mutation["completeness"],&request.completeness)||!id(metadata.mutation["storageId"],&request.storageId,true)||!text(metadata.mutation["nickname"],&request.nickname,true,500)||!text(metadata.mutation["notes"],&request.notes,true)){bad(error,"The Collection Edit request is invalid.");return false;}}
        else if(disassemble){
            if(metadata.mutation.size()!=3||!text(metadata.mutation["planId"],&request.planId,false,128)||!metadata.mutation["returns"].isArray()){bad(error,"The Collection disassembly request is invalid.");return false;}
            const auto rows=metadata.mutation["returns"].toArray();if(rows.isEmpty()||rows.size()>500){bad(error,"The Collection disassembly return plan is invalid.");return false;}
            QSet<qint64> indexes;for(const auto&value:rows){if(!value.isObject()){bad(error,"The Collection disassembly return plan is invalid.");return false;}const auto row=value.toObject();DisassemblyReturn decoded;qint64 quantity=0;if(row.size()!=3||!id(row["rowIndex"],&decoded.rowIndex)||!id(row["storageId"],&decoded.storageId)||!id(row["quantity"],&quantity)||quantity>INT_MAX||indexes.contains(decoded.rowIndex)){bad(error,"The Collection disassembly return plan is invalid.");return false;}decoded.quantity=int(quantity);indexes.insert(decoded.rowIndex);request.returns.append(decoded);}
        } else if(!active||metadata.mutation.size()!=2||!metadata.mutation["desiredActive"].isBool()){bad(error,"The Collection lifecycle request is invalid.");return false;}else request.desiredActive=metadata.mutation["desiredActive"].toBool();
    }
    *out=request;return true;
}

bool resultFromMutation(const RemoteMutationDto::Result& source,Result* out,RemoteMutationDto::Error* error)
{if(!out||!source.authoritative.value("item").isObject()){bad(error,"The Host returned an invalid Collection result.");return false;}*out={source.mutationId,source.operation,source.replayed,source.authoritative.value("item").toObject()};return true;}
}
