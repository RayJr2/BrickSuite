#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QtGlobal>
#include <limits>

class BrickSuiteWebSocketClient;
class BrickSuiteWebSocketServer;

class RebrickableCoordinationService : public QObject
{
    Q_OBJECT
public:
    struct State {
        bool coordinated = false;
        int participantCount = 0;
        int coordinatedIntervalMs = 0;
        quint64 revision = 0;
    };

    static constexpr int ProviderBaseIntervalMs = 1000;
    static constexpr int SafetyMarginMs = 250;
    static constexpr int RemovalGraceMs = 10000;
    static constexpr int FallbackGraceMs = 10000;
    static const QString Capability;
    static const QString RegisterOperation;
    static const QString StatusOperation;
    static const QString ChangedEvent;

    explicit RebrickableCoordinationService(BrickSuiteWebSocketServer& server,
                                             BrickSuiteWebSocketClient& client,
                                             QObject* parent = nullptr);
    static int coordinatedIntervalForParticipants(int participants)
    {
        if (participants <= 0) return 0;
        const qint64 value = qint64(ProviderBaseIntervalMs) * participants + SafetyMarginMs;
        return int(qMin<qint64>(value, std::numeric_limits<int>::max()));
    }
    static int effectiveInterval(int localIntervalMs, int coordinatedIntervalMs)
    { return qMax(localIntervalMs, qMax(0, coordinatedIntervalMs)); }
    static bool localIntervalEditable(int participantCount)
    { return participantCount <= 1; }

    State state() const { return m_state; }
    void configureHost(bool active, bool participating);
    void configureRemote(bool active, bool participating);
    void sessionDisconnected(const QString& sessionId);

#ifdef BRICKSUITE_TESTING
    void registerParticipantForTesting(const QString& sessionId, const QString& deviceId,
                                       bool participating);
    void expireRemovalGraceForTesting(const QString& deviceId);
    void expireFallbackGraceForTesting();
#endif

signals:
    void stateChanged(const RebrickableCoordinationService::State& state);

private:
    void registerOperations();
    void registerParticipant(const QString& sessionId, const QString& deviceId, bool participating);
    void removeSession(const QString& sessionId);
    void recalculateHost();
    void applyRemoteState(const QJsonObject& payload);
    void publishHostState();
    QJsonObject statePayload() const;
    static bool decodeState(const QJsonObject& payload, State* state);
    void beginFallback();

    BrickSuiteWebSocketServer& m_server;
    BrickSuiteWebSocketClient& m_client;
    bool m_hostActive = false;
    bool m_hostParticipating = false;
    bool m_remoteActive = false;
    bool m_remoteParticipating = false;
    State m_state;
    QHash<QString, QString> m_sessionDevices;
    QHash<QString, QSet<QString>> m_deviceSessions;
    QHash<QString, QTimer*> m_removalTimers;
    QTimer m_fallbackTimer;
};

Q_DECLARE_METATYPE(RebrickableCoordinationService::State)
