#include "InventoryBuildability.h"

namespace {
int percent(int value, int total)
{
    return total > 0 ? qBound(0, qRound(100.0 * value / total), 100) : 0;
}
}

int InventoryBuildabilitySetResult::loosePercent() const
{
    return percent(looseSatisfiedQuantity, totalQuantity);
}

int InventoryBuildabilitySetResult::advisoryPercent() const
{
    return percent(advisorySatisfiedQuantity, totalQuantity);
}
