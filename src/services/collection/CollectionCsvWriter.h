#pragma once
#include "../../models/export/CollectionExportTypes.h"
class CollectionCsvWriter { public: struct Result{bool success=false;QString message;QString csv;};static Result generate(const CollectionExportProjection&);static Result write(const QString&,const CollectionExportProjection&);};
