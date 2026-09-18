#include "MissingPartsExportService.h"

#include "../../api/ApiProvider.h"
#include "../../models/Build.h"
#include "../../models/ExternalMappingStatus.h"
#include "../../repositories/ColorRepository.h"
#include "../../repositories/ExternalColorMappingRepository.h"
#include "../../repositories/ExternalPartIdentifierRepository.h"
#include "../../repositories/ExternalPartMappingRepository.h"
#include "../../repositories/ManufacturerRepository.h"
#include "../../repositories/PartCategoryRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../services/application/dto/RemoteReadDtos.h"
#include "../parts/ElementIdentityService.h"

#include <QHash>

#include <algorithm>

namespace {
QStringList distinctSorted(QStringList values)
{
    for (QString& value : values) value = value.trimmed();
    values.removeAll(QString());
    std::sort(values.begin(), values.end(), [](const QString& left, const QString& right) {
        return QString::compare(left, right, Qt::CaseInsensitive) < 0;
    });
    values.erase(std::unique(values.begin(), values.end(), [](const QString& left,
                                                              const QString& right) {
        return left.compare(right, Qt::CaseInsensitive) == 0;
    }), values.end());
    return values;
}
}

MissingPartsExportService::MissingPartsExportService(const QSqlDatabase& database)
    : m_database(database)
{
}

QList<MissingPartsExportFieldDescriptor> MissingPartsExportService::fieldDescriptors()
{
    using F = MissingPartsExportField;
    return {
        {F::Build, "build", "Build", "Build", true, 0, false, false},
        {F::BuildReference, "buildReference", "Build Reference", "Set Number", true, 1, false, false},
        {F::PartNumber, "partNumber", "Part Number", "Part Number", true, 2, false, false},
        {F::PartName, "partName", "Part Name", "Part Name", true, 3, false, false},
        {F::Category, "category", "Category", "Category", false, 10, false, false},
        {F::Color, "color", "Color", "Color", true, 4, false, false},
        {F::QuantityMissing, "quantityMissing", "Quantity Missing", "Missing", true, 9, false, true},
        {F::Manufacturer, "manufacturer", "Manufacturer", "Manufacturer", false, 11, false, false},
        {F::LegoElementId, "legoElementId", "LEGO Element ID", "LEGO Element ID", false, 12, true, false},
        {F::RebrickablePartId, "rebrickablePartId", "Rebrickable Part ID", "Rebrickable Part ID", false, 13, false, false},
        {F::RebrickableColorId, "rebrickableColorId", "Rebrickable Color ID", "Rebrickable Color ID", false, 14, false, true},
        {F::BrickLinkPartId, "brickLinkPartId", "BrickLink Part ID", "BrickLink Part ID", false, 15, true, false},
        {F::BrickLinkColorId, "brickLinkColorId", "BrickLink Color ID", "BrickLink Color ID", false, 16, false, false},
        {F::Required, "required", "Required", "Required", true, 5, false, true},
        {F::Pulled, "pulled", "Pulled", "Pulled", true, 6, false, true},
        {F::Remaining, "remaining", "Remaining", "Remaining", true, 7, false, true},
        {F::Available, "available", "Available", "Available", true, 8, false, true}
    };
}

MissingPartsExportConfiguration MissingPartsExportService::defaultConfiguration()
{
    MissingPartsExportConfiguration result;
    auto fields = fieldDescriptors();
    std::sort(fields.begin(), fields.end(), [](const auto& left, const auto& right) {
        return left.defaultOrder < right.defaultOrder;
    });
    for (const auto& field : fields) {
        result.fieldOrder.append(field.id);
        if (field.defaultEnabled) result.enabledFields.insert(field.id);
    }
    return result;
}

