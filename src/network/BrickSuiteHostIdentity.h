#pragma once

#include <QSslCertificate>
#include <QSslKey>
#include <QDateTime>
#include <QString>

class BrickSuiteHostIdentity
{
public:
    struct Result
    {
        bool success = false;
        QSslCertificate certificate;
        QSslKey privateKey;
        QString fingerprint;
        QDateTime validFrom;
        QDateTime expiresUtc;
        QString error;
    };

    static Result loadOrCreate();
    static Result regenerate();
    static Result generateEphemeral();
#ifdef BRICKSUITE_TESTING
    static Result generateEphemeralForTesting(qint64 notBeforeOffsetSeconds,
                                              qint64 notAfterOffsetSeconds);
#endif
    static Result validate(const QSslCertificate& certificate, const QSslKey& privateKey);
    static QString certificatePath();
    static QString fingerprint(const QSslCertificate& certificate);
    static QString normalizedFingerprint(const QString& fingerprint);

private:
    static Result createAndPersist();
    static Result generate(bool persist, qint64 notBeforeOffsetSeconds = -300,
                           qint64 notAfterOffsetSeconds = 10LL * 365 * 24 * 60 * 60);
};
