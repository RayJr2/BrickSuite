/* BrickSuite - The Digital Twin Platform for Your Brick Workshop */
#pragma once

#include "../../models/PartReferenceEntry.h"
#include "../../models/UserPartReferenceEntry.h"
#include "PartReferenceManifest.h"
#include <QList>

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

    QList<PartReferenceEntry> effectiveEntries(QString* errorMessage = nullptr) const;
    QList<PartReferenceDestination> destinations() const;
    PartReferenceCustomizationResult add(int partId, const QString& catalog,
                                         const QString& section,
                                         PartReferencePlacement placement,
                                         const QString& anchorPartNumber = QString()) const;
    PartReferenceCustomizationResult remove(int userEntryId) const;
    static bool isStructuredCatalog(const QString& catalog);

private:
    const PartReferenceManifest& m_manifest;
};
