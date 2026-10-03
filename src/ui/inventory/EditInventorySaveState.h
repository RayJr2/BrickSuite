#pragma once

namespace EditInventorySaveState
{
inline bool canSave(bool recordLoaded, bool knownColorsLoading, int colorId, int quantity = 1)
{
    return recordLoaded && (quantity == 0 || (quantity > 0 && !knownColorsLoading && colorId > 0));
}
}
