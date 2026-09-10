#pragma once

struct RemoteStorageActionEligibilityInput {
    bool connected=false; bool workspaceCurrent=false; bool stale=true; bool pending=false;
    bool selected=false; bool selectedActive=false;
    bool canGet=false; bool canListTypes=false; bool canAdd=false; bool canEdit=false; bool canSetActive=false;
};
struct RemoteStorageActionEligibilityResult { bool add=false; bool edit=false; bool deactivate=false; bool reactivate=false; };
inline RemoteStorageActionEligibilityResult remoteStorageActionEligibility(const RemoteStorageActionEligibilityInput&i)
{
    const bool ready=i.connected&&i.workspaceCurrent&&!i.stale&&!i.pending;
    return {ready&&i.canAdd&&i.canListTypes,
            ready&&i.selected&&i.canEdit&&i.canGet&&i.canListTypes,
            ready&&i.selected&&i.selectedActive&&i.canSetActive&&i.canGet,
            ready&&i.selected&&!i.selectedActive&&i.canSetActive&&i.canGet};
}
