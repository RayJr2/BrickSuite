/* BrickSuite - The Digital Twin Platform for Your Brick Workshop */
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
