#include "RemoteSessionState.h"

#include <QDebug>
#include <QSettings>

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
QString RemoteSessionState::dataEpoch() const { return m_dataEpoch; }
bool RemoteSessionState::dataEpochSupported() const { return m_dataEpochSupported; }
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

bool RemoteSessionState::acceptsHostGlobal(const Snapshot& value) const
{
    return m_authenticated && value.hostIdentity == m_hostIdentity
        && value.sessionGeneration == m_sessionGeneration;
}

bool RemoteSessionState::acceptsEvent(
    quint64 sessionGeneration, const std::optional<qint64>& workspaceId) const
{
    return m_authenticated && sessionGeneration == m_sessionGeneration
        && (!workspaceId || *workspaceId == m_workspaceId);
}

void RemoteSessionState::authenticated(const QString& verifiedFingerprint)
{
    authenticatedWithEpoch(verifiedFingerprint, QString(), false);
}

void RemoteSessionState::authenticatedWithEpoch(const QString& verifiedFingerprint,
                                                const QString& dataEpoch,
                                                bool epochSupported)
{
    const QString identity = normalizedFingerprint(verifiedFingerprint);
    if (identity.isEmpty()) return;

    const bool hadSession = m_hadAuthenticatedSession;
    const bool sameHost = !m_hostIdentity.isEmpty() && identity == m_hostIdentity;
    const bool changedHost = !m_hostIdentity.isEmpty() && !sameHost;
    QSettings settings;
    settings.beginGroup(QStringLiteral("BrickSuiteNetwork/RetainedDataEpoch"));
    const bool retainedPresent = settings.contains(identity);
    const QString retainedEpoch = settings.value(identity).toString();
    const bool changedEpoch = epochSupported
        && (!retainedPresent || retainedEpoch != dataEpoch);
    const bool lostEpochSupport = !epochSupported && !retainedEpoch.isEmpty();
    if (epochSupported) settings.setValue(identity, dataEpoch);
    settings.endGroup();
    ++m_sessionGeneration;
    m_authenticated = true;
    m_hadAuthenticatedSession = true;

    if (changedHost || changedEpoch || lostEpochSupport) {
        m_hostIdentity = identity;
        m_dataEpoch = dataEpoch;
        m_dataEpochSupported = epochSupported;
        advanceWorkspaceGeneration(0);
        setDataState(DataState::NeverLoaded);
        emit hostIdentityChanged();
        emit operationalStateMustClear();
        if (changedEpoch && retainedPresent) emit dataEpochChanged();
    } else {
        m_hostIdentity = identity;
        m_dataEpoch = dataEpoch;
        m_dataEpochSupported = epochSupported;
    }

    qDebug().noquote() << "Remote session generation" << m_sessionGeneration
                       << (sameHost ? "restored for Host" : "established for Host")
                       << m_hostIdentity.left(12);

    emit authenticatedSessionEstablished(sameHost);
    if (hadSession && sameHost && !changedEpoch && !lostEpochSupport) {
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
