#include "../src/network/RemoteSessionState.h"
#include "../src/app/WorkspaceContext.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QTemporaryDir>

namespace {
bool check(bool condition, const char* message)
{
    if (!condition) qCritical().noquote() << message;
    return condition;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir settingsDirectory;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
    QSettings().clear();
    RemoteSessionState state;
    int restored = 0;
    int clears = 0;
    int refreshes = 0;
    QObject::connect(&state, &RemoteSessionState::sameHostSessionRestored,
                     [&restored]() { ++restored; });
    QObject::connect(&state, &RemoteSessionState::operationalStateMustClear,
                     [&clears]() { ++clears; });
    QObject::connect(&state, &RemoteSessionState::operationalStateShouldRefresh,
                     [&refreshes]() { ++refreshes; });

    bool ok = true;
    ok &= check(state.dataState() == RemoteSessionState::DataState::NeverLoaded,
                "A session with no successful connection must be NeverLoaded.");
    const QString hostA(64, QLatin1Char('A'));
    const QString hostB(64, QLatin1Char('B'));
    state.authenticated(hostA);
    const auto first = state.snapshot();
    ok &= check(first.hostIdentity == hostA,
                "The verified fingerprint must be the normalized Host identity.");
    ok &= check(state.isAuthenticated() && state.accepts(first),
                "The initial authenticated session snapshot must be accepted.");

    state.setWorkspaceId(7);
    const auto workspaceSeven = state.snapshot();
    ok &= check(workspaceSeven.workspaceGeneration > first.workspaceGeneration,
                "Selecting a Workspace must advance Workspace generation.");
    ok &= check(!state.accepts(first),
                "A result from the prior Workspace generation must be rejected.");
    ok &= check(state.acceptsHostGlobal(first),
                "A Host-global result from the current session must survive Workspace selection.");
    ok &= check(state.acceptsEvent(state.sessionGeneration(), qint64(7))
                && !state.acceptsEvent(state.sessionGeneration(), qint64(8)),
                "Only events for the current Workspace may be accepted.");

    state.disconnected();
    ok &= check(state.dataState() == RemoteSessionState::DataState::StaleDisconnected,
                "Disconnect after a successful load must mark Host state stale.");
    ok &= check(!state.accepts(workspaceSeven),
                "A result from the disconnected session must be rejected.");
    ok &= check(!state.acceptsHostGlobal(workspaceSeven),
                "A Host-global result from a disconnected session must be rejected.");
    ok &= check(!state.acceptsEvent(workspaceSeven.sessionGeneration, qint64(7)),
                "An event from a disconnected session must be rejected.");

    state.authenticated(hostA.toLower());
    ok &= check(restored == 1 && refreshes == 1 && clears == 0,
                "Same-Host reconnect must request refresh without clearing state.");
    ok &= check(state.workspaceId() == 7,
                "Same-Host reconnect must preserve Workspace identity.");
    ok &= check(state.sessionGeneration() > workspaceSeven.sessionGeneration,
                "Reconnect must advance session generation.");
    ok &= check(!state.acceptsEvent(workspaceSeven.sessionGeneration, qint64(7)),
                "An obsolete-session event must be rejected after reconnect.");
    ok &= check(!state.acceptsHostGlobal(workspaceSeven),
                "A Host-global result from an obsolete session must be rejected after reconnect.");

    state.authenticated(hostB);
    ok &= check(clears == 1 && state.hostIdentity() == hostB,
                "A different verified fingerprint must clear Host-owned state.");
    ok &= check(state.workspaceId() == 0,
                "A Host identity change must clear the selected Workspace.");

    state.setWorkspaceId(9);
    const auto workspaceNine = state.snapshot();
    state.setWorkspaceId(10);
    ok &= check(!state.accepts(workspaceNine),
                "An obsolete Workspace result must not be accepted.");

    RemoteSessionState epochState;
    int epochClears = 0;
    int epochChanges = 0;
    QObject::connect(&epochState, &RemoteSessionState::operationalStateMustClear,
                     [&] { ++epochClears; });
    QObject::connect(&epochState, &RemoteSessionState::dataEpochChanged,
                     [&] { ++epochChanges; });
    const QString epochA = QStringLiteral("11111111-1111-4111-8111-111111111111");
    const QString epochB = QStringLiteral("22222222-2222-4222-8222-222222222222");
    epochState.authenticatedWithEpoch(hostA, epochA, true);
    epochState.setWorkspaceId(21);
    epochState.disconnected();
    epochState.authenticatedWithEpoch(hostA, epochA, true);
    ok &= check(epochState.workspaceId() == 21 && epochClears == 1
                    && epochChanges == 0 && epochState.dataEpoch() == epochA,
                "First epoch-aware bootstrap clears legacy context and same-epoch reconnect preserves it.");
    epochState.disconnected();
    epochState.authenticatedWithEpoch(hostA, epochB, true);
    ok &= check(epochState.workspaceId() == 0 && epochClears == 2
                    && epochChanges == 1 && epochState.dataEpoch() == epochB,
                "Same Host with changed epoch clears all operational identity context.");

    RemoteSessionState legacyState;
    legacyState.authenticatedWithEpoch(hostA, QString(), false);
    ok &= check(!legacyState.dataEpochSupported() && legacyState.isAuthenticated(),
                "Protocol 1.2 Host without data epoch remains a supported legacy session.");

    WorkspaceContext context;
    const quint64 initialWorkspaceGeneration = context.generation();
    context.setCurrentWorkspaceId(12);
    ok &= check(context.generation() == initialWorkspaceGeneration + 1,
                "WorkspaceContext must advance generation when identity changes.");
    const quint64 selectedGeneration = context.generation();
    context.setCurrentWorkspaceId(12);
    ok &= check(context.generation() == selectedGeneration,
                "Reassigning the same Workspace must not manufacture a transition.");
    context.clearCurrentWorkspace();
    ok &= check(context.generation() == selectedGeneration + 1,
                "Clearing a selected Workspace must advance generation.");

    return ok ? 0 : 1;
}
