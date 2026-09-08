#include "ApplicationServices.h"

#include "../parts/PartReferenceManifest.h"

namespace {
const QString kHostUnavailable = QStringLiteral("BrickSuite Host shared data is not connected.");
ApplicationServiceStatus unavailable()
{ return {ApplicationServiceAvailability::UnavailableNotConnected, kHostUnavailable}; }

class Workspaces final : public WorkspaceApplicationService {
public:
    ApplicationServiceStatus status() const override{return unavailable();}
    QList<Workspace> list() const override{return {};} std::optional<Workspace> get(int) const override{return {};}
    bool exists(int) const override{return false;} bool create(Workspace&) const override{return false;} bool update(Workspace&) const override{return false;}
};
class Inventory final : public InventoryApplicationService {
public:
    ApplicationServiceStatus status() const override{return unavailable();}
    int count(const InventorySearchCriteria&) const override{return 0;} QList<InventorySearchResult> searchRows(const InventorySearchCriteria&) const override{return {};}
    std::optional<InventoryRecord> get(int) const override{return {};} QList<InventoryHistoryResult> history(int,int,int) const override{return {};}
};
class Builds final : public BuildApplicationService {
public:
    ApplicationServiceStatus status() const override{return unavailable();}
    QList<Build> list(int,bool) const override{return {};} std::optional<Build> get(int) const override{return {};}
    QList<BuildRequirement> requirements(int) const override{return {};} QList<MissingPartsService::MissingPart> missingParts(int,int) const override{return {};}
    BuildPullingService::PullingView pullingView(int) const override{BuildPullingService::PullingView r;r.message=kHostUnavailable;return r;}
};
class Collection final : public CollectionApplicationService {
public:
    ApplicationServiceStatus status() const override{return unavailable();}
    int count(const CollectionSearchCriteria&) const override{return 0;} QList<CollectionSearchResult> searchRows(const CollectionSearchCriteria&) const override{return {};}
    std::optional<CollectionSearchResult> getDisplay(int) const override{return {};}
};
class PartReference final : public SharedPartReferenceCustomizationService {
public:
    ApplicationServiceStatus status() const override{return unavailable();}
    QList<PartReferenceEntry> effectiveEntries(const PartReferenceManifest& manifest,QString* error) const override
    {if(error)*error=kHostUnavailable+QStringLiteral(" Built-in Part Reference entries remain available; shared customizations are unavailable.");return manifest.entries();}
    PartReferenceCustomizationResult add(const PartReferenceManifest&,const QString&,const QString&,const QString&,PartReferencePlacement,const QString&) const override{return {false,kHostUnavailable,0};}
    PartReferenceCustomizationResult remove(const PartReferenceManifest&,int) const override{return {false,kHostUnavailable,0};}
};
}

std::unique_ptr<ApplicationServices> createUnavailableHostApplicationServices()
{
    return std::make_unique<ApplicationServices>(std::make_unique<Workspaces>(),
        std::make_unique<Inventory>(),std::make_unique<Builds>(),
        std::make_unique<Collection>(),std::make_unique<PartReference>(),
        SharedDataSource::BrickSuiteHost);
}
