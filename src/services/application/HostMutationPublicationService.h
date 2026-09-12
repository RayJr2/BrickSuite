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
