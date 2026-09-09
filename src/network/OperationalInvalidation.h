#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>
#include <optional>

enum class OperationalInvalidationDomain
{
    Workspaces,
    Storage,
    Inventory,
    InventoryHistory,
    Builds,
    BuildRequirements,
    MissingParts,
    Pulling,
    Collection,
    PartReferenceCustomizations
};

struct OperationalInvalidation
{
    static constexpr int MaximumDomains = 10;
    static constexpr int MaximumPartNumberLength = 128;
    static constexpr quint64 MaximumJsonInteger = 9007199254740991ULL;
    static const QString Operation;
    static const QString Capability;

    quint64 sequence = 0;
    QList<OperationalInvalidationDomain> domains;
    std::optional<qint64> workspaceId;
    std::optional<qint64> buildId;
    std::optional<qint64> inventoryRecordId;
    std::optional<qint64> storageLocationId;
    std::optional<qint64> collectionItemId;
    QString partNumber;

    QJsonObject toPayload() const;
    static bool fromPayload(const QJsonObject& payload, OperationalInvalidation* result,
                            QString* error = nullptr);
    static bool validate(const OperationalInvalidation& value, bool requireSequence,
                         QString* error = nullptr);
};

QString operationalInvalidationDomainName(OperationalInvalidationDomain domain);
std::optional<OperationalInvalidationDomain> operationalInvalidationDomainFromName(
    const QString& name);

Q_DECLARE_METATYPE(OperationalInvalidation)
