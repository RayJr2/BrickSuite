#pragma once

#include "../../models/Manufacturer.h"
#include "../../repositories/ManufacturerRepository.h"

#include <QString>

class ManufacturerManagementService
{
public:
    enum class Error { None, DuplicateCode, DuplicateName, InvalidInput,
                       ProtectedOperation, NotFound, DatabaseFailure };
    struct Result { bool success = false; Error error = Error::None; QString message; int manufacturerId = 0; };
    struct UsageResult { bool success = false; QString message; ManufacturerUsage usage; };

    Result create(Manufacturer manufacturer) const;
    Result edit(Manufacturer manufacturer) const;
    Result setActive(int manufacturerId, bool active) const;
    UsageResult usage(int manufacturerId) const;

private:
    Result validateIdentity(const Manufacturer& manufacturer, int excludeId) const;
};
