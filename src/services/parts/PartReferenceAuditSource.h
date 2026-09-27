#pragma once
#include "../geometry/print/BatchPrintableModelService.h"

// Read-only snapshot of the same installed manifest and effective local entries
// used by Part Reference. Safe to call from an audit worker thread.
class PartReferenceAuditSource {
public:
    static PrintGeometry::BatchPrintCorpus load(const QString& databasePath,const QString& libraryRoot);
};
