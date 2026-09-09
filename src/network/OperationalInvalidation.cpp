#include "OperationalInvalidation.h"

#include <QJsonArray>
#include <QSet>
#include <cmath>

const QString OperationalInvalidation::Operation = QStringLiteral("shared.invalidated");
const QString OperationalInvalidation::Capability = QStringLiteral("shared.invalidations");

QString operationalInvalidationDomainName(OperationalInvalidationDomain domain)
{
    switch (domain) {
    case OperationalInvalidationDomain::Workspaces: return QStringLiteral("workspaces");
    case OperationalInvalidationDomain::Storage: return QStringLiteral("storage");
    case OperationalInvalidationDomain::Inventory: return QStringLiteral("inventory");
    case OperationalInvalidationDomain::InventoryHistory: return QStringLiteral("inventoryHistory");
    case OperationalInvalidationDomain::Builds: return QStringLiteral("builds");
    case OperationalInvalidationDomain::BuildRequirements: return QStringLiteral("buildRequirements");
    case OperationalInvalidationDomain::MissingParts: return QStringLiteral("missingParts");
    case OperationalInvalidationDomain::Pulling: return QStringLiteral("pulling");
    case OperationalInvalidationDomain::Collection: return QStringLiteral("collection");
    case OperationalInvalidationDomain::PartReferenceCustomizations:
        return QStringLiteral("partReferenceCustomizations");
    }
    return {};
}

std::optional<OperationalInvalidationDomain> operationalInvalidationDomainFromName(
    const QString& name)
{
    for (int value = int(OperationalInvalidationDomain::Workspaces);
         value <= int(OperationalInvalidationDomain::PartReferenceCustomizations); ++value) {
        const auto domain = static_cast<OperationalInvalidationDomain>(value);
        if (operationalInvalidationDomainName(domain) == name) return domain;
    }
    return std::nullopt;
}

namespace {
bool readOptionalId(const QJsonObject& payload, const QString& key,
                    std::optional<qint64>* target, QString* error)
{
    if (!payload.contains(key)) return true;
    const QJsonValue json = payload.value(key);
    const double value = json.toDouble(-1);
    if (!json.isDouble() || value < 1 || value > double(OperationalInvalidation::MaximumJsonInteger)
        || value != std::floor(value)) {
        if (error) *error = QStringLiteral("Invalid %1 in invalidation event.").arg(key);
        return false;
    }
    *target = qint64(value);
    return true;
}

bool requiresWorkspace(OperationalInvalidationDomain domain)
{
    return domain != OperationalInvalidationDomain::Workspaces
        && domain != OperationalInvalidationDomain::PartReferenceCustomizations;
}
}

QJsonObject OperationalInvalidation::toPayload() const
{
    QJsonArray domainValues;
    for (const auto domain : domains) domainValues.append(operationalInvalidationDomainName(domain));
    QJsonObject result{{QStringLiteral("sequence"), double(sequence)},
                       {QStringLiteral("domains"), domainValues}};
    const auto addId = [&result](const QString& key, const std::optional<qint64>& value) {
        if (value) result.insert(key, double(*value));
    };
    addId(QStringLiteral("workspaceId"), workspaceId);
    addId(QStringLiteral("buildId"), buildId);
    addId(QStringLiteral("inventoryRecordId"), inventoryRecordId);
    addId(QStringLiteral("storageLocationId"), storageLocationId);
    addId(QStringLiteral("collectionItemId"), collectionItemId);
    if (!partNumber.isEmpty()) result.insert(QStringLiteral("partNumber"), partNumber);
    return result;
}

