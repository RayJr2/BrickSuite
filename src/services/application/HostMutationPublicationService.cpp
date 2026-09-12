#include "HostMutationPublicationService.h"

#include <QDebug>

HostMutationPublicationService::HostMutationPublicationService(Sink sink)
    : m_sink(std::move(sink))
{}

OperationalInvalidation HostMutationPublicationService::invalidationFor(
    Workflow workflow, const Scope& scope)
{
    using D = OperationalInvalidationDomain;
    OperationalInvalidation result;
    switch (workflow) {
    case Workflow::Workspace:
        result.domains = {D::Workspaces};
        break;
    case Workflow::Storage:
        result.domains = {D::Storage};
        result.workspaceId = scope.workspaceId;
        result.storageLocationId = scope.storageLocationId;
        break;
    case Workflow::Inventory:
        result.domains = {D::Inventory, D::InventoryHistory, D::Builds,
                          D::BuildRequirements, D::MissingParts, D::Pulling};
        result.workspaceId = scope.workspaceId;
        result.inventoryRecordId = scope.inventoryRecordId;
        break;
    case Workflow::BuildMetadata:
        result.domains = {D::Builds};
        result.workspaceId = scope.workspaceId;
        result.buildId = scope.buildId;
        break;
    case Workflow::BuildRequirements:
        result.domains = {D::Builds, D::BuildRequirements, D::MissingParts, D::Pulling};
        result.workspaceId = scope.workspaceId;
        result.buildId = scope.buildId;
        break;
    case Workflow::BuildCancellation:
    case Workflow::BuildDisassembly:
    case Workflow::CompleteSetSpare:
        result.domains = {D::Builds, D::BuildRequirements, D::MissingParts, D::Pulling};
        if (scope.inventoryChanged) {
            result.domains.append(D::Inventory);
            result.domains.append(D::InventoryHistory);
        }
        if (scope.collectionChanged) result.domains.append(D::Collection);
        result.workspaceId = scope.workspaceId;
        result.buildId = scope.buildId;
        break;
    case Workflow::Pulling:
        result.domains = {D::Inventory, D::InventoryHistory, D::Builds,
                          D::BuildRequirements, D::MissingParts, D::Pulling};
        result.workspaceId = scope.workspaceId;
        result.buildId = scope.buildId;
        // Pulling can affect several Inventory records, so it deliberately
        // never claims a single inventoryRecordId.
        break;
    case Workflow::Collection:
        result.domains = {D::Collection};
        result.workspaceId = scope.workspaceId;
        result.collectionItemId = scope.collectionItemId;
        break;
    case Workflow::PartReferenceCustomization:
        result.domains = {D::PartReferenceCustomizations};
        result.partNumber = scope.partNumber.trimmed();
        break;
    }
    return result;
}

bool HostMutationPublicationService::publish(Workflow workflow, const Scope& scope,
                                             QString* error) const
{
    OperationalInvalidation invalidation = invalidationFor(workflow, scope);
    QString validationError;
    if (!OperationalInvalidation::validate(invalidation, false, &validationError)) {
        if (error) *error = validationError;
        qWarning().noquote() << "Host mutation invalidation was not published:"
                             << validationError;
        return false;
    }
    if (m_sink) m_sink(invalidation);
    return true;
}
