#pragma once

#include "BrickSuiteHostIdentity.h"
#include "BrickSuiteProtocol.h"
#include "BrickSuiteOperationDispatcher.h"
#include "OperationalInvalidation.h"

#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QTimer>

class QWebSocket;
class QWebSocketServer;

class BrickSuiteWebSocketServer : public QObject
{
    Q_OBJECT
public:
    explicit BrickSuiteWebSocketServer(QObject* parent = nullptr);
    ~BrickSuiteWebSocketServer() override;

    bool start(const QHostAddress& address, quint16 port, const QString& accessToken,
               QString* error = nullptr);
    bool startWithIdentity(const QHostAddress& address, quint16 port,
                           const QString& accessToken,
                           const BrickSuiteHostIdentity::Result& identity,
                           QString* error = nullptr);
    void stop();
    bool isListening() const;
    quint16 serverPort() const;
    QString fingerprint() const;
    int authenticatedClientCount() const;
    BrickSuiteOperationDispatcher& operationDispatcher();
    int broadcastInvalidation(OperationalInvalidation invalidation);
    void setOperationalAdmissionOpen(bool open);
    bool operationalAdmissionOpen() const { return m_operationalAdmissionOpen; }
    void broadcastFullOperationalInvalidation();

signals:
    void statusChanged();
    void serverError(const QString& message);

private:
    struct Session {
        QByteArray id;
        QByteArray challenge;
        qint64 challengeCreatedMs = 0;
        qint64 connectedMs = 0;
        bool challengeConsumed = false;
        bool authenticated = false;
        bool invalidationsReady = false;
        int authenticationFailures = 0;
        qint64 nextAuthenticationAllowedMs = 0;
        QTimer* authenticationTimer = nullptr;
        int protocolMinor = 0;
    };

    void acceptConnection();
    void receiveText(QWebSocket* socket, const QString& text);
    void dispatch(QWebSocket* socket, const BrickSuiteProtocol::Message& request);
    void send(QWebSocket* socket, const BrickSuiteProtocol::Message& message);
    void closeSession(QWebSocket* socket);
    void reject(QWebSocket* socket, const BrickSuiteProtocol::Message& request,
                const QString& code, const QString& message, bool retryable = false);

    QWebSocketServer* m_server = nullptr;
    QHash<QWebSocket*, Session> m_sessions;
    QString m_accessToken;
    BrickSuiteHostIdentity::Result m_identity;
    BrickSuiteOperationDispatcher m_dispatcher;
    quint64 m_nextInvalidationSequence = 1;
    bool m_operationalAdmissionOpen = true;
};
