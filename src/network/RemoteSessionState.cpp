#include "RemoteSessionState.h"

#include <QDebug>

namespace {
QString normalizedFingerprint(const QString& value)
{
    QString normalized;
    for (const QChar character : value) {
        const QChar upper = character.toUpper();
        if (character.isDigit() || (upper >= QLatin1Char('A') && upper <= QLatin1Char('F')))
            normalized.append(upper);
    }
    return normalized.size() == 64 ? normalized : QString();
}
}

RemoteSessionState::RemoteSessionState(QObject* parent) : QObject(parent) {}

QString RemoteSessionState::hostIdentity() const { return m_hostIdentity; }
quint64 RemoteSessionState::sessionGeneration() const { return m_sessionGeneration; }
quint64 RemoteSessionState::workspaceGeneration() const { return m_workspaceGeneration; }
int RemoteSessionState::workspaceId() const { return m_workspaceId; }
bool RemoteSessionState::isAuthenticated() const { return m_authenticated; }
RemoteSessionState::DataState RemoteSessionState::dataState() const { return m_dataState; }

RemoteSessionState::Snapshot RemoteSessionState::snapshot() const
{
    return {m_hostIdentity, m_sessionGeneration, m_workspaceGeneration, m_workspaceId};
}

bool RemoteSessionState::accepts(const Snapshot& value) const
{
    return m_authenticated && value.hostIdentity == m_hostIdentity
        && value.sessionGeneration == m_sessionGeneration
        && value.workspaceGeneration == m_workspaceGeneration
        && value.workspaceId == m_workspaceId;
}

void RemoteSessionState::authenticated(const QString& verifiedFingerprint)
{
    const QString identity = normalizedFingerprint(verifiedFingerprint);
    if (identity.isEmpty()) return;

    const bool hadSession = m_hadAuthenticatedSession;
    const bool sameHost = !m_hostIdentity.isEmpty() && identity == m_hostIdentity;
    const bool changedHost = !m_hostIdentity.isEmpty() && !sameHost;
    ++m_sessionGeneration;
    m_authenticated = true;
    m_hadAuthenticatedSession = true;

    if (changedHost) {
        m_hostIdentity = identity;
        advanceWorkspaceGeneration(0);
        setDataState(DataState::NeverLoaded);
        emit hostIdentityChanged();
        emit operationalStateMustClear();
    } else {
        m_hostIdentity = identity;
    }

    qDebug().noquote() << "Remote session generation" << m_sessionGeneration
                       << (sameHost ? "restored for Host" : "established for Host")
                       << m_hostIdentity.left(12);

    emit authenticatedSessionEstablished(sameHost);
    if (hadSession && sameHost) {
        emit sameHostSessionRestored();
        emit operationalStateShouldRefresh();
    }
}

void RemoteSessionState::disconnected()
{
    if (!m_authenticated) return;
    ++m_sessionGeneration;
    m_authenticated = false;
    setDataState(m_hostIdentity.isEmpty() ? DataState::NeverLoaded
                                         : DataState::StaleDisconnected);
    qDebug() << "Remote session lost; generation" << m_sessionGeneration;
    emit authenticatedSessionLost();
}

void RemoteSessionState::setWorkspaceId(int workspaceId)
{
    workspaceId = qMax(0, workspaceId);
    if (workspaceId == m_workspaceId) return;
    advanceWorkspaceGeneration(workspaceId);
}

void RemoteSessionState::markCurrent()
{
    if (m_authenticated) setDataState(DataState::Current);
}

void RemoteSessionState::setDataState(DataState state)
{
    if (m_dataState == state) return;
    m_dataState = state;
    emit dataStateChanged(state);
}

void RemoteSessionState::advanceWorkspaceGeneration(int workspaceId)
{
    m_workspaceId = workspaceId;
    ++m_workspaceGeneration;
    qDebug() << "Remote Workspace generation" << m_workspaceGeneration
             << "Workspace" << m_workspaceId;
    emit workspaceGenerationChanged(m_workspaceGeneration, m_workspaceId);
}
