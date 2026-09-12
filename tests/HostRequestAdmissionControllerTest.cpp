#include "../src/network/HostRequestAdmissionController.h"

#include <QCoreApplication>
#include <cstdio>
#include <vector>

namespace {
bool require(bool condition, const char* message)
{
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}

HostRequestContext paired(const QString& session, const QString& request,
                          const QString& device)
{
    return {session, request, 3, HostRequestContext::AuthenticationKind::PairedDevice, device};
}

HostRequestContext legacy(const QString& session, const QString& request)
{
    return {session, request, 2, HostRequestContext::AuthenticationKind::LegacySharedToken, {}};
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    using Controller = HostRequestAdmissionController;
    using Kind = Controller::WorkKind;
    using Rejection = Controller::Rejection;
    bool ok = true;

    const auto deviceA1 = paired(QStringLiteral("session-a1"), QStringLiteral("r1"),
                                 QStringLiteral("DEVICE-A"));
    const auto deviceA2 = paired(QStringLiteral("session-a2"), QStringLiteral("r2"),
                                 QStringLiteral("device-a"));
    const auto deviceB = paired(QStringLiteral("session-b"), QStringLiteral("r1"),
                                QStringLiteral("device-b"));
    ok &= require(deviceA1.fairnessOwner() == deviceA2.fairnessOwner(),
                  "sessions for the same trusted paired device share an owner");
    ok &= require(deviceA1.fairnessOwner() != deviceB.fairnessOwner(),
                  "different paired devices have different owners");
    ok &= require(legacy(QStringLiteral("legacy-a"), QStringLiteral("r1")).fairnessOwner()
                    != legacy(QStringLiteral("legacy-b"), QStringLiteral("r1")).fairnessOwner(),
                  "legacy clients use session ownership without a fabricated device identity");

    Controller reads;
    std::vector<std::shared_ptr<Controller::Lease>> heldReads;
    for (int i = 0; i < Controller::MaximumOwnerReads; ++i) {
        auto result = reads.admit(paired(i % 2 ? QStringLiteral("session-a2")
                                              : QStringLiteral("session-a1"),
                                           QStringLiteral("read-%1").arg(i),
                                           QStringLiteral("device-a")), Kind::Read);
        ok &= require(result.accepted(), "device read within shared allowance accepted");
        heldReads.push_back(std::move(result.lease));
    }
    auto excessRead = reads.admit(paired(QStringLiteral("session-a3"), QStringLiteral("excess"),
                                         QStringLiteral("device-a")), Kind::Read);
    ok &= require(excessRead.rejection == Rejection::OwnerLimit,
                  "extra read rejected before global capacity");
    auto otherRead = reads.admit(deviceB, Kind::Read);
    ok &= require(otherRead.accepted(), "another device can read while device A is at quota");

    auto duplicate = reads.admit(paired(QStringLiteral("session-b"), QStringLiteral("r1"),
                                        QStringLiteral("device-b")), Kind::Read);
    ok &= require(duplicate.rejection == Rejection::DuplicateRequestId,
                  "same session duplicate in-flight request ID rejected");
    auto sameIdOtherSession = reads.admit(paired(QStringLiteral("session-b2"), QStringLiteral("r1"),
                                                 QStringLiteral("device-b")), Kind::Read);
    ok &= require(sameIdOtherSession.accepted(), "same request ID in another session is allowed");
    otherRead.lease.reset();
    auto reused = reads.admit(deviceB, Kind::Read);
    ok &= require(reused.accepted(), "request ID is reusable after completion");
    heldReads.front().reset();
    auto replacement = reads.admit(paired(QStringLiteral("session-a1"), QStringLiteral("replacement"),
                                          QStringLiteral("device-a")), Kind::Read);
    ok &= require(replacement.accepted(), "owner capacity returns after completion");

    Controller writes;
    std::vector<std::shared_ptr<Controller::Lease>> heldWrites;
    for (int i = 0; i < Controller::MaximumOwnerWrites; ++i) {
        auto result = writes.admit(paired(QStringLiteral("write-a-%1").arg(i % 2),
                                          QStringLiteral("write-%1").arg(i),
                                          QStringLiteral("device-a")), Kind::Write);
        ok &= require(result.accepted(), "device write within shared allowance accepted");
        heldWrites.push_back(std::move(result.lease));
    }
    ok &= require(writes.admit(paired(QStringLiteral("write-a-3"), QStringLiteral("excess"),
                                      QStringLiteral("device-a")), Kind::Write).rejection
                    == Rejection::OwnerLimit,
                  "extra mutation rejected by owner quota");
    auto deviceBWrite = writes.admit(paired(QStringLiteral("write-b"), QStringLiteral("write-1"),
                                            QStringLiteral("device-b")), Kind::Write);
    ok &= require(deviceBWrite.accepted(), "another device can mutate while A is at quota");

    Controller global;
    std::vector<std::shared_ptr<Controller::Lease>> globalReads;
    for (int owner = 0; owner < 8; ++owner)
        for (int i = 0; i < Controller::MaximumOwnerReads; ++i) {
            auto result = global.admit(paired(QStringLiteral("s-%1").arg(owner),
                                              QStringLiteral("r-%1").arg(i),
                                              QStringLiteral("d-%1").arg(owner)), Kind::Read);
            ok &= require(result.accepted(), "global read capacity accepts exactly 64");
            globalReads.push_back(std::move(result.lease));
        }
    ok &= require(global.admit(paired(QStringLiteral("overflow"), QStringLiteral("read"),
                                      QStringLiteral("overflow")), Kind::Read).rejection
                    == Rejection::GlobalLimit,
                  "global read bound remains 64");

    std::vector<std::shared_ptr<Controller::Lease>> globalWrites;
    Controller globalWriteController;
    for (int owner = 0; owner < 4; ++owner)
        for (int i = 0; i < Controller::MaximumOwnerWrites; ++i) {
            auto result = globalWriteController.admit(
                paired(QStringLiteral("ws-%1").arg(owner), QStringLiteral("w-%1").arg(i),
                       QStringLiteral("wd-%1").arg(owner)), Kind::Write);
            ok &= require(result.accepted(), "global write capacity accepts exactly 16");
            globalWrites.push_back(std::move(result.lease));
        }
    ok &= require(globalWriteController.admit(
                      paired(QStringLiteral("wo"), QStringLiteral("w"), QStringLiteral("wo")),
                      Kind::Write).rejection == Rejection::GlobalLimit,
                  "global write bound remains 16");

    Controller disconnected;
    auto survivingRead = disconnected.admit(deviceA1, Kind::Read);
    ok &= require(survivingRead.accepted() && disconnected.snapshot().globalReads == 1,
                  "admitted work remains accounted independently of a socket");
    survivingRead.lease.reset();
    ok &= require(disconnected.snapshot().globalReads == 0
                      && disconnected.snapshot().trackedOwners == 0
                      && disconnected.snapshot().trackedSessions == 0,
                  "natural completion releases all disconnected-session accounting");

    const auto readSnapshot = reads.snapshot();
    ok &= require(readSnapshot.ownerRejections == 1 && readSnapshot.readHighWater >= 9,
                  "owner rejection and high-water diagnostics are retained");
    return ok ? 0 : 1;
}
