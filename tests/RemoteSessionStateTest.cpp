#include "../src/network/RemoteSessionState.h"
#include "../src/app/WorkspaceContext.h"

#include <QCoreApplication>
#include <QDebug>

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

    state.disconnected();
    ok &= check(state.dataState() == RemoteSessionState::DataState::StaleDisconnected,
                "Disconnect after a successful load must mark Host state stale.");
    ok &= check(!state.accepts(workspaceSeven),
                "A result from the disconnected session must be rejected.");

    state.authenticated(hostA.toLower());
    ok &= check(restored == 1 && refreshes == 1 && clears == 0,
                "Same-Host reconnect must request refresh without clearing state.");
    ok &= check(state.workspaceId() == 7,
                "Same-Host reconnect must preserve Workspace identity.");
    ok &= check(state.sessionGeneration() > workspaceSeven.sessionGeneration,
                "Reconnect must advance session generation.");

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
