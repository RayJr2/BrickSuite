#include "../src/services/application/ApplicationServices.h"
#include "../src/services/parts/PartReferenceManifest.h"
#include "../src/app/WorkspaceContext.h"

#include <QCoreApplication>
#include <cstdio>

namespace {
bool check(bool value, const char* message)
{
    if (!value) std::fprintf(stderr, "FAILED: %s\n", message);
    return value;
}

class FakeInventory final : public InventoryApplicationService
{
public:
    int count(const InventorySearchCriteria&) const override { return 501; }
    QList<InventorySearchResult> searchRows(const InventorySearchCriteria& criteria) const override
    {
        ++calls;
        lastOffset = criteria.offset;
        InventorySearchResult row;
        row.inventoryRecordId = 901;
        row.partNumber = QStringLiteral("poison-remote-part");
        row.rebrickableColorId = 4;
        return {row};
    }
    std::optional<InventoryRecord> get(int id) const override
    {
        InventoryRecord record;
        record.setId(id);
        return record;
    }
    QList<InventoryHistoryResult> history(int, int, int) const override
    {
        InventoryHistoryResult row;
        row.movementId = 77;
        return {row};
    }
    mutable int calls = 0;
    mutable int lastOffset = -1;
};

class FakeBuilds final : public BuildApplicationService
{
public:
    QList<Build> list(int workspaceId, bool) const override
    { Build b; b.setId(workspaceId + 100); return {b}; }
    std::optional<Build> get(int id) const override
    { Build b; b.setId(id); return b; }
    QList<BuildRequirement> requirements(int) const override
    { BuildRequirement r; r.setId(88); return {r}; }
    QList<MissingPartsService::MissingPart> missingParts(int, int) const override
    { MissingPartsService::MissingPart p; p.partNumber = "3001"; return {p}; }
    BuildPullingService::PullingView pullingView(int id) const override
    { BuildPullingService::PullingView v; v.success = true; v.summary.buildId = id; return v; }
};

class FakeCollection final : public CollectionApplicationService
{
public:
    int count(const CollectionSearchCriteria&) const override { return 1; }
    QList<CollectionSearchResult> searchRows(const CollectionSearchCriteria&) const override
    { CollectionSearchResult row; row.displayReference = "fig-test"; return {row}; }
    std::optional<CollectionSearchResult> getDisplay(int) const override
    { CollectionSearchResult row; row.displayName = "Injected"; return row; }
};
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    FakeInventory inventory;
    InventorySearchCriteria criteria;
    criteria.limit = 250;
    criteria.offset = 250;
    const auto page = inventory.search(criteria);
    ok &= check(page.total == 501 && page.rows.size() == 1, "paged inventory contract");
    ok &= check(page.rows.first().partNumber == "poison-remote-part",
                "injected inventory result retained");
    ok &= check(inventory.calls == 1 && inventory.lastOffset == 250,
                "criteria reaches injected service exactly once");
    ok &= check(inventory.get(901)->id() == 901 && inventory.history(1, 2, 3).first().movementId == 77,
                "inventory detail and history contracts");

    FakeBuilds builds;
    ok &= check(builds.list(5, false).first().id() == 105, "Build list contract");
    ok &= check(builds.get(9)->id() == 9 && builds.requirements(9).first().id() == 88,
                "Build detail and requirement contracts");
    ok &= check(builds.missingParts(1, 9).first().partNumber == "3001",
                "Missing Parts contract delegates a canonical Part identity");
    ok &= check(builds.pullingView(9).success && builds.pullingView(9).summary.buildId == 9,
                "pulling read contract");

    FakeCollection collection;
    CollectionSearchCriteria collectionCriteria;
    ok &= check(collection.search(collectionCriteria).total == 1,
                "Collection paged search contract");
    ok &= check(collection.getDisplay(1)->displayName == "Injected",
                "Collection detail contract");

    auto host = createUnavailableHostApplicationServices();
    ok &= check(host->sharedDataSource() == SharedDataSource::BrickSuiteHost,
                "Host composition identifies its source");
    ok &= check(!host->sharedStatus().isAvailable()
                    && host->sharedStatus().availability
                        == ApplicationServiceAvailability::UnavailableNotConnected,
                "Host composition reports not connected");
    ok &= check(host->workspaces().list().isEmpty()
                    && !host->workspaces().get(1).has_value()
                    && !host->workspaces().exists(1),
                "Host Workspace composition has no local fallback");
    ok &= check(host->inventory().count(criteria) == 0
                    && host->inventory().searchRows(criteria).isEmpty()
                    && !host->inventory().get(901).has_value()
                    && host->inventory().history(1, 2, 3).isEmpty(),
                "Host Inventory composition has no local fallback");
    ok &= check(host->builds().list(5, false).isEmpty()
                    && !host->builds().get(9).has_value()
                    && host->builds().requirements(9).isEmpty()
                    && host->builds().missingParts(5, 9).isEmpty()
                    && !host->builds().pullingView(9).success,
                "Host Build composition has no local fallback");
    ok &= check(host->collection().count(collectionCriteria) == 0
                    && host->collection().searchRows(collectionCriteria).isEmpty()
                    && !host->collection().getDisplay(1).has_value(),
                "Host Collection composition has no local fallback");
    PartReferenceManifest manifest;
    QString partReferenceError;
    const auto effective = host->partReferenceCustomizations().effectiveEntries(
        manifest, &partReferenceError);
    const auto add = host->partReferenceCustomizations().add(
        manifest, "3001", "Test", "Test", PartReferencePlacement::Append);
    ok &= check(effective.isEmpty() && !partReferenceError.isEmpty()
                    && !add.success && !add.message.isEmpty(),
                "Host Part Reference customization has no local fallback");
    WorkspaceContext hostWorkspaceContext;
    ok &= check(!hostWorkspaceContext.hasCurrentWorkspace()
                    && hostWorkspaceContext.currentWorkspaceId() == 0,
                "Host WorkspaceContext starts unset without a synthetic identity");
    return ok ? 0 : 1;
}
