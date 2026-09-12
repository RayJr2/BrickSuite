#pragma once

#include "BrickSuiteConnectionState.h"
#include "BrickSuiteProtocol.h"
#include "OperationalInvalidation.h"

#include <QHash>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>
#include <QPointer>
#include <functional>

class BrickSuiteWebSocketClient : public QObject
{
    Q_OBJECT
public:
    explicit BrickSuiteWebSocketClient(QObject* parent = nullptr);
    ~BrickSuiteWebSocketClient() override;

    void configure(const QUrl& endpoint, const QString& trustedFingerprint,
                   const QString& accessToken, bool reconnectAutomatically);
    void connectToHost();
    void disconnectFromHost();
    BrickSuiteConnectionStatus status() const;
    QString presentedFingerprint() const;
    QJsonObject capabilities() const;
    QString sendRequest(const QString& operation, const QJsonObject& payload = {});
    using Completion = std::function<void(const QJsonObject&)>;
    using Failure = std::function<void(const BrickSuiteProtocol::Error&)>;
    QString sendRequest(const QString& operation, const QJsonObject& payload,
                        QObject* context, Completion completion, Failure failure = {},
                        int timeoutMs = BrickSuiteProtocol::RequestTimeoutMs);
    bool supportsOperation(const QString& operation) const;
    bool supportsCapability(const QString& capability) const;
    quint64 authenticatedSessionGeneration() const;
    QString dataEpoch() const { return m_dataEpoch; }
    bool dataEpochSupported() const { return m_dataEpochSupported; }
#ifdef BRICKSUITE_TESTING
    void sendProtocolEventForTesting(const OperationalInvalidation& invalidation);
    int pendingRequestCountForTesting() const { return m_pending.size(); }
    QAbstractSocket::SocketState socketStateForTesting() const { return m_socket.state(); }
    bool authenticatedForTesting() const { return m_authenticated; }
    bool reconnectTimerActiveForTesting() const { return m_reconnectTimer.isActive(); }
    int reconnectAttemptForTesting() const { return m_reconnectAttempt; }
    int reconnectScheduleCountForTesting() const { return m_reconnectScheduleCount; }
    int transportDisconnectCountForTesting() const { return m_transportDisconnectCount; }
#endif

signals:
    void statusChanged(const BrickSuiteConnectionStatus& status);
    void trustRequired(const QString& fingerprint);
    void requestCompleted(const QString& requestId, const QJsonObject& payload);
    void requestFailed(const QString& requestId, const BrickSuiteProtocol::Error& error);
    void testConnectionCompleted(bool success, const QString& message);
    void authenticatedSessionEstablished(const QString& verifiedFingerprint);
    void authenticatedSessionEstablishedWithEpoch(const QString& verifiedFingerprint,
                                                   const QString& dataEpoch,
                                                   bool epochSupported);
    void authenticatedSessionLost();
    void invalidationReceived(const OperationalInvalidation& invalidation,
                              quint64 authenticatedSessionGeneration);
    void hostMaintenanceEnded();

private:
    struct Pending {
        QString operation;
        QTimer* timer = nullptr;
        QPointer<QObject> context;
        Completion completion;
        Failure failure;
    };

    void setStatus(BrickSuiteConnectionState state, const QString& message);
    void handleConnected();
    void handleDisconnected();
    void handleSslErrors(const QList<QSslError>& errors);
    void handleText(const QString& text);
    void handleResponse(const BrickSuiteProtocol::Message& message);
    void failPending(const QString& code, const QString& message, bool retryable);
    void scheduleReconnect();
    void sendHello();
    void sendAuthentication(const BrickSuiteProtocol::Message& hello);
    QString enqueueRequest(const QString& operation, const QJsonObject& payload,
                           QObject* context, Completion completion, Failure failure,
                           int timeoutMs);

    QWebSocket m_socket;
    QUrl m_endpoint;
    QString m_trustedFingerprint;
    QString m_accessToken;
    QString m_presentedFingerprint;
    BrickSuiteConnectionStatus m_status;
    QHash<QString, Pending> m_pending;
    QJsonObject m_capabilities;
    QTimer m_reconnectTimer;
    bool m_reconnectAutomatically = false;
    bool m_explicitDisconnect = true;
    int m_reconnectAttempt = 0;
    QElapsedTimer m_connectTimer;
    bool m_authenticated = false;
    quint64 m_authenticatedSessionGeneration = 0;
    QString m_dataEpoch;
    bool m_dataEpochSupported = false;
#ifdef BRICKSUITE_TESTING
    int m_reconnectScheduleCount = 0;
    int m_transportDisconnectCount = 0;
#endif
};

Q_DECLARE_METATYPE(BrickSuiteConnectionStatus)
Q_DECLARE_METATYPE(BrickSuiteProtocol::Error)
