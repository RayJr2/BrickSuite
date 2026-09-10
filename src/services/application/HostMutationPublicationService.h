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
        Pulling,
        Collection,
        PartReferenceCustomization
    };

    struct Scope {
        std::optional<qint64> workspaceId;
        std::optional<qint64> buildId;
        std::optional<qint64> inventoryRecordId;
        std::optional<qint64> storageLocationId;
        std::optional<qint64> collectionItemId;
        QString partNumber;
    };

    using Sink = std::function<void(const OperationalInvalidation&)>;

    explicit HostMutationPublicationService(Sink sink = {});

    bool publish(Workflow workflow, const Scope& scope = {}, QString* error = nullptr) const;
    static OperationalInvalidation invalidationFor(Workflow workflow, const Scope& scope = {});

private:
    Sink m_sink;
};
