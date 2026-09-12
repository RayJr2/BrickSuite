#include "RemoteRefreshCoordinator.h"

#include <QDebug>

namespace {
// Long enough to fold a normal mutation burst into one plan, while remaining
// short enough for a visible Host-backed view to feel immediate.
constexpr int CoalescingIntervalMs = 150;

template<typename T>
void mergeOptional(std::optional<T>* current, const std::optional<T>& incoming)
{
    if (!*current || !incoming || **current != *incoming) current->reset();
}
}

RemoteRefreshCoordinator::RemoteRefreshCoordinator(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<Projection>();
    m_coalescingTimer.setSingleShot(true);
    m_coalescingTimer.setInterval(CoalescingIntervalMs);
    connect(&m_coalescingTimer, &QTimer::timeout,
            this, &RemoteRefreshCoordinator::processPending);
}

void RemoteRefreshCoordinator::registerProjection(
    Projection projection, Visibility visible, Refresher refresher)
{
    Registration registration;
    registration.visible = std::move(visible);
    registration.refresher = std::move(refresher);
    m_registrations.insert(projection, std::move(registration));
}

void RemoteRefreshCoordinator::receiveInvalidation(
    const OperationalInvalidation& invalidation)
{
    if (!m_connected) return;
    QSet<Projection> plan;
    for (const auto domain : invalidation.domains)
        plan.unite(projectionsFor(domain));
    for (const Projection projection : std::as_const(plan)) {
        auto it = m_registrations.find(projection);
        if (it == m_registrations.end()) continue;
        if (it->cause.domains.isEmpty()) it->cause = invalidation;
        else mergeCause(&it->cause, invalidation);
        it->dirty = true;
        if (it->inFlight) {
            it->rerun = true;
            qDebug() << "Remote refresh rerun scheduled for projection" << int(projection);
        } else {
            m_pending.insert(projection);
        }
    }
    if (m_operationalAvailable && !m_pending.isEmpty()) m_coalescingTimer.start();
}

void RemoteRefreshCoordinator::surfaceBecameRelevant(Projection projection)
{
    auto it = m_registrations.find(projection);
    if (it == m_registrations.end() || !it->dirty || it->inFlight || !m_connected
        || !m_operationalAvailable) return;
    m_pending.remove(projection);
    start(projection);
}

void RemoteRefreshCoordinator::resetContext(bool connected, quint64 sessionGeneration,
                                            quint64 workspaceGeneration)
{
    m_connected = connected;
    m_sessionGeneration = sessionGeneration;
    m_workspaceGeneration = workspaceGeneration;
    ++m_contextSerial;
    m_pending.clear();
    m_coalescingTimer.stop();
    for (auto it = m_registrations.begin(); it != m_registrations.end(); ++it) {
        it->dirty = false;
        it->inFlight = false;
        it->rerun = false;
        it->cause = {};
    }
    qDebug() << "Remote refresh context reset; connected" << connected
             << "session" << sessionGeneration << "Workspace generation"
             << workspaceGeneration;
}

void RemoteRefreshCoordinator::setConnected(bool connected)
{
    if (m_connected == connected) return;
    m_connected = connected;
    ++m_contextSerial;
    m_pending.clear();
    m_coalescingTimer.stop();
    for (auto it = m_registrations.begin(); it != m_registrations.end(); ++it) {
        it->inFlight = false;
        it->rerun = false;
    }
}

void RemoteRefreshCoordinator::setOperationalAvailable(bool available)
{
    if (m_operationalAvailable == available) return;
    m_operationalAvailable = available;
    if (!available) {
        m_coalescingTimer.stop();
        return;
    }
    if (!m_pending.isEmpty()) m_coalescingTimer.start();
}

bool RemoteRefreshCoordinator::isDirty(Projection projection) const
{
    const auto it = m_registrations.constFind(projection);
    return it != m_registrations.cend() && it->dirty;
}

