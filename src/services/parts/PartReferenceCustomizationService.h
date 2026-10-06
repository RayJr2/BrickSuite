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
#include "../../models/UserPartReferenceEntry.h"
#include "PartReferenceManifest.h"
#include <QList>

class QSqlDatabase;
class QThread;

struct PartReferenceDestination
{
    QString catalog;
    QString section;
    bool structured = false;
};

struct PartReferenceCustomizationResult
{
    bool success = false;
    QString message;
    int userEntryId = 0;
};

class PartReferenceCustomizationService
{
public:
    explicit PartReferenceCustomizationService(const PartReferenceManifest& manifest);
    PartReferenceCustomizationService(const PartReferenceManifest& manifest,
                                      const QSqlDatabase& database);

    QList<PartReferenceEntry> effectiveEntries(QString* errorMessage = nullptr) const;
    QList<PartReferenceDestination> destinations() const;
    PartReferenceCustomizationResult add(int partId, const QString& catalog,
                                         const QString& section,
                                         PartReferencePlacement placement,
                                         const QString& anchorPartNumber = QString()) const;
    PartReferenceCustomizationResult remove(int userEntryId) const;
    static bool isStructuredCatalog(const QString& catalog);

private:
    QSqlDatabase serviceDatabase() const;
    const PartReferenceManifest& m_manifest;
    QString m_connectionName;
    QThread* m_ownerThread = nullptr;
};
