#include "CollectionExportService.h"

#include "../application/dto/RemoteReadDtos.h"

#include <QHash>

namespace {
QString buildIdentity(const QString& name,const QString& reference)
{
    if(reference.isEmpty())return name;
    if(name.isEmpty())return reference;
    return QStringLiteral("%1 (%2)").arg(name,reference);
}
}

QList<CollectionExportFieldDescriptor> CollectionExportService::fieldDescriptors()
{
    return {{"type","Type","Type",true},{"reference","Reference","Reference",true},
        {"name","Name","Name",true},{"nickname","Nickname","Nickname",true},
        {"state","State","State",true},{"condition","Condition","Condition",true},
        {"completeness","Completeness","Completeness",true},{"location","Collection Location","Collection Location",true},
        {"source","Source","Source",true},{"allowPartsSource","Consider for What Can I Build","Consider for What Can I Build",true},
        {"notes","Notes","Notes",false},{"collectionItemId","Collection Item ID","Collection Item ID",false},
        {"sourceBuildReference","Source Build Reference","Source Build Reference",false},
        {"sourceBuildName","Source Build Name","Source Build Name",false},
        {"createdUtc","Created","Created",false},{"modifiedUtc","Modified","Modified",false}};
}

CollectionExportConfiguration CollectionExportService::defaultConfiguration()
{CollectionExportConfiguration c;for(const auto&f:fieldDescriptors()){c.fieldOrder<<f.id;if(f.defaultEnabled)c.enabledFields.insert(f.id);}return c;}

CollectionExportConfiguration CollectionExportService::normalizeConfiguration(const QStringList&order,const QStringList&enabled)
{
    if(order.isEmpty())return defaultConfiguration();QHash<QString,CollectionExportFieldDescriptor> known;for(const auto&f:fieldDescriptors())known.insert(f.id,f);
    CollectionExportConfiguration c;for(const auto&id:order)if(known.contains(id)&&!c.fieldOrder.contains(id))c.fieldOrder<<id;for(const auto&f:fieldDescriptors())if(!c.fieldOrder.contains(f.id))c.fieldOrder<<f.id;for(const auto&id:enabled)if(known.contains(id))c.enabledFields.insert(id);return c;
}

QList<CollectionExportRow> CollectionExportService::createRows(const QList<CollectionSearchResult>&source)
{
    QList<CollectionExportRow> rows;rows.reserve(source.size());for(const auto&x:source){CollectionExportRow r;r.collectionItemId=x.item.id;r.type=collectionItemTypeToString(x.item.type);r.reference=x.displayReference;r.name=x.displayName;r.nickname=x.item.nickname;r.state=collectionItemStateToString(x.item.state);r.condition=collectionItemConditionToString(x.item.condition);r.completeness=collectionItemCompletenessToString(x.item.completeness);r.location=x.item.storageLocationId>0?x.locationName:QStringLiteral("Unassigned");r.sourceBuildReference=x.sourceBuildReference;r.sourceBuildName=x.sourceBuildName;r.source=x.item.sourceBuildId>0?QStringLiteral("Build: %1").arg(buildIdentity(x.sourceBuildName,x.sourceBuildReference)):QStringLiteral("Catalog / Existing Collection");r.allowPartsSource=x.item.allowPartsSource;r.notes=x.item.notes;r.createdUtc=x.item.createdUtc.toUTC().toString(Qt::ISODateWithMs);r.modifiedUtc=x.item.modifiedUtc.toUTC().toString(Qt::ISODateWithMs);rows<<r;}return rows;
}

QList<CollectionExportRow> CollectionExportService::createRemoteRows(const QList<RemoteReadDto::CollectionExportRow>&source)
{QList<CollectionExportRow> rows;rows.reserve(source.size());for(const auto&x:source){CollectionExportRow r;r.collectionItemId=x.collectionItemId;r.type=x.type;r.reference=x.reference;r.name=x.name;r.nickname=x.nickname;r.state=x.state;r.condition=x.condition;r.completeness=x.completeness;r.location=x.location;r.source=x.source;r.allowPartsSource=x.allowPartsSource;r.notes=x.notes;r.sourceBuildReference=x.sourceBuildReference;r.sourceBuildName=x.sourceBuildName;r.createdUtc=x.createdUtc;r.modifiedUtc=x.modifiedUtc;rows<<r;}return rows;}

CollectionExportProjection CollectionExportService::project(const QList<CollectionExportRow>&rows,const CollectionExportConfiguration&c,int maximumRows)
{
    CollectionExportProjection p;QHash<QString,CollectionExportFieldDescriptor> known;for(const auto&f:fieldDescriptors())known.insert(f.id,f);for(const auto&id:c.fieldOrder)if(c.enabledFields.contains(id)&&known.contains(id)){p.fields<<known.value(id);p.headers<<known.value(id).header;}
    const int count=maximumRows<0?rows.size():qMin(maximumRows,rows.size());for(int i=0;i<count;++i){const auto&r=rows.at(i);QStringList values;for(const auto&f:p.fields){if(f.id=="type")values<<r.type;else if(f.id=="reference")values<<r.reference;else if(f.id=="name")values<<r.name;else if(f.id=="nickname")values<<r.nickname;else if(f.id=="state")values<<r.state;else if(f.id=="condition")values<<r.condition;else if(f.id=="completeness")values<<r.completeness;else if(f.id=="location")values<<r.location;else if(f.id=="source")values<<r.source;else if(f.id=="allowPartsSource")values<<(r.allowPartsSource?QStringLiteral("Yes"):QStringLiteral("No"));else if(f.id=="notes")values<<r.notes;else if(f.id=="collectionItemId")values<<QString::number(r.collectionItemId);else if(f.id=="sourceBuildReference")values<<r.sourceBuildReference;else if(f.id=="sourceBuildName")values<<r.sourceBuildName;else if(f.id=="createdUtc")values<<r.createdUtc;else if(f.id=="modifiedUtc")values<<r.modifiedUtc;}p.rows<<values;}return p;
}