bool OperationalInvalidation::fromPayload(const QJsonObject& payload,
                                           OperationalInvalidation* result, QString* error)
{
    if (!result) return false;
    static const QSet<QString> allowed{QStringLiteral("sequence"), QStringLiteral("domains"),
        QStringLiteral("workspaceId"), QStringLiteral("buildId"),
        QStringLiteral("inventoryRecordId"), QStringLiteral("storageLocationId"),
        QStringLiteral("collectionItemId"), QStringLiteral("partNumber")};
    for (auto it = payload.constBegin(); it != payload.constEnd(); ++it) {
        if (!allowed.contains(it.key())) {
            if (error) *error = QStringLiteral("Unsupported invalidation payload field.");
            return false;
        }
    }
    OperationalInvalidation value;
    const QJsonValue sequenceValue = payload.value(QStringLiteral("sequence"));
    const double sequence = sequenceValue.toDouble(-1);
    if (!sequenceValue.isDouble() || sequence < 1 || sequence > double(MaximumJsonInteger)
        || sequence != std::floor(sequence)) {
        if (error) *error = QStringLiteral("Invalid invalidation sequence.");
        return false;
    }
    value.sequence = quint64(sequence);
    const QJsonValue domainsValue = payload.value(QStringLiteral("domains"));
    if (!domainsValue.isArray() || domainsValue.toArray().isEmpty()
        || domainsValue.toArray().size() > MaximumDomains) {
        if (error) *error = QStringLiteral("Invalid invalidation domain list.");
        return false;
    }
    QSet<QString> seen;
    for (const QJsonValue& json : domainsValue.toArray()) {
        if (!json.isString() || seen.contains(json.toString())) {
            if (error) *error = QStringLiteral("Invalid or duplicate invalidation domain.");
            return false;
        }
        const auto domain = operationalInvalidationDomainFromName(json.toString());
        if (!domain) {
            if (error) *error = QStringLiteral("Unknown invalidation domain.");
            return false;
        }
        seen.insert(json.toString());
        value.domains.append(*domain);
    }
    if (!readOptionalId(payload, QStringLiteral("workspaceId"), &value.workspaceId, error)
        || !readOptionalId(payload, QStringLiteral("buildId"), &value.buildId, error)
        || !readOptionalId(payload, QStringLiteral("inventoryRecordId"), &value.inventoryRecordId, error)
        || !readOptionalId(payload, QStringLiteral("storageLocationId"), &value.storageLocationId, error)
        || !readOptionalId(payload, QStringLiteral("collectionItemId"), &value.collectionItemId, error))
        return false;
    if (payload.contains(QStringLiteral("partNumber"))) {
        if (!payload.value(QStringLiteral("partNumber")).isString()) return false;
        value.partNumber = payload.value(QStringLiteral("partNumber")).toString().trimmed();
        if (value.partNumber.isEmpty()) {
            if (error) *error = QStringLiteral("Invalid partNumber in invalidation event.");
            return false;
        }
    }
    if (!validate(value, true, error)) return false;
    *result = value;
    return true;
}

bool OperationalInvalidation::validate(const OperationalInvalidation& value,
                                        bool requireSequence, QString* error)
{
    if ((requireSequence && value.sequence == 0) || value.sequence > MaximumJsonInteger
        || value.domains.isEmpty() || value.domains.size() > MaximumDomains) {
        if (error) *error = QStringLiteral("Invalid invalidation event.");
        return false;
    }
    QSet<int> seen;
    bool scoped = false;
    bool hostWide = false;
    for (const auto domain : value.domains) {
        const int numeric = int(domain);
        if (numeric < int(OperationalInvalidationDomain::Workspaces)
            || numeric > int(OperationalInvalidationDomain::PartReferenceCustomizations)
            || seen.contains(numeric)) {
            if (error) *error = QStringLiteral("Invalid or duplicate invalidation domain.");
            return false;
        }
        seen.insert(numeric);
        if (requiresWorkspace(domain)) scoped = true;
        else hostWide = true;
    }
    if (scoped && hostWide) {
        if (error) *error = QStringLiteral("Host-wide and Workspace-scoped domains cannot be mixed.");
        return false;
    }
    if (scoped && !value.workspaceId) {
        if (error) *error = QStringLiteral("Workspace-scoped invalidation requires workspaceId.");
        return false;
    }
    const auto validId = [](const std::optional<qint64>& id) {
        return !id || (*id > 0 && quint64(*id) <= MaximumJsonInteger);
    };
    if (!validId(value.workspaceId) || !validId(value.buildId)
        || !validId(value.inventoryRecordId) || !validId(value.storageLocationId)
        || !validId(value.collectionItemId)
        || value.partNumber.size() > MaximumPartNumberLength) {
        if (error) *error = QStringLiteral("Invalid invalidation identifier.");
        return false;
    }
    return true;
}
