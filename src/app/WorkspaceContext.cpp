#include "WorkspaceContext.h"

WorkspaceContext::WorkspaceContext(QObject* parent)
    : QObject(parent)
{}

int WorkspaceContext::currentWorkspaceId() const
{
    return m_currentWorkspaceId;
}

bool WorkspaceContext::hasCurrentWorkspace() const
{
    return m_currentWorkspaceId > 0;
}

void WorkspaceContext::setCurrentWorkspaceId(int workspaceId)
{
    if (m_currentWorkspaceId == workspaceId)
        return;

    m_currentWorkspaceId = workspaceId;

    emit currentWorkspaceChanged(m_currentWorkspaceId);
}

void WorkspaceContext::clearCurrentWorkspace()
{
    setCurrentWorkspaceId(0);
}