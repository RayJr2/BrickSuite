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
 */

#pragma once

#include <QString>

class CredentialStore
{
public:
    struct ReadResult
    {
        bool success = false;
        bool found = false;
        QString value;
        QString error;
    };

    static ReadResult read(const QString& credentialName);

    static bool write(const QString& credentialName,
                      const QString& value,
                      QString* error = nullptr);

    static bool remove(const QString& credentialName,
                       QString* error = nullptr);

    static QString backendName();

private:
    CredentialStore() = delete;
};
