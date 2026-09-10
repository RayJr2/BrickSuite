#include "../src/services/application/HostMutationPublicationService.h"

#include <QCoreApplication>
#include <QDebug>

namespace {
bool require(bool condition, const char* message)
{
    if (!condition) qCritical() << message;
    return condition;
}

bool has(const OperationalInvalidation& value, OperationalInvalidationDomain domain)
{
    return value.domains.contains(domain);
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    using Service = HostMutationPublicationService;
    using D = OperationalInvalidationDomain;

    int publications = 0;
    OperationalInvalidation observed;
    Service service([&](const OperationalInvalidation& value) {
        ++publications;
        observed = value;
    });

    Service::Scope scope;
    scope.workspaceId = 7;
    scope.buildId = 11;
    if (!require(service.publish(Service::Workflow::Pulling, scope),
                 "Pulling publication should validate")
        || !require(publications == 1, "One workflow must produce one event")
        || !require(has(observed, D::Inventory) && has(observed, D::InventoryHistory)
                        && has(observed, D::Builds) && has(observed, D::BuildRequirements)
                        && has(observed, D::MissingParts) && has(observed, D::Pulling),
                    "Pulling compound domains are incomplete")
        || !require(observed.workspaceId == 7 && observed.buildId == 11,
                    "Pulling scope was not preserved")
        || !require(!observed.inventoryRecordId,
                    "Multi-record Pulling must not claim one Inventory record"))
        return 1;

    scope = {};
    scope.workspaceId = 3;
    scope.inventoryRecordId = 19;
    if (!require(service.publish(Service::Workflow::Inventory, scope),
                 "Inventory publication should validate")
        || !require(publications == 2, "Inventory should publish once")
        || !require(observed.inventoryRecordId == 19,
                    "Single Inventory scope was not preserved"))
        return 1;

    scope = {};
    scope.partNumber = QStringLiteral(" 3001 ");
    if (!require(service.publish(Service::Workflow::PartReferenceCustomization, scope),
                 "Customization publication should validate")
        || !require(publications == 3 && observed.domains == QList<D>{D::PartReferenceCustomizations},
                    "Customization must publish only its Host-wide domain")
        || !require(observed.partNumber == QStringLiteral("3001"),
                    "Part Number should be normalized"))
        return 1;

    QString error;
    if (!require(!service.publish(Service::Workflow::Storage, {}, &error),
                 "Workspace-scoped publication without Workspace must fail")
        || !require(publications == 3, "Invalid publication must not reach the sink")
        || !require(!error.isEmpty(), "Invalid publication should explain failure"))
        return 1;

    Service standalone;
    scope = {};
    scope.workspaceId = 1;
    if (!require(standalone.publish(Service::Workflow::Collection, scope),
                 "No-server publication should remain a successful no-op"))
        return 1;

    const auto workspace = Service::invalidationFor(Service::Workflow::Workspace);
    if (!require(workspace.domains == QList<D>{D::Workspaces} && !workspace.workspaceId,
                 "Workspace publication must remain Host-wide"))
        return 1;

    scope = {};
    scope.workspaceId = 4;
    scope.storageLocationId = 9;
    const auto storage = Service::invalidationFor(Service::Workflow::Storage, scope);
    if (!require(storage.domains == QList<D>{D::Storage}
                     && storage.workspaceId == 4 && storage.storageLocationId == 9,
                 "Storage mapping or scope is incorrect"))
        return 1;

    scope = {};
    scope.workspaceId = 4;
    scope.buildId = 12;
    const auto metadata = Service::invalidationFor(Service::Workflow::BuildMetadata, scope);
    const auto requirements = Service::invalidationFor(Service::Workflow::BuildRequirements, scope);
    if (!require(metadata.domains == QList<D>{D::Builds},
                 "Build metadata must not invalidate requirement projections")
        || !require(has(requirements, D::Builds) && has(requirements, D::BuildRequirements)
                        && has(requirements, D::MissingParts) && has(requirements, D::Pulling)
                        && !has(requirements, D::Inventory),
                    "Build requirement mapping is incorrect"))
        return 1;

    scope = {};
    scope.workspaceId = 4;
    scope.collectionItemId = 15;
    const auto collection = Service::invalidationFor(Service::Workflow::Collection, scope);
    if (!require(collection.domains == QList<D>{D::Collection}
                     && collection.collectionItemId == 15,
                 "Collection mapping or scope is incorrect"))
        return 1;

    qInfo() << "Host mutation publication tests passed.";
    return 0;
}
