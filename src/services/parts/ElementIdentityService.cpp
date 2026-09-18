#include "ElementIdentityService.h"

#include "../../database/DatabaseManager.h"
#include "../../repositories/ManufacturerRepository.h"
#include "../../repositories/PartElementIdentifierRepository.h"

namespace {
constexpr auto RebrickableProvider = "Rebrickable";
}

QString ElementIdentityResult::label() const
{
    return elementIds.size() > 1 ? QStringLiteral("LEGO Elements:")
                                 : QStringLiteral("LEGO Element:");
}

QString ElementIdentityResult::displayText() const
{
    if (!applicable)
        return QStringLiteral("Not applicable");
    if (elementIds.isEmpty())
        return QStringLiteral("Not available");
    return elementIds.join(QStringLiteral(", "));
}

ElementIdentityService::ElementIdentityService(QSqlDatabase database)
    : m_database(database.isValid() ? database : DatabaseManager::instance().database())
{
}

ElementIdentityResult ElementIdentityService::forInventory(
    int partId, int colorId, int manufacturerId) const
{
    ElementIdentityResult result;
    const auto manufacturer = ManufacturerRepository(m_database).getById(manufacturerId);
    result.applicable = manufacturer && manufacturer->supportsLegoElementIds();
    if (result.applicable && partId > 0 && colorId > 0)
        result.elementIds = forPartColor(partId, colorId);
    return result;
}

QStringList ElementIdentityService::forPartColor(int partId, int colorId) const
{
    QStringList result;
    if (partId <= 0 || colorId <= 0)
        return result;

    const auto mappings = PartElementIdentifierRepository(m_database).findByPartColor(
        partId, colorId, QString::fromLatin1(RebrickableProvider));
    result.reserve(mappings.size());
    for (const auto& mapping : mappings)
        result.append(mapping.elementId);
    return result;
}

std::optional<ElementPartColorIdentity> ElementIdentityService::fromElementId(
    const QString& elementId) const
{
    const auto mapping = PartElementIdentifierRepository(m_database).findByElementId(
        QString::fromLatin1(RebrickableProvider), elementId.trimmed());
    if (mapping.id <= 0)
        return std::nullopt;
    return ElementPartColorIdentity{mapping.partId, mapping.colorId};
}
