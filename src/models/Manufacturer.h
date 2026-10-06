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

class Manufacturer
{
public:
    int id() const { return m_id; }
    void setId(int value) { m_id = value; }

    QString code() const { return m_code; }
    void setCode(const QString& value) { m_code = value; }

    QString name() const { return m_name; }
    void setName(const QString& value) { m_name = value; }

    QString websiteUrl() const { return m_websiteUrl; }
    void setWebsiteUrl(const QString& value) { m_websiteUrl = value; }

    bool supportsLegoElementIds() const { return m_supportsLegoElementIds; }
    void setSupportsLegoElementIds(bool value) { m_supportsLegoElementIds = value; }

    bool isActive() const { return m_isActive; }
    void setIsActive(bool value) { m_isActive = value; }

    QString notes() const { return m_notes; }
    void setNotes(const QString& value) { m_notes = value; }

    QDateTime createdUtc() const { return m_createdUtc; }
    void setCreatedUtc(const QDateTime& value) { m_createdUtc = value; }

    QDateTime modifiedUtc() const { return m_modifiedUtc; }
    void setModifiedUtc(const QDateTime& value) { m_modifiedUtc = value; }

    QString origin() const { return m_origin; }
    void setOrigin(const QString& value) { m_origin = value; }

private:
    int m_id = 0;
    QString m_code;
    QString m_name;
    QString m_websiteUrl;
    bool m_supportsLegoElementIds = false;
    bool m_isActive = true;
    QString m_notes;
    QDateTime m_createdUtc;
    QDateTime m_modifiedUtc;
    QString m_origin = QStringLiteral("User");
};
