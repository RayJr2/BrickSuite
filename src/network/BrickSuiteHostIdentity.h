/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

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
