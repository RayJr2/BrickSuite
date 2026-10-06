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

#include <QDateTime>
#include <QString>
#include <QVector>
#include <optional>

struct PairedDeviceRecord
{
    QString deviceId;
    QString friendlyName;
    QString firstPairedUtc;
    QString lastSeenUtc;
    QString clientVersion;
    QString platform;
    QString credentialReference;
    bool active = true;
};

class PairedDeviceRegistry
{
public:
    explicit PairedDeviceRegistry(QString path = QString());

    static QString defaultPath();
    bool load(QString* error = nullptr);
    bool add(const PairedDeviceRecord& record, QString* error = nullptr);
    bool updateLastSeen(const QString& deviceId, const QDateTime& utc,
                        QString* error = nullptr);
    bool rename(const QString& deviceId, const QString& friendlyName,
                QString* error = nullptr);
    bool deactivate(const QString& deviceId, QString* error = nullptr);
    bool deactivateAll(QString* error = nullptr);
    bool remove(const QString& deviceId, QString* error = nullptr);
    bool removeAllInactive(QString* error = nullptr);
    std::optional<PairedDeviceRecord> find(const QString& deviceId) const;
    QVector<PairedDeviceRecord> devices() const { return m_devices; }
    QString path() const { return m_path; }
    bool available() const { return m_available; }
    QString lastError() const { return m_lastError; }

private:
    bool save(QString* error);
    QString m_path;
    QVector<PairedDeviceRecord> m_devices;
    bool m_available = true;
    QString m_lastError;
};
