#include "HostRequestAdmissionController.h"

#include <QDebug>

namespace {
bool shouldLogRejection(quint64 count)
{
    // Log the first rejection and exponentially spaced repeats so a flooding
    // client cannot create unbounded warning noise.
    return count == 1 || (count & (count - 1)) == 0;
}
}

HostRequestAdmissionController::Lease::Lease(
    std::shared_ptr<State> state, QString owner, QString sessionId,
    QString requestId, WorkKind kind)
    : m_state(std::move(state)), m_owner(std::move(owner)),
      m_sessionId(std::move(sessionId)), m_requestId(std::move(requestId)), m_kind(kind) {}

HostRequestAdmissionController::Lease::~Lease()
{
    if (!m_state) return;
    auto ownerIt = m_state->owners.find(m_owner);
    if (ownerIt != m_state->owners.end()) {
        if (m_kind == WorkKind::Read) {
            --ownerIt->reads;
            --m_state->globalReads;
        } else {
            --ownerIt->writes;
            --m_state->globalWrites;
        }
        if (ownerIt->reads == 0 && ownerIt->writes == 0)
            m_state->owners.erase(ownerIt);
    }
    auto sessionIt = m_state->inFlightRequests.find(m_sessionId);
    if (sessionIt != m_state->inFlightRequests.end()) {
        sessionIt->remove(m_requestId);
        if (sessionIt->isEmpty()) m_state->inFlightRequests.erase(sessionIt);
    }
}

HostRequestAdmissionController::Result HostRequestAdmissionController::admit(
    const HostRequestContext& context, WorkKind kind)
{
    const QString owner = context.fairnessOwner();
    const auto sessionIt = m_state->inFlightRequests.constFind(context.sessionId);
    if (sessionIt != m_state->inFlightRequests.cend()
        && sessionIt->contains(context.requestId)) {
        return {Rejection::DuplicateRequestId, {}};
    }

    OwnerCounts& counts = m_state->owners[owner];
    const int ownerCount = kind == WorkKind::Read ? counts.reads : counts.writes;
    const int ownerLimit = kind == WorkKind::Read ? MaximumOwnerReads : MaximumOwnerWrites;
    if (ownerCount >= ownerLimit) {
        ++counts.rejections;
        ++m_state->ownerRejections;
        if (shouldLogRejection(counts.rejections))
            qWarning() << "Host operational request rejected by owner quota" << owner
                       << (kind == WorkKind::Read ? "read" : "write")
                       << "count" << counts.rejections;
        return {Rejection::OwnerLimit, {}};
    }

    const int globalCount = kind == WorkKind::Read ? m_state->globalReads : m_state->globalWrites;
    const int globalLimit = kind == WorkKind::Read ? MaximumGlobalReads : MaximumGlobalWrites;
    if (globalCount >= globalLimit) {
        ++m_state->globalRejections;
        if (counts.reads == 0 && counts.writes == 0) m_state->owners.remove(owner);
        if (shouldLogRejection(m_state->globalRejections))
            qWarning() << "Host operational request rejected at global capacity"
                       << (kind == WorkKind::Read ? "read" : "write")
                       << "count" << m_state->globalRejections;
        return {Rejection::GlobalLimit, {}};
    }

    if (kind == WorkKind::Read) {
        ++counts.reads;
        ++m_state->globalReads;
        if (m_state->globalReads > m_state->readHighWater) {
            m_state->readHighWater = m_state->globalReads;
            qDebug() << "Host admitted read high-water" << m_state->readHighWater;
        }
    } else {
        ++counts.writes;
        ++m_state->globalWrites;
        if (m_state->globalWrites > m_state->writeHighWater) {
            m_state->writeHighWater = m_state->globalWrites;
            qDebug() << "Host admitted write high-water" << m_state->writeHighWater;
        }
    }
    m_state->inFlightRequests[context.sessionId].insert(context.requestId);
    return {Rejection::None, std::shared_ptr<Lease>(
        new Lease(m_state, owner, context.sessionId, context.requestId, kind))};
}

HostRequestAdmissionController::Snapshot HostRequestAdmissionController::snapshot() const
{
    return {m_state->globalReads, m_state->globalWrites,
            m_state->readHighWater, m_state->writeHighWater,
            m_state->ownerRejections, m_state->globalRejections,
            int(m_state->owners.size()), int(m_state->inFlightRequests.size())};
}

int HostRequestAdmissionController::ownerOutstanding(
    const QString& owner, WorkKind kind) const
{
    const auto it = m_state->owners.constFind(owner);
    if (it == m_state->owners.cend()) return 0;
    return kind == WorkKind::Read ? it->reads : it->writes;
}

bool HostRequestAdmissionController::isRequestInFlight(
    const QString& sessionId, const QString& requestId) const
{
    const auto it = m_state->inFlightRequests.constFind(sessionId);
    return it != m_state->inFlightRequests.cend() && it->contains(requestId);
}
