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

#include "PartReferenceAuditSource.h"
#include "PartReferenceManifest.h"
#include "PartReferenceCustomizationService.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QUuid>

PrintGeometry::BatchPrintCorpus PartReferenceAuditSource::load(const QString& databasePath,const QString& libraryRoot)
{
    using namespace PrintGeometry;
    BatchPrintCorpus result;PartReferenceManifest manifest;
    if(!manifest.load(&result.diagnostic))return result;
    const auto catalog=BatchPrintableModelService::loadCatalog(databasePath,&result.diagnostic);
    if(!result.diagnostic.isEmpty())return result;
    const QString name=QStringLiteral("reference-audit-")+QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        auto database=QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),name);
        database.setDatabaseName(databasePath);database.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if(!database.open())result.diagnostic=database.lastError().text();
        else {
            const auto entries=PartReferenceCustomizationService(manifest,database).effectiveEntries(&result.diagnostic);
            if(result.diagnostic.isEmpty())result=BatchPrintableModelService::partReferenceCorpus(entries,catalog,libraryRoot);
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(name);return result;
}
