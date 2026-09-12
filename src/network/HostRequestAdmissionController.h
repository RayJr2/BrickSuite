#pragma once

#include "HostRequestContext.h"

#include <QHash>
#include <QSet>
#include <memory>

class HostRequestAdmissionController
{
public:
    enum class WorkKind { Read, Write };
    enum class Rejection { None, DuplicateRequestId, OwnerLimit, GlobalLimit };

    static constexpr int MaximumOwnerReads = 8;
    static constexpr int MaximumOwnerWrites = 4;
    static constexpr int MaximumGlobalReads = 64;
    static constexpr int MaximumGlobalWrites = 16;

    struct Snapshot {
        int globalReads = 0;
        int globalWrites = 0;
        int readHighWater = 0;
        int writeHighWater = 0;
        quint64 ownerRejections = 0;
        quint64 globalRejections = 0;
        int trackedOwners = 0;
        int trackedSessions = 0;
    };

    class Lease;
    struct Result {
        Rejection rejection = Rejection::None;
        std::shared_ptr<Lease> lease;
        bool accepted() const { return rejection == Rejection::None && lease; }
    };

    Result admit(const HostRequestContext& context, WorkKind kind);
    Snapshot snapshot() const;
    int ownerOutstanding(const QString& owner, WorkKind kind) const;
    bool isRequestInFlight(const QString& sessionId, const QString& requestId) const;

private:
    struct OwnerCounts { int reads = 0; int writes = 0; quint64 rejections = 0; };
    struct State {
        QHash<QString, OwnerCounts> owners;
        QHash<QString, QSet<QString>> inFlightRequests;
        int globalReads = 0;
        int globalWrites = 0;
        int readHighWater = 0;
        int writeHighWater = 0;
        quint64 ownerRejections = 0;
        quint64 globalRejections = 0;
    };
    std::shared_ptr<State> m_state = std::make_shared<State>();
};

class HostRequestAdmissionController::Lease
{
public:
    ~Lease();
    Lease(const Lease&) = delete;
    Lease& operator=(const Lease&) = delete;

private:
    friend class HostRequestAdmissionController;
    Lease(std::shared_ptr<State> state, QString owner, QString sessionId,
          QString requestId, WorkKind kind);
    std::shared_ptr<State> m_state;
    QString m_owner;
    QString m_sessionId;
    QString m_requestId;
    WorkKind m_kind;
};
