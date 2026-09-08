#pragma once

// M26 ownership boundary: catalog/reference services remain local to each
// device. These contracts cover shared, Host-authoritative application data.
// The composition root supplies local implementations today and may supply
// remote implementations later; UI code must not choose a data source.

#include "../../models/Build.h"
#include "../../models/BuildRequirement.h"
#include "../../models/CollectionSearchCriteria.h"
#include "../../models/CollectionSearchResult.h"
#include "../../models/InventoryHistoryResult.h"
#include "../../models/InventoryRecord.h"
#include "../../models/InventorySearchCriteria.h"
#include "../../models/InventorySearchResult.h"
#include "../../models/PartReferenceEntry.h"
#include "../../models/Workspace.h"
#include "../builds/BuildPullingService.h"
#include "../builds/MissingPartsService.h"
#include "../parts/PartReferenceCustomizationService.h"

#include <QList>
#include <memory>
#include <optional>

class PartReferenceManifest;

class WorkspaceApplicationService
{
public:
    virtual ~WorkspaceApplicationService() = default;
    virtual QList<Workspace> list() const = 0;
    virtual std::optional<Workspace> get(int workspaceId) const = 0;
    virtual bool exists(int workspaceId) const = 0;
    virtual bool create(Workspace& workspace) const = 0;
    virtual bool update(Workspace& workspace) const = 0;
};

class InventoryApplicationService
{
public:
    struct Page { QList<InventorySearchResult> rows; int total = 0; };
    virtual ~InventoryApplicationService() = default;
    virtual int count(const InventorySearchCriteria& criteria) const = 0;
    virtual QList<InventorySearchResult> searchRows(
        const InventorySearchCriteria& criteria) const = 0;
    Page search(const InventorySearchCriteria& criteria) const
    { return {searchRows(criteria), count(criteria)}; }
    virtual std::optional<InventoryRecord> get(int inventoryRecordId) const = 0;
    virtual QList<InventoryHistoryResult> history(int workspaceId, int partId,
                                                   int colorId) const = 0;
};

class BuildApplicationService
{
public:
    virtual ~BuildApplicationService() = default;
    virtual QList<Build> list(int workspaceId, bool includeArchived) const = 0;
    virtual std::optional<Build> get(int buildId) const = 0;
    virtual QList<BuildRequirement> requirements(int buildId) const = 0;
    virtual QList<MissingPartsService::MissingPart> missingParts(int workspaceId,
                                                                 int buildId) const = 0;
    virtual BuildPullingService::PullingView pullingView(int buildId) const = 0;
};

class CollectionApplicationService
{
public:
    struct Page { QList<CollectionSearchResult> rows; int total = 0; };
    virtual ~CollectionApplicationService() = default;
    virtual int count(const CollectionSearchCriteria& criteria) const = 0;
    virtual QList<CollectionSearchResult> searchRows(
        const CollectionSearchCriteria& criteria) const = 0;
    Page search(const CollectionSearchCriteria& criteria) const
    { return {searchRows(criteria), count(criteria)}; }
    virtual std::optional<CollectionSearchResult> getDisplay(int itemId) const = 0;
};

class SharedPartReferenceCustomizationService
{
public:
    virtual ~SharedPartReferenceCustomizationService() = default;
    virtual QList<PartReferenceEntry> effectiveEntries(
        const PartReferenceManifest& manifest, QString* errorMessage) const = 0;
    virtual PartReferenceCustomizationResult add(
        const PartReferenceManifest& manifest, const QString& canonicalPartNumber,
        const QString& catalog, const QString& section, PartReferencePlacement placement,
        const QString& anchorPartNumber = QString()) const = 0;
    virtual PartReferenceCustomizationResult remove(
        const PartReferenceManifest& manifest, int userEntryId) const = 0;
};

class ApplicationServices
{
public:
    ApplicationServices();
    ApplicationServices(std::unique_ptr<WorkspaceApplicationService> workspaces,
                        std::unique_ptr<InventoryApplicationService> inventory,
                        std::unique_ptr<BuildApplicationService> builds,
                        std::unique_ptr<CollectionApplicationService> collection,
                        std::unique_ptr<SharedPartReferenceCustomizationService> partReference);

    WorkspaceApplicationService& workspaces() const;
    InventoryApplicationService& inventory() const;
    BuildApplicationService& builds() const;
    CollectionApplicationService& collection() const;
    SharedPartReferenceCustomizationService& partReferenceCustomizations() const;

private:
    std::unique_ptr<WorkspaceApplicationService> m_workspaces;
    std::unique_ptr<InventoryApplicationService> m_inventory;
    std::unique_ptr<BuildApplicationService> m_builds;
    std::unique_ptr<CollectionApplicationService> m_collection;
    std::unique_ptr<SharedPartReferenceCustomizationService> m_partReference;
};
