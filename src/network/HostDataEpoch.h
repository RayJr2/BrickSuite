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

#include <QString>

// Installation-local operational database lineage. Stored outside BrickSuite.db,
// independent of TLS identity, and deliberately not treated as a secret.
class HostDataEpoch
{
public:
    struct LoadResult {
        bool success = false;
        bool bootstrapped = false;
        QString epoch;
        QString error;
    };

    static QString storageDirectory();
    static LoadResult loadOrBootstrap(const QString& directory = QString());
    static LoadResult loadExisting(const QString& directory = QString());
    static QString current();
    static QString create();
    static bool persist(const QString& epoch, QString* error = nullptr,
                        const QString& directory = QString());
    static bool isValid(const QString& epoch);

private:
    static LoadResult load(const QString& directory, bool permitBootstrap);
};
