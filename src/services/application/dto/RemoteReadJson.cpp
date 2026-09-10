#include "RemoteReadJson.h"

#include <QJsonValue>

namespace {
bool fail(RemoteReadJson::DecodeError* error, const QString& message)
{ if (error) error->message = message; return false; }
bool integer(const QJsonObject& o, const char* key, qint64 minimum, qint64 maximum, qint64* out)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (!v.isDouble()) return false;
    const double d = v.toDouble(); const qint64 n = static_cast<qint64>(d);
    if (d != static_cast<double>(n) || n < minimum || n > maximum) return false;
    *out = n; return true;
}
bool textField(const QJsonObject& o, const char* key, QString* out, bool required = true)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isUndefined() && !required) { out->clear(); return true; }
    if (!v.isString() || v.toString().size() > RemoteReadDto::MaximumTextLength) return false;
    *out = v.toString(); return !required || !out->trimmed().isEmpty();
}
}

namespace RemoteReadJson {
QString utc(const QDateTime& value) { return value.toUTC().toString(Qt::ISODateWithMs); }
bool parseUtc(const QJsonValue& value, QDateTime* result)
{
    if (!value.isString()) return false;
    const QDateTime parsed = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
    if (!parsed.isValid() || parsed.offsetFromUtc() != 0) return false;
    *result = parsed.toUTC(); return true;
}
bool pageRequest(const QJsonObject& o, RemoteReadDto::PageRequest* out, DecodeError* error)
{
    qint64 page = 0, size = 0;
    if (!integer(o, "page", 1, INT_MAX, &page)
        || !integer(o, "pageSize", 1, RemoteReadDto::MaximumPageSize, &size))
        return fail(error, QStringLiteral("page and pageSize are outside the supported range."));
    if (page - 1 > INT_MAX / size)
        return fail(error, QStringLiteral("The requested page offset is too large."));
    out->page = int(page); out->pageSize = int(size); return true;
}
QJsonObject toJson(const RemoteReadDto::WorkspaceSummary& v)
{ return {{"workspaceId", double(v.workspaceId)}, {"name", v.name}}; }
bool fromJson(const QJsonObject& o, RemoteReadDto::WorkspaceSummary* v, DecodeError* e)
{
    if (!integer(o,"workspaceId",1,9007199254740991LL,&v->workspaceId) || !textField(o,"name",&v->name))
        return fail(e,QStringLiteral("Invalid Workspace summary.")); return true;
}
QJsonObject toJson(const RemoteReadDto::StorageSummary& v)
{ return {{"storageId",double(v.storageId)},{"parentStorageId",double(v.parentStorageId)},{"name",v.name},{"displayPath",v.displayPath},{"typeName",v.typeName},{"sortOrder",v.sortOrder},{"active",v.active},{"allowsInventory",v.allowsInventory},{"allowsCollection",v.allowsCollection}}; }
bool fromJson(const QJsonObject& o, RemoteReadDto::StorageSummary* v, DecodeError* e)
{
    qint64 order = 0;
    if (!integer(o,"storageId",1,9007199254740991LL,&v->storageId)
        || !integer(o,"parentStorageId",0,9007199254740991LL,&v->parentStorageId)
        || !textField(o,"name",&v->name) || !textField(o,"displayPath",&v->displayPath,false)
        || !textField(o,"typeName",&v->typeName,false) || !integer(o,"sortOrder",INT_MIN,INT_MAX,&order)
        || !o.value("active").isBool() || !o.value("allowsInventory").isBool()
        || !o.value("allowsCollection").isBool()) return fail(e,QStringLiteral("Invalid Storage summary."));
    v->sortOrder=int(order); v->active=o.value("active").toBool();
    v->allowsInventory=o.value("allowsInventory").toBool(); v->allowsCollection=o.value("allowsCollection").toBool(); return true;
}
QJsonObject toJson(const RemoteReadDto::InventoryRow& v)
{ return {{"inventoryRecordId",double(v.inventoryRecordId)},{"workspaceId",double(v.workspaceId)},{"partNumber",v.partNumber},{"partNameFallback",v.partNameFallback},{"rebrickableCategoryId",v.rebrickableCategoryId},{"rebrickableColorId",v.rebrickableColorId},{"colorNameFallback",v.colorNameFallback},{"quantity",v.quantity},{"storageId",double(v.storageId)},{"storagePath",v.storagePath},{"manufacturerDisplay",v.manufacturerDisplay},{"condition",v.condition},{"ownershipType",v.ownershipType}}; }
bool fromJson(const QJsonObject& o, RemoteReadDto::InventoryRow* v, DecodeError* e)
{
    qint64 category=0, color=0, quantity=0;
    if (!integer(o,"inventoryRecordId",1,9007199254740991LL,&v->inventoryRecordId)
        || !integer(o,"workspaceId",1,9007199254740991LL,&v->workspaceId)
        || !textField(o,"partNumber",&v->partNumber) || !textField(o,"partNameFallback",&v->partNameFallback,false)
        || !integer(o,"rebrickableCategoryId",-1,INT_MAX,&category)
        || !integer(o,"rebrickableColorId",-1,INT_MAX,&color) || !integer(o,"quantity",0,INT_MAX,&quantity)
        || !integer(o,"storageId",0,9007199254740991LL,&v->storageId)
        || !textField(o,"colorNameFallback",&v->colorNameFallback,false) || !textField(o,"storagePath",&v->storagePath,false)
        || !textField(o,"manufacturerDisplay",&v->manufacturerDisplay,false) || !textField(o,"condition",&v->condition)
        || !textField(o,"ownershipType",&v->ownershipType)) return fail(e,QStringLiteral("Invalid Inventory row."));
    v->rebrickableCategoryId=int(category); v->rebrickableColorId=int(color); v->quantity=int(quantity); return true;
}
QJsonObject toJson(const RemoteReadDto::InventoryDetail& v){QJsonObject o=toJson(static_cast<const RemoteReadDto::InventoryRow&>(v));o["createdUtc"]=utc(v.createdUtc);o["modifiedUtc"]=utc(v.modifiedUtc);return o;}
bool fromJson(const QJsonObject&o,RemoteReadDto::InventoryDetail*v,DecodeError*e){return fromJson(o,static_cast<RemoteReadDto::InventoryRow*>(v),e)&&parseUtc(o.value("createdUtc"),&v->createdUtc)&&parseUtc(o.value("modifiedUtc"),&v->modifiedUtc);}
QJsonObject toJson(const RemoteReadDto::InventoryHistoryRow&v){return{{"movementId",double(v.movementId)},{"movementType",v.movementType},{"quantityChange",v.quantityChange},{"fromStorageId",double(v.fromStorageId)},{"fromStoragePath",v.fromStoragePath},{"toStorageId",double(v.toStorageId)},{"toStoragePath",v.toStoragePath},{"condition",v.condition},{"ownershipType",v.ownershipType},{"referenceType",v.referenceType},{"referenceId",v.referenceId},{"notes",v.notes},{"createdUtc",utc(v.createdUtc)}};}
bool fromJson(const QJsonObject&o,RemoteReadDto::InventoryHistoryRow*v,DecodeError*e){qint64 qty=0;if(!integer(o,"movementId",1,9007199254740991LL,&v->movementId)||!integer(o,"quantityChange",INT_MIN,INT_MAX,&qty)||!integer(o,"fromStorageId",0,9007199254740991LL,&v->fromStorageId)||!integer(o,"toStorageId",0,9007199254740991LL,&v->toStorageId)||!textField(o,"movementType",&v->movementType)||!textField(o,"fromStoragePath",&v->fromStoragePath,false)||!textField(o,"toStoragePath",&v->toStoragePath,false)||!textField(o,"condition",&v->condition,false)||!textField(o,"ownershipType",&v->ownershipType,false)||!textField(o,"referenceType",&v->referenceType,false)||!textField(o,"referenceId",&v->referenceId,false)||!textField(o,"notes",&v->notes,false)||!parseUtc(o.value("createdUtc"),&v->createdUtc))return fail(e,"Invalid Inventory history row.");v->quantityChange=int(qty);return true;}
QJsonObject toJson(const RemoteReadDto::BuildSummary& v)
{ return {{"buildId",double(v.buildId)},{"workspaceId",double(v.workspaceId)},{"buildType",v.buildType},{"name",v.name},{"setNumber",v.setNumber},{"minifigNumber",v.minifigNumber},{"inventoryMode",v.inventoryMode},{"manufacturerDisplay",v.manufacturerDisplay},{"status",v.status},{"notes",v.notes},{"active",v.active}}; }
bool fromJson(const QJsonObject& o, RemoteReadDto::BuildSummary* v, DecodeError* e)
{
    if (!integer(o,"buildId",1,9007199254740991LL,&v->buildId) || !integer(o,"workspaceId",1,9007199254740991LL,&v->workspaceId)
        || !textField(o,"buildType",&v->buildType) || !textField(o,"name",&v->name)
        || !textField(o,"setNumber",&v->setNumber,false) || !textField(o,"minifigNumber",&v->minifigNumber,false)
        || !textField(o,"inventoryMode",&v->inventoryMode) || !textField(o,"manufacturerDisplay",&v->manufacturerDisplay,false)
        || !textField(o,"status",&v->status) || !textField(o,"notes",&v->notes,false) || !o.value("active").isBool())
        return fail(e,QStringLiteral("Invalid Build summary.")); v->active=o.value("active").toBool(); return true;
}
QJsonObject toJson(const RemoteReadDto::BuildRequirement&v){return{{"requirementId",double(v.requirementId)},{"buildId",double(v.buildId)},{"partNumber",v.partNumber},{"partNameFallback",v.partNameFallback},{"rebrickableColorId",v.rebrickableColorId},{"colorNameFallback",v.colorNameFallback},{"substitutePartNumber",v.substitutePartNumber},{"substituteRebrickableColorId",v.substituteRebrickableColorId},{"quantityRequired",v.quantityRequired},{"quantityPulled",v.quantityPulled},{"spare",v.spare},{"owned",v.owned},{"thisRequirementAllocated",v.thisRequirementAllocated},{"otherAllocated",v.otherAllocated},{"available",v.available},{"missing",v.missing}};}
bool fromJson(const QJsonObject&o,RemoteReadDto::BuildRequirement*v,DecodeError*e){qint64 color=0,subColor=0,required=0,pulled=0;if(!integer(o,"requirementId",1,9007199254740991LL,&v->requirementId)||!integer(o,"buildId",1,9007199254740991LL,&v->buildId)||!integer(o,"rebrickableColorId",-1,INT_MAX,&color)||!integer(o,"substituteRebrickableColorId",-1,INT_MAX,&subColor)||!integer(o,"quantityRequired",0,INT_MAX,&required)||!integer(o,"quantityPulled",0,INT_MAX,&pulled)||!textField(o,"partNumber",&v->partNumber)||!textField(o,"partNameFallback",&v->partNameFallback,false)||!textField(o,"colorNameFallback",&v->colorNameFallback,false)||!textField(o,"substitutePartNumber",&v->substitutePartNumber,false)||!o.value("spare").isBool())return fail(e,"Invalid Build requirement.");v->rebrickableColorId=int(color);v->substituteRebrickableColorId=int(subColor);v->quantityRequired=int(required);v->quantityPulled=int(pulled);v->spare=o.value("spare").toBool();qint64 value=0;auto optional=[&](const char*key,int*target){if(!o.contains(key))return true;if(!integer(o,key,0,INT_MAX,&value))return false;*target=int(value);return true;};if(!optional("owned",&v->owned)||!optional("thisRequirementAllocated",&v->thisRequirementAllocated)||!optional("otherAllocated",&v->otherAllocated)||!optional("available",&v->available)||!optional("missing",&v->missing))return fail(e,"Invalid Build requirement availability.");return true;}
QJsonObject toJson(const RemoteReadDto::MissingPart&v){return{{"partNumber",v.partNumber},{"partNameFallback",v.partNameFallback},{"rebrickableColorId",v.rebrickableColorId},{"colorNameFallback",v.colorNameFallback},{"required",v.required},{"pulled",v.pulled},{"remaining",v.remaining},{"owned",v.owned},{"thisBuildAllocated",v.thisBuildAllocated},{"otherBuildsAllocated",v.otherBuildsAllocated},{"available",v.available},{"missing",v.missing}};}
bool fromJson(const QJsonObject&o,RemoteReadDto::MissingPart*v,DecodeError*e){qint64 color=0,required=0,pulled=0,remaining=0,owned=0,thisAllocated=0,otherAllocated=0,available=0,missing=0;if(!textField(o,"partNumber",&v->partNumber)||!textField(o,"partNameFallback",&v->partNameFallback,false)||!textField(o,"colorNameFallback",&v->colorNameFallback,false)||!integer(o,"rebrickableColorId",-1,INT_MAX,&color)||!integer(o,"required",0,INT_MAX,&required)||!integer(o,"pulled",0,INT_MAX,&pulled)||!integer(o,"remaining",0,INT_MAX,&remaining)||!integer(o,"owned",0,INT_MAX,&owned)||!integer(o,"thisBuildAllocated",0,INT_MAX,&thisAllocated)||!integer(o,"otherBuildsAllocated",0,INT_MAX,&otherAllocated)||!integer(o,"available",0,INT_MAX,&available)||!integer(o,"missing",0,INT_MAX,&missing))return fail(e,"Invalid Missing Part row.");v->rebrickableColorId=int(color);v->required=int(required);v->pulled=int(pulled);v->remaining=int(remaining);v->owned=int(owned);v->thisBuildAllocated=int(thisAllocated);v->otherBuildsAllocated=int(otherAllocated);v->available=int(available);v->missing=int(missing);return true;}
QJsonObject toJson(const RemoteReadDto::PullingRow&v){return{{"requirementId",double(v.requirementId)},{"allocationId",double(v.allocationId)},{"inventoryRecordId",double(v.inventoryRecordId)},{"storageId",double(v.storageId)},{"storagePath",v.storagePath},{"partNumber",v.partNumber},{"partNameFallback",v.partNameFallback},{"rebrickableColorId",v.rebrickableColorId},{"colorNameFallback",v.colorNameFallback},{"quantityRequired",v.quantityRequired},{"quantityPulled",v.quantityPulled},{"quantityAllocated",v.quantityAllocated},{"inventoryQuantity",v.inventoryQuantity},{"substitution",v.substitution}};}
bool fromJson(const QJsonObject&o,RemoteReadDto::PullingRow*v,DecodeError*e){qint64 color=0,required=0,pulled=0,allocated=0,inventory=0;if(!integer(o,"requirementId",1,9007199254740991LL,&v->requirementId)||!integer(o,"allocationId",1,9007199254740991LL,&v->allocationId)||!integer(o,"inventoryRecordId",1,9007199254740991LL,&v->inventoryRecordId)||!integer(o,"storageId",0,9007199254740991LL,&v->storageId)||!integer(o,"rebrickableColorId",-1,INT_MAX,&color)||!integer(o,"quantityRequired",0,INT_MAX,&required)||!integer(o,"quantityPulled",0,INT_MAX,&pulled)||!integer(o,"quantityAllocated",0,INT_MAX,&allocated)||(o.contains("inventoryQuantity")&&!integer(o,"inventoryQuantity",0,INT_MAX,&inventory))||!textField(o,"storagePath",&v->storagePath,false)||!textField(o,"partNumber",&v->partNumber)||!textField(o,"partNameFallback",&v->partNameFallback,false)||!textField(o,"colorNameFallback",&v->colorNameFallback,false)||!o.value("substitution").isBool())return fail(e,"Invalid Pulling row.");v->rebrickableColorId=int(color);v->quantityRequired=int(required);v->quantityPulled=int(pulled);v->quantityAllocated=int(allocated);v->inventoryQuantity=int(inventory);v->substitution=o.value("substitution").toBool();return true;}
QJsonObject toJson(const RemoteReadDto::CollectionSummary& v)
{ return {{"collectionItemId",double(v.collectionItemId)},{"workspaceId",double(v.workspaceId)},{"type",v.type},{"setNumber",v.setNumber},{"minifigNumber",v.minifigNumber},{"referenceFallback",v.referenceFallback},{"titleFallback",v.titleFallback},{"state",v.state},{"condition",v.condition},{"completeness",v.completeness},{"storageId",double(v.storageId)},{"storagePath",v.storagePath},{"nickname",v.nickname},{"active",v.active}}; }
bool fromJson(const QJsonObject& o, RemoteReadDto::CollectionSummary* v, DecodeError* e)
{
    if (!integer(o,"collectionItemId",1,9007199254740991LL,&v->collectionItemId) || !integer(o,"workspaceId",1,9007199254740991LL,&v->workspaceId)
        || !integer(o,"storageId",0,9007199254740991LL,&v->storageId) || !textField(o,"type",&v->type)
        || !textField(o,"setNumber",&v->setNumber,false) || !textField(o,"minifigNumber",&v->minifigNumber,false) || !textField(o,"referenceFallback",&v->referenceFallback,false)
        || !textField(o,"titleFallback",&v->titleFallback,false) || !textField(o,"state",&v->state)
        || !textField(o,"condition",&v->condition) || !textField(o,"completeness",&v->completeness)
        || !textField(o,"storagePath",&v->storagePath,false) || !textField(o,"nickname",&v->nickname,false)
        || !o.value("active").isBool()) return fail(e,QStringLiteral("Invalid Collection summary."));
    v->active=o.value("active").toBool(); return true;
}
QJsonObject toJson(const RemoteReadDto::CollectionDetail&v){QJsonObject o=toJson(static_cast<const RemoteReadDto::CollectionSummary&>(v));o["notes"]=v.notes;o["createdUtc"]=utc(v.createdUtc);o["modifiedUtc"]=utc(v.modifiedUtc);return o;}
bool fromJson(const QJsonObject&o,RemoteReadDto::CollectionDetail*v,DecodeError*e){return fromJson(o,static_cast<RemoteReadDto::CollectionSummary*>(v),e)&&textField(o,"notes",&v->notes,false)&&parseUtc(o.value("createdUtc"),&v->createdUtc)&&parseUtc(o.value("modifiedUtc"),&v->modifiedUtc);}
QJsonObject toJson(const RemoteReadDto::PartReferenceCustomization& v)
{ return {{"customizationId",double(v.customizationId)},{"partNumber",v.partNumber},{"partNameFallback",v.partNameFallback},{"catalog",v.catalog},{"section",v.section},{"displayOrder",v.displayOrder},{"representativeFor",v.representativeFor},{"notes",v.notes}}; }
bool fromJson(const QJsonObject& o, RemoteReadDto::PartReferenceCustomization* v, DecodeError* e)
{
    qint64 order=0;
    if (!integer(o,"customizationId",1,9007199254740991LL,&v->customizationId) || !integer(o,"displayOrder",0,INT_MAX,&order)
        || !textField(o,"partNumber",&v->partNumber) || !textField(o,"partNameFallback",&v->partNameFallback,false)
        || !textField(o,"catalog",&v->catalog) || !textField(o,"section",&v->section)
        || !textField(o,"representativeFor",&v->representativeFor,false) || !textField(o,"notes",&v->notes,false))
        return fail(e,QStringLiteral("Invalid Part Reference customization.")); v->displayOrder=int(order); return true;
}
} // namespace RemoteReadJson