MissingPartsExportConfiguration MissingPartsExportService::normalizeConfiguration(
    const QStringList& savedOrder, const QStringList& savedEnabled)
{
    if (savedOrder.isEmpty()) return defaultConfiguration();

    const auto descriptors = fieldDescriptors();
    QHash<QString, MissingPartsExportFieldDescriptor> known;
    for (const auto& descriptor : descriptors) known.insert(descriptor.id, descriptor);

    MissingPartsExportConfiguration result;
    for (const QString& id : savedOrder) {
        if (known.contains(id) && !result.fieldOrder.contains(id))
            result.fieldOrder.append(id);
    }
    const QSet<QString> previouslyKnown(savedOrder.begin(), savedOrder.end());
    auto defaults = defaultConfiguration();
    for (const QString& id : defaults.fieldOrder) {
        if (!result.fieldOrder.contains(id)) result.fieldOrder.append(id);
    }
    for (const QString& id : savedEnabled) {
        if (known.contains(id)) result.enabledFields.insert(id);
    }
    for (const auto& descriptor : descriptors) {
        if (!previouslyKnown.contains(descriptor.id) && descriptor.defaultEnabled)
            result.enabledFields.insert(descriptor.id);
    }
    return result;
}

QString MissingPartsExportService::value(const MissingPartsExportRow& row,
                                         MissingPartsExportField field)
{
    using F = MissingPartsExportField;
    switch (field) {
    case F::Build: return row.buildName;
    case F::BuildReference: return row.buildReference;
    case F::PartNumber: return row.partNumber;
    case F::PartName: return row.partName;
    case F::Category: return row.category;
    case F::Color: return row.colorName;
    case F::QuantityMissing: return QString::number(row.missing);
    case F::Manufacturer: return row.manufacturer;
    case F::LegoElementId: return row.legoElementIds.join(';');
    case F::RebrickablePartId: return row.rebrickablePartId;
    case F::RebrickableColorId:
        return row.rebrickableColorId >= 0 ? QString::number(row.rebrickableColorId) : QString();
    case F::BrickLinkPartId: return row.brickLinkPartIds.join(';');
    case F::BrickLinkColorId: return row.brickLinkColorId;
    case F::Required: return QString::number(row.required);
    case F::Pulled: return QString::number(row.pulled);
    case F::Remaining: return QString::number(row.remaining);
    case F::Available: return QString::number(row.available);
    }
    return {};
}

MissingPartsExportProjection MissingPartsExportService::project(
    const QList<MissingPartsExportRow>& rows,
    const MissingPartsExportConfiguration& configuration)
{
    MissingPartsExportProjection result;
    QHash<QString, MissingPartsExportFieldDescriptor> descriptors;
    for (const auto& descriptor : fieldDescriptors()) descriptors.insert(descriptor.id, descriptor);
    for (const QString& id : configuration.fieldOrder) {
        if (!configuration.enabledFields.contains(id) || !descriptors.contains(id)) continue;
        result.fields.append(descriptors.value(id));
        result.headers.append(descriptors.value(id).csvHeader);
    }
    for (const auto& source : rows) {
        QStringList row;
        for (const auto& field : result.fields) row.append(value(source, field.field));
        result.rows.append(row);
    }
    return result;
}