bool RemoteRefreshCoordinator::isInFlight(Projection projection) const
{
    const auto it = m_registrations.constFind(projection);
    return it != m_registrations.cend() && it->inFlight;
}

QSet<RemoteRefreshCoordinator::Projection> RemoteRefreshCoordinator::projectionsFor(
    OperationalInvalidationDomain domain)
{
    using P = Projection;
    switch (domain) {
    case OperationalInvalidationDomain::Workspaces: return {P::Workspaces};
    case OperationalInvalidationDomain::Storage:
        return {P::Storage, P::Inventory, P::InventoryLocations,
                P::Collection, P::CollectionLocations, P::Pulling,
                P::InventoryHistory};
    case OperationalInvalidationDomain::Inventory:
        return {P::Inventory, P::Builds, P::MissingParts, P::Pulling,
                P::InventoryHistory};
    case OperationalInvalidationDomain::InventoryHistory: return {P::InventoryHistory};
    case OperationalInvalidationDomain::Builds:
        return {P::Builds, P::MissingParts, P::Pulling};
    case OperationalInvalidationDomain::BuildRequirements:
        return {P::BuildRequirements, P::MissingParts, P::Pulling};
    case OperationalInvalidationDomain::MissingParts: return {P::MissingParts};
    case OperationalInvalidationDomain::Pulling:
        return {P::Pulling, P::Builds, P::MissingParts,
                P::Inventory, P::InventoryHistory};
    case OperationalInvalidationDomain::Collection:
        // Collection membership controls Add/View Collection actions in Builds.
        return {P::Collection, P::Builds};
    case OperationalInvalidationDomain::PartReferenceCustomizations:
        return {P::PartReferenceCustomizations};
    }
    return {};
}

void RemoteRefreshCoordinator::mergeCause(OperationalInvalidation* target,
                                          const OperationalInvalidation& source)
{
    for (const auto domain : source.domains)
        if (!target->domains.contains(domain)) target->domains.append(domain);
    target->sequence = qMax(target->sequence, source.sequence);
    mergeOptional(&target->workspaceId, source.workspaceId);
    mergeOptional(&target->buildId, source.buildId);
    mergeOptional(&target->inventoryRecordId, source.inventoryRecordId);
    mergeOptional(&target->storageLocationId, source.storageLocationId);
    mergeOptional(&target->collectionItemId, source.collectionItemId);
    if (target->partNumber != source.partNumber) target->partNumber.clear();
}

void RemoteRefreshCoordinator::processPending()
{
    const auto pending = m_pending;
    m_pending.clear();
    qDebug() << "Coalesced remote refresh plan contains" << pending.size()
             << "projection(s).";
    for (const Projection projection : pending) {
        auto it = m_registrations.find(projection);
        if (it == m_registrations.end() || !it->dirty || it->inFlight) continue;
        if (it->visible && it->visible()) start(projection);
        else qDebug() << "Remote projection marked dirty" << int(projection);
    }
}

void RemoteRefreshCoordinator::start(Projection projection)
{
    auto it = m_registrations.find(projection);
    if (it == m_registrations.end() || !m_connected || !m_operationalAvailable
        || it->inFlight || !it->refresher) return;
    it->dirty = false;
    it->inFlight = true;
    it->rerun = false;
    it->runContext = m_contextSerial;
    const quint64 context = m_contextSerial;
    const OperationalInvalidation cause = it->cause;
    it->cause = {};
    emit refreshStarted(projection);
    qDebug() << "Remote projection refresh started" << int(projection);
    it->refresher(cause, [this, projection, context](bool succeeded) {
        auto current = m_registrations.find(projection);
        if (current == m_registrations.end() || context != m_contextSerial) return;
        current->inFlight = false;
        const bool rerun = current->rerun;
        if (!succeeded) current->dirty = true;
        if (rerun) {
            current->rerun = false;
            current->dirty = true;
        }
        if (rerun && current->dirty && m_connected
            && current->visible && current->visible())
            start(projection);
    });
}
