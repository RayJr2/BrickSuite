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
