#pragma once

#include <QString>

namespace BuildActionEligibility {

inline bool supportsStockFulfillment(bool active,
                                     const QString& inventoryMode,
                                     const QString& status)
{
    return active
           && inventoryMode == QStringLiteral("Stock")
           && (status == QStringLiteral("Planned")
               || status == QStringLiteral("Pulling"));
}

} // namespace BuildActionEligibility
