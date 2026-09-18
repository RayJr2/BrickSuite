#include "InventoryExportService.h"

#include "../../api/ApiProvider.h"
#include "../../models/ExternalMappingStatus.h"
#include "../../repositories/ExternalColorMappingRepository.h"
#include "../../repositories/ExternalPartIdentifierRepository.h"
#include "../../repositories/ExternalPartMappingRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../services/application/dto/RemoteReadDtos.h"
#include "../parts/ElementIdentityService.h"

#include <QHash>
#include <QSqlQuery>
#include <algorithm>

namespace {
QStringList sortedDistinct(QStringList values)
{
    for (QString& value : values) value = value.trimmed();
    values.removeAll({});
    std::sort(values.begin(), values.end(), [](const QString& a, const QString& b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    values.erase(std::unique(values.begin(), values.end(), [](const QString& a, const QString& b) {
        return a.compare(b, Qt::CaseInsensitive) == 0;
    }), values.end());
    return values;
}
}

InventoryExportService::InventoryExportService(const QSqlDatabase& database)
    : m_database(database) {}

QList<InventoryExportFieldDescriptor> InventoryExportService::fieldDescriptors()
{
    return {
        {"partNumber", "Part Number", "Part Number", true, 0, false},
        {"partName", "Part Name", "Part Name", true, 1, false},
        {"category", "Category", "Category", true, 2, false},
        {"color", "Color", "Color", true, 3, false},
        {"quantity", "Quantity", "Quantity", true, 4, true},
        {"storage", "Storage Location", "Storage Location", true, 5, false},
        {"manufacturer", "Manufacturer", "Manufacturer", true, 6, false},
        {"condition", "Condition", "Condition", true, 7, false},
        {"ownership", "Ownership", "Ownership", true, 8, false},
        {"recordId", "Inventory Record ID", "Inventory Record ID", false, 9, true},
        {"legoElementId", "LEGO Element ID", "LEGO Element ID", false, 10, false},
        {"rebrickablePartId", "Rebrickable Part ID", "Rebrickable Part ID", false, 11, false},
        {"rebrickableColorId", "Rebrickable Color ID", "Rebrickable Color ID", false, 12, true},
        {"brickLinkPartId", "BrickLink Part ID", "BrickLink Part ID", false, 13, false},
        {"brickLinkColorId", "BrickLink Color ID", "BrickLink Color ID", false, 14, false}
    };
}

QSet<QString> InventoryExportService::remoteEnrichmentFieldIds()
{
    return {QStringLiteral("legoElementId"), QStringLiteral("rebrickablePartId"),
            QStringLiteral("brickLinkPartId"), QStringLiteral("brickLinkColorId")};
}

InventoryExportConfiguration InventoryExportService::defaultConfiguration()
{
    InventoryExportConfiguration result;
    for (const auto& field : fieldDescriptors()) {
        result.fieldOrder.append(field.id);
        if (field.defaultEnabled) result.enabledFields.insert(field.id);
    }
    return result;
}

InventoryExportConfiguration InventoryExportService::normalizeConfiguration(
    const QStringList& order, const QStringList& enabled)
{
    if (order.isEmpty()) return defaultConfiguration();
    QHash<QString, InventoryExportFieldDescriptor> known;
    for (const auto& field : fieldDescriptors()) known.insert(field.id, field);
    InventoryExportConfiguration result;
    for (const QString& id : order)
        if (known.contains(id) && !result.fieldOrder.contains(id)) result.fieldOrder.append(id);
    for (const auto& field : fieldDescriptors())
        if (!result.fieldOrder.contains(field.id)) result.fieldOrder.append(field.id);
    for (const QString& id : enabled)
        if (known.contains(id)) result.enabledFields.insert(id);
    return result;
}

QList<InventoryExportRow> InventoryExportService::createRows(
    const QList<InventorySearchResult>& source,const QSet<QString>& enabledFields) const
{
    QList<InventoryExportRow> result;result.reserve(source.size());
    for(const auto&value:source){InventoryExportRow row;row.inventoryRecordId=value.inventoryRecordId;
        row.storageLocationId=value.storageLocationId;row.partId=value.partId;row.colorId=value.colorId;
        row.manufacturerId=value.manufacturerId;row.partNumber=value.partNumber;row.partName=value.partName;
        row.category=value.categoryName;row.color=value.colorName;row.quantity=value.quantity;
        row.storagePath=value.storageLocationName;row.manufacturer=value.manufacturerName;
        row.condition=value.condition;row.ownership=value.ownershipType;
        row.rebrickableColorId=value.rebrickableColorId;result.append(row);}
    enrichRows(result,enabledFields);return result;
}

void InventoryExportService::enrichRows(QList<InventoryExportRow>&rows,const QSet<QString>&fields) const
{
    const bool parts=fields.contains("rebrickablePartId");
    const bool elements=fields.contains("legoElementId");
    const bool brickParts=fields.contains("brickLinkPartId");
    const bool brickColors=fields.contains("brickLinkColorId");
    if(!parts&&!elements&&!brickParts&&!brickColors)return;
    QSet<int> partIds,colorIds,manufacturerIds;QSet<quint64> pairs;
    for(const auto&r:rows){partIds.insert(r.partId);colorIds.insert(r.colorId);manufacturerIds.insert(r.manufacturerId);pairs.insert((quint64(quint32(r.partId))<<32)|quint32(r.colorId));}
    auto chunks=[](const QSet<int>&ids){QList<QList<int>> result;QList<int> current;for(int id:ids){if(id<=0)continue;current<<id;if(current.size()==400){result<<current;current.clear();}}if(!current.isEmpty())result<<current;return result;};
    auto placeholders=[](int count){QStringList p;for(int i=0;i<count;++i)p<<QStringLiteral("?");return p.join(',');};
    QHash<int,QString> rbParts,blColors,mappedParts;QHash<int,QStringList> blParts;QHash<quint64,QStringList> elementMap;QSet<int> elementManufacturers;
    if(parts)for(const auto&chunk:chunks(partIds)){QSqlQuery q(m_database);q.prepare(QStringLiteral("SELECT id,rebrickable_part_id FROM part WHERE id IN (%1)").arg(placeholders(chunk.size())));for(int id:chunk)q.addBindValue(id);if(q.exec())while(q.next())rbParts.insert(q.value(0).toInt(),q.value(1).toString());}
    if(elements){for(const auto&chunk:chunks(manufacturerIds)){QSqlQuery q(m_database);q.prepare(QStringLiteral("SELECT id FROM manufacturer WHERE supports_lego_element_ids=1 AND id IN (%1)").arg(placeholders(chunk.size())));for(int id:chunk)q.addBindValue(id);if(q.exec())while(q.next())elementManufacturers.insert(q.value(0).toInt());}for(const auto&chunk:chunks(partIds)){QSqlQuery q(m_database);q.prepare(QStringLiteral("SELECT part_id,color_id,element_id FROM part_element_identifier WHERE is_active=1 AND part_id IN (%1) ORDER BY element_id COLLATE NOCASE").arg(placeholders(chunk.size())));for(int id:chunk)q.addBindValue(id);if(q.exec())while(q.next()){const quint64 key=(quint64(quint32(q.value(0).toInt()))<<32)|quint32(q.value(1).toInt());if(pairs.contains(key))elementMap[key].append(q.value(2).toString());}}}
    const QString provider=apiProviderName(ApiProvider::BrickLink);
    if(brickParts){for(const auto&chunk:chunks(partIds)){QSqlQuery q(m_database);q.prepare(QStringLiteral("SELECT part_id,external_id FROM external_part_mapping WHERE provider=? AND mapping_status='Mapped' AND part_id IN (%1)").arg(placeholders(chunk.size())));q.addBindValue(provider);for(int id:chunk)q.addBindValue(id);if(q.exec())while(q.next())mappedParts.insert(q.value(0).toInt(),q.value(1).toString().trimmed());QSqlQuery ids(m_database);ids.prepare(QStringLiteral("SELECT part_id,external_id FROM external_part_identifier WHERE provider=? COLLATE NOCASE AND is_active=1 AND part_id IN (%1) ORDER BY external_id COLLATE NOCASE").arg(placeholders(chunk.size())));ids.addBindValue(provider);for(int id:chunk)ids.addBindValue(id);if(ids.exec())while(ids.next())blParts[ids.value(0).toInt()].append(ids.value(1).toString());}for(auto it=mappedParts.cbegin();it!=mappedParts.cend();++it)blParts[it.key()]={it.value()};for(auto it=blParts.begin();it!=blParts.end();++it)it.value()=sortedDistinct(it.value());}
    if(brickColors)for(const auto&chunk:chunks(colorIds)){QSqlQuery q(m_database);q.prepare(QStringLiteral("SELECT color_id,external_id FROM external_color_mapping WHERE provider=? AND mapping_status='Mapped' AND color_id IN (%1)").arg(placeholders(chunk.size())));q.addBindValue(provider);for(int id:chunk)q.addBindValue(id);if(q.exec())while(q.next())blColors.insert(q.value(0).toInt(),q.value(1).toString().trimmed());}
    for(auto&r:rows){if(parts)r.rebrickablePartId=rbParts.value(r.partId);if(elements){const quint64 key=(quint64(quint32(r.partId))<<32)|quint32(r.colorId);r.legoElementIds=elementManufacturers.contains(r.manufacturerId)?sortedDistinct(elementMap.value(key)):QStringList{};}if(brickParts)r.brickLinkPartIds=blParts.value(r.partId);if(brickColors)r.brickLinkColorId=blColors.value(r.colorId);}
}

QList<InventoryExportRow> InventoryExportService::createRemoteRows(
    const QList<RemoteReadDto::InventoryExportRow>& source)
{
    QList<InventoryExportRow> result;
    for (const auto& value : source) {
        InventoryExportRow row;
        row.inventoryRecordId=value.inventoryRecordId; row.partNumber=value.partNumber;
        row.partName=value.partName; row.category=value.category; row.color=value.color;
        row.quantity=value.quantity; row.storagePath=value.storagePath;
        row.manufacturer=value.manufacturer; row.condition=value.condition;
        row.ownership=value.ownership; row.legoElementIds=value.legoElementIds;
        row.rebrickablePartId=value.rebrickablePartId;
        row.rebrickableColorId=value.rebrickableColorId;
        row.brickLinkPartIds=value.brickLinkPartIds; row.brickLinkColorId=value.brickLinkColorId;
        result.append(row);
    }
    return result;
}

InventoryExportProjection InventoryExportService::project(
    const QList<InventoryExportRow>& rows, const InventoryExportConfiguration& config,
    int maximumRows)
{
    InventoryExportProjection result;
    QHash<QString, InventoryExportFieldDescriptor> known;
    for (const auto& field : fieldDescriptors()) known.insert(field.id, field);
    for (const QString& id : config.fieldOrder) if (config.enabledFields.contains(id) && known.contains(id)) {
        result.fields.append(known.value(id)); result.headers.append(known.value(id).header);
    }
    const int count = maximumRows < 0 ? rows.size() : qMin(maximumRows, rows.size());
    for (int i=0; i<count; ++i) {
        const auto& r=rows.at(i); QStringList values;
        for (const auto& f : result.fields) {
            if(f.id=="partNumber")values<<r.partNumber; else if(f.id=="partName")values<<r.partName;
            else if(f.id=="category")values<<r.category; else if(f.id=="color")values<<r.color;
            else if(f.id=="quantity")values<<QString::number(r.quantity); else if(f.id=="storage")values<<r.storagePath;
            else if(f.id=="manufacturer")values<<r.manufacturer; else if(f.id=="condition")values<<r.condition;
            else if(f.id=="ownership")values<<r.ownership; else if(f.id=="recordId")values<<QString::number(r.inventoryRecordId);
            else if(f.id=="legoElementId")values<<r.legoElementIds.join(';'); else if(f.id=="rebrickablePartId")values<<r.rebrickablePartId;
            else if(f.id=="rebrickableColorId")values<<(r.rebrickableColorId>=0?QString::number(r.rebrickableColorId):QString());
            else if(f.id=="brickLinkPartId")values<<r.brickLinkPartIds.join(';'); else if(f.id=="brickLinkColorId")values<<r.brickLinkColorId;
        }
        result.rows.append(values);
    }
    return result;
}
