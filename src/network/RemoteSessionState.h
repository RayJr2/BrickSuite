#pragma once

#include <QObject>
#include <QString>
#include <optional>

class RemoteSessionState : public QObject
{
    Q_OBJECT
public:
    enum class DataState { NeverLoaded, Current, StaleDisconnected };
    Q_ENUM(DataState)

    struct Snapshot {
        QString hostIdentity;
        quint64 sessionGeneration = 0;
        quint64 workspaceGeneration = 0;
        int workspaceId = 0;
    };

    explicit RemoteSessionState(QObject* parent = nullptr);

    QString hostIdentity() const;
    quint64 sessionGeneration() const;
    quint64 workspaceGeneration() const;
    int workspaceId() const;
    bool isAuthenticated() const;
    DataState dataState() const;
    Snapshot snapshot() const;
    bool accepts(const Snapshot& snapshot) const;
    bool acceptsEvent(quint64 sessionGeneration,
                      const std::optional<qint64>& workspaceId) const;

public slots:
    void authenticated(const QString& verifiedFingerprint);
    void disconnected();
    void setWorkspaceId(int workspaceId);
    void markCurrent();

signals:
    void authenticatedSessionEstablished(bool sameHost);
    void sameHostSessionRestored();
    void authenticatedSessionLost();
    void hostIdentityChanged();
    void workspaceGenerationChanged(quint64 generation, int workspaceId);
    void dataStateChanged(RemoteSessionState::DataState state);
    void operationalStateMustClear();
    void operationalStateShouldRefresh();

private:
    void setDataState(DataState state);
    void advanceWorkspaceGeneration(int workspaceId);

    QString m_hostIdentity;
    quint64 m_sessionGeneration = 0;
    quint64 m_workspaceGeneration = 0;
    int m_workspaceId = 0;
    bool m_authenticated = false;
    bool m_hadAuthenticatedSession = false;
    DataState m_dataState = DataState::NeverLoaded;
};
