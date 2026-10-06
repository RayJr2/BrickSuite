/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

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
    PartReferenceCustomizations,
    Buildability
};

struct OperationalInvalidation
{
    static constexpr int MaximumDomains = 11;
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
    OperationalInvalidation forProtocolMinor(int protocolMinor) const;
    static bool fromPayload(const QJsonObject& payload, OperationalInvalidation* result,
                            QString* error = nullptr);
    static bool validate(const OperationalInvalidation& value, bool requireSequence,
                         QString* error = nullptr);
};

QString operationalInvalidationDomainName(OperationalInvalidationDomain domain);
std::optional<OperationalInvalidationDomain> operationalInvalidationDomainFromName(
    const QString& name);

Q_DECLARE_METATYPE(OperationalInvalidation)
