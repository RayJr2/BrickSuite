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

#include "../../network/OperationalInvalidation.h"

#include <functional>
#include <optional>

class HostMutationPublicationService
{
public:
    enum class Workflow {
        Workspace,
        Storage,
        Inventory,
        BuildMetadata,
        BuildRequirements,
        BuildCancellation,
        BuildDisassembly,
        CompleteSetSpare,
        Pulling,
        Collection,
        CollectionDisassembly,
        PartReferenceCustomization
    };

    struct Scope {
        std::optional<qint64> workspaceId;
        std::optional<qint64> buildId;
        std::optional<qint64> inventoryRecordId;
        std::optional<qint64> storageLocationId;
        std::optional<qint64> collectionItemId;
        QString partNumber;
        bool inventoryChanged = false;
        bool collectionChanged = false;
    };

    using Sink = std::function<void(const OperationalInvalidation&)>;

    explicit HostMutationPublicationService(Sink sink = {});

    bool publish(Workflow workflow, const Scope& scope, QString* error = nullptr) const;
    bool publish(Workflow workflow, QString* error = nullptr) const
    { return publish(workflow, Scope{}, error); }
    static OperationalInvalidation invalidationFor(Workflow workflow, const Scope& scope);
    static OperationalInvalidation invalidationFor(Workflow workflow)
    { return invalidationFor(workflow, Scope{}); }

private:
    Sink m_sink;
};