QList<MissingPartsExportRow> MissingPartsExportService::createRows(
    const Build& build, const QList<MissingPartsService::MissingPart>& missingParts) const
{
    QList<MissingPartsExportRow> result;
    PartRepository parts(m_database);
    PartCategoryRepository categories(m_database);
    ColorRepository colors(m_database);
    ManufacturerRepository manufacturers(m_database);
    ElementIdentityService elements(m_database);
    ExternalPartMappingRepository partMappings(m_database);
    ExternalPartIdentifierRepository partIdentifiers(m_database);
    ExternalColorMappingRepository colorMappings(m_database);
    const QString brickLink = apiProviderName(ApiProvider::BrickLink);
    const auto manufacturer = manufacturers.getById(build.manufacturerId());
    const auto legoManufacturer = manufacturers.getById(manufacturers.legoManufacturerId());
    QHash<int, QString> rebrickablePartIds;
    QHash<int, QString> categoryNames;
    QHash<quint64, QStringList> elementIds;
    QHash<int, QStringList> brickLinkPartIds;
    QHash<int, QString> brickLinkColorIds;
    QHash<int, int> rebrickableColorIds;

    for (const auto& source : missingParts) {
        MissingPartsExportRow row;
        row.buildName = build.name();
        row.buildReference = build.setNumber();
        row.partId = source.partId;
        row.colorId = source.colorId;
        row.partNumber = source.partNumber;
        row.partName = source.partName;
        row.colorName = source.colorName;
        row.missing = source.missing;
        row.manufacturer = manufacturer ? manufacturer->name() : QString();
        row.required = source.required;
        row.pulled = source.pulled;
        row.remaining = source.remaining;
        row.available = source.available;

        if (!rebrickablePartIds.contains(source.partId)) {
            QString rebrickablePartId;
            QString categoryName;
            if (const auto part = parts.getById(source.partId)) {
                rebrickablePartId = part->rebrickablePartId();
                if (const auto category = categories.getById(part->partCategoryId()))
                    categoryName = category->name();
            }
            rebrickablePartIds.insert(source.partId, rebrickablePartId);
            categoryNames.insert(source.partId, categoryName);
        }
        row.rebrickablePartId = rebrickablePartIds.value(source.partId);
        row.category = categoryNames.value(source.partId);

        const quint64 partColorKey = (quint64(quint32(source.partId)) << 32)
                                     | quint32(source.colorId);
        if (!elementIds.contains(partColorKey))
            elementIds.insert(partColorKey,
                              elements.forPartColor(source.partId, source.colorId));
        if (manufacturer && manufacturer->supportsLegoElementIds())
            row.legoElementIds = elementIds.value(partColorKey);
        if (legoManufacturer && legoManufacturer->supportsLegoElementIds())
            row.pickABrickElementCandidates = elementIds.value(partColorKey);

        if (!brickLinkPartIds.contains(source.partId)) {
            QStringList identifiers;
            const auto mappedPart = partMappings.getByPartAndProvider(source.partId, brickLink);
            if (mappedPart && mappedPart->status == ExternalMappingStatus::Mapped
                && !mappedPart->externalId.trimmed().isEmpty()) {
                identifiers = {mappedPart->externalId.trimmed()};
            } else {
                for (const auto& id : partIdentifiers.findByPartAndProvider(source.partId, brickLink))
                    identifiers.append(id.externalId);
                identifiers = distinctSorted(identifiers);
            }
            brickLinkPartIds.insert(source.partId, identifiers);
        }
        row.brickLinkPartIds = brickLinkPartIds.value(source.partId);

        if (!brickLinkColorIds.contains(source.colorId)) {
            QString externalId;
            const auto mappedColor = colorMappings.getByColorAndProvider(source.colorId, brickLink);
            if (mappedColor && mappedColor->status == ExternalMappingStatus::Mapped)
                externalId = mappedColor->externalId.trimmed();
            brickLinkColorIds.insert(source.colorId, externalId);
        }
        row.brickLinkColorId = brickLinkColorIds.value(source.colorId);

        if (!rebrickableColorIds.contains(source.colorId)) {
            const auto color = colors.getById(source.colorId);
            rebrickableColorIds.insert(source.colorId,
                                       color ? color->rebrickableId() : -1);
        }
        row.rebrickableColorId = rebrickableColorIds.value(source.colorId, -1);
        result.append(row);
    }
    return result;
}

QList<MissingPartsExportRow> MissingPartsExportService::createRemoteRows(
    const RemoteReadDto::BuildDetail& build,
    const QList<RemoteReadDto::MissingPart>& missingParts)
{
    QList<MissingPartsExportRow> result;
    for (const auto& source : missingParts) {
        MissingPartsExportRow row;
        row.buildName = build.name;
        row.buildReference = build.setNumber;
        row.partNumber = source.partNumber;
        row.partName = source.partNameFallback;
        row.category = source.categoryName;
        row.colorName = source.colorNameFallback;
        row.missing = source.missing;
        row.manufacturer = source.manufacturerDisplay.isEmpty()
                               ? build.manufacturerDisplay : source.manufacturerDisplay;
        row.legoElementIds = source.legoElementIds;
        row.pickABrickElementCandidates = source.pickABrickElementCandidates;
        row.rebrickablePartId = source.rebrickablePartId;
        row.rebrickableColorId = source.rebrickableColorId;
        row.brickLinkPartIds = source.brickLinkPartIds;
        row.brickLinkColorId = source.brickLinkColorId;
        row.required = source.required;
        row.pulled = source.pulled;
        row.remaining = source.remaining;
        row.available = source.available;
        result.append(row);
    }
    return result;
}
