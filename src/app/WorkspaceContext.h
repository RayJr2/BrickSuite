#pragma once

#include <QObject>

class WorkspaceContext : public QObject
{
    Q_OBJECT

public:
    explicit WorkspaceContext(QObject* parent = nullptr);

    int currentWorkspaceId() const;
    bool hasCurrentWorkspace() const;

    void setCurrentWorkspaceId(int workspaceId);
    void clearCurrentWorkspace();

signals:
    void currentWorkspaceChanged(int workspaceId);

private:
    int m_currentWorkspaceId = 0;
};