#pragma once

namespace EditInventorySaveState
{
inline bool canSave(bool recordLoaded, bool knownColorsLoading, int colorId)
{
    return recordLoaded && !knownColorsLoading && colorId > 0;
}
}
