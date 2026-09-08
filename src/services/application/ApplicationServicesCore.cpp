#include "ApplicationServices.h"

ApplicationServices::ApplicationServices(std::unique_ptr<WorkspaceApplicationService> w,
    std::unique_ptr<InventoryApplicationService> i, std::unique_ptr<BuildApplicationService> b,
    std::unique_ptr<CollectionApplicationService> c,
    std::unique_ptr<SharedPartReferenceCustomizationService> p, SharedDataSource source)
    : m_workspaces(std::move(w)), m_inventory(std::move(i)), m_builds(std::move(b)),
      m_collection(std::move(c)), m_partReference(std::move(p)), m_source(source) {}

WorkspaceApplicationService& ApplicationServices::workspaces() const { return *m_workspaces; }
InventoryApplicationService& ApplicationServices::inventory() const { return *m_inventory; }
BuildApplicationService& ApplicationServices::builds() const { return *m_builds; }
CollectionApplicationService& ApplicationServices::collection() const { return *m_collection; }
SharedPartReferenceCustomizationService& ApplicationServices::partReferenceCustomizations() const
{ return *m_partReference; }
SharedDataSource ApplicationServices::sharedDataSource() const { return m_source; }
ApplicationServiceStatus ApplicationServices::sharedStatus() const { return m_workspaces->status(); }
