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

#include "../../models/PartReferenceEntry.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class PartReferenceManifest
{
public:
    static constexpr int ExpectedEntryCount = 2985;
    static constexpr int ExpectedCatalogCount = 38;

    bool load(QString* errorMessage = nullptr);

    bool isLoaded() const;
    int entryCount() const;

    const QList<PartReferenceEntry>& entries() const;
    QStringList catalogs() const;
    QList<PartReferenceEntry> entriesForCatalog(const QString& catalog) const;
    QList<PartReferenceEntry> search(const QString& text) const;
    const PartReferenceEntry* findByPartNumber(const QString& partNumber) const;

private:
    void clear();
    static QString normalizedKey(const QString& value);

    QList<PartReferenceEntry> m_entries;
    QStringList m_catalogs;
    QHash<QString, int> m_entryIndexByPartNumber;
    bool m_loaded = false;
};
