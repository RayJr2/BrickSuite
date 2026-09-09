#include "ApplicationServices.h"

#include "../../repositories/BuildRepository.h"
#include "../../repositories/BuildRequirementRepository.h"
#include "../../repositories/CollectionRepository.h"
#include "../../repositories/InventoryMovementRepository.h"
#include "../../repositories/InventoryRecordRepository.h"
#include "../../repositories/PartRepository.h"
#include "../../repositories/WorkspaceRepository.h"
#include "../parts/PartReferenceManifest.h"

namespace {
class LocalWorkspaceService final : public WorkspaceApplicationService
{
public:
    QList<Workspace> list() const override { return WorkspaceRepository().getAll(); }
    std::optional<Workspace> get(int id) const override { return WorkspaceRepository().getById(id); }
    bool exists(int id) const override { return get(id).has_value(); }
    bool create(Workspace& item) const override { return WorkspaceRepository().create(item); }
    bool update(Workspace& item) const override { return WorkspaceRepository().update(item); }
};

class LocalInventoryService final : public InventoryApplicationService
{
public:
    int count(const InventorySearchCriteria& criteria) const override
    { return InventoryRecordRepository().count(criteria); }
    QList<InventorySearchResult> searchRows(const InventorySearchCriteria& criteria) const override
    { return InventoryRecordRepository().search(criteria); }
    std::optional<InventoryRecord> get(int id) const override
    { return InventoryRecordRepository().getById(id); }
    QList<InventoryHistoryResult> history(int workspaceId, int partId, int colorId) const override
    { return InventoryMovementRepository().getHistoryForPartColor(workspaceId, partId, colorId); }
};

class LocalBuildService final : public BuildApplicationService
{
public:
    QList<Build> list(int workspaceId, bool archived) const override
    { return BuildRepository().getByWorkspace(workspaceId, archived); }
    std::optional<Build> get(int id) const override { return BuildRepository().getById(id); }
    QList<BuildRequirement> requirements(int id) const override
    { return BuildRequirementRepository().getByBuild(id); }
    QList<MissingPartsService::MissingPart> missingParts(int workspaceId, int buildId) const override
    { return MissingPartsService().getMissingParts(workspaceId, buildId); }
    BuildPullingService::PullingView pullingView(int buildId) const override
    { return BuildPullingService().getPullingView(buildId); }
};

class LocalCollectionService final : public CollectionApplicationService
{
public:
    int count(const CollectionSearchCriteria& criteria) const override
    { return CollectionRepository().count(criteria); }
    QList<CollectionSearchResult> searchRows(const CollectionSearchCriteria& criteria) const override
    { return CollectionRepository().search(criteria); }
    std::optional<CollectionSearchResult> getDisplay(int id) const override
    { return CollectionRepository().displayById(id); }
};

class LocalPartReferenceService final : public SharedPartReferenceCustomizationService
{
public:
    QList<PartReferenceEntry> effectiveEntries(const PartReferenceManifest& manifest,
                                                QString* error) const override
    { return PartReferenceCustomizationService(manifest).effectiveEntries(error); }
    PartReferenceCustomizationResult add(const PartReferenceManifest& manifest,
        const QString& partNumber, const QString& catalog, const QString& section,
        PartReferencePlacement placement, const QString& anchor) const override
    {
        const auto part = PartRepository().getByPartNumber(partNumber.trimmed());
        if (!part) return {false, QStringLiteral("The selected Part is not available."), 0};
        return PartReferenceCustomizationService(manifest).add(
            part->id(), catalog, section, placement, anchor);
    }
    PartReferenceCustomizationResult remove(const PartReferenceManifest& manifest,
                                              int id) const override
    { return PartReferenceCustomizationService(manifest).remove(id); }
};

class BoundWorkspaceService final : public WorkspaceApplicationService
{
public:
    explicit BoundWorkspaceService(const QSqlDatabase& database) : m_database(database) {}
    QList<Workspace> list() const override { return WorkspaceRepository(m_database).getAll(); }
    std::optional<Workspace> get(int id) const override
    { return WorkspaceRepository(m_database).getById(id); }
    bool exists(int id) const override { return get(id).has_value(); }
    bool create(Workspace&) const override { return false; }
    bool update(Workspace&) const override { return false; }
private:
    QSqlDatabase m_database;
};

class BoundInventoryService final : public InventoryApplicationService
{
public:
    explicit BoundInventoryService(const QSqlDatabase& database) : m_database(database) {}
    int count(const InventorySearchCriteria& criteria) const override
    { return InventoryRecordRepository(m_database).count(criteria); }
    QList<InventorySearchResult> searchRows(const InventorySearchCriteria& criteria) const override
    { return InventoryRecordRepository(m_database).search(criteria); }
    std::optional<InventoryRecord> get(int id) const override
    { return InventoryRecordRepository(m_database).getById(id); }
    QList<InventoryHistoryResult> history(int workspaceId, int partId, int colorId) const override
    { return InventoryMovementRepository(m_database).getHistoryForPartColor(
          workspaceId, partId, colorId); }
private:
    QSqlDatabase m_database;
};

class BoundBuildService final : public BuildApplicationService
{
public:
    explicit BoundBuildService(const QSqlDatabase& database) : m_database(database) {}
    QList<Build> list(int workspaceId, bool archived) const override
    { return BuildRepository(m_database).getByWorkspace(workspaceId, archived); }
    std::optional<Build> get(int id) const override
    { return BuildRepository(m_database).getById(id); }
    QList<BuildRequirement> requirements(int id) const override
    { return BuildRequirementRepository(m_database).getByBuild(id); }
    QList<MissingPartsService::MissingPart> missingParts(int workspaceId, int buildId) const override
    { return MissingPartsService(m_database).getMissingParts(workspaceId, buildId); }
    BuildPullingService::PullingView pullingView(int buildId) const override
    { return BuildPullingService(m_database).getPullingView(buildId); }
private:
    QSqlDatabase m_database;
};

class BoundCollectionService final : public CollectionApplicationService
{
public:
    explicit BoundCollectionService(const QSqlDatabase& database) : m_database(database) {}
    int count(const CollectionSearchCriteria& criteria) const override
    { return CollectionRepository(m_database).count(criteria); }
    QList<CollectionSearchResult> searchRows(const CollectionSearchCriteria& criteria) const override
    { return CollectionRepository(m_database).search(criteria); }
    std::optional<CollectionSearchResult> getDisplay(int id) const override
    { return CollectionRepository(m_database).displayById(id); }
private:
    QSqlDatabase m_database;
};

class BoundPartReferenceService final : public SharedPartReferenceCustomizationService
{
public:
    explicit BoundPartReferenceService(const QSqlDatabase& database) : m_database(database) {}
    QList<PartReferenceEntry> effectiveEntries(const PartReferenceManifest& manifest,
                                                QString* error) const override
    { return PartReferenceCustomizationService(manifest, m_database).effectiveEntries(error); }
    PartReferenceCustomizationResult add(const PartReferenceManifest&, const QString&,
        const QString&, const QString&, PartReferencePlacement, const QString&) const override
    { return {false, QStringLiteral("Host read services do not support mutations."), 0}; }
    PartReferenceCustomizationResult remove(const PartReferenceManifest&, int) const override
    { return {false, QStringLiteral("Host read services do not support mutations."), 0}; }
private:
    QSqlDatabase m_database;
};

}

ApplicationServices::ApplicationServices()
    : ApplicationServices(std::make_unique<LocalWorkspaceService>(),
                          std::make_unique<LocalInventoryService>(),
                          std::make_unique<LocalBuildService>(),
                          std::make_unique<LocalCollectionService>(),
                          std::make_unique<LocalPartReferenceService>()) {}

std::unique_ptr<ApplicationServices> createConnectionBoundReadApplicationServices(
    const QSqlDatabase& database)
{
    return std::make_unique<ApplicationServices>(
        std::make_unique<BoundWorkspaceService>(database),
        std::make_unique<BoundInventoryService>(database),
        std::make_unique<BoundBuildService>(database),
        std::make_unique<BoundCollectionService>(database),
        std::make_unique<BoundPartReferenceService>(database),
        SharedDataSource::ThisComputer);
}
