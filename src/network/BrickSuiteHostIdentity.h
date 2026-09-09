#pragma once

#include <QSslCertificate>
#include <QSslKey>
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
        QString error;
    };

    static Result loadOrCreate();
    static Result regenerate();
    static Result generateEphemeral();
    static QString certificatePath();
    static QString fingerprint(const QSslCertificate& certificate);
    static QString normalizedFingerprint(const QString& fingerprint);

private:
    static Result createAndPersist();
    static Result generate(bool persist);
};
